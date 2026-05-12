import * as fs from "fs";

import { Cell } from "../../../data/cell/cells/Cell";
import { EnumCon, makeEnum } from "../../../data/cell/cells/EnumCell";
import { finish } from "../../../data/index";
import { DBC } from "../../DBCFiles";
import { spell_bonus_dataRow } from "../../sql/spell_bonus_data";
import { SQL } from "../../SQLFiles";
import { MaybeSQLEntity } from "../Misc/SQLDBCEntity";
import { SpellEffect } from "./SpellEffect";

type BonusScalarKind = "AP" | "SP" | "BV";

export enum BonusDataScalingMode {
    BOTH = 0,
    HIGHEST = 1,
}

export type BonusDataScalingModeInput = EnumCon<keyof typeof BonusDataScalingMode>;

type BonusScalarCallSite = {
    filePath: string;
    line: number;
    column: number;
};

type ExtractedSpellRegistration = {
    moduleName: string | null;
    tagName: string | null;
    constName: string | null;
    scriptKind: "create" | "load";
    parentedId: number | null;
};

type BonusScalarRegistration = {
    spellId: number;
    effectIndex: number;
    scalarKind: BonusScalarKind;
    scalarValue: number;
    spellName: string;
    familyId: number;
    familyName: string;
    className: string;
    specName: string;
    effectType: number;
    auraType: number;
    auraPeriodMs: number;
    chainAmplitude: number;
    durationMs: number;
    castTimeMs: number;
    cooldownMs: number;
    isPeriodic: boolean;
    hasCastTime: boolean;
    hasCooldown: boolean;
    targetCount: number;
    isAoe: boolean;
    powerCostPct: number;
    scalingMode: number;
    filePath: string;
    line: number;
    column: number;
    moduleName: string;
    tagName: string;
    constName: string;
    scriptKind: string;
    parentedId: number | null;
    sourceExpr: string;
    exportable: boolean;
    sourceKind: "setter" | "inferred";
};

type BonusScalarTiming = {
    effectType: number;
    auraType: number;
    auraPeriodMs: number;
    chainAmplitude: number;
    durationMs: number;
    castTimeMs: number;
    cooldownMs: number;
    isPeriodic: boolean;
    hasCastTime: boolean;
    hasCooldown: boolean;
    targetCount: number;
    isAoe: boolean;
    powerCostPct: number;
};

type TriggerParentEdge = {
    parentId: number;
    isChannelTrigger: boolean;
};

type TriggerParentInfo = {
    parentId: number | null;
    isChannelTrigger: boolean;
};

type InferredBonusScalarSource = {
    filePath: string;
    line: number;
    column: number;
    moduleName: string;
    tagName: string;
    constName: string;
    scriptKind: string;
    parentedId: number | null;
};

let TRIGGER_PARENT_INDEX: Map<number, TriggerParentEdge> | null = null;
let INTERNAL_SPELL_TAGS: Map<number, string> | null = null;
const DATASCRIPT_TAG_FILE_CACHE = new Map<string, string | null>();
const INFERRED_BONUS_SOURCE_CACHE = new Map<string, InferredBonusScalarSource | null>();

const SPELL_FAMILY_NAMES: Record<number, string> = {
    0: "GENERIC",
    2: "CRAFTING",
    3: "MAGE",
    4: "WARRIOR",
    5: "WARLOCK",
    6: "PRIEST",
    7: "DRUID",
    8: "ROGUE",
    9: "HUNTER",
    10: "PALADIN",
    11: "SHAMAN",
    13: "POTION",
    15: "DEATHKNIGHT",
    17: "PET",
    19: "TINKER",
    22: "STATS",
};

const powersBonusScalarRegistrations = new Map<string, BonusScalarRegistration>();

SQL.Databases.world_dest.writeEarly(`
DROP TABLE IF EXISTS \`powers_bonus_data_registry\`;
CREATE TABLE \`powers_bonus_data_registry\` (
    id INT NOT NULL AUTO_INCREMENT,
    spell_id INT NOT NULL,
    effect_index INT NOT NULL,
    scalar_kind VARCHAR(8) NOT NULL,
    scalar_value FLOAT NOT NULL DEFAULT 0,
    spell_name VARCHAR(255) NOT NULL DEFAULT '',
    family_id INT NOT NULL DEFAULT 0,
    family_name VARCHAR(40) NOT NULL DEFAULT 'GENERIC',
    class_name VARCHAR(80) NOT NULL DEFAULT 'Generic',
    spec_name VARCHAR(80) NOT NULL DEFAULT 'Generic',
    effect_type INT NOT NULL DEFAULT 0,
    aura_type INT NOT NULL DEFAULT 0,
    aura_period_ms INT NOT NULL DEFAULT 0,
    chain_amplitude FLOAT NOT NULL DEFAULT 1,
    duration_ms INT NOT NULL DEFAULT 0,
    cast_time_ms INT NOT NULL DEFAULT 0,
    cooldown_ms INT NOT NULL DEFAULT 0,
    is_periodic TINYINT(1) NOT NULL DEFAULT 0,
    has_cast_time TINYINT(1) NOT NULL DEFAULT 0,
    has_cooldown TINYINT(1) NOT NULL DEFAULT 0,
    target_count INT NOT NULL DEFAULT 1,
    is_aoe TINYINT(1) NOT NULL DEFAULT 0,
    power_cost_pct INT NOT NULL DEFAULT 0,
    scaling_mode INT NOT NULL DEFAULT 0,
    module_name VARCHAR(100) NOT NULL DEFAULT '',
    file_path VARCHAR(500) NOT NULL DEFAULT '',
    line_number INT NOT NULL DEFAULT 0,
    column_number INT NOT NULL DEFAULT 0,
    script_kind VARCHAR(20) NOT NULL DEFAULT '',
    tag_name VARCHAR(100) NOT NULL DEFAULT '',
    parented_id INT NULL,
    const_name VARCHAR(100) NOT NULL DEFAULT '',
    source_expr TEXT,
    exportable TINYINT(1) NOT NULL DEFAULT 0,
    source_kind VARCHAR(20) NOT NULL DEFAULT 'database',
    registered_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY \`unique_powers_bonus_scalar\` (\`spell_id\`, \`effect_index\`, \`scalar_kind\`),
    KEY \`idx_powers_bonus_family\` (\`family_id\`, \`spec_name\`),
    KEY \`idx_powers_bonus_file\` (\`file_path\`)
) ENGINE=INNODB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
`);

function escapeSql(str: string): string {
    return str.replace(/'/g, "''").replace(/\\/g, "\\\\");
}

function registryKey(spellId: number, effectIndex: number, scalarKind: BonusScalarKind): string {
    return `${spellId}:${effectIndex}:${scalarKind}`;
}

function familyName(familyId: number): string {
    return SPELL_FAMILY_NAMES[familyId] ?? `FAMILY_${familyId}`;
}

function resolveBonusDataScalingMode(scalingMode: BonusDataScalingModeInput): BonusDataScalingMode {
    return makeEnum(BonusDataScalingMode, scalingMode) as BonusDataScalingMode;
}

function titleCase(value: string): string {
    const trimmed = value.trim();
    if (!trimmed) return "Generic";
    return trimmed.charAt(0).toUpperCase() + trimmed.slice(1);
}

function inferClassSpec(filePath: string, familyId: number): { className: string; specName: string } {
    const normalized = filePath.split("\\").join("/");
    const parts = normalized.split("/").filter(part => part.length > 0);
    const classesIndex = parts.findIndex(part => part.toLowerCase() === "classes");
    let className = familyName(familyId).replace(/_/g, "").toLowerCase();
    className = className === "generic" || className.startsWith("family") ? "Generic" : titleCase(className);

    if (classesIndex !== -1 && parts.length > classesIndex + 1) {
        className = titleCase(parts[classesIndex + 1]);
    }

    let specName = "Generic";
    if (classesIndex !== -1 && parts.length > classesIndex + 2) {
        const candidate = parts[classesIndex + 2].replace(/\.ts$/i, "");
        const lower = candidate.toLowerCase();
        if (
            lower !== "base-spells" &&
            lower !== "generalextra" &&
            lower !== "extra" &&
            lower !== "datascripts" &&
            !lower.includes("extra") &&
            !lower.includes("base")
        ) {
            specName = titleCase(candidate);
        }
    }

    const fileName = (parts[parts.length - 1] ?? "").replace(/\.ts$/i, "").toLowerCase();
    if (fileName === "base-spells" || fileName.includes("extra")) {
        specName = "Generic";
    }

    return { className, specName };
}

function isPeriodicAura(auraType: number): boolean {
    return auraType === 3 || auraType === 8 || auraType === 53 || auraType === 62 || auraType === 89;
}

function isAreaImplicitTarget(target: number): boolean {
    return [
        2, 3, 4, 7, 8, 15, 16, 20, 24, 30, 31, 33, 34, 37, 45, 51, 52, 54, 56, 58, 59, 61, 93, 104, 107, 109, 110, 111,
    ].includes(target);
}

function getTriggerParentInfo(spellId: number): TriggerParentInfo {
    if (TRIGGER_PARENT_INDEX === null) {
        TRIGGER_PARENT_INDEX = new Map<number, TriggerParentEdge>();
        for (const parent of DBC.Spell.queryAll({} as any)) {
            const parentId = parent.ID.get();
            for (let effectIndex = 0; effectIndex < 3; effectIndex++) {
                const childId = parent.EffectTriggerSpell.getIndex(effectIndex) ?? 0;
                if (childId !== 0 && !TRIGGER_PARENT_INDEX.has(childId)) {
                    const delayMs = parent.EffectMiscValue.getIndex(effectIndex) ?? 0;
                    TRIGGER_PARENT_INDEX.set(childId, {
                        parentId,
                        isChannelTrigger: delayMs > 0,
                    });
                }
            }
        }
    }

    const directEdge = TRIGGER_PARENT_INDEX.get(spellId) ?? null;
    let parentId = directEdge?.parentId ?? null;
    let isChannelTrigger = directEdge?.isChannelTrigger ?? false;
    const seen = new Set<number>([spellId]);
    while (parentId !== null && !seen.has(parentId)) {
        seen.add(parentId);
        const nextEdge = TRIGGER_PARENT_INDEX.get(parentId) ?? null;
        if (nextEdge === null) {
            break;
        }
        isChannelTrigger = isChannelTrigger || nextEdge.isChannelTrigger;
        parentId = nextEdge.parentId;
    }
    return { parentId, isChannelTrigger };
}

function getSpellTiming(spellId: number, effectIndex: number): BonusScalarTiming {
    const row = DBC.Spell.findById(spellId);
    if (!row) {
        return {
            effectType: 0,
            auraType: 0,
            auraPeriodMs: 0,
            chainAmplitude: 1,
            durationMs: 0,
            castTimeMs: 0,
            cooldownMs: 0,
            isPeriodic: false,
            hasCastTime: false,
            hasCooldown: false,
            targetCount: 1,
            isAoe: false,
            powerCostPct: 0,
        };
    }

    const parentInfo = getTriggerParentInfo(spellId);
    const parentRow = DBC.Spell.findById(parentInfo.parentId ?? 0);
    const parentDurationRow = parentRow ? DBC.SpellDuration.findById(parentRow.DurationIndex.get()) : undefined;
    const childDurationRow = DBC.SpellDuration.findById(row.DurationIndex.get());
    const parentCastRow = parentRow ? DBC.SpellCastTimes.findById(parentRow.CastingTimeIndex.get()) : undefined;
    const childCastRow = DBC.SpellCastTimes.findById(row.CastingTimeIndex.get());
    const childDurationMs = Math.max(
        0,
        childDurationRow?.Duration.get() ?? 0,
        childDurationRow?.MaxDuration.get() ?? 0,
    );
    const childCastTimeMs = Math.max(0, childCastRow?.Base.get() ?? 0, childCastRow?.Minimum.get() ?? 0);
    const childIsInstant = childDurationMs === 0 && childCastTimeMs === 0;
    const parentDurationMs = childIsInstant && parentInfo.isChannelTrigger ? Math.max(
        0,
        parentDurationRow?.Duration.get() ?? 0,
        parentDurationRow?.MaxDuration.get() ?? 0,
    ) : 0;
    const parentCastTimeMs = childIsInstant && parentInfo.isChannelTrigger ? Math.max(0, parentCastRow?.Base.get() ?? 0, parentCastRow?.Minimum.get() ?? 0) : 0;
    const parentCooldownMs = childIsInstant && parentInfo.isChannelTrigger && parentRow ? Math.max(0, parentRow.RecoveryTime.get(), parentRow.CategoryRecoveryTime.get()) : 0;
    const childCooldownMs = Math.max(0, row.RecoveryTime.get(), row.CategoryRecoveryTime.get());
    const durationMs = parentDurationMs || childDurationMs;
    const castTimeMs = parentCastTimeMs || childCastTimeMs;
    const cooldownMs = parentCooldownMs || childCooldownMs;
    const auraType = row.EffectAura.getIndex(effectIndex) ?? 0;
    const chainAmplitude = row.EffectChainAmplitude.getIndex(effectIndex) ?? 1;
    const targetCount = Math.max(1, row.MaxTargets.get() || 1);
    const implicitTargetA = row.ImplicitTargetA.getIndex(effectIndex) ?? 0;
    const implicitTargetB = row.ImplicitTargetB.getIndex(effectIndex) ?? 0;

    return {
        effectType: row.Effect.getIndex(effectIndex) ?? 0,
        auraType,
        auraPeriodMs: row.EffectAuraPeriod.getIndex(effectIndex) ?? 0,
        chainAmplitude,
        durationMs,
        castTimeMs,
        cooldownMs,
        isPeriodic: isPeriodicAura(auraType),
        hasCastTime: castTimeMs > 0,
        hasCooldown: cooldownMs > 0,
        targetCount,
        isAoe: targetCount > 1 || chainAmplitude > 1 || isAreaImplicitTarget(implicitTargetA) || isAreaImplicitTarget(implicitTargetB),
        powerCostPct: (childIsInstant && parentInfo.isChannelTrigger ? parentRow?.ManaCostPct.get() : 0) || row.ManaCostPct.get() || 0,
    };
}

function normalizeStackFilePath(raw: string): string {
    let filePath = raw.trim();

    if (filePath.startsWith("file:///")) {
        filePath = filePath.substring("file:///".length);
    } else if (filePath.startsWith("file://")) {
        filePath = filePath.substring("file://".length);
    }

    filePath = filePath.replace(/^[\(\s]+|[\)\s]+$/g, "");
    const windowsDriveMatch = filePath.match(/^([A-Za-z]):(.*)$/);
    if (windowsDriveMatch) {
        filePath = `${windowsDriveMatch[1].toUpperCase()}:${windowsDriveMatch[2].split("\\").join("/")}`;
    } else {
        filePath = filePath.split("\\").join("/");
    }

    if (filePath.endsWith(".js")) {
        const buildIndex = filePath.indexOf("/build/");
        if (buildIndex !== -1) {
            const beforeBuild = filePath.substring(0, buildIndex);
            const afterBuild = filePath.substring(buildIndex + "/build/".length);
            filePath = `${beforeBuild}/datascripts/${afterBuild}`.replace(/\.js$/, ".ts");
        } else {
            filePath = filePath.replace(/\.js$/, ".ts");
        }
    }

    return filePath;
}

function parseStackFrame(rawLine: string): BonusScalarCallSite | null {
    let match = rawLine.match(/\((.*):(\d+):(\d+)\)/);
    if (!match) match = rawLine.match(/^\s*at\s+(.*):(\d+):(\d+)\s*$/);
    if (!match) match = rawLine.match(/(.*\.(?:ts|js)):(\d+):(\d+)/);
    if (!match) return null;

    const line = parseInt(match[2], 10);
    const column = parseInt(match[3], 10);
    if (!Number.isFinite(line) || !Number.isFinite(column)) return null;

    let filePath = normalizeStackFilePath(match[1]);
    if (filePath.startsWith("async ")) filePath = filePath.substring("async ".length).trim();

    return { filePath, line, column };
}

function getBonusScalarCallSite(): BonusScalarCallSite | null {
    const stack = new Error().stack;
    if (!stack) return null;

    for (const rawLine of stack.split("\n")) {
        if (rawLine.includes("node:internal") || rawLine.includes("/internal/")) continue;
        if (rawLine.includes("SpellBonusData") || rawLine.includes("SQLDBCEntity")) continue;
        if (rawLine.includes("BonusDataScalarCell")) continue;

        const candidate = parseStackFrame(rawLine);
        if (!candidate) continue;
        if (fs.existsSync(candidate.filePath) || fs.existsSync(candidate.filePath.split("/").join("\\"))) {
            return candidate;
        }
    }

    return null;
}

function findActualPath(filePath: string): string | null {
    const variations = [filePath, filePath.split("\\").join("/"), filePath.split("/").join("\\")];
    for (const candidate of variations) {
        if (fs.existsSync(candidate) && fs.statSync(candidate).isFile()) {
            return candidate;
        }
    }
    return null;
}

function extractConstNameNear(lines: string[], startIndex: number): string | null {
    for (let i = startIndex; i >= Math.max(0, startIndex - 2); i--) {
        const match = lines[i].trim().match(/^(export\s+)?const\s+([a-zA-Z_$][a-zA-Z0-9_$]*)\s*=/);
        if (match) return match[2];
    }
    return null;
}

function extractSetExpression(filePath: string, line: number, scalarKind: BonusScalarKind): string {
    const actualPath = findActualPath(filePath);
    if (!actualPath) return "";

    const lines = fs.readFileSync(actualPath, "utf-8").split("\n");
    const start = Math.max(0, line - 1);
    const text = lines.slice(start, Math.min(lines.length, start + 4)).join("\n");
    const match = text.match(new RegExp(`BonusData\\.${scalarKind}Bonus\\.set\\s*\\(([\\s\\S]*?)\\)`));
    return match ? match[1].trim() : "";
}

function extractSpellRegistration(filePath: string, line: number): ExtractedSpellRegistration {
    const actualPath = findActualPath(filePath);
    if (!actualPath) {
        return { moduleName: null, tagName: null, constName: null, scriptKind: "create", parentedId: null };
    }

    const lines = fs.readFileSync(actualPath, "utf-8").split("\n");
    const startLine = Math.min(Math.max(0, line - 1), lines.length - 1);
    let callStart = -1;
    for (let i = startLine; i >= Math.max(0, startLine - 180); i--) {
        const lower = lines[i].toLowerCase();
        if (
            lower.includes("std.spells.create(") ||
            lower.includes("spells.create(") ||
            lower.includes("std.spells.load(") ||
            lower.includes("spells.load(")
        ) {
            callStart = i;
            break;
        }
    }

    if (callStart === -1) {
        return { moduleName: null, tagName: null, constName: null, scriptKind: "create", parentedId: null };
    }

    const constName = extractConstNameNear(lines, callStart);
    const callText = lines.slice(callStart, Math.min(lines.length, callStart + 8)).join(" ").replace(/\s+/g, " ");
    const isLoad = callText.toLowerCase().includes("spells.load");
    if (isLoad) {
        return { moduleName: null, tagName: null, constName, scriptKind: "load", parentedId: null };
    }

    const createMatch = callText.match(/\(\s*(['"`])((?:[^\\]|\\.)*?)\1\s*,\s*(['"`])((?:[^\\]|\\.)*?)\3(?:\s*,\s*(\d+))?/);
    if (!createMatch) {
        return { moduleName: null, tagName: null, constName, scriptKind: "create", parentedId: null };
    }

    return {
        moduleName: createMatch[2].replace(/\\(.)/g, "$1"),
        tagName: createMatch[4].replace(/\\(.)/g, "$1"),
        constName,
        scriptKind: "create",
        parentedId: createMatch[5] ? parseInt(createMatch[5], 10) : null,
    };
}

function getRelativeFilePath(filePath: string): string {
    const p = filePath.split("\\").join("/");
    const lower = p.toLowerCase();
    const releaseIdx = lower.indexOf("/release/");
    if (releaseIdx !== -1) return p.substring(releaseIdx + "/release/".length);
    if (lower.startsWith("release/")) return p.substring("release/".length);

    const datascriptsIdx = lower.indexOf("datascripts/");
    if (datascriptsIdx !== -1) return p.substring(datascriptsIdx);

    return p;
}

function candidateReleaseRoots(): string[] {
    const cwd = normalizeStackFilePath(process.cwd?.() ?? "");
    const candidates = [cwd, `${cwd}/release`];
    const roots: string[] = [];
    for (const candidate of candidates) {
        if (!candidate) continue;
        const normalized = candidate.replace(/\/$/, "");
        if (roots.includes(normalized)) continue;
        if (fs.existsSync(normalized) && fs.statSync(normalized).isDirectory()) {
            roots.push(normalized);
        }
    }
    return roots;
}

function loadInternalSpellTags(): Map<number, string> {
    if (INTERNAL_SPELL_TAGS !== null) return INTERNAL_SPELL_TAGS;

    INTERNAL_SPELL_TAGS = new Map<number, string>();
    for (const releaseRoot of candidateReleaseRoots()) {
        const internalIdsPath = `${releaseRoot}/modules/dh-core/addon/internal-ids.ts`;
        const actualPath = findActualPath(internalIdsPath);
        if (!actualPath) continue;

        const lines = fs.readFileSync(actualPath, "utf-8").split("\n");
        for (const line of lines) {
            const match = line.match(/^\s*(\d+)\s*:\s*(["'`])((?:[^\\]|\\.)*?)\2/);
            if (!match) continue;
            const spellId = parseInt(match[1], 10);
            if (!Number.isFinite(spellId)) continue;
            const tag = match[3].replace(/\\(.)/g, "$1").split(":").slice(1).join(":");
            if (tag) INTERNAL_SPELL_TAGS.set(spellId, tag);
        }
        if (INTERNAL_SPELL_TAGS.size > 0) break;
    }

    return INTERNAL_SPELL_TAGS;
}

function readInternalSpellTag(spellId: number): string | null {
    return loadInternalSpellTags().get(spellId) ?? null;
}

function findDatascriptFileContainingTag(tag: string): string | null {
    if (DATASCRIPT_TAG_FILE_CACHE.has(tag)) return DATASCRIPT_TAG_FILE_CACHE.get(tag) ?? null;

    const roots = candidateReleaseRoots().map(root => `${root}/modules`);
    const matches: string[] = [];
    const visit = (dir: string): void => {
        const actualDir = findActualPath(dir) ?? dir;
        if (!fs.existsSync(actualDir) || !fs.statSync(actualDir).isDirectory()) return;
        for (const entry of fs.readdirSync(actualDir, { withFileTypes: true })) {
            const child = `${actualDir}/${entry.name}`;
            const normalized = child.split("\\").join("/");
            if (entry.isDirectory()) {
                visit(child);
                continue;
            }
            if (!entry.isFile() || !entry.name.endsWith(".ts")) continue;
            if (normalized.includes("/addon/internal-ids.ts")) continue;
            if (!normalized.includes("/datascripts/")) continue;
            const contents = fs.readFileSync(child, "utf-8");
            if (contents.includes(tag)) matches.push(normalized);
        }
    };

    for (const root of roots) visit(root);
    const found = matches.find(path => path.includes("/datascripts/classes/")) ?? matches[0] ?? null;
    DATASCRIPT_TAG_FILE_CACHE.set(tag, found);
    return found;
}

function findSpellStartLine(lines: string[], spellId: number, tagName: string | null): number {
    for (let i = 0; i < lines.length; i++) {
        const window = lines.slice(i, Math.min(lines.length, i + 8)).join(" ");
        const lower = window.toLowerCase();
        if (tagName && lower.includes("spells.create(") && window.includes(tagName)) return i;
        if (lower.includes("spells.load(") && window.includes(`Spells.load(${spellId})`)) return i;
    }
    return -1;
}

function findSpellEndLine(lines: string[], spellStartLine: number): number {
    for (let i = spellStartLine + 1; i < lines.length; i++) {
        if (/^\s*(export\s+)?const\s+[a-zA-Z_$][a-zA-Z0-9_$]*\s*=/.test(lines[i])) {
            return i;
        }
    }
    return lines.length;
}

function findEffectStartLine(lines: string[], spellStartLine: number, spellEndLine: number, effectIndex: number): number | null {
    let addModIndex = 0;
    for (let i = spellStartLine; i < spellEndLine; i++) {
        if (lines[i].includes(`.Effects.mod(${effectIndex}`)) return i;
        if (lines[i].includes(".Effects.addMod(")) {
            if (addModIndex === effectIndex) return i;
            addModIndex += 1;
        }
    }
    return null;
}

function inferBonusScalarSource(spellId: number, effectIndex: number): InferredBonusScalarSource | null {
    const cacheKey = `${spellId}:${effectIndex}`;
    if (INFERRED_BONUS_SOURCE_CACHE.has(cacheKey)) return INFERRED_BONUS_SOURCE_CACHE.get(cacheKey) ?? null;

    const tagName = readInternalSpellTag(spellId);
    const filePath = tagName ? findDatascriptFileContainingTag(tagName) : null;
    if (!filePath) {
        INFERRED_BONUS_SOURCE_CACHE.set(cacheKey, null);
        return null;
    }

    const lines = fs.readFileSync(filePath, "utf-8").split("\n");
    const spellStartLine = findSpellStartLine(lines, spellId, tagName);
    if (spellStartLine === -1) {
        INFERRED_BONUS_SOURCE_CACHE.set(cacheKey, null);
        return null;
    }

    const spellEndLine = findSpellEndLine(lines, spellStartLine);
    const effectStartLine = findEffectStartLine(lines, spellStartLine, spellEndLine, effectIndex);
    const sourceLine = effectStartLine ?? spellStartLine;
    const registration = extractSpellRegistration(filePath, sourceLine + 1);
    const source: InferredBonusScalarSource = {
        filePath,
        line: sourceLine + 1,
        column: 1,
        moduleName: registration.moduleName ?? "",
        tagName: registration.tagName ?? tagName ?? "",
        constName: registration.constName ?? "",
        scriptKind: registration.scriptKind ?? "",
        parentedId: registration.parentedId,
    };
    INFERRED_BONUS_SOURCE_CACHE.set(cacheKey, source);
    return source;
}
function registerBonusScalar(owner: SpellEffect, scalarKind: BonusScalarKind, scalarValue: number, scalingMode?: number): void {
    const spellId = owner.row.ID.get();
    const effectIndex = owner.index;
    const callSite = getBonusScalarCallSite();
    const filePath = callSite?.filePath ?? "";
    const familyId = owner.row.SpellClassSet.get();
    const extracted = callSite ? extractSpellRegistration(callSite.filePath, callSite.line) : null;
    const relativeFilePath = filePath ? getRelativeFilePath(filePath) : "";
    const inferred = inferClassSpec(relativeFilePath || filePath, familyId);
    const timing = getSpellTiming(spellId, effectIndex);

    powersBonusScalarRegistrations.set(registryKey(spellId, effectIndex, scalarKind), {
        spellId,
        effectIndex,
        scalarKind,
        scalarValue,
        spellName: owner.row.Name.enGB.get(),
        familyId,
        familyName: familyName(familyId),
        className: inferred.className,
        specName: inferred.specName,
        ...timing,
        scalingMode: scalingMode ?? 0,
        filePath: relativeFilePath,
        line: callSite?.line ?? 0,
        column: callSite?.column ?? 0,
        moduleName: extracted?.moduleName ?? "",
        tagName: extracted?.tagName ?? "",
        constName: extracted?.constName ?? "",
        scriptKind: extracted?.scriptKind ?? "",
        parentedId: extracted?.parentedId ?? null,
        sourceExpr: callSite ? extractSetExpression(callSite.filePath, callSite.line, scalarKind) : "",
        exportable: callSite !== null,
        sourceKind: "setter",
    });
}

function spellRowMetadata(spellId: number): { spellName: string; familyId: number; familyName: string } {
    const row = DBC.Spell.findById(spellId);
    if (!row) {
        return { spellName: "", familyId: 0, familyName: "GENERIC" };
    }

    const familyId = row.SpellClassSet.get();
    return {
        spellName: row.Name.enGB.get(),
        familyId,
        familyName: familyName(familyId),
    };
}

function makeDatabaseRegistration(row: spell_bonus_dataRow, scalarKind: BonusScalarKind, scalarValue: number): BonusScalarRegistration {
    const spellId = row.entry.get();
    const effectIndex = row.effect.get();
    const metadata = spellRowMetadata(spellId);
    const source = inferBonusScalarSource(spellId, effectIndex);
    const sourceFilePath = source ? getRelativeFilePath(source.filePath) : "";
    const inferred = inferClassSpec(sourceFilePath, metadata.familyId);
    const timing = getSpellTiming(spellId, effectIndex);

    return {
        spellId,
        effectIndex,
        scalarKind,
        scalarValue,
        spellName: metadata.spellName || row.comments.get(),
        familyId: metadata.familyId,
        familyName: metadata.familyName,
        className: inferred.className,
        specName: inferred.specName,
        ...timing,
        scalingMode: row.scaling_mode.get(),
        filePath: sourceFilePath,
        line: source?.line ?? 0,
        column: source?.column ?? 0,
        moduleName: source?.moduleName ?? "",
        tagName: source?.tagName ?? "",
        constName: source?.constName ?? "",
        scriptKind: source?.scriptKind ?? "",
        parentedId: source?.parentedId ?? null,
        sourceExpr: source ? "missing BonusData setter" : "",
        exportable: false,
        sourceKind: "inferred",
    };
}
function writeBonusScalarRegistration(registry: BonusScalarRegistration): void {
    const parentedId = registry.parentedId !== null ? registry.parentedId : "NULL";
    SQL.Databases.world_dest.write(`
        INSERT INTO \`powers_bonus_data_registry\` (
            spell_id, effect_index, scalar_kind, scalar_value, spell_name,
            family_id, family_name, class_name, spec_name,
            effect_type, aura_type, aura_period_ms, chain_amplitude,
            duration_ms, cast_time_ms, cooldown_ms,
            is_periodic, has_cast_time, has_cooldown, target_count,
            is_aoe, power_cost_pct, scaling_mode,
            module_name, file_path, line_number, column_number,
            script_kind, tag_name, parented_id, const_name, source_expr,
            exportable, source_kind
        ) VALUES (
            ${registry.spellId},
            ${registry.effectIndex},
            '${escapeSql(registry.scalarKind)}',
            ${Number.isFinite(registry.scalarValue) ? registry.scalarValue : 0},
            '${escapeSql(registry.spellName)}',
            ${registry.familyId},
            '${escapeSql(registry.familyName)}',
            '${escapeSql(registry.className)}',
            '${escapeSql(registry.specName)}',
            ${registry.effectType},
            ${registry.auraType},
            ${registry.auraPeriodMs},
            ${Number.isFinite(registry.chainAmplitude) ? registry.chainAmplitude : 1},
            ${registry.durationMs},
            ${registry.castTimeMs},
            ${registry.cooldownMs},
            ${registry.isPeriodic ? 1 : 0},
            ${registry.hasCastTime ? 1 : 0},
            ${registry.hasCooldown ? 1 : 0},
            ${registry.targetCount},
            ${registry.isAoe ? 1 : 0},
            ${registry.powerCostPct},
            ${Number.isFinite(registry.scalingMode) ? registry.scalingMode : 0},
            '${escapeSql(registry.moduleName)}',
            '${escapeSql(registry.filePath)}',
            ${registry.line},
            ${registry.column},
            '${escapeSql(registry.scriptKind)}',
            '${escapeSql(registry.tagName)}',
            ${parentedId},
            '${escapeSql(registry.constName)}',
            '${escapeSql(registry.sourceExpr)}',
            ${registry.exportable ? 1 : 0},
            '${escapeSql(registry.sourceKind)}'
        )
        ON DUPLICATE KEY UPDATE
            scalar_value = VALUES(scalar_value),
            spell_name = VALUES(spell_name),
            family_id = VALUES(family_id),
            family_name = VALUES(family_name),
            class_name = VALUES(class_name),
            spec_name = VALUES(spec_name),
            effect_type = VALUES(effect_type),
            aura_type = VALUES(aura_type),
            aura_period_ms = VALUES(aura_period_ms),
            chain_amplitude = VALUES(chain_amplitude),
            duration_ms = VALUES(duration_ms),
            cast_time_ms = VALUES(cast_time_ms),
            cooldown_ms = VALUES(cooldown_ms),
            is_periodic = VALUES(is_periodic),
            has_cast_time = VALUES(has_cast_time),
            has_cooldown = VALUES(has_cooldown),
            target_count = VALUES(target_count),
            is_aoe = VALUES(is_aoe),
            power_cost_pct = VALUES(power_cost_pct),
            scaling_mode = VALUES(scaling_mode),
            module_name = VALUES(module_name),
            file_path = VALUES(file_path),
            line_number = VALUES(line_number),
            column_number = VALUES(column_number),
            script_kind = VALUES(script_kind),
            tag_name = VALUES(tag_name),
            parented_id = VALUES(parented_id),
            const_name = VALUES(const_name),
            source_expr = VALUES(source_expr),
            exportable = VALUES(exportable),
            source_kind = VALUES(source_kind),
            registered_at = CURRENT_TIMESTAMP
    `);
}

class BonusDataScalarCell extends Cell<number, SpellEffect> {
    constructor(
        owner: SpellEffect,
        private readonly bonusData: SpellBonusData,
        private readonly scalarKind: BonusScalarKind,
        private readonly getter: (sql: spell_bonus_dataRow) => Cell<number, any>,
    ) {
        super(owner);
    }

    exists() {
        return this.bonusData.exists();
    }

    get(): number {
        const sql = this.bonusData.getSQL();
        return sql ? this.getter(sql).get() : 0;
    }

    set(value: number): SpellEffect {
        const sql = MaybeSQLEntity.getOrCreateSQL(this.bonusData);
        this.getter(sql).set(value);
        sql.scaling_mode.set(BonusDataScalingMode.BOTH);
        registerBonusScalar(this.owner, this.scalarKind, value, BonusDataScalingMode.BOTH);
        return this.owner;
    }
}

finish("powers-register-bonus-data-scalars", () => {
    const rows = SQL.spell_bonus_data.queryAll({});
    for (const row of rows) {
        const values: Array<[BonusScalarKind, number]> = [
            ["AP", row.ap.get()],
            ["SP", row.sp.get()],
            ["BV", row.bv.get()],
        ];

        for (const [scalarKind, scalarValue] of values) {
            if (!Number.isFinite(scalarValue) || scalarValue === 0) continue;
            const key = registryKey(row.entry.get(), row.effect.get(), scalarKind);
            writeBonusScalarRegistration(
                powersBonusScalarRegistrations.get(key) ??
                makeDatabaseRegistration(row, scalarKind, scalarValue)
            );
        }
    }

    powersBonusScalarRegistrations.clear();
});

export class SpellBonusData extends MaybeSQLEntity<SpellEffect,spell_bonus_dataRow> {
    protected createSQL(): spell_bonus_dataRow {
        return SQL.spell_bonus_data.add(this.owner.row.ID.get(), this.owner.index, {})
            .effect.set(this.owner.index)
            .ap.set(0)
            .sp.set(0)
            .bv.set(0)
            .scaling_mode.set(BonusDataScalingMode.BOTH)
            .comments.set(`${this.owner.row.Name.enGB.get()}`)
    }
    protected findSQL(): spell_bonus_dataRow {
        return SQL.spell_bonus_data.query({entry: this.owner.row.ID.get(), effect: this.owner.index});
    }
    protected isValidSQL(sql: spell_bonus_dataRow): boolean {
        return sql.entry.get() === this.owner.row.ID.get() && sql.effect.get() === this.owner.index;
    }

    get SPBonus() { return new BonusDataScalarCell(this.owner, this, "SP", sql => sql.sp); }
    get APBonus() { return new BonusDataScalarCell(this.owner, this, "AP", sql => sql.ap); }
    get BVBonus() { return new BonusDataScalarCell(this.owner, this, "BV", sql => sql.bv); }

    set(ap: number, sp: number, bv: number, scalingMode: BonusDataScalingModeInput): SpellEffect {
        const resolvedMode = resolveBonusDataScalingMode(scalingMode);
        const sql = MaybeSQLEntity.getOrCreateSQL(this);
        sql.ap.set(ap)
            .sp.set(sp)
            .bv.set(bv)
            .scaling_mode.set(resolvedMode);

        registerBonusScalar(this.owner, "AP", ap, resolvedMode);
        registerBonusScalar(this.owner, "SP", sp, resolvedMode);
        registerBonusScalar(this.owner, "BV", bv, resolvedMode);
        return this.owner;
    }
}
