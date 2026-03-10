import * as crypto from 'crypto';
import * as fs from 'fs';
import * as path from 'path';
import { commands } from "../util/Commands";
import { wfs } from "../util/FileSystem";
import { resfp } from '../util/FileTree';
import { ipaths } from "../util/Paths";
import { wsys } from "../util/System";
import { term } from '../util/Terminal';
import { util } from '../util/Util';
import { Addon } from "./Addon";
import { BunnyCdn } from "./BunnyCdn";
import {
    buildChunkedReleasePublishLayout,
    createChunkedRelease,
    verifyChunkedRelease as verifyChunkedReleaseOutput,
} from "./ChunkedRelease";
import { Datascripts } from "./Datascripts";
import { Dataset } from "./Dataset";
import { Identifier } from "./Identifiers";
import { NodeConfig } from "./NodeConfig";
import { createReleaseManifest, ReleaseArtifact, ReleaseManifest } from "./ReleaseManifest";
import {
    readSupplementaryPatchConfig,
    resolveLocalPackageOutputPath,
    resolveTargetOutputPath,
    ResolvedSupplementaryPatchConfig,
} from "./SupplementaryPatches";

export interface PackageMeta {
    size: number;
    md5s: string[];
    filename: string;
    chunkSize: number;
}

type Entry = { src: string; dst: string };
type MPQCompression = 'zlib' | 'pkware' | 'bzip2' | 'sparse' | 'lzma';
type PackagedArtifact = ReleaseArtifact & { packageName: string };

/** Partition entries into segments each under maxBytes (estimated by src file size). */
function partitionEntriesBySize(entries: Entry[], maxBytes: number): Entry[][] {
    if (entries.length === 0 || maxBytes <= 0) {
        return entries.length === 0 ? [] : [entries];
    }
    const segments: Entry[][] = [];
    let current: Entry[] = [];
    let currentSize = 0;
    for (const entry of entries) {
        let size = 0;
        try {
            size = wfs.stat(entry.src).size;
        } catch {
            size = 0;
        }
        if (currentSize + size > maxBytes && current.length > 0) {
            segments.push(current);
            current = [];
            currentSize = 0;
        }
        current.push(entry);
        currentSize += size;
    }
    if (current.length > 0) {
        segments.push(current);
    }
    return segments;
}

/** Ensure MPQ/target name has a "patch-" prefix (e.g. "A" -> "patch-A", "patch-B.MPQ" unchanged). */
function ensurePatchPrefix(name: string): string {
    const n = name.trim();
    if (/^patch-/i.test(n)) {
        return n;
    }
    return 'patch-' + (n.charAt(0).toUpperCase() + n.slice(1).toLowerCase().replace(/\.mpq$/i, ''));
}

/** Return output name for an MPQ segment. When splitting: patch-A01, patch-A02, etc. When single segment: original name (e.g. patch-A). */
function segmentOutputName(baseMpqName: string, segmentIndex: number, totalSegments: number): string {
    const base = ensurePatchPrefix(baseMpqName.replace(/\.mpq$/i, ''));
    const hasMpqSuffix = baseMpqName.toLowerCase().endsWith('.mpq');
    if (totalSegments === 1 && segmentIndex === 0) {
        return hasMpqSuffix ? `${base}.MPQ` : base;
    }
    const segmentNum = String(segmentIndex + 1).padStart(2, '0');
    const suffix = hasMpqSuffix ? '.MPQ' : '.MPQ';
    return `${base}${segmentNum}${suffix}`;
}

export class Package {
    private static sanitizeFileName(value: string) {
        return value.replace(/[\\/:*?"<>|]+/g, '-');
    }

    private static normalizeCompression(value: string): MPQCompression {
        switch (value.trim().toLowerCase()) {
            case 'zlib':
                return 'zlib';
            case 'pkware':
            case 'pkzip':
                return 'pkware';
            case 'bzip2':
                return 'bzip2';
            case 'sparse':
                return 'sparse';
            case 'lzma':
                return 'lzma';
            default:
                throw new Error(
                    `Unsupported MPQ compression "${value}". `
                    + `Expected one of: zlib, pkware, pkzip, bzip2, sparse, lzma.`
                );
        }
    }

    private static extractCompressionArg(args: string[]) {
        const filtered: string[] = [];
        let compression: MPQCompression | undefined = undefined;

        for (let i = 0; i < args.length; ++i) {
            const arg = args[i];
            const lower = arg.toLowerCase();

            if (lower === '--compression') {
                const value = args[i + 1];
                if (value === undefined) {
                    throw new Error('Missing value for --compression');
                }
                compression = this.normalizeCompression(value);
                i += 1;
                continue;
            }

            if (lower.startsWith('--compression=')) {
                compression = this.normalizeCompression(arg.substring('--compression='.length));
                continue;
            }

            filtered.push(arg);
        }

        return { args: filtered, compression };
    }

    private static buildArchive(outputPath: string, entries: Entry[], folder: boolean, compression?: MPQCompression) {
        if (entries.length === 0) {
            throw new Error(`Refusing to build an MPQ with no files: ${outputPath}`);
        }

        const listfilePath = path.join(resfp(ipaths.bin.package), `${path.basename(outputPath)}.listfile`);
        const listfileContent = entries
            .map(entry => `${path.resolve(entry.src)}\t${entry.dst.replace(/\//g, '\\').trim()}`)
            .join('\n') + '\n';

        wfs.write(listfilePath, listfileContent);
        wfs.mkDirs(path.dirname(outputPath));

        wsys.exec(
            `"${ipaths.bin.mpqbuilder.mpqbuilder_exe.get()}" "${path.resolve(listfilePath)}" "${path.resolve(outputPath)}"`
            + (compression !== undefined ? ` "${compression}"` : ''),
            'inherit'
        );

        if (folder) {
            const mirrorDirPath = outputPath.replace(/\.[^.]+$/, '');
            wfs.mkDirs(mirrorDirPath, true);
            for (const entry of entries) {
                const destPath = path.join(mirrorDirPath, entry.dst.replace(/[\\/]+/g, path.sep));
                wfs.mkDirs(path.dirname(destPath));
                wfs.copy(entry.src, destPath);
            }
        }
    }

    private static calculatePackageMeta(packageFile: string): PackageMeta {
        const chunkSize = NodeConfig.LauncherPatchChunkSize;
        const meta: PackageMeta = {
            md5s: [],
            size: wfs.stat(packageFile).size,
            filename: path.basename(packageFile),
            chunkSize,
        };

        const handle = fs.openSync(packageFile, 'r');
        try {
            const buf = Buffer.alloc(chunkSize);
            while (true) {
                const nread = fs.readSync(handle, buf, 0, chunkSize, null);
                if (nread === 0) {
                    break;
                }
                meta.md5s.push(
                    crypto.createHash('md5')
                        .update(nread < chunkSize ? buf.slice(0, nread) : buf)
                        .digest('hex')
                );
            }
        } finally {
            fs.closeSync(handle);
        }

        return meta;
    }

    private static writeLegacyMeta(dataset: Dataset, artifacts: PackagedArtifact[]) {
        if (artifacts.length === 0) {
            return;
        }

        const metas = artifacts.map(artifact => this.calculatePackageMeta(artifact.sourcePath));
        ipaths.package.join(`${dataset.fullName}.meta.json`).toFile().writeJson(metas);
    }

    private static stageReleaseArtifacts(dataset: Dataset, artifacts: ReleaseArtifact[]) {
        const stageDir = path.join(resfp(ipaths.package), `${dataset.fullName}.release`);
        wfs.mkDirs(stageDir, true);

        const stagedArtifacts: ReleaseArtifact[] = artifacts.map(artifact => {
            const stagedReleasePath = this.canonicalizeStagedReleasePath(artifact.releasePath);
            const stagePath = path.join(stageDir, stagedReleasePath.replace(/\//g, path.sep));
            wfs.mkDirs(path.dirname(stagePath));
            wfs.copy(artifact.sourcePath, stagePath);
            return {
                sourcePath: stagePath,
                releasePath: stagedReleasePath,
            };
        });

        return {
            stageDir,
            stagedArtifacts,
            manifestPath: path.join(stageDir, 'manifest.json'),
        };
    }

    private static stageReleaseDir(dataset: Dataset) {
        return path.join(resfp(ipaths.package), `${dataset.fullName}.release`);
    }

    private static readStagedReleaseArtifacts(dataset: Dataset) {
        const stageDir = this.stageReleaseDir(dataset);
        const manifestPath = path.join(stageDir, 'manifest.json');
        if (!wfs.exists(manifestPath)) {
            throw new Error(
                `Missing staged release manifest at ${manifestPath}. Run "package client ${dataset.fullName} --pipeline" first.`
            );
        }

        const manifest = JSON.parse(wfs.read(manifestPath)) as ReleaseManifest;
        return manifest.files.map(file => ({
            releasePath: file.path.replace(/\\/g, '/'),
            sourcePath: path.join(stageDir, file.path.replace(/\//g, path.sep)),
        }));
    }

    private static isBasePatchAReleasePath(releasePath: string) {
        const normalized = releasePath.replace(/\\/g, '/').toLowerCase().replace(/^\/+/, '');
        const baseName = path.posix.basename(normalized);
        const isBaseA =
            baseName === 'a.mpq'
            || baseName === 'patch-a.mpq'
            || baseName.endsWith('.a.mpq')
            || /^patch-a(-\d+)?\.mpq$/.test(baseName)
            || /^patch-a\d{2}\.mpq$/.test(baseName);
        if (!isBaseA) {
            return false;
        }
        return normalized === baseName || normalized.startsWith('data/');
    }

    private static canonicalizeStagedReleasePath(releasePath: string) {
        const normalized = releasePath.replace(/\\/g, '/');
        if (!this.isBasePatchAReleasePath(normalized)) {
            return normalized;
        }
        const baseName = path.posix.basename(normalized);
        const directory = path.posix.dirname(normalized);
        if (baseName === 'a.mpq' || baseName === 'patch-a.mpq') {
            return `${directory}/patch-A.MPQ`;
        }
        return normalized;
    }

    private static canonicalizeChunkedInstallPath(releasePath: string) {
        const normalized = this.canonicalizeStagedReleasePath(releasePath);
        if (normalized.includes('/')) {
            return normalized;
        }

        const baseName = path.posix.basename(normalized);
        if (!baseName.toLowerCase().endsWith('.mpq')) {
            return normalized;
        }

        return `Data/${baseName}`;
    }

    private static resolveChunkedReleaseInputs(dataset: Dataset, config: ResolvedSupplementaryPatchConfig) {
        const stagedArtifacts = this.readStagedReleaseArtifacts(dataset);
        const stagedByPath = new Map(stagedArtifacts.map(artifact => [artifact.releasePath, artifact]));
        const basePatchMatches = stagedArtifacts.filter(artifact => this.isBasePatchAReleasePath(artifact.releasePath));

        if (basePatchMatches.length === 0) {
            throw new Error(
                `Unable to find the base patch-A artifact (patch-A.MPQ or patch-A01.MPQ, patch-A02.MPQ, etc.) in ${this.stageReleaseDir(dataset)}. `
                + `Run "package client ${dataset.fullName} --pipeline" and ensure it produced the general patch.`
            );
        }

        const requiredPaths = new Set<string>(basePatchMatches.map(a => a.releasePath));
        stagedArtifacts
            .filter(a => !this.isBasePatchAReleasePath(a.releasePath))
            .forEach(a => requiredPaths.add(a.releasePath));

        return Array.from(requiredPaths)
            .sort((left, right) => left.localeCompare(right))
            .map(releasePath => {
                const artifact = stagedByPath.get(releasePath);
                if (!artifact || !wfs.isFile(artifact.sourcePath)) {
                    throw new Error(
                        `Missing required patch artifact ${releasePath} in ${this.stageReleaseDir(dataset)}. `
                        + `Run "package client ${dataset.fullName} --pipeline" and verify supplementary packaging completed successfully.`
                    );
                }
                return {
                    sourcePath: artifact.sourcePath,
                    releasePath: this.canonicalizeChunkedInstallPath(artifact.releasePath),
                };
            });
    }

    private static async uploadReleaseArtifacts(
        config: ResolvedSupplementaryPatchConfig,
        stagedArtifacts: ReleaseArtifact[],
        manifestPath: string,
        dryRun: boolean
    ) {
        const remotePath = (filePath: string) =>
            config.publishPrefix.length > 0
                ? `${config.publishPrefix}/${filePath.replace(/\\/g, '/')}`
                : filePath.replace(/\\/g, '/');

        await BunnyCdn.uploadFile(remotePath('manifest.json'), manifestPath, 'application/json', dryRun);
        for (const artifact of stagedArtifacts) {
            await BunnyCdn.uploadFile(
                remotePath(artifact.releasePath),
                artifact.sourcePath,
                'application/octet-stream',
                dryRun
            );
        }
    }

    private static chunkedReleaseDir(dataset: Dataset) {
        return path.join(resfp(ipaths.package), `${dataset.fullName}.chunked-release`);
    }

    private static chunkedReleaseManifestPath(dataset: Dataset) {
        return path.join(this.chunkedReleaseDir(dataset), 'manifest.json');
    }

    private static normalizeRemotePath(value: string) {
        return value.replace(/\\/g, '/').replace(/^\/+/, '');
    }

    private static resolveChunkedFallbackRemotePath(remoteVersionRoot: string, artifact: ReleaseArtifact) {
        const fileName = path.posix.basename(artifact.releasePath.replace(/\\/g, '/'));
        return this.normalizeRemotePath(`${remoteVersionRoot}/${fileName}`);
    }

    private static resolveChunkedPublishInputs(dataset: Dataset, config: ResolvedSupplementaryPatchConfig) {
        const manifestPath = this.chunkedReleaseManifestPath(dataset);
        if (!wfs.exists(manifestPath)) {
            throw new Error(
                `Missing chunked release manifest at ${manifestPath}. Run "package chunked ${dataset.fullName}" first.`
            );
        }

        const publishLayout = buildChunkedReleasePublishLayout(manifestPath, config.publishPrefix);
        const fallbackArtifacts = this.readStagedReleaseArtifacts(dataset).map(artifact => ({
            ...artifact,
            remotePath: this.resolveChunkedFallbackRemotePath(publishLayout.remoteVersionRoot, artifact),
        }));

        fallbackArtifacts.forEach(artifact => {
            if (!wfs.isFile(artifact.sourcePath)) {
                throw new Error(`Missing staged fallback artifact ${artifact.sourcePath}`);
            }
        });

        return {
            manifestPath,
            publishLayout,
            fallbackArtifacts,
        };
    }

    private static async uploadChunkedReleaseArtifacts(
        dataset: Dataset,
        config: ResolvedSupplementaryPatchConfig,
        dryRun: boolean
    ) {
        const { manifestPath, publishLayout, fallbackArtifacts } = this.resolveChunkedPublishInputs(dataset, config);
        const publishedManifestPath = path.join(publishLayout.outputDir, '.publish-manifest.json');
        const totalSteps = publishLayout.bundles.length + fallbackArtifacts.length + 2;
        let completedSteps = 0;
        const logUploadStep = (label: string, remotePath: string) => {
            term.log(
                'client',
                `Chunk publish ${completedSteps + 1}/${totalSteps}: ${label} -> ${remotePath}`
            );
        };

        wfs.write(publishedManifestPath, JSON.stringify(publishLayout.publishedManifest, null, 4));
        try {
            for (const bundle of publishLayout.bundles) {
                logUploadStep(`bundle ${bundle.bundleId}`, bundle.remotePath);
                await BunnyCdn.uploadFile(bundle.remotePath, bundle.localPath, 'application/octet-stream', dryRun);
                completedSteps += 1;
            }

            for (const artifact of fallbackArtifacts) {
                logUploadStep(`fallback ${artifact.releasePath}`, artifact.remotePath);
                await BunnyCdn.uploadFile(artifact.remotePath, artifact.sourcePath, 'application/octet-stream', dryRun);
                completedSteps += 1;
            }

            logUploadStep('versioned manifest', publishLayout.remoteManifestPath);
            await BunnyCdn.uploadFile(
                publishLayout.remoteManifestPath,
                publishedManifestPath,
                'application/json',
                dryRun
            );
            completedSteps += 1;
            logUploadStep('latest manifest', publishLayout.remoteLatestManifestPath);
            await BunnyCdn.uploadFile(
                publishLayout.remoteLatestManifestPath,
                publishedManifestPath,
                'application/json',
                dryRun
            );
            completedSteps += 1;

            term.log(
                'client',
                `Published chunked release ${publishLayout.version} with `
                + `${publishLayout.bundles.length} bundle(s), `
                + `${fallbackArtifacts.length} fallback file(s), and latest manifest from ${manifestPath}`
            );
        } finally {
            if (wfs.exists(publishedManifestPath)) {
                wfs.remove(publishedManifestPath);
            }
        }
    }

    private static buildLocalPackageArtifacts(
        dataset: Dataset,
        entriesByMpq: { [mpq: string]: Entry[] },
        folder: boolean,
        supplementaryConfig: ResolvedSupplementaryPatchConfig,
        compression?: MPQCompression
    ) {
        const artifacts: PackagedArtifact[] = [];
        const maxPatchFileSizeMB = NodeConfig.LauncherMaxPatchFileSizeMB ?? 0;
        const maxBytes = maxPatchFileSizeMB > 0 ? maxPatchFileSizeMB * 1024 * 1024 : 0;
        term.debug('client', `Packaging ${Object.keys(entriesByMpq).length} MPQ(s)`);

        for (const [mpq, entries] of Object.entries(entriesByMpq)) {
            const effectiveMpq = ensurePatchPrefix(mpq);
            const segments = maxBytes > 0 ? partitionEntriesBySize(entries, maxBytes) : [entries];
            for (let segmentIndex = 0; segmentIndex < segments.length; segmentIndex++) {
                const segmentEntries = segments[segmentIndex];
                if (segmentEntries.length === 0) {
                    continue;
                }
                const outputName = segmentOutputName(effectiveMpq, segmentIndex, segments.length);
                term.debug('client', `Packaging ${outputName}`);
                const logicalClientName = outputName.toLowerCase().endsWith('.mpq') ? outputName : `${outputName}.MPQ`;
                const packageFile = path.join(resfp(ipaths.package), logicalClientName);
                this.buildArchive(packageFile, segmentEntries, folder, compression);
                artifacts.push({
                    packageName: path.basename(packageFile),
                    sourcePath: packageFile,
                    releasePath: resolveLocalPackageOutputPath(outputName, logicalClientName, supplementaryConfig),
                });
            }
        }

        return artifacts;
    }

    private static collectGroupEntries(groupDir: string) {
        const files: string[] = [];
        wfs.iterate(groupDir, filePath => {
            if (wfs.isFile(filePath)) {
                files.push(filePath);
            }
        });

        return files
            .sort((left, right) => left.localeCompare(right))
            .map(filePath => ({
                src: filePath,
                dst: path.relative(groupDir, filePath).replace(/\//g, '\\').replace(/\\/g, '\\'),
            }));
    }

    private static extractSupplementaryArchive(archivePath: string, extractDir: string) {
        wfs.mkDirs(extractDir, true);
        wsys.exec(
            `"${ipaths.bin.sZip.sza_exe.get()}" x "${archivePath}" -o"${extractDir}" -y`,
            'inherit'
        );
    }

    /** Strip one top-level path segment so archive contents merge into group root (e.g. ArchiveName/WORLD/... -> WORLD/...). */
    private static flattenArchiveEntryPath(relativePath: string): string {
        const normalized = relativePath.replace(/\\/g, '/').replace(/^\/+/, '');
        const firstSlash = normalized.indexOf('/');
        if (firstSlash === -1) {
            return normalized; // file at archive root stays at group root
        }
        return normalized.substring(firstSlash + 1);
    }

    private static collectSupplementaryArchiveEntries(extractDir: string) {
        const extractedFiles: string[] = [];
        wfs.iterate(extractDir, extractedPath => {
            if (wfs.isFile(extractedPath)) {
                extractedFiles.push(extractedPath);
            }
        });

        return extractedFiles.map(extractedPath => {
            const relativePath = path.relative(extractDir, extractedPath).replace(/\\/g, '/').replace(/^\/+/, '');
            const entryName = this.flattenArchiveEntryPath(relativePath);
            return { sourcePath: extractedPath, entryName };
        });
    }

    private static async buildSupplementaryPackageArtifacts(
        dataset: Dataset,
        config: ResolvedSupplementaryPatchConfig,
        folder: boolean,
        compression?: MPQCompression
    ) {
        if (config.inputs.length === 0) {
            return [] as PackagedArtifact[];
        }

        const workingRoot = path.join(resfp(ipaths.package), `${dataset.fullName}.supplementary-work`);
        const downloadsDir = path.join(workingRoot, 'downloads');
        const groupsRoot = path.join(workingRoot, 'groups');
        const extractsRoot = path.join(workingRoot, 'extracts');
        wfs.mkDirs(workingRoot, true);

        const targetDirs: { [target: string]: string } = {};
        for (let index = 0; index < config.inputs.length; ++index) {
            const input = config.inputs[index];
            const zipName = this.sanitizeFileName(path.basename(input.remotePath) || `${input.target}.zip`);
            const zipPath = path.join(downloadsDir, `${index.toString().padStart(3, '0')}-${zipName}`);

            try {
                await BunnyCdn.downloadFile(input.remotePath, zipPath);
            } catch (err) {
                if (input.required) {
                    throw err;
                }
                term.log('client', `Skipping optional supplementary zip ${input.remotePath}: ${err}`);
                continue;
            }

            const targetDir = targetDirs[input.target]
                || (targetDirs[input.target] = path.join(groupsRoot, this.sanitizeFileName(input.target)));
            wfs.mkDirs(targetDir);

            const extractDir = path.join(extractsRoot, index.toString().padStart(3, '0'));
            this.extractSupplementaryArchive(zipPath, extractDir);

            for (const { sourcePath, entryName } of this.collectSupplementaryArchiveEntries(extractDir)) {
                if (!entryName || entryName.split('/').includes('..')) {
                    throw new Error(`Unsafe supplementary archive entry ${entryName} in ${input.remotePath}`);
                }

                const targetPath = path.join(targetDir, ...entryName.split('/'));
                wfs.mkDirs(path.dirname(targetPath));
                wfs.copy(sourcePath, targetPath);
            }
        }

        const artifacts: PackagedArtifact[] = [];
        const maxPatchFileSizeMB = NodeConfig.LauncherMaxPatchFileSizeMB ?? 0;
        const maxBytes = maxPatchFileSizeMB > 0 ? maxPatchFileSizeMB * 1024 * 1024 : 0;

        for (const [target, targetDir] of Object.entries(targetDirs)) {
            const entries = this.collectGroupEntries(targetDir);
            if (entries.length === 0) {
                continue;
            }

            const segments = maxBytes > 0 ? partitionEntriesBySize(entries, maxBytes) : [entries];
            for (let segmentIndex = 0; segmentIndex < segments.length; segmentIndex++) {
                const segmentEntries = segments[segmentIndex];
                if (segmentEntries.length === 0) {
                    continue;
                }
                const outputName = segmentOutputName(target, segmentIndex, segments.length);
                const baseName = this.sanitizeFileName(
                    outputName.toLowerCase().endsWith('.mpq')
                        ? outputName
                        : `${outputName}.MPQ`
                );
                const packageFile = path.join(resfp(ipaths.package), baseName);
                this.buildArchive(packageFile, segmentEntries, folder, compression);
                artifacts.push({
                    packageName: path.basename(packageFile),
                    sourcePath: packageFile,
                    releasePath: resolveTargetOutputPath(outputName, config),
                });
            }
        }

        return artifacts;
    }

    static async packageClient(
        dataset: Dataset,
        fullDBC: boolean,
        fullInterface: boolean,
        folder: boolean,
        pipeline: boolean,
        publish: boolean,
        dryRun: boolean,
        compression?: MPQCompression
    ) {
        term.log('client', `Packaging client for ${dataset.name}`)
        await Datascripts.build(dataset,['--no-shutdown']);
        await Addon.build(dataset);

        // Step 1: Resolve mappings
        let mapstr: [string,string[]][] = dataset.config.PackageMapping
            .map(x=>x.split(':'))
            .map(([mpq,modules])=>[mpq,modules.split(',')])
        let mappings: {[mod: string]: /*mpq*/ string } = {}
        let buildModules = dataset.modules().map(x=>x.fullName).concat('_build')
        buildModules.concat(['luaxml','dbc']).forEach(x=>{
            let bestMpq: string = "";
            let bestLen: number = 0;
            mapstr.forEach(([mpq,modules])=>{
                modules.forEach(mod=>{
                    if((mod == '*' || util.isModuleOrParent(x,mod)) && mod.length > bestLen) {
                        bestLen = mod.length;
                        bestMpq = mpq;
                    }
                });
            });
            if(bestLen != 0) {
                mappings[x] = bestMpq
            } else {
                term.log(
                      'dataset'
                    , `Module ${x} has no package mapping in dataset ${dataset.fullName}, will not build it`
                )
            }
        })

        // Step 2: Build list of (src path, path inside archive) per MPQ — same layout as build-data patches
        let entriesByMpq: { [mpq: string]: Entry[] } = {};
        const addEntry = (mod: string, src: string, dst: string) => {
            const mpq = mappings[mod];
            if (!mpq) return;
            // WoW MPQ archived names conventionally use backslashes.
            const dstNorm = dst.replace(/\//g, '\\').trim();
            if (!entriesByMpq[mpq]) entriesByMpq[mpq] = [];
            entriesByMpq[mpq].push({ src, dst: dstNorm });
        };

        if (mappings['dbc']) {
            dataset.path.dbc.iterate('FLAT', 'FILES', 'FULL', node => {
                if (!fullDBC) {
                    const rel = node.relativeTo(dataset.path.dbc);
                    const src = dataset.path.dbc_source.join(rel);
                    if (src.exists() && wfs.readBin(node).equals(wfs.readBin(src))) return;
                }
                addEntry('dbc', node.abs().get(), `DBFilesClient\\${node.basename().get()}`);
            });
        }

        if (mappings['luaxml']) {
            dataset.path.luaxml.iterate('RECURSE', 'FILES', 'FULL', node => {
                const rel = node.relativeTo(dataset.path.luaxml);
                if (!fullInterface) {
                    const src = dataset.path.luaxml_source.join(rel);
                    if (src.exists() && wfs.readBin(node).equals(wfs.readBin(src))) return;
                }
                addEntry('luaxml', node.abs().get(), rel.get());
            });
        }

        dataset.modules()
            .filter(x => mappings[x.fullName] && x.assets.exists())
            .forEach(x => {
                x.path.assets.iterate('RECURSE', 'FILES', 'FULL', node => {
                    const lower = node.toLowerCase();
                    if (lower.endsWith('.png') || lower.endsWith('.blend') || lower.endsWith('.psd') || lower.endsWith('.json') || lower.endsWith('.dbc')) return;
                    addEntry(x.fullName, node.abs().get(), node.relativeTo(x.assets.path).get());
                });
            });

        // Remove old package outputs (dataset-prefixed and patch-*.MPQ / patch-* dirs)
        ipaths.package.iterateDef(node => {
            const base = node.basename().get();
            if (base.startsWith(dataset.fullName)) {
                node.remove();
            } else if (base.toLowerCase().startsWith('patch-') && (base.toLowerCase().endsWith('.mpq') || node.isDirectory())) {
                node.remove();
            }
        });

        const supplementaryConfig = readSupplementaryPatchConfig(dataset.fullName);
        const localArtifacts = this.buildLocalPackageArtifacts(dataset, entriesByMpq, folder, supplementaryConfig, compression);
        const supplementaryArtifacts = pipeline
            ? await this.buildSupplementaryPackageArtifacts(dataset, supplementaryConfig, folder, compression)
            : [];

        const allArtifacts = localArtifacts.concat(supplementaryArtifacts);
        this.writeLegacyMeta(dataset, allArtifacts);

        if (pipeline) {
            const { stagedArtifacts, manifestPath } = this.stageReleaseArtifacts(dataset, allArtifacts);
            const manifest = createReleaseManifest(dataset.fullName, stagedArtifacts);
            wfs.write(manifestPath, JSON.stringify(manifest, null, 4));
            ipaths.package.join(`${dataset.fullName}.manifest.json`).toFile().writeJson(manifest);

            if (publish) {
                await this.uploadReleaseArtifacts(supplementaryConfig, stagedArtifacts, manifestPath, dryRun);
            }
        }
    }

    static packageDirectory(sourceDir: string, outputPatchName: string, compression?: MPQCompression) {
        const resolvedSourceDir = path.resolve(sourceDir);
        if (!wfs.exists(resolvedSourceDir)) {
            throw new Error(`Package source directory does not exist: ${resolvedSourceDir}`);
        }
        if (!wfs.isDirectory(resolvedSourceDir)) {
            throw new Error(`Package source path is not a directory: ${resolvedSourceDir}`);
        }

        const sourceName = this.sanitizeFileName(path.basename(resolvedSourceDir));
        const outputName = this.sanitizeFileName(
            outputPatchName.toLowerCase().endsWith('.mpq')
                ? outputPatchName
                : `${outputPatchName}.MPQ`
        );
        const outputPath = path.join(resfp(ipaths.package), sourceName, outputName);
        const entries = this.collectGroupEntries(resolvedSourceDir);
        term.log('client', `Packaging directory ${resolvedSourceDir} to ${outputPath} (${entries.length} file(s))`);
        this.buildArchive(outputPath, entries, false, compression);
    }

    static buildChunkedRelease(dataset: Dataset) {
        const supplementaryConfig = readSupplementaryPatchConfig(dataset.fullName);
        const artifacts = this.resolveChunkedReleaseInputs(dataset, supplementaryConfig);
        const outputDir = this.chunkedReleaseDir(dataset);
        term.log('client', `Building chunked release for ${dataset.fullName}`);
        const result = createChunkedRelease(dataset.fullName, artifacts, outputDir);
        term.log(
            'client',
            `Wrote chunked release manifest with ${result.manifest.files.length} file(s) and `
            + `${Object.keys(result.manifest.bundles).length} bundle(s) to ${result.outputDir}`
        );
    }

    static verifyChunkedRelease(dataset: Dataset) {
        const manifestPath = this.chunkedReleaseManifestPath(dataset);
        if (!wfs.exists(manifestPath)) {
            throw new Error(
                `Missing chunked release manifest at ${manifestPath}. Run "package chunked ${dataset.fullName}" first.`
            );
        }

        term.log('client', `Verifying chunked release for ${dataset.fullName}`);
        const result = verifyChunkedReleaseOutput(manifestPath);
        term.log(
            'client',
            `Verified ${result.filesVerified} file(s), ${result.indexedChunks} chunk(s), and `
            + `${result.bundlesVerified} bundle(s). Reassembled files written to ${result.verificationDir}`
        );
    }

    static async publishChunkedRelease(dataset: Dataset, dryRun: boolean) {
        const supplementaryConfig = readSupplementaryPatchConfig(dataset.fullName);
        term.log('client', `Publishing chunked release for ${dataset.fullName}`);
        await this.uploadChunkedReleaseArtifacts(dataset, supplementaryConfig, dryRun);
    }

    static Command = commands.addCommand('package')

    static initialize() {
        term.debug('misc', `Initializing packages`)
        this.Command.addCommand(
              'client'
            , 'dataset --fullDBC --fullInterface --folder --pipeline --publish --dry-run --compression=<type>'
            , 'Packages client data for the specified dataset'
            , async args => {
                const parsed = this.extractCompressionArg(args);
                const lower = parsed.args.map(x=>x.toLowerCase())
                const fullDBC = lower.includes('--fulldbc');
                const fullInterface = lower.includes('--fullinterface');
                const folder = lower.includes('--folder');
                const publish = lower.includes('--publish');
                const pipeline = publish || lower.includes('--pipeline');
                const dryRun = lower.includes('--dry-run');
                await Promise.all(Identifier.getDatasets(
                      parsed.args
                    , 'MATCH_ANY'
                    , NodeConfig.DefaultDataset
                ).map(x=>this.packageClient(x,fullDBC,fullInterface,folder,pipeline,publish,dryRun,parsed.compression)))
            }
        )
        this.Command.addCommand(
              'directory'
            , 'source-dir output-patch-name [compression|--compression=<type>]'
            , 'Packages the contents of a directory into an MPQ under release/package/<source-dir-name>/'
            , async args => {
                const parsed = this.extractCompressionArg(args);
                if (parsed.args.length < 2 || parsed.args.length > 3) {
                    throw new Error('Expected package directory <source-dir> <output-patch-name> [compression]');
                }

                const sourceDir = parsed.args[0];
                const outputPatchName = parsed.args[1];
                const compression = parsed.compression
                    || (parsed.args[2] ? this.normalizeCompression(parsed.args[2]) : undefined);
                this.packageDirectory(sourceDir, outputPatchName, compression);
            }
        )
        this.Command.addCommand(
              'chunked'
            , 'dataset'
            , 'Builds a local chunked and bundled release from an existing staged package output'
            , async args => {
                await Promise.all(Identifier.getDatasets(
                      args
                    , 'MATCH_ANY'
                    , NodeConfig.DefaultDataset
                ).map(x=>this.buildChunkedRelease(x)))
            }
        )
        this.Command.addCommand(
              'verify-chunked'
            , 'dataset'
            , 'Verifies a local chunked release by validating bundles and reassembling files'
            , async args => {
                await Promise.all(Identifier.getDatasets(
                      args
                    , 'MATCH_ANY'
                    , NodeConfig.DefaultDataset
                ).map(x=>this.verifyChunkedRelease(x)))
            }
        )
        this.Command.addCommand(
              'publish-chunked'
            , 'dataset --dry-run'
            , 'Publishes a local chunked release and fallback files to Bunny Storage'
            , async args => {
                const lower = args.map(x=>x.toLowerCase());
                const dryRun = lower.includes('--dry-run');
                await Promise.all(Identifier.getDatasets(
                      args
                    , 'MATCH_ANY'
                    , NodeConfig.DefaultDataset
                ).map(x=>this.publishChunkedRelease(x, dryRun)))
            }
        )
    }
}