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
type UnpackedClientArchive = { folderName: string; archivePath: string };
type ExcludeArgs = { args: string[]; patterns: string[] };
type ConfigArgs = { args: string[]; configPath?: string };
type MapArgs = { args: string[]; maps: string[] };
type MapIncludeRule = {
    map: string;
    bounds?: {
        minX?: number;
        minY?: number;
        maxX?: number;
        maxY?: number;
    };
};
type ClientPackageConfig = {
    inputClientDir?: string;
    sourceClientDir?: string;
    unpackedDir?: string;
    outputClientDir?: string;
    minipack?: {
        compression?: MPQCompression;
        include?: (string | MapIncludeRule)[];
        exclude?: string[];
        excludePatterns?: string[];
        excludeLists?: string[];
        removeFiles?: string[];
        assetRepository?: {
            enabled?: boolean;
            folderName?: string;
            archivePath?: string;
            curateFromAdts?: boolean;
            usageFile?: string;
        };
    };
    scanMapTiles?: {
        outputFile?: string;
        maps?: string[];
    };
};

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
    private static defaultClientCleanupFiles() {
        return [
            'Repair.exe',
        ];
    }

    private static defaultSharedAssetRepositoryFolderName() {
        return 'asset-repository';
    }

    private static defaultSharedAssetArchivePath() {
        return 'patch-Z.MPQ';
    }

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

    private static compareArchiveNamesDescending(left: string, right: string) {
        return right.localeCompare(left, undefined, { sensitivity: 'base' });
    }

    private static getClientLocales(dataDir: string) {
        return wfs.readDir(dataDir, false, 'directories')
            .map(dirPath => path.basename(dirPath))
            .filter(locale => {
                const localeDir = path.join(dataDir, locale);
                return wfs.readDir(localeDir, false, 'files')
                    .some(filePath => path.basename(filePath).toLowerCase().includes(`-${locale.toLowerCase()}.mpq`));
            })
            .sort((left, right) => left.localeCompare(right, undefined, { sensitivity: 'base' }));
    }

    private static pushIfExists(target: string[], seen: Set<string>, filePath: string) {
        const resolved = path.resolve(filePath);
        if (!wfs.exists(resolved) || !wfs.isFile(resolved) || !resolved.toLowerCase().endsWith('.mpq')) {
            return;
        }
        const key = resolved.toLowerCase();
        if (seen.has(key)) {
            return;
        }
        seen.add(key);
        target.push(resolved);
    }

    private static pushMatchesDescending(target: string[], seen: Set<string>, directory: string, matcher: RegExp) {
        wfs.readDir(directory, false, 'files')
            .filter(filePath => matcher.test(path.basename(filePath)))
            .sort((left, right) => this.compareArchiveNamesDescending(path.basename(left), path.basename(right)))
            .forEach(filePath => this.pushIfExists(target, seen, filePath));
    }

    private static appendCanonicalPatchArchives(target: string[], seen: Set<string>, dataDir: string, locales: string[]) {
        const storedArchives: string[] = [];
        const wildcardCandidates: string[] = [];
        wfs.readDir(dataDir, false, 'files')
            .filter(filePath => /^patch-.\.mpq$/i.test(path.basename(filePath)))
            .forEach(filePath => wildcardCandidates.push(path.resolve(filePath)));

        locales.forEach(locale => {
            const localeDir = path.join(dataDir, locale);
            const localeMatcher = new RegExp(`^patch-${locale.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')}-.\\.mpq$`, 'i');
            wfs.readDir(localeDir, false, 'files')
                .filter(filePath => localeMatcher.test(path.basename(filePath)))
                .forEach(filePath => wildcardCandidates.push(path.resolve(filePath)));
        });

        wildcardCandidates
            .sort((left, right) => this.compareArchiveNamesDescending(
                path.relative(dataDir, left).replace(/\\/g, '/'),
                path.relative(dataDir, right).replace(/\\/g, '/')
            ))
            .forEach(filePath => storedArchives.push(filePath));

        locales.forEach(locale => {
            const localeDir = path.join(dataDir, locale);
            storedArchives.push(path.join(dataDir, 'patch.MPQ'));
            storedArchives.push(path.join(localeDir, `patch-${locale}.MPQ`));
        });

        // LoadArchives() builds the patch list, then InitializeWowConfig()
        // opens it from the end back to the beginning.
        storedArchives.reverse().forEach(filePath => this.pushIfExists(target, seen, filePath));
    }

    private static removeNonManifestClientArchives(outputDataDir: string, archives: UnpackedClientArchive[]) {
        const allowed = new Set(
            archives.map(archive =>
                path.resolve(path.join(outputDataDir, archive.archivePath.replace(/\//g, path.sep))).toLowerCase()
            )
        );

        wfs.iterate(outputDataDir, filePath => {
            const resolved = path.resolve(filePath);
            if (!wfs.isFile(resolved) || !resolved.toLowerCase().endsWith('.mpq')) {
                return;
            }

            if (!allowed.has(resolved.toLowerCase())) {
                term.log('client', `Removing non-manifest archive ${resolved}`);
                wfs.remove(resolved);
            }
        });
    }

    private static collectClientArchives(dataDir: string) {
        const archives: string[] = [];
        const seen = new Set<string>();
        const locales = this.getClientLocales(dataDir);

        this.pushIfExists(archives, seen, path.join(dataDir, 'common.MPQ'));
        this.pushIfExists(archives, seen, path.join(dataDir, 'common-2.MPQ'));
        this.pushIfExists(archives, seen, path.join(dataDir, 'expansion.MPQ'));
        this.pushIfExists(archives, seen, path.join(dataDir, 'lichking.MPQ'));

        locales.forEach(locale => {
            const localeDir = path.join(dataDir, locale);
            this.pushIfExists(archives, seen, path.join(localeDir, `base-${locale}.MPQ`));
            this.pushIfExists(archives, seen, path.join(localeDir, `locale-${locale}.MPQ`));
            this.pushIfExists(archives, seen, path.join(localeDir, `speech-${locale}.MPQ`));
            this.pushIfExists(archives, seen, path.join(localeDir, `expansion-locale-${locale}.MPQ`));
            this.pushIfExists(archives, seen, path.join(localeDir, `expansion-speech-${locale}.MPQ`));
            this.pushIfExists(archives, seen, path.join(localeDir, `lichking-locale-${locale}.MPQ`));
            this.pushIfExists(archives, seen, path.join(localeDir, `lichking-speech-${locale}.MPQ`));
        });

        this.appendCanonicalPatchArchives(archives, seen, dataDir, locales);

        return archives;
    }

    private static archiveOutputFolderName(archivePath: string) {
        return path.basename(archivePath).replace(/\.mpq$/i, '');
    }

    private static extractionManifestName() {
        return '.tswow-manifest.txt';
    }

    private static unpackedClientManifestName() {
        return '.tswow-client.json';
    }

    private static packageConfigFileNames() {
        return ['tswow-package.json', '.tswow-package.json'];
    }

    private static defaultPackageConfigPath() {
        return this.packageConfigFileNames()
            .map(name => path.resolve(name))
            .find(candidate => wfs.exists(candidate) && wfs.isFile(candidate));
    }

    private static readPackageConfig(configPath?: string) {
        const resolvedPath = configPath
            ? path.resolve(configPath)
            : this.defaultPackageConfigPath();

        if (!resolvedPath) {
            return undefined;
        }

        if (!wfs.exists(resolvedPath) || !wfs.isFile(resolvedPath)) {
            throw new Error(`Package config does not exist: ${resolvedPath}`);
        }

        const parsed = JSON.parse(wfs.read(resolvedPath)) as ClientPackageConfig;
        if (!parsed || typeof parsed !== 'object' || Array.isArray(parsed)) {
            throw new Error(`Invalid package config at ${resolvedPath}`);
        }

        return {
            configPath: resolvedPath,
            configDir: path.dirname(resolvedPath),
            config: parsed,
        };
    }

    private static resolveConfigPath(configDir: string, value?: string) {
        return value ? path.resolve(configDir, value) : undefined;
    }

    private static readPatternsFromFiles(filePaths: string[]) {
        const patterns: string[] = [];
        filePaths.forEach(filePath => {
            const resolvedPath = path.resolve(filePath);
            if (!wfs.exists(resolvedPath) || !wfs.isFile(resolvedPath)) {
                throw new Error(`Exclude list does not exist: ${resolvedPath}`);
            }

            wfs.read(resolvedPath)
                .split(/\r?\n/)
                .map(x => x.trim())
                .filter(x => x.length > 0 && !x.startsWith('#'))
                .forEach(x => patterns.push(x));
        });
        return patterns;
    }

    private static getConfiguredExcludePatterns(configPath?: string) {
        const loaded = this.readPackageConfig(configPath);
        if (!loaded) {
            return [];
        }

        const minipack = loaded.config.minipack || {};
        const inlinePatterns = [
            ...(minipack.exclude || []),
            ...(minipack.excludePatterns || []),
        ];
        const listFiles = (minipack.excludeLists || [])
            .map(filePath => this.resolveConfigPath(loaded.configDir, filePath))
            .filter((filePath): filePath is string => filePath !== undefined);
        return inlinePatterns.concat(this.readPatternsFromFiles(listFiles));
    }

    private static getConfiguredIncludeRules(configPath?: string): MapIncludeRule[] {
        const loaded = this.readPackageConfig(configPath);
        if (!loaded) {
            return [];
        }

        return (loaded.config.minipack?.include || [])
            .map(entry => typeof entry === 'string'
                ? { map: entry }
                : entry)
            .filter((entry): entry is MapIncludeRule =>
                !!entry
                && typeof entry === 'object'
                && typeof entry.map === 'string'
                && entry.map.trim().length > 0
            );
    }

    private static getConfiguredClientCleanupFiles(configPath?: string) {
        const loaded = this.readPackageConfig(configPath);
        const configured = loaded?.config.minipack?.removeFiles || [];
        return Array.from(new Set(
            this.defaultClientCleanupFiles()
                .concat(configured)
                .map(x => x.replace(/\\/g, '/').replace(/^\/+/, ''))
                .filter(x => x.length > 0)
        ));
    }

    private static getConfiguredAssetRepositorySettings(configPath?: string) {
        const loaded = this.readPackageConfig(configPath);
        const configured = loaded?.config.minipack?.assetRepository;
        const configDir = loaded?.configDir || process.cwd();
        const folderName = configured?.folderName?.trim() || this.defaultSharedAssetRepositoryFolderName();
        const usageFile = configured?.usageFile
            ? path.resolve(configDir, configured.usageFile)
            : undefined;

        return {
            enabled: configured?.enabled !== false,
            folderName,
            archivePath: configured?.archivePath?.trim() || this.defaultSharedAssetArchivePath(),
            curateFromAdts: configured?.curateFromAdts !== false,
            usageFile,
        };
    }

    private static sharedAssetExtensions() {
        return new Set([
            '.wmo',
        ]);
    }

    private static isSharedAssetPath(relativePath: string) {
        const normalized = relativePath.replace(/\\/g, '/').replace(/^\/+/, '');
        return this.sharedAssetExtensions().has(path.extname(normalized).toLowerCase());
    }

    private static getSharedAssetRepositoryDir(unpackedDir: string, folderName?: string) {
        return path.join(path.resolve(unpackedDir), folderName || this.defaultSharedAssetRepositoryFolderName());
    }

    private static normalizeArchiveRelativePath(relativePath: string) {
        return relativePath.replace(/\\/g, '/').replace(/^\/+/, '');
    }

    private static normalizedMatchesFilePattern(normalizedPath: string, candidate: string) {
        const normalizedCandidate = candidate.replace(/\\/g, '/').replace(/^\/+/, '');
        return normalizedPath.toLowerCase() === normalizedCandidate.toLowerCase();
    }

    private static getAssetRepositoryEntries(assetRepositoryDir: string) {
        if (!wfs.exists(assetRepositoryDir) || !wfs.isDirectory(assetRepositoryDir)) {
            return new Map<string, Entry>();
        }

        return new Map(
            this.collectGroupEntries(assetRepositoryDir)
                .map(entry => [this.normalizeArchiveRelativePath(entry.dst).toLowerCase(), {
                    src: entry.src,
                    dst: this.normalizeArchiveRelativePath(entry.dst),
                }] as const)
        );
    }

    private static collectIncludedAdtAssetPaths(
        archives: UnpackedClientArchive[],
        unpackedDir: string,
        excludePatterns: string[],
        includeRules: MapIncludeRule[]
    ) {
        const assets = new Set<string>();

        archives.forEach(archive => {
            const sourceFolder = path.join(unpackedDir, archive.folderName);
            if (!wfs.exists(sourceFolder) || !wfs.isDirectory(sourceFolder)) {
                return;
            }

            this.filterEntries(this.collectGroupEntries(sourceFolder), excludePatterns, includeRules)
                .forEach(entry => {
                    const normalized = this.normalizeArchiveRelativePath(entry.dst);
                    if (!/\.adt$/i.test(normalized)) {
                        return;
                    }

                    this.parseAdtAssets(entry.src)
                        .forEach(asset => assets.add(this.normalizeArchiveRelativePath(asset)));
                });
        });

        return assets;
    }

    private static collectAssetDependencyMatches(assetPath: string, assetEntries: Map<string, Entry>) {
        const matches = new Set<string>();
        const normalized = this.normalizeArchiveRelativePath(assetPath);
        const normalizedLower = normalized.toLowerCase();
        const exact = assetEntries.get(normalizedLower);
        if (exact) {
            matches.add(exact.dst);
        }

        const extension = path.extname(normalizedLower);
        const directory = path.posix.dirname(normalizedLower);
        const basename = path.posix.basename(normalizedLower, extension);
        const directoryPrefix = directory === '.' ? '' : `${directory}/`;

        for (const [candidateKey, candidate] of assetEntries.entries()) {
            if (candidateKey === normalizedLower) {
                continue;
            }

            if (directoryPrefix && !candidateKey.startsWith(directoryPrefix)) {
                continue;
            }

            const candidateBasename = path.posix.basename(candidateKey);

            if (extension === '.m2') {
                if (new RegExp(`^${basename}(?:\\d+)?\\.(skin|phys|anim)$`, 'i').test(candidateBasename)) {
                    matches.add(candidate.dst);
                }
            } else if (extension === '.wmo') {
                if (new RegExp(`^${basename}(?:_\\d{3}|_lod\\d+)?\\.wmo$`, 'i').test(candidateBasename)) {
                    matches.add(candidate.dst);
                }
            }
        }

        return matches;
    }

    private static collectCuratedAssetEntries(
        archives: UnpackedClientArchive[],
        unpackedDir: string,
        excludePatterns: string[],
        includeRules: MapIncludeRule[],
        assetRepositoryDir: string,
        usageOutputFile?: string
    ) {
        const assetEntries = this.getAssetRepositoryEntries(assetRepositoryDir);
        if (assetEntries.size === 0) {
            return [] as Entry[];
        }

        const referencedAssets = this.collectIncludedAdtAssetPaths(
            archives,
            unpackedDir,
            excludePatterns,
            includeRules
        );

        const selected = new Set<string>();
        referencedAssets.forEach(asset => {
            this.collectAssetDependencyMatches(asset, assetEntries)
                .forEach(match => selected.add(match));
        });

        const entries = Array.from(selected)
            .map(relativePath => assetEntries.get(relativePath.toLowerCase()))
            .filter((entry): entry is Entry => entry !== undefined)
            .sort((left, right) => left.dst.localeCompare(right.dst, undefined, { sensitivity: 'base' }));

        if (usageOutputFile) {
            const lines = Array.from(referencedAssets)
                .sort((left, right) => left.localeCompare(right, undefined, { sensitivity: 'base' }));
            wfs.write(usageOutputFile, lines.join('\n') + (lines.length > 0 ? '\n' : ''));
        }

        return entries;
    }

    private static removeClientFiles(outputClientDir: string, filePatterns: string[]) {
        const resolvedOutputClientDir = path.resolve(outputClientDir);
        filePatterns.forEach(pattern => {
            const normalizedPattern = pattern.replace(/\\/g, '/').replace(/^\/+/, '');
            const targetPath = path.join(resolvedOutputClientDir, ...normalizedPattern.split('/'));
            if (wfs.exists(targetPath)) {
                term.log('client', `Removing copied client file ${targetPath}`);
                wfs.remove(targetPath);
            }
        });
    }

    private static readExtractionManifest(archiveOutputDir: string) {
        const manifestPath = path.join(archiveOutputDir, this.extractionManifestName());
        if (!wfs.exists(manifestPath) || !wfs.isFile(manifestPath)) {
            throw new Error(`Missing extraction manifest at ${manifestPath}`);
        }

        const entries = wfs.read(manifestPath)
            .split(/\r?\n/)
            .map(x => x.trim())
            .filter(x => x.length > 0);

        wfs.remove(manifestPath);
        return entries;
    }

    private static extractedFilePath(archiveOutputDir: string, relativePath: string) {
        return path.join(archiveOutputDir, ...relativePath.replace(/\\/g, '/').split('/'));
    }

    private static writeUnpackedClientManifest(outputDir: string, archives: UnpackedClientArchive[]) {
        wfs.write(
            path.join(outputDir, this.unpackedClientManifestName()),
            JSON.stringify({ archives }, null, 4)
        );
    }

    private static readUnpackedClientManifest(outputDir: string) {
        const manifestPath = path.join(outputDir, this.unpackedClientManifestName());
        if (!wfs.exists(manifestPath) || !wfs.isFile(manifestPath)) {
            throw new Error(
                `Missing unpacked client manifest at ${manifestPath}. `
                + `Run "package unpack-client <input-dir> <output-dir>" again first.`
            );
        }

        const parsed = JSON.parse(wfs.read(manifestPath)) as { archives?: UnpackedClientArchive[] };
        if (!parsed.archives || !Array.isArray(parsed.archives)) {
            throw new Error(`Invalid unpacked client manifest at ${manifestPath}`);
        }
        return parsed.archives;
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

    private static extractConfigArg(args: string[]): ConfigArgs {
        const filtered: string[] = [];
        let configPath: string | undefined = undefined;

        for (let i = 0; i < args.length; ++i) {
            const arg = args[i];
            const lower = arg.toLowerCase();

            if (lower === '--config') {
                const value = args[i + 1];
                if (value === undefined) {
                    throw new Error('Missing value for --config');
                }
                configPath = value;
                i += 1;
                continue;
            }

            if (lower.startsWith('--config=')) {
                configPath = arg.substring('--config='.length);
                continue;
            }

            filtered.push(arg);
        }

        return { args: filtered, configPath };
    }

    private static extractMapArgs(args: string[]): MapArgs {
        const filtered: string[] = [];
        const maps: string[] = [];

        for (let i = 0; i < args.length; ++i) {
            const arg = args[i];
            const lower = arg.toLowerCase();

            if (lower === '--map') {
                const value = args[i + 1];
                if (value === undefined) {
                    throw new Error('Missing value for --map');
                }
                maps.push(value);
                i += 1;
                continue;
            }

            if (lower.startsWith('--map=')) {
                maps.push(arg.substring('--map='.length));
                continue;
            }

            filtered.push(arg);
        }

        return { args: filtered, maps };
    }

    private static extractExcludeArgs(args: string[]): ExcludeArgs {
        const filtered: string[] = [];
        const patterns: string[] = [];

        for (let i = 0; i < args.length; ++i) {
            const arg = args[i];
            const lower = arg.toLowerCase();

            if (lower === '--exclude') {
                const value = args[i + 1];
                if (value === undefined) {
                    throw new Error('Missing value for --exclude');
                }
                patterns.push(value);
                i += 1;
                continue;
            }

            if (lower.startsWith('--exclude=')) {
                patterns.push(arg.substring('--exclude='.length));
                continue;
            }

            if (lower === '--exclude-list') {
                const value = args[i + 1];
                if (value === undefined) {
                    throw new Error('Missing value for --exclude-list');
                }
                patterns.push(...this.readPatternsFromFiles([value]));
                i += 1;
                continue;
            }

            if (lower.startsWith('--exclude-list=')) {
                patterns.push(...this.readPatternsFromFiles([arg.substring('--exclude-list='.length)]));
                continue;
            }

            filtered.push(arg);
        }

        return {
            args: filtered,
            patterns,
        };
    }

    private static wildcardToRegExp(pattern: string) {
        const normalized = pattern.replace(/\\/g, '/').replace(/^\/+/, '');
        const escaped = normalized.replace(/[.+^${}()|[\]\\]/g, '\\$&');
        return new RegExp(`^${escaped.replace(/\*/g, '.*').replace(/\?/g, '.')}$`, 'i');
    }

    private static tileMatchesIncludeRule(tile: { mapDirectory: string; x: number; y: number }, rule: MapIncludeRule) {
        if (tile.mapDirectory.toLowerCase() !== rule.map.toLowerCase()) {
            return false;
        }

        if (!rule.bounds) {
            return true;
        }

        const { minX, minY, maxX, maxY } = rule.bounds;
        if (minX !== undefined && tile.x < minX) return false;
        if (minY !== undefined && tile.y < minY) return false;
        if (maxX !== undefined && tile.x > maxX) return false;
        if (maxY !== undefined && tile.y > maxY) return false;
        return true;
    }

    private static filterEntries(entries: Entry[], excludePatterns: string[], includeRules: MapIncludeRule[] = []) {
        const matchers = excludePatterns.map(x => this.wildcardToRegExp(x));
        return entries.filter(entry => {
            const normalized = entry.dst.replace(/\\/g, '/').replace(/^\/+/, '');
            if (matchers.some(matcher => matcher.test(normalized))) {
                return false;
            }

            if (includeRules.length === 0) {
                return true;
            }

            const tile = this.parseTilePath(normalized);
            if (!tile) {
                return true;
            }

            return includeRules.some(rule => this.tileMatchesIncludeRule(tile, rule));
        });
    }

    private static parseNullTerminatedStrings(data: Buffer) {
        const values: string[] = [];
        let start = 0;
        for (let i = 0; i < data.length; ++i) {
            if (data[i] === 0) {
                if (i > start) {
                    values.push(data.toString('utf8', start, i));
                }
                start = i + 1;
            }
        }
        if (start < data.length) {
            values.push(data.toString('utf8', start));
        }
        return values;
    }

    private static parseChunkOffsets(data: Buffer) {
        const offsets: number[] = [];
        for (let i = 0; i + 4 <= data.length; i += 4) {
            offsets.push(data.readUInt32LE(i));
        }
        return offsets;
    }

    private static resolveChunkStrings(dataChunk: Buffer | undefined, offsetChunk: Buffer | undefined) {
        if (!dataChunk || !offsetChunk) {
            return [];
        }

        const rawStrings = this.parseNullTerminatedStrings(dataChunk);
        const byOffset = new Map<number, string>();
        let cursor = 0;
        rawStrings.forEach(value => {
            byOffset.set(cursor, value);
            cursor += Buffer.byteLength(value) + 1;
        });

        return this.parseChunkOffsets(offsetChunk)
            .map(offset => byOffset.get(offset))
            .filter((value): value is string => value !== undefined);
    }

    private static collectReferencedChunkAssets(indexChunk: Buffer | undefined, entrySize: number, names: string[]) {
        const assets = new Set<string>();
        if (!indexChunk || names.length === 0) {
            return assets;
        }

        for (let i = 0; i + 4 <= indexChunk.length; i += entrySize) {
            const index = indexChunk.readUInt32LE(i);
            const asset = names[index];
            if (asset) {
                assets.add(asset.replace(/\\/g, '/'));
            }
        }

        return assets;
    }

    private static parseAdtAssets(adtPath: string) {
        const buffer = wfs.readBin(adtPath);
        const chunks = new Map<string, Buffer>();

        let offset = 0;
        while (offset + 8 <= buffer.length) {
            const chunkId = buffer.toString('ascii', offset, offset + 4);
            const chunkSize = buffer.readUInt32LE(offset + 4);
            const chunkStart = offset + 8;
            const chunkEnd = chunkStart + chunkSize;
            if (chunkEnd > buffer.length) {
                break;
            }
            chunks.set(chunkId, buffer.slice(chunkStart, chunkEnd));
            offset = chunkEnd;
        }

        const textures = this.parseNullTerminatedStrings(chunks.get('MTEX') || Buffer.alloc(0))
            .map(x => x.replace(/\\/g, '/'));
        const modelNames = this.resolveChunkStrings(chunks.get('MMDX'), chunks.get('MMID'));
        const wmoNames = this.resolveChunkStrings(chunks.get('MWMO'), chunks.get('MWID'));
        const modelRefs = this.collectReferencedChunkAssets(chunks.get('MDDF'), 36, modelNames);
        const wmoRefs = this.collectReferencedChunkAssets(chunks.get('MODF'), 64, wmoNames);

        const assets = new Set<string>();
        textures.forEach(x => assets.add(x));
        modelRefs.forEach(x => assets.add(x));
        wmoRefs.forEach(x => assets.add(x));
        return assets;
    }

    private static readWdbcTable(dbcPath: string) {
        const buffer = wfs.readBin(dbcPath);
        if (buffer.length < 20 || buffer.toString('ascii', 0, 4) !== 'WDBC') {
            throw new Error(`Unsupported DBC format for ${dbcPath}`);
        }

        const recordCount = buffer.readUInt32LE(4);
        const fieldCount = buffer.readUInt32LE(8);
        const recordSize = buffer.readUInt32LE(12);
        const stringBlockSize = buffer.readUInt32LE(16);
        const recordsOffset = 20;
        const stringBlockOffset = recordsOffset + recordCount * recordSize;
        if (stringBlockOffset + stringBlockSize > buffer.length) {
            throw new Error(`Malformed DBC string block in ${dbcPath}`);
        }

        return {
            buffer,
            recordCount,
            fieldCount,
            recordSize,
            stringBlockOffset,
            stringBlockSize,
        };
    }

    private static readWdbcStringField(
        table: ReturnType<typeof Package.readWdbcTable>,
        rowIndex: number,
        fieldIndex: number
    ) {
        if (fieldIndex < 0 || fieldIndex >= table.fieldCount) {
            return '';
        }

        const rowOffset = 20 + rowIndex * table.recordSize;
        const fieldOffset = rowOffset + fieldIndex * 4;
        if (fieldOffset + 4 > table.stringBlockOffset) {
            return '';
        }

        const stringOffset = table.buffer.readUInt32LE(fieldOffset);
        if (stringOffset >= table.stringBlockSize) {
            return '';
        }

        const start = table.stringBlockOffset + stringOffset;
        let end = start;
        while (end < table.buffer.length && table.buffer[end] !== 0) {
            end += 1;
        }

        return table.buffer.toString('utf8', start, end);
    }

    private static readWdbcUIntField(
        table: ReturnType<typeof Package.readWdbcTable>,
        rowIndex: number,
        fieldIndex: number
    ) {
        if (fieldIndex < 0 || fieldIndex >= table.fieldCount) {
            return 0;
        }

        const rowOffset = 20 + rowIndex * table.recordSize;
        const fieldOffset = rowOffset + fieldIndex * 4;
        if (fieldOffset + 4 > table.stringBlockOffset) {
            return 0;
        }

        return table.buffer.readUInt32LE(fieldOffset);
    }

    private static normalizeClientAssetPath(value: string) {
        const normalized = value.trim().replace(/\\/g, '/').replace(/^\/+/, '');
        if (!normalized) {
            return '';
        }

        if (/\.mdx$/i.test(normalized)) {
            return normalized.replace(/\.mdx$/i, '.m2');
        }

        return normalized;
    }

    private static collectWdbcStringFieldValues(dbcPath: string, fieldIndices: number[]) {
        const table = this.readWdbcTable(dbcPath);
        const values = new Set<string>();

        for (let rowIndex = 0; rowIndex < table.recordCount; ++rowIndex) {
            fieldIndices.forEach(fieldIndex => {
                const value = this.normalizeClientAssetPath(this.readWdbcStringField(table, rowIndex, fieldIndex));
                if (value) {
                    values.add(value);
                }
            });
        }

        return values;
    }

    private static collectFileDataPaths(dbcPath: string) {
        const table = this.readWdbcTable(dbcPath);
        const values = new Map<number, string>();

        for (let rowIndex = 0; rowIndex < table.recordCount; ++rowIndex) {
            const id = this.readWdbcUIntField(table, rowIndex, 0);
            const filename = this.normalizeClientAssetPath(this.readWdbcStringField(table, rowIndex, 1));
            const filepath = this.normalizeClientAssetPath(this.readWdbcStringField(table, rowIndex, 2));

            let resolved = '';
            if (filepath && filename) {
                resolved = filepath.toLowerCase().endsWith(filename.toLowerCase())
                    ? filepath
                    : `${filepath.replace(/\/+$/, '')}/${filename}`;
            } else {
                resolved = filepath || filename;
            }

            if (id > 0 && resolved) {
                values.set(id, resolved);
            }
        }

        return values;
    }

    private static collectSpellVisualMissilePaths(spellVisualPath: string, fileDataPaths: Map<number, string>) {
        const table = this.readWdbcTable(spellVisualPath);
        const values = new Set<string>();

        for (let rowIndex = 0; rowIndex < table.recordCount; ++rowIndex) {
            const fileDataId = this.readWdbcUIntField(table, rowIndex, 8);
            const resolved = fileDataPaths.get(fileDataId);
            if (resolved) {
                values.add(resolved);
            }
        }

        return values;
    }

    private static collectUnpackedEntriesByPath(
        archives: UnpackedClientArchive[],
        unpackedDir: string
    ) {
        const entries = new Map<string, string>();

        archives.forEach(archive => {
            const sourceFolder = path.join(unpackedDir, archive.folderName);
            if (!wfs.exists(sourceFolder) || !wfs.isDirectory(sourceFolder)) {
                return;
            }

            this.collectGroupEntries(sourceFolder).forEach(entry => {
                entries.set(this.normalizeArchiveRelativePath(entry.dst).toLowerCase(), entry.src);
            });
        });

        return entries;
    }

    private static collectDbcReferencedAssetPaths(
        archives: UnpackedClientArchive[],
        unpackedDir: string
    ) {
        const unpackedEntries = this.collectUnpackedEntriesByPath(archives, unpackedDir);
        const values = new Set<string>();
        const addStrings = (relativePath: string, fieldIndices: number[]) => {
            const entryPath = unpackedEntries.get(relativePath.toLowerCase());
            if (!entryPath) {
                return;
            }

            this.collectWdbcStringFieldValues(entryPath, fieldIndices)
                .forEach(value => values.add(value));
        };

        addStrings('DBFilesClient/CreatureModelData.dbc', [2]);
        addStrings('DBFilesClient/CreatureDisplayInfo.dbc', [6, 7, 9]);
        addStrings('DBFilesClient/GameObjectDisplayInfo.dbc', [1]);
        addStrings('DBFilesClient/GameObjectArtKit.dbc', [1, 2, 3, 4, 5, 6, 7]);
        addStrings('DBFilesClient/SpellVisualEffectName.dbc', [2]);
        addStrings('DBFilesClient/ItemDisplayInfo.dbc', [1, 2, 3, 4, 15, 16, 17, 18, 19, 20, 21, 22]);
        addStrings('DBFilesClient/ItemVisualEffects.dbc', [1]);
        addStrings('DBFilesClient/Vehicle.dbc', [29, 30, 31, 32]);

        const fileDataPath = unpackedEntries.get('dbfilesclient/filedata.dbc');
        const spellVisualPath = unpackedEntries.get('dbfilesclient/spellvisual.dbc');
        if (fileDataPath && spellVisualPath) {
            const fileDataValues = this.collectFileDataPaths(fileDataPath);
            this.collectSpellVisualMissilePaths(spellVisualPath, fileDataValues)
                .forEach(value => values.add(value));
        }

        return values;
    }

    private static collectAdtFiles(inputPath: string) {
        const resolvedInputPath = path.resolve(inputPath);
        if (!wfs.exists(resolvedInputPath)) {
            throw new Error(`ADT input path does not exist: ${resolvedInputPath}`);
        }

        const adtFiles: string[] = [];
        if (wfs.isFile(resolvedInputPath)) {
            if (!resolvedInputPath.toLowerCase().endsWith('.adt')) {
                throw new Error(`ADT input file is not an .adt: ${resolvedInputPath}`);
            }
            adtFiles.push(resolvedInputPath);
        } else {
            wfs.iterate(resolvedInputPath, filePath => {
                if (wfs.isFile(filePath) && filePath.toLowerCase().endsWith('.adt')) {
                    adtFiles.push(path.resolve(filePath));
                }
            });
        }

        return adtFiles.sort((left, right) => left.localeCompare(right));
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

    static unpackClient(inputDir: string, outputDir: string) {
        const resolvedInputDir = path.resolve(inputDir);
        const resolvedOutputDir = path.resolve(outputDir);
        const dataDir = path.join(resolvedInputDir, 'Data');
        const assetRepositoryDir = this.getSharedAssetRepositoryDir(resolvedOutputDir);

        if (!wfs.exists(resolvedInputDir)) {
            throw new Error(`Client input directory does not exist: ${resolvedInputDir}`);
        }
        if (!wfs.isDirectory(resolvedInputDir)) {
            throw new Error(`Client input path is not a directory: ${resolvedInputDir}`);
        }
        if (!wfs.exists(dataDir) || !wfs.isDirectory(dataDir)) {
            throw new Error(`Client Data directory does not exist: ${dataDir}`);
        }

        const archives = this.collectClientArchives(dataDir);
        if (archives.length === 0) {
            throw new Error(`Found no MPQ archives under ${dataDir}`);
        }

        wfs.mkDirs(resolvedOutputDir, true);
        term.log('client', `Unpacking ${archives.length} MPQ archive(s) from ${resolvedInputDir} to ${resolvedOutputDir}`);
        const latestExtractedByPath = new Map<string, string>();
        const manifestArchives: UnpackedClientArchive[] = [];

        for (const archivePath of archives) {
            const relativeArchivePath = path.relative(dataDir, archivePath).replace(/\\/g, '/');
            const folderName = this.archiveOutputFolderName(archivePath);
            const archiveOutputDir = path.join(resolvedOutputDir, folderName);
            manifestArchives.push({
                folderName,
                archivePath: relativeArchivePath,
            });
            term.log('client', `Extracting ${relativeArchivePath} to ${archiveOutputDir}`);
            wsys.exec(
                `"${ipaths.bin.mpqbuilder.mpqbuilder_exe.get()}" extract "${archivePath}" "${archiveOutputDir}"`,
                'inherit'
            );

            for (const extractedRelativePath of this.readExtractionManifest(archiveOutputDir)) {
                const normalizedPath = extractedRelativePath.replace(/\\/g, '/');
                const extractedPath = this.extractedFilePath(archiveOutputDir, normalizedPath);
                let finalPath = extractedPath;

                if (this.isSharedAssetPath(normalizedPath) && wfs.exists(extractedPath)) {
                    finalPath = path.join(assetRepositoryDir, ...normalizedPath.split('/'));
                    wfs.move(extractedPath, finalPath);
                }

                const previousPath = latestExtractedByPath.get(normalizedPath);
                if (previousPath && previousPath !== finalPath && wfs.exists(previousPath)) {
                    wfs.remove(previousPath);
                }
                latestExtractedByPath.set(normalizedPath, finalPath);
            }
        }

        this.writeUnpackedClientManifest(resolvedOutputDir, manifestArchives);
    }

    static restoreClient(
        unpackedDir: string,
        sourceClientDir: string,
        outputClientDir: string,
        excludePatterns: string[] = [],
        includeRules: MapIncludeRule[] = [],
        compression?: MPQCompression,
        configPath?: string
    ) {
        const resolvedUnpackedDir = path.resolve(unpackedDir);
        const resolvedSourceClientDir = path.resolve(sourceClientDir);
        const resolvedOutputClientDir = path.resolve(outputClientDir);
        const assetRepositorySettings = this.getConfiguredAssetRepositorySettings(configPath);
        const assetRepositoryDir = this.getSharedAssetRepositoryDir(resolvedUnpackedDir, assetRepositorySettings.folderName);
        const clientCleanupFiles = this.getConfiguredClientCleanupFiles(configPath);

        if (!wfs.exists(resolvedUnpackedDir) || !wfs.isDirectory(resolvedUnpackedDir)) {
            throw new Error(`Unpacked client directory does not exist: ${resolvedUnpackedDir}`);
        }
        if (!wfs.exists(resolvedSourceClientDir) || !wfs.isDirectory(resolvedSourceClientDir)) {
            throw new Error(`Source client directory does not exist: ${resolvedSourceClientDir}`);
        }

        const archives = this.readUnpackedClientManifest(resolvedUnpackedDir);
        const outputDataDir = path.join(resolvedOutputClientDir, 'Data');

        term.log('client', `Copying source client from ${resolvedSourceClientDir} to ${resolvedOutputClientDir}`);
        wfs.copy(resolvedSourceClientDir, resolvedOutputClientDir, true);
        this.removeNonManifestClientArchives(outputDataDir, archives);
        this.removeClientFiles(resolvedOutputClientDir, clientCleanupFiles);

        for (const archive of archives) {
            const sourceFolder = path.join(resolvedUnpackedDir, archive.folderName);
            const outputArchivePath = path.join(outputDataDir, archive.archivePath.replace(/\//g, path.sep));

            if (wfs.exists(outputArchivePath)) {
                wfs.remove(outputArchivePath);
            }

            if (!wfs.exists(sourceFolder) || !wfs.isDirectory(sourceFolder)) {
                term.log('client', `Skipping missing unpacked patch folder ${sourceFolder}`);
                continue;
            }

            const entries = this.filterEntries(this.collectGroupEntries(sourceFolder), excludePatterns, includeRules)
                .filter(entry => !this.isSharedAssetPath(entry.dst));
            if (entries.length === 0) {
                term.log('client', `Skipping empty patch folder ${sourceFolder}`);
                continue;
            }

            term.log('client', `Repacking ${archive.archivePath} from ${sourceFolder} (${entries.length} file(s))`);
            this.buildArchive(outputArchivePath, entries, false, compression);
        }

        if (!assetRepositorySettings.enabled) {
            return;
        }

        const assetEntries = assetRepositorySettings.curateFromAdts
            ? this.collectCuratedAssetEntries(
                archives,
                resolvedUnpackedDir,
                excludePatterns,
                includeRules,
                assetRepositoryDir,
                assetRepositorySettings.usageFile
            )
            : Array.from(this.getAssetRepositoryEntries(assetRepositoryDir).values())
                .sort((left, right) => left.dst.localeCompare(right.dst, undefined, { sensitivity: 'base' }));

        if (assetEntries.length === 0) {
            term.log('client', `Skipping shared asset patch because no asset files were selected from ${assetRepositoryDir}`);
            return;
        }

        const outputArchivePath = path.join(outputDataDir, assetRepositorySettings.archivePath.replace(/\//g, path.sep));
        if (wfs.exists(outputArchivePath)) {
            wfs.remove(outputArchivePath);
        }

        term.log('client', `Repacking ${assetRepositorySettings.archivePath} from ${assetRepositoryDir} (${assetEntries.length} file(s))`);
        this.buildArchive(outputArchivePath, assetEntries, false, compression);
    }

    static scanAdtAssets(inputPath: string, outputFile: string) {
        const adtFiles = this.collectAdtFiles(inputPath);
        if (adtFiles.length === 0) {
            throw new Error(`Found no ADT files under ${path.resolve(inputPath)}`);
        }

        const assets = new Set<string>();
        adtFiles.forEach(adtPath => {
            this.parseAdtAssets(adtPath).forEach(asset => assets.add(asset));
        });

        const sortedAssets = Array.from(assets).sort((left, right) => left.localeCompare(right));
        const resolvedOutputFile = path.resolve(outputFile);
        wfs.write(resolvedOutputFile, sortedAssets.join('\n') + '\n');
        term.log('client', `Scanned ${adtFiles.length} ADT file(s) and wrote ${sortedAssets.length} unique asset path(s) to ${resolvedOutputFile}`);
    }

    private static parseTilePath(relativePath: string) {
        const normalized = relativePath.replace(/\\/g, '/');
        const match = /^World\/Maps\/([^/]+)\/([^/]+)_(\d+)_(\d+)(?:_[^/]+)?\.adt$/i.exec(normalized);
        if (!match) {
            return undefined;
        }

        const [, mapDirectory, fileMapDirectory, xText, yText] = match;
        if (mapDirectory.toLowerCase() !== fileMapDirectory.toLowerCase()) {
            return undefined;
        }

        return {
            mapDirectory,
            x: parseInt(xText, 10),
            y: parseInt(yText, 10),
            normalizedPath: normalized,
        };
    }

    static scanMapTiles(
        unpackedDir: string,
        outputFile: string,
        excludePatterns: string[] = [],
        includeRules: MapIncludeRule[] = [],
        mapFilters: string[] = []
    ) {
        const resolvedUnpackedDir = path.resolve(unpackedDir);
        if (!wfs.exists(resolvedUnpackedDir) || !wfs.isDirectory(resolvedUnpackedDir)) {
            throw new Error(`Unpacked client directory does not exist: ${resolvedUnpackedDir}`);
        }

        const archives = this.readUnpackedClientManifest(resolvedUnpackedDir);
        const filteredMaps = mapFilters.map(x => x.toLowerCase());
        const mapData = new Map<string, {
            minX: number;
            minY: number;
            maxX: number;
            maxY: number;
            tiles: Map<string, {
                x: number;
                y: number;
                files: string[];
                archives: string[];
            }>;
        }>();

        archives.forEach(archive => {
            const sourceFolder = path.join(resolvedUnpackedDir, archive.folderName);
            if (!wfs.exists(sourceFolder) || !wfs.isDirectory(sourceFolder)) {
                return;
            }

            this.filterEntries(this.collectGroupEntries(sourceFolder), excludePatterns, includeRules)
                .forEach(entry => {
                    const tile = this.parseTilePath(entry.dst);
                    if (!tile) {
                        return;
                    }

                    if (filteredMaps.length > 0 && !filteredMaps.includes(tile.mapDirectory.toLowerCase())) {
                        return;
                    }

                    if (!mapData.has(tile.mapDirectory)) {
                        mapData.set(tile.mapDirectory, {
                            minX: tile.x,
                            minY: tile.y,
                            maxX: tile.x,
                            maxY: tile.y,
                            tiles: new Map(),
                        });
                    }

                    const mapEntry = mapData.get(tile.mapDirectory)!;
                    mapEntry.minX = Math.min(mapEntry.minX, tile.x);
                    mapEntry.minY = Math.min(mapEntry.minY, tile.y);
                    mapEntry.maxX = Math.max(mapEntry.maxX, tile.x);
                    mapEntry.maxY = Math.max(mapEntry.maxY, tile.y);

                    const tileKey = `${tile.x},${tile.y}`;
                    if (!mapEntry.tiles.has(tileKey)) {
                        mapEntry.tiles.set(tileKey, {
                            x: tile.x,
                            y: tile.y,
                            files: [],
                            archives: [],
                        });
                    }

                    const tileEntry = mapEntry.tiles.get(tileKey)!;
                    tileEntry.files.push(tile.normalizedPath);
                    if (!tileEntry.archives.includes(archive.archivePath)) {
                        tileEntry.archives.push(archive.archivePath);
                    }
                });
        });

        const summary = {
            unpackedDir: resolvedUnpackedDir,
            excludedPatterns: excludePatterns,
            includeRules,
            maps: Array.from(mapData.entries())
                .sort(([left], [right]) => left.localeCompare(right))
                .map(([mapDirectory, info]) => ({
                    mapDirectory,
                    tileCount: info.tiles.size,
                    bounds: {
                        minX: info.minX,
                        minY: info.minY,
                        maxX: info.maxX,
                        maxY: info.maxY,
                    },
                    tiles: Array.from(info.tiles.values())
                        .sort((left, right) => left.y - right.y || left.x - right.x),
                })),
        };

        const resolvedOutputFile = path.resolve(outputFile);
        wfs.write(resolvedOutputFile, JSON.stringify(summary, null, 4));
        term.log(
            'client',
            `Scanned ${summary.maps.length} map(s) and wrote ${summary.maps.reduce((sum, map) => sum + map.tileCount, 0)} tile(s) to ${resolvedOutputFile}`
        );
    }

    private static collectMapTileSummary(
        unpackedDir: string,
        excludePatterns: string[] = [],
        includeRules: MapIncludeRule[] = []
    ) {
        const resolvedUnpackedDir = path.resolve(unpackedDir);
        if (!wfs.exists(resolvedUnpackedDir) || !wfs.isDirectory(resolvedUnpackedDir)) {
            throw new Error(`Unpacked client directory does not exist: ${resolvedUnpackedDir}`);
        }

        const archives = this.readUnpackedClientManifest(resolvedUnpackedDir);
        const mapData = new Map<string, {
            minX: number;
            minY: number;
            maxX: number;
            maxY: number;
            tileCount: number;
            tilesSeen: Set<string>;
        }>();

        archives.forEach(archive => {
            const sourceFolder = path.join(resolvedUnpackedDir, archive.folderName);
            if (!wfs.exists(sourceFolder) || !wfs.isDirectory(sourceFolder)) {
                return;
            }

            this.filterEntries(this.collectGroupEntries(sourceFolder), excludePatterns, includeRules)
                .forEach(entry => {
                    const tile = this.parseTilePath(entry.dst);
                    if (!tile) {
                        return;
                    }

                    if (!mapData.has(tile.mapDirectory)) {
                        mapData.set(tile.mapDirectory, {
                            minX: tile.x,
                            minY: tile.y,
                            maxX: tile.x,
                            maxY: tile.y,
                            tileCount: 0,
                            tilesSeen: new Set<string>(),
                        });
                    }

                    const mapEntry = mapData.get(tile.mapDirectory)!;
                    mapEntry.minX = Math.min(mapEntry.minX, tile.x);
                    mapEntry.minY = Math.min(mapEntry.minY, tile.y);
                    mapEntry.maxX = Math.max(mapEntry.maxX, tile.x);
                    mapEntry.maxY = Math.max(mapEntry.maxY, tile.y);

                    const tileKey = `${tile.x},${tile.y}`;
                    if (!mapEntry.tilesSeen.has(tileKey)) {
                        mapEntry.tilesSeen.add(tileKey);
                        mapEntry.tileCount += 1;
                    }
                });
        });

        return Array.from(mapData.entries())
            .sort(([left], [right]) => left.localeCompare(right))
            .map(([mapDirectory, info]) => ({
                mapDirectory,
                tileCount: info.tileCount,
                bounds: {
                    minX: info.minX,
                    minY: info.minY,
                    maxX: info.maxX,
                    maxY: info.maxY,
                },
            }));
    }

    static scaffoldMapConfig(unpackedDir: string, outputFile: string, configPath?: string) {
        const excludePatterns = this.getConfiguredExcludePatterns(configPath);
        const maps = this.collectMapTileSummary(unpackedDir, excludePatterns);
        if (maps.length === 0) {
            throw new Error(`Found no map tiles under ${path.resolve(unpackedDir)}`);
        }

        const resolvedUnpackedDir = path.resolve(unpackedDir);
        const resolvedOutputFile = path.resolve(outputFile);
        const generated = {
            unpackedDir: resolvedUnpackedDir,
            scanMapTiles: {
                outputFile: path.join(resolvedUnpackedDir, 'map-tiles.json'),
                maps: maps.map(x => x.mapDirectory),
            },
            minipack: {
                include: [
                    {
                        map: 'ExampleMap',
                        bounds: {
                            minX: 0,
                            minY: 0,
                            maxX: 1,
                            maxY: 1,
                        },
                    },
                ],
                excludePatterns: [
                    'World/Maps/ExampleMap/*.wdl',
                ],
            },
            mapTrimTemplates: maps.map(map => ({
                mapDirectory: map.mapDirectory,
                tileCount: map.tileCount,
                bounds: map.bounds,
                notes: `Replace the example entries below with the ranges you want to keep for ${map.mapDirectory}.`,
                exampleIncludeEntry: {
                    map: map.mapDirectory,
                    bounds: {
                        minX: map.bounds.minX,
                        minY: map.bounds.minY,
                        maxX: Math.min(map.bounds.minX + 1, map.bounds.maxX),
                        maxY: Math.min(map.bounds.minY + 1, map.bounds.maxY),
                    },
                },
                exampleSecondaryRange: {
                    minX: map.bounds.minX,
                    minY: map.bounds.minY,
                    maxX: Math.min(map.bounds.minX + 1, map.bounds.maxX),
                    maxY: Math.min(map.bounds.minY + 1, map.bounds.maxY),
                },
            })),
        };

        wfs.write(resolvedOutputFile, JSON.stringify(generated, null, 4));
        term.log('client', `Wrote map config scaffold for ${maps.length} map(s) to ${resolvedOutputFile}`);
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
              'unpack-client'
            , '[input-dir output-dir] [--config=<file>]'
            , 'Unpacks every MPQ under the configured client/Data into same-named folders, centralizes shared assets, and removes files superseded by later patches'
            , async args => {
                const configured = this.extractConfigArg(args);
                const loaded = this.readPackageConfig(configured.configPath);
                if (configured.args.length > 2) {
                    throw new Error('Expected package unpack-client [input-dir output-dir] [--config=<file>]');
                }

                const inputDir = configured.args[0]
                    || loaded?.config.inputClientDir
                    || loaded?.config.sourceClientDir;
                const outputDir = configured.args[1]
                    || loaded?.config.unpackedDir;

                if (!inputDir || !outputDir) {
                    throw new Error(
                        'Expected package unpack-client <input-dir> <output-dir> or a config with inputClientDir/sourceClientDir and unpackedDir'
                    );
                }

                this.unpackClient(
                    loaded ? this.resolveConfigPath(loaded.configDir, inputDir)! : inputDir,
                    loaded ? this.resolveConfigPath(loaded.configDir, outputDir)! : outputDir
                );
            }
        )
        this.Command.addCommand(
              'minipack'
            , '[unpacked-dir source-client-dir output-client-dir] [compression|--compression=<type>] [--exclude=<pattern>] [--exclude-list=<file>] [--config=<file>]'
            , 'Rebuilds MPQs from an unpacked client tree, curates shared assets from ADT usage, and copies them into a fresh client directory'
            , async args => {
                const configured = this.extractConfigArg(args);
                const loaded = this.readPackageConfig(configured.configPath);
                const excluded = this.extractExcludeArgs(configured.args);
                const parsed = this.extractCompressionArg(excluded.args);
                if (parsed.args.length > 4) {
                    throw new Error(
                        'Expected package minipack [unpacked-dir source-client-dir output-client-dir] [compression] [--exclude=<pattern>] [--exclude-list=<file>] [--config=<file>]'
                    );
                }

                const unpackedDir = parsed.args[0]
                    || loaded?.config.unpackedDir;
                const sourceClientDir = parsed.args[1]
                    || loaded?.config.sourceClientDir
                    || loaded?.config.inputClientDir;
                const outputClientDir = parsed.args[2]
                    || loaded?.config.outputClientDir;
                const compression = parsed.compression
                    || (parsed.args[3] ? this.normalizeCompression(parsed.args[3]) : undefined)
                    || loaded?.config.minipack?.compression;
                const excludePatterns = this.getConfiguredExcludePatterns(configured.configPath)
                    .concat(excluded.patterns);
                const includeRules = this.getConfiguredIncludeRules(configured.configPath);

                if (!unpackedDir || !sourceClientDir || !outputClientDir) {
                    throw new Error(
                        'Expected package minipack <unpacked-dir> <source-client-dir> <output-client-dir> or a config with unpackedDir, sourceClientDir/inputClientDir, and outputClientDir'
                    );
                }

                this.restoreClient(
                    loaded ? this.resolveConfigPath(loaded.configDir, unpackedDir)! : unpackedDir,
                    loaded ? this.resolveConfigPath(loaded.configDir, sourceClientDir)! : sourceClientDir,
                    loaded ? this.resolveConfigPath(loaded.configDir, outputClientDir)! : outputClientDir,
                    excludePatterns,
                    includeRules,
                    compression,
                    configured.configPath
                );
            }
        )
        this.Command.addCommand(
              'scan-adt-assets'
            , 'adt-path output-file'
            , 'Scans ADT files and writes a deduplicated asset list (textures, M2s, WMOs)'
            , async args => {
                if (args.length !== 2) {
                    throw new Error('Expected package scan-adt-assets <adt-path> <output-file>');
                }
                this.scanAdtAssets(args[0], args[1]);
            }
        )
        this.Command.addCommand(
              'scan-map-tiles'
            , '[unpacked-dir output-file] [--map=<directory>] [--config=<file>]'
            , 'Scans the unpacked client tree and writes the ADT tile set that minipack would currently include'
            , async args => {
                const configured = this.extractConfigArg(args);
                const loaded = this.readPackageConfig(configured.configPath);
                const mapped = this.extractMapArgs(configured.args);
                if (mapped.args.length > 2) {
                    throw new Error('Expected package scan-map-tiles [unpacked-dir output-file] [--map=<directory>] [--config=<file>]');
                }

                const unpackedDir = mapped.args[0]
                    || loaded?.config.unpackedDir;
                const outputFile = mapped.args[1]
                    || loaded?.config.scanMapTiles?.outputFile
                    || (unpackedDir ? path.join(unpackedDir, 'map-tiles.json') : undefined);
                const mapFilters = (loaded?.config.scanMapTiles?.maps || []).concat(mapped.maps);
                const excludePatterns = this.getConfiguredExcludePatterns(configured.configPath);
                const includeRules = this.getConfiguredIncludeRules(configured.configPath);

                if (!unpackedDir || !outputFile) {
                    throw new Error(
                        'Expected package scan-map-tiles <unpacked-dir> <output-file> or a config with unpackedDir and scanMapTiles.outputFile'
                    );
                }

                this.scanMapTiles(
                    loaded ? this.resolveConfigPath(loaded.configDir, unpackedDir)! : unpackedDir,
                    loaded ? this.resolveConfigPath(loaded.configDir, outputFile)! : outputFile,
                    excludePatterns,
                    includeRules,
                    mapFilters
                );
            }
        )
        this.Command.addCommand(
              'scaffold-map-config'
            , '[unpacked-dir output-file] [--config=<file>]'
            , 'Scans unpacked map tiles and writes a config stub with a populated map list and dummy exclusion entries'
            , async args => {
                const configured = this.extractConfigArg(args);
                const loaded = this.readPackageConfig(configured.configPath);
                if (configured.args.length > 2) {
                    throw new Error('Expected package scaffold-map-config [unpacked-dir output-file] [--config=<file>]');
                }

                const unpackedDir = configured.args[0]
                    || loaded?.config.unpackedDir;
                const outputFile = configured.args[1]
                    || (loaded
                        ? path.join(loaded.configDir, 'tswow-map-trim.generated.json')
                        : path.resolve('tswow-map-trim.generated.json'));

                if (!unpackedDir) {
                    throw new Error(
                        'Expected package scaffold-map-config <unpacked-dir> <output-file> or a config with unpackedDir'
                    );
                }

                this.scaffoldMapConfig(
                    loaded ? this.resolveConfigPath(loaded.configDir, unpackedDir)! : unpackedDir,
                    loaded ? this.resolveConfigPath(loaded.configDir, outputFile)! : outputFile,
                    configured.configPath
                );
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
