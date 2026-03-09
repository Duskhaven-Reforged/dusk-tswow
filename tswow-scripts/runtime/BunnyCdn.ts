import * as fs from 'fs';
import * as path from 'path';
import * as request from 'request';
import { wfs } from "../util/FileSystem";
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
    const key = process.env[envName];
    if (!key) {
        throw new Error(`Missing Bunny access key environment variable ${envName}`);
    }
    return key;
}

function maybeAbsoluteUrl(remotePath: string, baseUrl: string) {
    return remotePath.startsWith('http://') || remotePath.startsWith('https://')
        ? remotePath
        : new URL(encodeRemotePath(remotePath), baseUrl).toString();
}

function isStorageUrl(url: string) {
    try {
        return new URL(url).hostname.includes('storage.bunnycdn.com');
    } catch (_err) {
        return false;
    }
}

export namespace BunnyCdn {
    export function pullUrl(remotePath: string) {
        return maybeAbsoluteUrl(remotePath, resolvePullBaseUrl());
    }

    export function storageUrl(remotePath: string) {
        return maybeAbsoluteUrl(remotePath, resolveStorageBaseUrl());
    }

    export function downloadFile(remotePath: string, localPath: string) {
        const url = pullUrl(remotePath);
        term.debug('client', `Downloading supplementary patch ${url}`);
        return new Promise<void>((resolve, reject) => {
            wfs.mkDirs(path.dirname(localPath));

            let settled = false;
            const finish = (err?: Error) => {
                if (settled) return;
                settled = true;
                if (err) {
                    if (fs.existsSync(localPath)) {
                        fs.unlinkSync(localPath);
                    }
                    reject(err);
                } else {
                    resolve();
                }
            };

            const writer = fs.createWriteStream(localPath);
            const req = request.get({
                url,
                headers: isStorageUrl(url)
                    ? { AccessKey: accessKey() }
                    : undefined,
            });

            req.on('response', res => {
                if ((res.statusCode || 500) >= 400) {
                    req.abort();
                    finish(new Error(`Failed to download ${url}: ${res.statusCode}`));
                }
            });

            req.on('error', err => finish(err));
            writer.on('error', err => finish(err));
            writer.on('finish', () => finish());
            req.pipe(writer);
        });
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
                },
            }, (err, res) => {
                if (err) {
                    finish(err);
                    return;
                }
                if ((res?.statusCode || 500) >= 400) {
                    finish(new Error(`Failed to upload ${localPath} to ${url}: ${res?.statusCode}`));
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
