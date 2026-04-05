import * as fs from 'fs';
import * as path from 'path';
import { ipaths } from "../util/Paths";

export interface SupplementaryPatchInput {
    remotePath: string;
    target: string;
    required?: boolean;
}

export interface SupplementaryPatchTarget {
    outputPath?: string;
}

export interface SupplementaryPatchDatasetConfig {
    publishPrefix?: string;
    localPackages?: { [packageName: string]: string };
    targets?: { [target: string]: SupplementaryPatchTarget };
    inputs?: SupplementaryPatchInput[];
}

export interface SupplementaryPatchConfigFile {
    datasets?: { [datasetName: string]: SupplementaryPatchDatasetConfig };
}

export interface ResolvedSupplementaryPatchConfig {
    publishPrefix: string;
    localPackages: { [packageName: string]: string };
    targets: { [target: string]: SupplementaryPatchTarget };
    inputs: Required<SupplementaryPatchInput>[];
}

export function supplementaryPatchConfigPath() {
    return path.resolve(ipaths.node_conf.dirname().get(), 'supplementary-patches.json');
}

function normalizeRelativePath(value: string) {
    return value.replace(/\\/g, '/').replace(/^\/+/, '').trim();
}

function normalizePrefix(value: string | undefined, fallback: string) {
    if (value === undefined) return fallback;
    return value.replace(/\\/g, '/').replace(/^\/+|\/+$/g, '');
}

function mergeDatasetConfig(
    base: SupplementaryPatchDatasetConfig | undefined,
    override: SupplementaryPatchDatasetConfig | undefined,
    datasetName: string
): ResolvedSupplementaryPatchConfig {
    const inputs = [
        ...(base?.inputs || []),
        ...(override?.inputs || []),
    ].map(input => ({
        remotePath: normalizeRelativePath(input.remotePath),
        target: input.target.trim(),
        required: input.required !== false,
    }));

    const localPackages = {
        ...(base?.localPackages || {}),
        ...(override?.localPackages || {}),
    };

    Object.keys(localPackages).forEach(key => {
        localPackages[key] = normalizeRelativePath(localPackages[key]);
    });

    const targets = {
        ...(base?.targets || {}),
        ...(override?.targets || {}),
    };

    Object.values(targets).forEach(target => {
        if (target.outputPath) {
            target.outputPath = normalizeRelativePath(target.outputPath);
        }
    });

    return {
        publishPrefix: normalizePrefix(
            override?.publishPrefix ?? base?.publishPrefix,
            ''
        ),
        localPackages,
        targets,
        inputs,
    };
}

export function readSupplementaryPatchConfig(datasetName: string): ResolvedSupplementaryPatchConfig {
    const configPath = supplementaryPatchConfigPath();
    let raw: SupplementaryPatchConfigFile = {};
    if (fs.existsSync(configPath)) {
        raw = JSON.parse(fs.readFileSync(configPath, 'utf-8')) as SupplementaryPatchConfigFile;
    } else {
        raw = {};
    }

    const base = raw.datasets?.['*'];
    const specific = raw.datasets?.[datasetName];
    return mergeDatasetConfig(base, specific, datasetName);
}

export function resolveTargetOutputPath(targetName: string, config: ResolvedSupplementaryPatchConfig) {
    const override = config.targets[targetName]?.outputPath;
    if (override) {
        return normalizeRelativePath(override);
    }

    const fileName = targetName.toLowerCase().endsWith('.mpq')
        ? targetName
        : `${targetName}.MPQ`;

    return normalizeRelativePath(`Data/${fileName}`);
}

export function resolveLocalPackageOutputPath(
    packageKey: string,
    packageFileName: string,
    config: ResolvedSupplementaryPatchConfig
) {
    const override = config.localPackages[packageKey] || config.localPackages[packageFileName];
    if (override) {
        return normalizeRelativePath(override);
    }
    return normalizeRelativePath(`Data/${packageFileName}`);
}
