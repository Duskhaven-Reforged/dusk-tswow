import * as crypto from 'crypto';
import * as fs from 'fs';
import * as path from 'path';
import { wfs } from "../util/FileSystem";
import { ReleaseArtifact } from "./ReleaseManifest";

export interface ChunkedReleaseFile {
    path: string;
    size: number;
    checksum: string;
    chunks: string[];
}

export interface ChunkedReleaseBundleChunk {
    id: string;
    offset: number;
    size: number;
    rawSize: number;
}

export interface ChunkedReleaseBundle {
    urlSuffix: string;
    size: number;
    chunks: ChunkedReleaseBundleChunk[];
}

export interface ChunkedReleaseManifest {
    version: string;
    dataset: string;
    generatedAt: string;
    compression: 'none';
    chunking: {
        algorithm: 'gear-hash-cdc';
        minChunkSize: number;
        avgChunkSize: number;
        maxChunkSize: number;
        targetBundleSize: number;
        maskBits: number;
    };
    files: ChunkedReleaseFile[];
    bundles: { [bundleId: string]: ChunkedReleaseBundle };
}

export interface ChunkedReleaseResult {
    manifest: ChunkedReleaseManifest;
    outputDir: string;
    manifestPath: string;
}

export interface ChunkedReleaseVerificationResult {
    manifest: ChunkedReleaseManifest;
    outputDir: string;
    verificationDir: string;
    bundlesVerified: number;
    indexedChunks: number;
    filesVerified: number;
}

export interface ChunkedReleasePublishBundle {
    bundleId: string;
    localPath: string;
    remotePath: string;
}

export interface ChunkedReleasePublishLayout {
    manifest: ChunkedReleaseManifest;
    publishedManifest: ChunkedReleaseManifest;
    outputDir: string;
    version: string;
    versionRoot: string;
    remoteVersionRoot: string;
    remoteManifestPath: string;
    remoteLatestManifestPath: string;
    bundles: ChunkedReleasePublishBundle[];
}

interface ChunkingOptions {
    minChunkSize: number;
    avgChunkSize: number;
    maxChunkSize: number;
    targetBundleSize: number;
}

interface IndexedChunkLocation {
    bundleId: string;
    bundlePath: string;
    offset: number;
    size: number;
    rawSize: number;
}

const DEFAULT_OPTIONS: ChunkingOptions = {
    minChunkSize: 256 * 1024,
    avgChunkSize: 512 * 1024,
    maxChunkSize: 1024 * 1024,
    targetBundleSize: 64 * 1024 * 1024,
};

function sha256Buffer(buffer: Buffer) {
    return crypto.createHash('sha256').update(buffer).digest('hex');
}

function sha256File(filePath: string) {
    const handle = fs.openSync(filePath, 'r');
    const hash = crypto.createHash('sha256');
    try {
        const buf = Buffer.alloc(1024 * 1024);
        while (true) {
            const nread = fs.readSync(handle, buf, 0, buf.length, null);
            if (nread === 0) {
                break;
            }
            hash.update(nread < buf.length ? buf.slice(0, nread) : buf);
        }
    } finally {
        fs.closeSync(handle);
    }
    return hash.digest('hex');
}

function normalizeManifestRelativePath(value: string, fieldName: string) {
    const normalized = value.replace(/\\/g, '/').replace(/^\/+/, '').trim();
    if (!normalized || normalized.split('/').includes('..')) {
        throw new Error(`Invalid ${fieldName} path ${value}`);
    }
    return normalized;
}

function normalizeRemoteRoot(value: string) {
    return value.replace(/\\/g, '/').replace(/^\/+|\/+$/g, '');
}

function combineRemotePath(...parts: string[]) {
    return parts
        .map(part => normalizeRemoteRoot(part))
        .filter(part => part.length > 0)
        .join('/');
}

function normalizeReleaseVersion(version: string) {
    const normalized = version
        .trim()
        .replace(/[:]/g, '-')
        .replace(/[\\/?#%*:|"<>]+/g, '-')
        .replace(/\s+/g, '-');
    if (!normalized) {
        throw new Error(`Invalid chunked release version ${version}`);
    }
    return normalized;
}

function createGearTable() {
    const table: number[] = [];
    let seed = 0x12345678;
    for (let i = 0; i < 256; ++i) {
        seed = (seed * 1664525 + 1013904223) >>> 0;
        table.push(seed);
    }
    return table;
}

const GEAR_TABLE = createGearTable();

function createMaskBits(avgChunkSize: number) {
    return Math.max(8, Math.round(Math.log2(avgChunkSize)));
}

function chunkFile(
    filePath: string,
    options: ChunkingOptions,
    onChunk: (buffer: Buffer) => void
) {
    const maskBits = createMaskBits(options.avgChunkSize);
    const mask = maskBits >= 31 ? 0x7fffffff : ((1 << maskBits) - 1);
    const readBuffer = Buffer.alloc(1024 * 1024);
    const handle = fs.openSync(filePath, 'r');

    let chunkParts: Buffer[] = [];
    let chunkSize = 0;
    let rollingHash = 0;

    const flushChunk = () => {
        if (chunkSize === 0) {
            return;
        }
        onChunk(Buffer.concat(chunkParts, chunkSize));
        chunkParts = [];
        chunkSize = 0;
        rollingHash = 0;
    };

    try {
        while (true) {
            const nread = fs.readSync(handle, readBuffer, 0, readBuffer.length, null);
            if (nread === 0) {
                break;
            }

            let sliceStart = 0;
            for (let i = 0; i < nread; ++i) {
                const byte = readBuffer[i];
                rollingHash = ((((rollingHash << 1) >>> 0) + GEAR_TABLE[byte]) >>> 0);
                chunkSize += 1;

                const shouldSplit =
                    chunkSize >= options.minChunkSize &&
                    (((rollingHash & mask) === 0) || chunkSize >= options.maxChunkSize);

                if (!shouldSplit) {
                    continue;
                }

                chunkParts.push(Buffer.from(readBuffer.subarray(sliceStart, i + 1)));
                flushChunk();
                sliceStart = i + 1;
            }

            if (sliceStart < nread) {
                chunkParts.push(Buffer.from(readBuffer.subarray(sliceStart, nread)));
            }
        }
    } finally {
        fs.closeSync(handle);
    }

    flushChunk();
}

class BundleWriter {
    private bundleIndex = 0;
    private currentBundleId = '';
    private currentBundlePath = '';
    private currentHandle = -1;
    private currentSize = 0;
    private readonly chunkRefs = new Map<string, { bundleId: string; offset: number; size: number }>();
    private readonly bundles: { [bundleId: string]: ChunkedReleaseBundle } = {};

    constructor(
        private readonly outputDir: string,
        private readonly targetBundleSize: number
    ) {}

    private openNextBundle() {
        if (this.currentHandle !== -1) {
            fs.closeSync(this.currentHandle);
        }

        this.currentBundleId = `bundle-${this.bundleIndex.toString().padStart(4, '0')}`;
        this.currentBundlePath = path.join(this.outputDir, 'bundles', `${this.currentBundleId}.bundle`);
        this.currentHandle = fs.openSync(this.currentBundlePath, 'w');
        this.currentSize = 0;
        this.bundles[this.currentBundleId] = {
            urlSuffix: `bundles/${this.currentBundleId}.bundle`,
            size: 0,
            chunks: [],
        };
        this.bundleIndex += 1;
    }

    addChunk(chunkId: string, buffer: Buffer) {
        const existing = this.chunkRefs.get(chunkId);
        if (existing) {
            return existing;
        }

        if (this.currentHandle === -1 || (this.currentSize > 0 && this.currentSize + buffer.length > this.targetBundleSize)) {
            this.openNextBundle();
        }

        const offset = this.currentSize;
        fs.writeSync(this.currentHandle, buffer, 0, buffer.length, null);
        this.currentSize += buffer.length;

        const bundle = this.bundles[this.currentBundleId];
        bundle.size = this.currentSize;
        bundle.chunks.push({
            id: chunkId,
            offset,
            size: buffer.length,
            rawSize: buffer.length,
        });

        const ref = { bundleId: this.currentBundleId, offset, size: buffer.length };
        this.chunkRefs.set(chunkId, ref);
        return ref;
    }

    finish() {
        if (this.currentHandle !== -1) {
            fs.closeSync(this.currentHandle);
            this.currentHandle = -1;
        }
        return this.bundles;
    }
}

export function createChunkedRelease(
    dataset: string,
    artifacts: ReleaseArtifact[],
    outputDir: string
) {
    const options = DEFAULT_OPTIONS;
    const maskBits = createMaskBits(options.avgChunkSize);
    wfs.mkDirs(outputDir, true);
    wfs.mkDirs(path.join(outputDir, 'bundles'));

    const bundleWriter = new BundleWriter(outputDir, options.targetBundleSize);
    const files: ChunkedReleaseFile[] = artifacts
        .map(artifact => {
            const chunks: string[] = [];
            chunkFile(artifact.sourcePath, options, buffer => {
                const chunkId = sha256Buffer(buffer);
                bundleWriter.addChunk(chunkId, buffer);
                chunks.push(chunkId);
            });

            return {
                path: artifact.releasePath.replace(/\\/g, '/'),
                size: fs.statSync(artifact.sourcePath).size,
                checksum: sha256File(artifact.sourcePath),
                chunks,
            };
        })
        .sort((left, right) => left.path.localeCompare(right.path));

    const manifest: ChunkedReleaseManifest = {
        version: new Date().toISOString(),
        dataset,
        generatedAt: new Date().toISOString(),
        compression: 'none',
        chunking: {
            algorithm: 'gear-hash-cdc',
            minChunkSize: options.minChunkSize,
            avgChunkSize: options.avgChunkSize,
            maxChunkSize: options.maxChunkSize,
            targetBundleSize: options.targetBundleSize,
            maskBits,
        },
        files,
        bundles: bundleWriter.finish(),
    };

    const manifestPath = path.join(outputDir, 'manifest.json');
    wfs.write(manifestPath, JSON.stringify(manifest, null, 4));

    return {
        manifest,
        outputDir,
        manifestPath,
    } as ChunkedReleaseResult;
}

export function readChunkedReleaseManifest(manifestPath: string) {
    return JSON.parse(wfs.read(manifestPath)) as ChunkedReleaseManifest;
}

export function buildChunkedReleasePublishLayout(manifestPath: string, publishPrefix = ''): ChunkedReleasePublishLayout {
    const manifest = readChunkedReleaseManifest(manifestPath);
    const outputDir = path.dirname(manifestPath);
    const version = normalizeReleaseVersion(manifest.version);
    const versionRoot = combineRemotePath('releases', version);
    const remoteRoot = normalizeRemoteRoot(publishPrefix);
    const remotePath = (...parts: string[]) => combineRemotePath(remoteRoot, ...parts);

    const bundles = Object.entries(manifest.bundles)
        .sort(([left], [right]) => left.localeCompare(right))
        .map(([bundleId, bundle]) => {
            const localRelativePath = normalizeManifestRelativePath(bundle.urlSuffix, `bundle ${bundleId} urlSuffix`);
            const localPath = path.join(outputDir, ...localRelativePath.split('/'));
            if (!wfs.isFile(localPath)) {
                throw new Error(`Missing bundle file ${localPath}`);
            }
            return {
                bundleId,
                localPath,
                remotePath: remotePath(versionRoot, 'bundles', `${bundleId}.bundle`),
            };
        });

    const publishedBundles: { [bundleId: string]: ChunkedReleaseBundle } = {};
    Object.entries(manifest.bundles).forEach(([bundleId, bundle]) => {
        publishedBundles[bundleId] = {
            ...bundle,
            urlSuffix: combineRemotePath(versionRoot, 'bundles', `${bundleId}.bundle`),
        };
    });

    const publishedManifest: ChunkedReleaseManifest = {
        ...manifest,
        bundles: publishedBundles,
    };

    return {
        manifest,
        publishedManifest,
        outputDir,
        version,
        versionRoot,
        remoteVersionRoot: remotePath(versionRoot),
        remoteManifestPath: remotePath(versionRoot, 'manifest.json'),
        remoteLatestManifestPath: remotePath('manifest.json'),
        bundles,
    };
}

export function verifyChunkedRelease(manifestPath: string): ChunkedReleaseVerificationResult {
    const manifest = readChunkedReleaseManifest(manifestPath);
    if (manifest.compression !== 'none') {
        throw new Error(`Unsupported chunked release compression ${manifest.compression}`);
    }

    const outputDir = path.dirname(manifestPath);
    const verificationDir = path.join(outputDir, 'verify');
    wfs.mkDirs(verificationDir, true);

    const chunkIndex = new Map<string, IndexedChunkLocation>();
    const bundleHandles = new Map<string, number>();

    const getBundleHandle = (bundlePath: string) => {
        const existing = bundleHandles.get(bundlePath);
        if (existing !== undefined) {
            return existing;
        }

        const handle = fs.openSync(bundlePath, 'r');
        bundleHandles.set(bundlePath, handle);
        return handle;
    };

    try {
        for (const [bundleId, bundle] of Object.entries(manifest.bundles)) {
            const urlSuffix = normalizeManifestRelativePath(bundle.urlSuffix, `bundle ${bundleId} urlSuffix`);
            const bundlePath = path.join(outputDir, ...urlSuffix.split('/'));
            if (!wfs.isFile(bundlePath)) {
                throw new Error(`Missing bundle file ${bundlePath}`);
            }

            const stat = fs.statSync(bundlePath);
            if (stat.size !== bundle.size) {
                throw new Error(
                    `Bundle size mismatch for ${bundleId}: manifest=${bundle.size}, file=${stat.size}`
                );
            }

            let expectedOffset = 0;
            for (const chunk of bundle.chunks) {
                if (chunkIndex.has(chunk.id)) {
                    throw new Error(`Chunk ${chunk.id} appears in multiple bundles`);
                }
                if (chunk.offset !== expectedOffset) {
                    throw new Error(
                        `Bundle ${bundleId} chunk ${chunk.id} has unexpected offset ${chunk.offset}, expected ${expectedOffset}`
                    );
                }
                if (chunk.size < 0 || chunk.rawSize < 0) {
                    throw new Error(`Bundle ${bundleId} chunk ${chunk.id} has invalid size metadata`);
                }
                if (chunk.offset + chunk.size > bundle.size) {
                    throw new Error(`Bundle ${bundleId} chunk ${chunk.id} extends past the end of the bundle`);
                }

                chunkIndex.set(chunk.id, {
                    bundleId,
                    bundlePath,
                    offset: chunk.offset,
                    size: chunk.size,
                    rawSize: chunk.rawSize,
                });
                expectedOffset += chunk.size;
            }

            if (expectedOffset !== bundle.size) {
                throw new Error(
                    `Bundle ${bundleId} contents total ${expectedOffset} bytes, expected ${bundle.size}`
                );
            }
        }

        for (const file of manifest.files) {
            const releasePath = normalizeManifestRelativePath(file.path, 'file');
            const verifyPath = path.join(verificationDir, ...releasePath.split('/'));
            wfs.mkDirs(path.dirname(verifyPath));

            const hash = crypto.createHash('sha256');
            const writer = fs.openSync(verifyPath, 'w');
            let totalBytes = 0;
            try {
                for (const chunkId of file.chunks) {
                    const location = chunkIndex.get(chunkId);
                    if (!location) {
                        throw new Error(`Missing chunk ${chunkId} required by ${releasePath}`);
                    }

                    const buffer = Buffer.alloc(location.size);
                    const nread = fs.readSync(
                        getBundleHandle(location.bundlePath),
                        buffer,
                        0,
                        location.size,
                        location.offset
                    );
                    if (nread !== location.size) {
                        throw new Error(
                            `Failed to read chunk ${chunkId} from ${location.bundleId}: expected ${location.size}, got ${nread}`
                        );
                    }

                    fs.writeSync(writer, buffer, 0, buffer.length, null);
                    hash.update(buffer);
                    totalBytes += location.rawSize;
                }
            } finally {
                fs.closeSync(writer);
            }

            const checksum = hash.digest('hex');
            if (totalBytes !== file.size) {
                throw new Error(
                    `Reassembled size mismatch for ${releasePath}: manifest=${file.size}, rebuilt=${totalBytes}`
                );
            }
            if (checksum !== file.checksum) {
                throw new Error(
                    `Checksum mismatch for ${releasePath}: manifest=${file.checksum}, rebuilt=${checksum}`
                );
            }
        }
    } finally {
        for (const handle of bundleHandles.values()) {
            fs.closeSync(handle);
        }
    }

    return {
        manifest,
        outputDir,
        verificationDir,
        bundlesVerified: Object.keys(manifest.bundles).length,
        indexedChunks: chunkIndex.size,
        filesVerified: manifest.files.length,
    };
}
