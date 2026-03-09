import * as crypto from 'crypto';
import * as fs from 'fs';

export interface ReleaseManifestFile {
    path: string;
    size: number;
    checksum: string;
}

export interface ReleaseManifest {
    version: string;
    dataset: string;
    generatedAt: string;
    compression: 'none';
    files: ReleaseManifestFile[];
}

export interface ReleaseArtifact {
    sourcePath: string;
    releasePath: string;
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

export function createReleaseManifest(dataset: string, artifacts: ReleaseArtifact[]): ReleaseManifest {
    const files = artifacts
        .map(artifact => ({
            path: artifact.releasePath.replace(/\\/g, '/'),
            size: fs.statSync(artifact.sourcePath).size,
            checksum: sha256File(artifact.sourcePath),
        }))
        .sort((left, right) => left.path.localeCompare(right.path));

    return {
        version: new Date().toISOString(),
        dataset,
        generatedAt: new Date().toISOString(),
        compression: 'none',
        files,
    };
}
