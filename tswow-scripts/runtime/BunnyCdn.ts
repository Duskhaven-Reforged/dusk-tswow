import * as fs from 'fs';
import * as http from 'http';
import * as https from 'https';
import * as path from 'path';
import * as request from 'request';
import { term } from "../util/Terminal";
import { NodeConfig } from "./NodeConfig";

function ensureTrailingSlash(url: string) {
    return url.endsWith('/') ? url : `${url}/`;
}

function normalizeRemotePath(remotePath: string) {
    return remotePath.replace(/\\/g, '/').replace(/^\/+/, '');
}

function encodeRemotePath(remotePath: string) {
    return normalizeRemotePath(remotePath)
        .split('/')
        .map(part => encodeURIComponent(part))
        .join('/');
}

function resolvePullBaseUrl() {
    const configured = NodeConfig.LauncherFileServerUrl || process.env.VITE_FILESERVER_URL;
    if (!configured) {
        throw new Error(
            `Missing Bunny pull-zone URL. Set Launcher.FileServerUrl in node.conf or VITE_FILESERVER_URL in the environment.`
        );
    }
    return ensureTrailingSlash(configured);
}

function resolveStorageBaseUrl() {
    const endpoint = ensureTrailingSlash(NodeConfig.LauncherBunnyStorageEndpoint || 'https://storage.bunnycdn.com');
    const zone = (NodeConfig.LauncherBunnyStorageZone || 'duskhaven-patches').replace(/^\/+|\/+$/g, '');
    return `${endpoint}${zone}/`;
}

function accessKey() {
    const envName = NodeConfig.LauncherBunnyAccessKeyEnv || 'BUNNY_ACCESS_KEY';
    const key = process.env[envName]?.trim();
    if (!key) {
        throw new Error(`Missing Bunny access key environment variable ${envName}`);
    }
    return key;
}

function accessKeyEnvName() {
    return NodeConfig.LauncherBunnyAccessKeyEnv || 'BUNNY_ACCESS_KEY';
}

function maybeAbsoluteUrl(remotePath: string, baseUrl: string) {
    return remotePath.startsWith('http://') || remotePath.startsWith('https://')
        ? remotePath
        : new URL(encodeRemotePath(remotePath), baseUrl).toString();
}

type DownloadStream = {
    response: http.IncomingMessage;
    totalBytes: number;
    startByte: number;
    alreadyComplete?: boolean;
};

function openDownloadStream(url: string, startByte = 0, redirectCount = 0): Promise<DownloadStream> {
    return new Promise((resolve, reject) => {
        const parsedUrl = new URL(url);
        const transport = parsedUrl.protocol === 'https:' ? https : http;
        const headers: Record<string, string> = {
            AccessKey: accessKey(),
            'Accept-Encoding': 'identity',
            'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64)',
        };

        if (startByte > 0) {
            headers.Range = `bytes=${startByte}-`;
        }

        const req = transport.get(parsedUrl, {
            headers,
        }, res => {
            const status = res.statusCode || 0;

            if (status >= 300 && status < 400 && res.headers.location) {
                if (redirectCount >= 5) {
                    res.resume();
                    reject(new Error(`Too many redirects while downloading ${url}`));
                    return;
                }

                const nextUrl = new URL(res.headers.location, parsedUrl).toString();
                res.resume();
                openDownloadStream(nextUrl, startByte, redirectCount + 1).then(resolve, reject);
                return;
            }

            if (status >= 400) {
                if (status === 416 && startByte > 0) {
                    const contentRange = `${res.headers['content-range'] || ''}`;
                    const match = contentRange.match(/bytes\s+\*\/(\d+)/i);
                    if (match) {
                        const actualTotal = parseInt(match[1], 10);
                        res.resume();
                        resolve({
                            response: res,
                            totalBytes: actualTotal,
                            startByte,
                            alreadyComplete: startByte >= actualTotal,
                        });
                        return;
                    }
                }

                const message = res.statusMessage || 'Request failed';
                res.resume();
                reject(new Error(`Failed to download: ${status} ${message}`));
                return;
            }

            if (startByte > 0 && status !== 206) {
                res.resume();
                reject(new Error(`Server did not honor range request for ${url} starting at byte ${startByte}`));
                return;
            }

            let totalBytes = parseInt(`${res.headers['content-length'] || '0'}`, 10) || 0;
            if (status === 206) {
                const contentRange = `${res.headers['content-range'] || ''}`;
                const match = contentRange.match(/bytes\s+(\d+)-(\d+)\/(\d+|\*)/i);
                if (!match) {
                    res.resume();
                    reject(new Error(`Missing content-range for resumed download of ${url}`));
                    return;
                }
                const rangeStart = parseInt(match[1], 10);
                const rangeTotal = match[3] === '*' ? 0 : parseInt(match[3], 10);
                if (rangeStart !== startByte) {
                    res.resume();
                    reject(new Error(`Unexpected resume offset for ${url}: requested ${startByte}, got ${rangeStart}`));
                    return;
                }
                totalBytes = rangeTotal || (startByte + totalBytes);
            }

            resolve({
                response: res,
                totalBytes,
                startByte,
            });
        });

        req.on('error', reject);
    });
}

export namespace BunnyCdn {
    export function pullUrl(remotePath: string) {
        return maybeAbsoluteUrl(remotePath, resolvePullBaseUrl());
    }

    export function storageUrl(remotePath: string) {
        return maybeAbsoluteUrl(remotePath, resolveStorageBaseUrl());
    }

    function renderProgress(received: number, total: number) {
        const width = 40;

        const percent = total ? received / total : 0;
        const filled = Math.floor(width * percent);

        const bar =
            '#'.repeat(filled) +
            '>'.repeat(filled < width ? 1 : 0) +
            '-'.repeat(width - filled - (filled < width ? 1 : 0));

        const mbReceived = (received / (1024 * 1024)).toFixed(1);
        const mbTotal = total ? (total / (1024 * 1024)).toFixed(1) : '?';

        process.stdout.write(
            `\r\x1b[KDownloading: [${bar}] ${(percent * 100).toFixed(1)}% (${mbReceived}/${mbTotal} MB)`
        );
    }

    export async function downloadFile(remotePath: string, localPath: string) {
        const url = pullUrl(remotePath);

        const dir = path.dirname(localPath);
        if (!fs.existsSync(dir)) {
            fs.mkdirSync(dir, { recursive: true });
        }

        const tempPath = `${localPath}.part`;
        if (fs.existsSync(tempPath)) {
            fs.rmSync(tempPath, { force: true });
        }

        term.log('package', `Downloading ${remotePath} -> ${localPath}`);
        const maxAttempts = 4;
        let expectedTotal = 0;
        let lastError: Error | undefined;

        for (let attempt = 1; attempt <= maxAttempts; ++attempt) {
            const startByte = fs.existsSync(tempPath) ? fs.statSync(tempPath).size : 0;
            if (expectedTotal > 0 && startByte >= expectedTotal) {
                break;
            }

            if (attempt > 1) {
                term.warn('package', `Retrying ${remotePath} from byte ${startByte} (attempt ${attempt}/${maxAttempts})`);
            }

            try {
                await new Promise<void>((resolve, reject) => {
                    let settled = false;
                    let loaded = startByte;
                    let responseComplete = false;
                    let finalized = false;

                    const writer = fs.createWriteStream(tempPath, { flags: startByte > 0 ? 'a' : 'w' });
                    const bytesWritten = () => startByte + writer.bytesWritten;
                    const hasReceivedWholeBody = () => expectedTotal > 0 && Math.max(loaded, bytesWritten()) >= expectedTotal;

                    const finalizeResponse = () => {
                        if (finalized) {
                            return;
                        }
                        finalized = true;
                        if (!writer.writableEnded) {
                            writer.end();
                        }
                    };

                    const finish = (err?: Error) => {
                        if (settled) {
                            return;
                        }
                        settled = true;

                        if (err) {
                            try {
                                writer.destroy();
                            } catch {}
                            reject(err);
                            return;
                        }

                        resolve();
                    };

                    writer.on('error', err => finish(err));
                    writer.on('finish', () => {
                        if (!responseComplete || !hasReceivedWholeBody()) {
                            finish(new Error(
                                `Download aborted by remote host for ${remotePath} `
                                + `(received ${Math.max(loaded, bytesWritten())} of ${expectedTotal || '?'} bytes; temp file: ${tempPath})`
                            ));
                            return;
                        }
                        finish();
                    });

                    openDownloadStream(url, startByte).then(stream => {
                        if (stream.alreadyComplete) {
                            expectedTotal = stream.totalBytes;
                            responseComplete = true;
                            finalizeResponse();
                            return;
                        }

                        const res = stream.response;
                        expectedTotal = stream.totalBytes;

                        if (startByte > 0 && expectedTotal > 0) {
                            renderProgress(startByte, expectedTotal);
                        }

                        res.on('data', (chunk: Buffer) => {
                            loaded += chunk.length;
                            renderProgress(loaded, expectedTotal);
                        });

                        res.on('end', () => {
                            responseComplete = res.complete || hasReceivedWholeBody();
                            if (!responseComplete) {
                                finish(new Error(
                                    `Download aborted by remote host for ${remotePath} `
                                    + `(received ${Math.max(loaded, bytesWritten())} of ${expectedTotal || '?'} bytes; temp file: ${tempPath})`
                                ));
                                return;
                            }
                            finalizeResponse();
                        });

                        res.on('aborted', () => {
                            if (hasReceivedWholeBody()) {
                                responseComplete = true;
                                finalizeResponse();
                                return;
                            }
                            finish(new Error(
                                `Download aborted by remote host for ${remotePath} `
                                + `(received ${Math.max(loaded, bytesWritten())} of ${expectedTotal || '?'} bytes; temp file: ${tempPath})`
                            ));
                        });

                        res.on('close', () => {
                            if (hasReceivedWholeBody()) {
                                responseComplete = true;
                                finalizeResponse();
                            }
                        });

                        res.on('error', err => {
                            if (hasReceivedWholeBody()) {
                                responseComplete = true;
                                finalizeResponse();
                                return;
                            }
                            finish(err);
                        });

                        res.pipe(writer);
                    }).catch(err => finish(err));
                });

                if (expectedTotal > 0 && fs.statSync(tempPath).size >= expectedTotal) {
                    break;
                }
            } catch (err: any) {
                lastError = err;
                if (attempt === maxAttempts) {
                    process.stdout.write('\n');
                    throw err;
                }
            }
        }

        const finalSize = fs.existsSync(tempPath) ? fs.statSync(tempPath).size : 0;
        process.stdout.write('\n');
        if (expectedTotal > 0 && finalSize < expectedTotal) {
            throw lastError || new Error(
                `Download aborted by remote host for ${remotePath} `
                + `(received ${finalSize} of ${expectedTotal} bytes; temp file: ${tempPath})`
            );
        }

        if (fs.existsSync(localPath)) {
            fs.rmSync(localPath, { force: true });
        }
        fs.renameSync(tempPath, localPath);
        return localPath;
    }

    export function uploadFile(remotePath: string, localPath: string, contentType = 'application/octet-stream', dryRun = false) {
        const url = storageUrl(remotePath);
        if (dryRun) {
            term.log('client', `Dry run: would upload ${localPath} -> ${url}`);
            return Promise.resolve();
        }

        term.debug('client', `Uploading release artifact ${localPath} -> ${url}`);
        return new Promise<void>((resolve, reject) => {
            const stat = fs.statSync(localPath);
            let settled = false;
            const finish = (err?: Error) => {
                if (settled) return;
                settled = true;
                if (err) {
                    reject(err);
                } else {
                    resolve();
                }
            };

            const req = request.put({
                url,
                headers: {
                    AccessKey: accessKey(),
                    'Content-Type': contentType,
                    'Content-Length': stat.size,
                    'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64)',
                    'Accept': '*/*',
                    'Connection': 'keep-alive'
                },
            }, (err, res) => {
                if (err) {
                    finish(err);
                    return;
                }
                if ((res?.statusCode || 500) >= 400) {
                    const responseText = typeof res?.body === 'string'
                        ? res.body.trim()
                        : '';
                    finish(new Error(
                        `Failed to upload ${localPath} to ${url}: ${res?.statusCode}`
                        + ` (storageZone=${NodeConfig.LauncherBunnyStorageZone || 'duskhaven-patches'}`
                        + ` env=${accessKeyEnvName()}`
                        + ` keyPresent=${process.env[accessKeyEnvName()] ? 'yes' : 'no'}`
                        + `${responseText.length > 0 ? ` body=${responseText}` : ''})`
                    ));
                    return;
                }
                finish();
            });

            req.on('error', err => finish(err));
            fs.createReadStream(localPath)
                .on('error', err => finish(err))
                .pipe(req);
        });
    }
}
