/*
* This file is part of tswow (https://github.com/tswow)
*
* Copyright (C) 2020 tswow <https://github.com/tswow/>
* This program is free software: you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation, version 3.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <https://www.gnu.org/licenses/>.
*/
import * as fs from "fs";
import * as path from "path";
import { makeEnumCell } from "../../../data/cell/cells/EnumCell";
import { makeMaskCell32 } from "../../../data/cell/cells/MaskCell";
import { MulticastCell } from "../../../data/cell/cells/MulticastCell";
import { Transient } from "../../../data/cell/serialization/Transient";
import { CellSystem } from "../../../data/cell/systems/CellSystem";
import { finish } from "../../../data/index";
import { Table } from "../../../data/table/Table";
import { ItemRow } from "../../dbc/Item";
import { DBC } from "../../DBCFiles";
import { item_templateQuery, item_templateRow } from "../../sql/item_template";
import { SQL } from "../../SQLFiles";
import { ClassMask } from "../Class/ClassRegistry";
import { EnchantmentRegistry } from "../Enchant/Enchantment";
import { HolidayRegistry } from "../GameEvent/Holiday";
import { CellBasic } from "../GameObject/ElevatorKeyframes";
import { GemRegistry } from "../Gem/Gem";
import { getInlineID } from "../InlineScript/InlineScript";
import { LockRegistry } from "../Locks/Locks";
import { Loot, LootSet, LootSetRef } from "../Loot/Loot";
import { CodegenSettings, GenerateCode } from "../Misc/Codegen";
import { DurationCell } from "../Misc/DurationCell";
import { MainEntityID } from "../Misc/Entity";
import { Ids, StaticIDGenerator } from "../Misc/Ids";
import { MaybeDBCEntity } from "../Misc/SQLDBCEntity";
import { PageTextRegistry } from "../PageText/PageText";
import { RaceMask } from "../Race/RaceType";
import { RegistryStatic } from "../Refs/Registry";
import { TotemCategoryRegistry } from "../TotemCategory/TotemCategory";
import { BagFamily } from "./BagFamily";
import { ItemAmmoType } from "./ItemAmmoTypes";
import { ItemBonding } from "./ItemBonding";
import { ItemClass } from "./ItemClass";
import { ItemDamages } from "./ItemDamage";
import { ItemDisplayinfoRegistry } from "./ItemDisplayInfo";
import { ItemFlags } from "./ItemFlags";
import { ItemFlagsCustom } from "./ItemFlagsCustom";
import { ItemFlagsExtra } from "./ItemFlagsExtra";
import { ItemFoodType } from "./ItemFoodType";
import { ItemInventoryType } from "./ItemInventoryType";
import { ItemMaterial } from "./ItemMaterial";
import { ItemMoneyLoot } from "./ItemMoneyLoot";
import { ItemPrice } from "./ItemPrice";
import { ItemQuality } from "./ItemQuality";
import { ItemRequiredFaction } from "./ItemRequiredFaction";
import { ItemRequirements } from "./ItemRequirements";
import { ItemResistance } from "./ItemResistances";
import { ItemScalingStat } from "./ItemScalingStat";
import { ItemSetRegistry } from "./ItemSet";
import { ItemSetName, ItemSetNameRow } from "./ItemSetName";
import { ItemSheath } from "./ItemSheath";
import { ItemSockets } from "./ItemSocket";
import { ItemSpells } from "./ItemSpells";
import { ItemStats } from "./ItemStats";
import { ItemDescription, ItemName } from "./ItemText";
import { PageMaterialCell } from "./PageMaterial";
import { Stat } from "./ItemStats";

/**
 * Loothaven Item Registry (embedded)
 *
 * This is intentionally embedded in the base ItemTemplate implementation so:
 * - `.Loothaven.set(true)` is always available without cross-module imports
 * - runtime and types come from the same place
 */

type LoothavenRegistrationOptions = {
    moduleName?: string;
    filePath?: string;
    /** Item tag (second argument of std.Items.create). No const required. */
    tagName?: string;
    /** Variable name holding the item (e.g. from `const beltofarugal = std.Items.create(...)`). */
    constName?: string;
    /** Parent/display ID from create call when present (third argument of std.Items.create(mod, tag, parentedId)). */
    parentedId?: number | null;
    metadata?: any;
};

// Single source of truth: loothaven_registry is created only here (world_dest).
// We DROP then CREATE so the schema always matches; the table is repopulated by the finish("loothaven-register-items") callback in this file.
console.log("Creating loothaven_registry table.");
SQL.Databases.world_dest.writeEarly(`
DROP TABLE IF EXISTS \`loothaven_registry\`;
CREATE TABLE \`loothaven_registry\` (
    id INT NOT NULL AUTO_INCREMENT,
    item_entry INT(10) UNSIGNED NOT NULL,
    item_name VARCHAR(255) NOT NULL,
    item_level INT NOT NULL DEFAULT 0,
    item_class INT NOT NULL DEFAULT 0,
    item_subclass INT NOT NULL DEFAULT 0,
    item_quality VARCHAR(20) NOT NULL DEFAULT 'WHITE',
    inventory_type VARCHAR(50) NOT NULL DEFAULT 'NON_EQUIP',
    required_level INT NOT NULL DEFAULT 0,
    module_name VARCHAR(100) NOT NULL DEFAULT '',
    file_path VARCHAR(500) NOT NULL DEFAULT '',
    tag_name VARCHAR(100) NOT NULL DEFAULT '',
    parented_id INT NULL,
    const_name VARCHAR(100) NOT NULL DEFAULT '',
    registered_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    metadata TEXT,
    PRIMARY KEY (id),
    UNIQUE KEY \`unique_item_entry\` (\`item_entry\`),
    KEY \`idx_item_name\` (\`item_name\`),
    KEY \`idx_module\` (\`module_name\`),
    KEY \`idx_file_path\` (\`file_path\`)
) ENGINE=INNODB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
`);

// Track items that need to be registered
const loothavenItemsToRegister: Array<{ item: ItemTemplate; options: LoothavenRegistrationOptions }> = [];
const loothavenRegistered = new WeakSet<object>();

function escapeSql(str: string): string
{
    return str.replace(/'/g, "''").replace(/\\/g, "\\\\");
}

// Backwards-compatible explicit registration (for legacy `registerItem()` wrappers).
export function registerLoothavenItemExplicit(item: ItemTemplate, options: LoothavenRegistrationOptions = {}): void
{
    loothavenItemsToRegister.push({ item, options });
}

function registerLoothavenItemAuto(item: ItemTemplate, metadata?: any): void
{
    const callSite = getLoothavenCallSiteInfo();
    if (!callSite)
    {
        console.error("Loothaven: could not determine call site from stack trace");
        return;
    }

    const extracted = extractItemTag(callSite.filePath, callSite.line);
    if (!extracted)
    {
        console.error(`Loothaven: could not extract item tag from ${callSite.filePath}:${callSite.line}`);
        console.error(`Loothaven: stack trace was: ${new Error().stack}`);
        return;
    }

    const moduleName = extractModuleName(callSite.filePath);
    const relativeFilePath = getRelativeFilePath(callSite.filePath);
    
    console.log(`Loothaven: registering item with tag "${extracted.tag}", filePath: "${relativeFilePath}"`);

    loothavenItemsToRegister.push({
        item,
        options: {
            moduleName,
            filePath: relativeFilePath,
            tagName: extracted.tag,
            constName: extracted.constName ?? undefined,
            parentedId: extracted.parentedId ?? undefined,
            metadata: metadata && Object.keys(metadata).length > 0 ? metadata : undefined,
        },
    });
}

function normalizeStackFilePath(raw: string): string
{
    let filePath = raw.trim();
    
    // Remove file:/// or file:// prefix
    if (filePath.startsWith("file:///"))
        filePath = filePath.substring("file:///".length);
    else if (filePath.startsWith("file://"))
        filePath = filePath.substring("file://".length);
    
    // Remove any leading/trailing parentheses or whitespace
    filePath = filePath.replace(/^[\(\s]+|[\)\s]+$/g, "");
    
    // Normalize separators (but preserve Windows drive letters)
    // Handle Windows paths like "C:/path" or "C:\path" or "C:/.path"
    const windowsDriveMatch = filePath.match(/^([A-Za-z]):(.*)$/);
    if (windowsDriveMatch)
    {
        const drive = windowsDriveMatch[1].toUpperCase();
        const rest = windowsDriveMatch[2];
        // Normalize separators in the rest of the path
        filePath = drive + ":" + rest.split("\\").join("/");
    }
    else
    {
        filePath = filePath.split("\\").join("/");
    }
    
    return filePath;
}

function getLoothavenCallSiteInfo(): { filePath: string; line: number; column: number } | null
{
    const stack = new Error().stack;
    if (!stack)
        return null;

    const lines = stack.split("\n");
    for (const rawLine of lines)
    {
        // Skip internal frames and this file
        if (rawLine.includes("node:internal") || rawLine.includes("/internal/"))
            continue;
        if (rawLine.includes("std/Item/ItemTemplate") || rawLine.includes("ItemTemplate.ts") || rawLine.includes("ItemTemplate.js"))
            continue;
        if (rawLine.includes("createLoothavenRegistrar") || rawLine.includes("registerLoothavenItemAuto"))
            continue;

        // Try to match stack trace formats:
        // "    at Object.<anonymous> (C:/path/file.ts:123:45)"
        // "    at C:/path/file.ts:123:45"
        // "    at file:///C:/path/file.ts:123:45"
        let match = rawLine.match(/\(([^)]+):(\d+):(\d+)\)/);
        if (!match)
            match = rawLine.match(/([^:\s]+\.(ts|js)):(\d+):(\d+)/);
        
        if (!match)
            continue;

        let filePath = match[1];
        const line = parseInt(match[2] || match[3]);
        const column = parseInt(match[3] || match[4]);

        filePath = normalizeStackFilePath(filePath);

        // If we got a built .js path, map it back to the .ts file (best-effort)
        if (filePath.endsWith(".js"))
        {
            const normalized = filePath.replace(/\\/g, "/");
            
            // Try to map build/ path back to datascripts/ path
            // e.g., .../build/Instances/sfk/cloth.js -> .../datascripts/Instances/sfk/cloth.ts
            const buildIndex = normalized.indexOf("/build/");
            if (buildIndex !== -1)
            {
                // Extract everything before /build/ and everything after /build/
                const beforeBuild = normalized.substring(0, buildIndex);
                const afterBuild = normalized.substring(buildIndex + "/build/".length);
                
                // Replace /build/ with /datascripts/ and change .js to .ts
                filePath = (beforeBuild + "/datascripts/" + afterBuild).replace(/\.js$/, ".ts");
                filePath = normalizeStackFilePath(filePath);
            }
            else
            {
                // Fallback: just change .js to .ts
                filePath = filePath.replace(/\.js$/, ".ts");
            }
        }

        // Verify the file exists before returning
        if (fs.existsSync(filePath) || fs.existsSync(filePath.split("\\").join("/")))
        {
            return { filePath, line, column };
        }
    }

    return null;
}

function extractItemTag(filePath: string, line: number): { tag: string; constName: string | null; parentedId: number | null } | null
{
    try
    {
        // Try multiple path variations
        let actualPath: string | null = null;
        const pathVariations = [
            filePath,
            filePath.split("\\").join("/"),
            filePath.split("/").join("\\"),
        ];
        
        for (const testPath of pathVariations)
        {
            if (fs.existsSync(testPath) && fs.statSync(testPath).isFile())
            {
                actualPath = testPath;
                break;
            }
        }
        
        if (!actualPath)
        {
            console.error(`Loothaven: file not found: ${filePath} (tried variations: ${pathVariations.join(", ")})`);
            return null;
        }

        const sourceCode = fs.readFileSync(actualPath, "utf-8");
        const lines = sourceCode.split("\n");

        // Search backwards from the call site line to find the const declaration
        const startLine = Math.min(line - 1, lines.length - 1);
        const searchRange = 100;
        let constLineIndex = -1;
        let extractedConstName: string | null = null;

        for (let i = startLine; i >= Math.max(0, startLine - searchRange); i--)
        {
            const lineText = lines[i].trim();
            
            // Look for "export const VARIABLENAME =" or "const VARIABLENAME ="
            const exportMatch = lineText.match(/^(export\s+)?const\s+([a-zA-Z_$][a-zA-Z0-9_$]*)\s*=/);
            if (exportMatch)
            {
                constLineIndex = i;
                extractedConstName = exportMatch[2];
                break;
            }
        }

        // If no const declaration, search backwards for std.Items.create( (unchained pattern)
        let searchStartLine = constLineIndex;
        if (searchStartLine === -1)
        {
            for (let i = startLine; i >= Math.max(0, startLine - searchRange); i--)
            {
                const lineText = lines[i];
                const lineLower = lineText.toLowerCase();
                if (lineLower.includes("std.items.create(") || lineLower.includes("items.create("))
                {
                    searchStartLine = i;
                    break;
                }
            }
        }
        if (searchStartLine === -1)
        {
            console.error(`Loothaven: no const declaration and no std.Items.create( found in ${actualPath} searching backwards from line ${line}`);
            return null;
        }

        // Now search forward from the start line to find std.Items.create() call
        // The call might span multiple lines, so we need to handle that
        let createCallStart = -1;
        let createCallEnd = -1;
        let openParens = 0;
        let inString = false;
        let stringChar = '';
        let createCallText = '';

        for (let i = searchStartLine; i < Math.min(searchStartLine + 50, lines.length); i++)
        {
            const lineText = lines[i];
            
            // Check if this line contains std.Items.create before searching character by character
            const lineLower = lineText.toLowerCase();
            const hasCreateCall = lineLower.includes('std.items.create') || lineLower.includes('items.create');
            
            for (let j = 0; j < lineText.length; j++)
            {
                const char = lineText[j];
                const prevChar = j > 0 ? lineText[j - 1] : '';
                
                // Handle string literals
                if (!inString && (char === '"' || char === "'" || char === '`'))
                {
                    inString = true;
                    stringChar = char;
                }
                else if (inString && char === stringChar && prevChar !== '\\')
                {
                    inString = false;
                    stringChar = '';
                }
                
                if (inString) continue;
                
                // Track parentheses
                if (char === '(')
                {
                    if (openParens === 0 && createCallStart === -1)
                    {
                        // Check if this is std.Items.create(
                        // Look backwards up to 50 characters to find the function name
                        const searchStart = Math.max(0, j - 50);
                        const beforeParen = lineText.substring(searchStart, j).trim();
                        // Also check previous line if we're at the start of this line
                        let fullContext = beforeParen;
                        if (j < 10 && i > searchStartLine)
                        {
                            fullContext = lines[i - 1].trim() + ' ' + beforeParen;
                        }
                        
                        if (fullContext.toLowerCase().includes('std.items.create') || 
                            fullContext.toLowerCase().includes('items.create'))
                        {
                            createCallStart = i;
                            openParens = 1;
                            createCallText = lineText.substring(j);
                            continue;
                        }
                    }
                    else if (createCallStart !== -1)
                    {
                        openParens++;
                        createCallText += char;
                    }
                }
                else if (char === ')' && createCallStart !== -1)
                {
                    openParens--;
                    createCallText += char;
                    if (openParens === 0)
                    {
                        createCallEnd = i;
                        break;
                    }
                }
                else if (createCallStart !== -1)
                {
                    createCallText += char;
                }
            }
            
            if (createCallEnd !== -1) break;
        }

        if (createCallStart === -1 || createCallEnd === -1)
        {
            console.error(`Loothaven: could not find std.Items.create() call starting from line ${searchStartLine + 1} in ${actualPath}`);
            return null;
        }

        // Extract the tag (second argument), optional parented ID (third argument) from std.Items.create('mod', 'tag'[, parentedId])
        // Note: createCallText starts with '(' since we captured from the opening parenthesis
        // Normalize whitespace and handle quotes properly
        const normalizedCall = createCallText.replace(/\s+/g, ' ').trim();
        
        // Match the arguments: ('mod', 'tag') or ("mod", "tag") or ('mod', 'tag', 6392)
        const tagMatch = normalizedCall.match(/\(\s*(['"`])((?:[^\\]|\\.)*?)\1\s*,\s*(['"`])((?:[^\\]|\\.)*?)\3(?:\s*,\s*(\d+))?\s*\)/);
        if (tagMatch && tagMatch[4])
        {
            // Unescape the tag string
            let tag = tagMatch[4];
            tag = tag.replace(/\\(.)/g, '$1'); // Unescape characters
            const parentedId = tagMatch[5] ? parseInt(tagMatch[5], 10) : null;
            console.log(`Loothaven: extracted tag "${tag}"${extractedConstName ? `, const "${extractedConstName}"` : ""}${parentedId !== null ? `, parentedId ${parentedId}` : ""} from std.Items.create() call at line ${createCallStart + 1} in ${actualPath}`);
            return { tag, constName: extractedConstName, parentedId };
        }

        console.error(`Loothaven: could not extract tag from std.Items.create() call: ${normalizedCall.substring(0, 200)}`);
        return null;
    }
    catch (error)
    {
        console.error(`Loothaven: error reading file ${filePath}: ${error}`);
    }

    return null;
}

function extractModuleName(filePath: string): string
{
    const p = filePath.split("\\").join("/");

    // Prefer explicit dh-* submodule names when present
    if (p.includes("/dh-loot/"))
        return "dh-loot";
    if (p.includes("/dh-professions/") || p.includes("/crafting/items/"))
        return "dh-professions";

    return "dh-professions";
}

function getRelativeFilePath(filePath: string): string
{
    // Normalize path separators
    const p = filePath.split("\\").join("/");
    const pLower = p.toLowerCase();
    
    // Find the /release/ directory in the path (case-insensitive search)
    let releaseIdx = pLower.indexOf("/release/");
    if (releaseIdx === -1)
    {
        // Also check for paths that start with "release/" (no leading slash)
        // This handles relative paths or paths without drive letters
        if (pLower.startsWith("release/"))
        {
            releaseIdx = 0;
            const relativePath = p.substring("release/".length);
            console.log(`Loothaven: file path "${filePath}" -> relative path "${relativePath}" (found release/ at start)`);
            return relativePath;
        }
    }
    
    if (releaseIdx !== -1)
    {
        // Extract the substring starting from /release/ to get the relative path
        const relativePath = p.substring(releaseIdx + "/release/".length);
        console.log(`Loothaven: file path "${filePath}" -> relative path "${relativePath}"`);
        return relativePath;
    }
    
    // Fallback: try to find datascripts/ if /release/ not found
    let datascriptsIdx = pLower.indexOf("datascripts/");
    if (datascriptsIdx === -1)
    {
        // Check if path starts with datascripts/
        if (pLower.startsWith("datascripts/"))
        {
            datascriptsIdx = 0;
        }
    }
    
    if (datascriptsIdx !== -1)
    {
        const relativePath = p.substring(datascriptsIdx);
        console.log(`Loothaven: file path "${filePath}" -> relative path (datascripts fallback) "${relativePath}"`);
        return relativePath;
    }

    console.warn(`Loothaven: could not find /release/ or datascripts/ in path "${filePath}", using full path`);
    return p;
}

function createLoothavenRegistrar(item: ItemTemplate)
{
    const registrar = {
        set: (value: boolean | Record<string, any>) =>
        {
            if (value === false)
                return item;

            if (loothavenRegistered.has(item as any))
            {
                console.warn("Loothaven: item already registered");
                return item;
            }

            const metadata = (typeof value === "object" && value !== null) ? value : {};
            registerLoothavenItemAuto(item, metadata);
            loothavenRegistered.add(item as any);
            return item;
        },
        register: (metadata?: Record<string, any>) =>
        {
            return registrar.set(metadata || true);
        },
    };

    return registrar;
}

finish("loothaven-register-items", () =>
{
    for (const { item, options } of loothavenItemsToRegister)
    {
        try
        {
            const itemId = item.ID;
            if (!itemId || itemId === 0)
            {
                console.warn("Loothaven: skipping registration for item - ID not assigned");
                continue;
            }

            const itemName = item.Name.enGB.get();
            const itemLevel = item.ItemLevel.get();
            const itemClass = item.Class.getClass();
            const itemSubclass = item.Class.getSubclass();
            const itemQuality = item.Quality.get().toString();
            const inventoryType = item.InventoryType.get().toString();
            const requiredLevel = item.RequiredLevel.get();

            const moduleName = options.moduleName || extractModuleName(options.filePath || "");
            const filePath = options.filePath || "";
            const tagName = options.tagName ?? options.constName ?? "";
            const constName = options.constName ?? "";
            const parentedId = options.parentedId != null ? options.parentedId : null;
            const metadataJson = options.metadata ? JSON.stringify(options.metadata) : null;

            SQL.Databases.world_dest.write(`
                INSERT INTO \`loothaven_registry\` (
                    item_entry, item_name, item_level, item_class, item_subclass,
                    item_quality, inventory_type, required_level,
                    module_name, file_path, tag_name, parented_id, const_name, metadata
                ) VALUES (
                    ${itemId},
                    '${escapeSql(itemName)}',
                    ${itemLevel},
                    ${itemClass},
                    ${itemSubclass},
                    '${escapeSql(itemQuality)}',
                    '${escapeSql(inventoryType)}',
                    ${requiredLevel},
                    '${escapeSql(moduleName)}',
                    '${escapeSql(filePath)}',
                    '${escapeSql(tagName)}',
                    ${parentedId !== null ? parentedId : "NULL"},
                    '${escapeSql(constName)}',
                    ${metadataJson ? `'${escapeSql(metadataJson)}'` : "NULL"}
                )
                ON DUPLICATE KEY UPDATE
                    item_name = VALUES(item_name),
                    item_level = VALUES(item_level),
                    item_class = VALUES(item_class),
                    item_subclass = VALUES(item_subclass),
                    item_quality = VALUES(item_quality),
                    inventory_type = VALUES(inventory_type),
                    required_level = VALUES(required_level),
                    module_name = VALUES(module_name),
                    file_path = VALUES(file_path),
                    tag_name = VALUES(tag_name),
                    parented_id = VALUES(parented_id),
                    const_name = VALUES(const_name),
                    metadata = VALUES(metadata),
                    registered_at = CURRENT_TIMESTAMP
            `);
        }
        catch (error)
        {
            console.error(`Loothaven: error registering item: ${error}`);
        }
    }

    loothavenItemsToRegister.length = 0;
});

export class ItemDBC extends MaybeDBCEntity<ItemTemplate,ItemRow> {
    protected createDBC(): ItemRow {
        return DBC.Item.add(this.owner.ID)
            .SheatheType.set(0)
            .Material.set(0)
            .Sound_Override_Subclassid.set(0)
            .SubclassID.set(this.owner.row.subclass.get())
            .ClassID.set(this.owner.row.class.get())
            .DisplayInfoID.set(this.owner.row.displayid.get())
            .InventoryType.set(this.owner.row.InventoryType.get())
    }
    protected findDBC(): ItemRow {
        return DBC.Item.findById(this.owner.ID)
    }
    protected isValidDBC(dbc: ItemRow): boolean {
        return dbc.ID.get() === this.owner.ID;
    }

    get ClassID()       { return this.wrapDBC(0,dbc=>dbc.ClassID)}
    get SubclassID()    { return this.wrapDBC(0,dbc=>dbc.SubclassID)}
    get SoundOverride() { return this.wrapDBC(0,dbc=>dbc.Sound_Override_Subclassid)}
    get Material()      { return this.wrapDBC(0,dbc=>dbc.Material)}
    get DisplayInfoID() { return this.wrapDBC(0,dbc=>dbc.DisplayInfoID)}
    get InventoryType() { return this.wrapDBC(0,dbc=>dbc.InventoryType)}
    get SheatheType()   { return this.wrapDBC(0,dbc=>dbc.SheatheType)}
}

export class ItemDBCRow extends CellSystem<ItemTemplate> {
    protected readonly DBC = new ItemDBC(this.owner)
    exists() { return this.DBC.exists(); }
    get() { return this.DBC.getOrCreateDBC(); }
    mod(callback: (row: ItemRow)=>void) {
        callback(this.get());
        return this.owner;
    }

    static dbc(inst: ItemTemplate) {
        return inst.DBCRow.DBC;
    }
}

export class ItemTemplate extends MainEntityID<item_templateRow> {
    @Transient
    protected get dbc() { return ItemDBCRow.dbc(this); }
    readonly DBCRow = new ItemDBCRow(this);
    protected ItemSetNameRow = new ItemSetNameRow(this);
    static ItemSetNameRow(template: ItemTemplate) {
        return template.ItemSetNameRow;
    }

    get Name() { return new ItemName(this); }
    get ItemSetName() { return new ItemSetName(this); }
    get Socket() { return new ItemSockets(this); }
    get StartQuest() { return this.wrap(this.row.startquest); }
    get Lock() { return LockRegistry.ref(this, this.row.lockid); }
    get RandomProperty() { return this.wrap(this.row.RandomProperty); }
    get RandomSuffix() { return this.wrap(this.row.RandomSuffix); }
    get InlineScripts() {
        return getInlineID(
              this
            , this.ID
            , 'Item'
            , 'livescript'
        ) as _hidden.Item<this>
    }

    /** Only applicable if item is a shield */
    get Block() { return this.wrap(this.row.block); }
    get ItemSet() { return ItemSetRegistry.ref(this, this.row.itemset); }
    get Resistances() { return new ItemResistance(this); }
    get Stats() { return new ItemStats(this); }
    get Area() { return this.wrap(this.row.area); }
    get Map() { return this.wrap(this.row.Map); }
    get BagFamily() {
        return makeMaskCell32(BagFamily,this, this.row.BagFamily);
    }
    get TotemCategory() {
        return TotemCategoryRegistry.ref(this, this.row.TotemCategory);
    }
    get Loothaven() {
        return createLoothavenRegistrar(this);
    }
    get Sheath() {
        return makeEnumCell(ItemSheath,this, this.row.sheath);
    }
    get ScalingStats() { return new ItemScalingStat(this); }
    get Armor() { return this.wrap(this.row.armor); }
    get BonusArmor() { return this.wrap(this.row.ArmorDamageModifier); }
    get Delay() {
        return new DurationCell(
            this, 'MILLISECONDS', false, this.row.delay
        )
    }
    get RangeMod() { return this.wrap(this.row.RangedModRange); }
    get Description() { return new ItemDescription(this); }
    get Quality() {
        return makeEnumCell(ItemQuality,this, this.row.Quality);
    }
    get Durability() { return this.wrap(this.row.MaxDurability); }
    get Disenchant() { return Loot.Disenchant.ref(this, this.row.DisenchantID); }
    get RequiredLevel() { return this.wrap(this.row.RequiredLevel); }
    get ItemLevel() { return this.wrap(this.row.ItemLevel); }
    get RequiredSpell() { return this.wrap(this.row.requiredspell); }
    get RequiredHonorRank() { return this.wrap(this.row.requiredhonorrank); }
    get ClassMask() { return makeMaskCell32(ClassMask, this, this.row.AllowableClass, true); }
    get RaceMask() { return makeMaskCell32(RaceMask, this, this.row.AllowableRace, true); }
    get MaxCount() { return this.wrap(this.row.maxcount); }
    get MaxStack() { return this.wrap(this.row.stackable); }
    get Bonding() {
        return makeEnumCell(ItemBonding,this, this.row.bonding);
    }
    get Damage() { return new ItemDamages(this); }
    get Requirements() { return new ItemRequirements(this); }
    get Spells() { return new ItemSpells(this); }
    get Class() { return new ItemClass(this); }
    get SoundOverride() {
        return new MulticastCell(this,[
              this.row.SoundOverrideSubclass
            , this.dbc.SoundOverride
        ])
    }
    get Price() { return new ItemPrice(this); }
    get Material() {
        return makeEnumCell(ItemMaterial,this
            , new MulticastCell(this, [this.row.Material,this.dbc.Material])
        );
    }
    get Flags() {
        return makeMaskCell32(ItemFlags,this, this.row.Flags);
    }
    get InventoryType() {
        return makeEnumCell(ItemInventoryType,this
            , new MulticastCell(this, [
                  this.row.InventoryType
                , this.dbc.InventoryType
                , new CellBasic(this,()=>0,(value)=>{
                    if(this.ItemSetNameRow.exists()) {
                        this.ItemSetNameRow.InventoryType.set(value);
                    }
                })
            ])
        );
    }
    get SheatheType() { return this.dbc.SheatheType; }
    get RequiredFaction() { return new ItemRequiredFaction(this); }
    get ContainerSlots() { return this.wrap(this.row.ContainerSlots); }
    get RequiredDisenchantSkill() { return this.wrap(this.row.RequiredDisenchantSkill); }
    get Duration() { return this.wrap(this.row.duration); }
    get Holiday() { return HolidayRegistry.ref(this, this.row.HolidayId); }
    get ScriptName() { return this.wrap(this.row.ScriptName); }
    get FoodType() {
        return makeEnumCell(ItemFoodType,this, this.row.FoodType);
    }
    get MoneyLoot() { return new ItemMoneyLoot(this); }
    get FlagsCustom() {
        return makeMaskCell32(ItemFlagsCustom,this, this.row.flagsCustom);
    }

    get Loot() { return new LootSetRef(this, new LootSet(this.ID, SQL.item_loot_template)); }

    /**
     * This is readonly, because changing the gem properties
     * will also require changing the item id in the
     * enchantment connected to the gem.
     *
     * To create a new gem, see `std.Gems.create(...)` and its parenting options.
     */
    get GemProperties() { return GemRegistry.readOnlyRef(this, this.row.GemProperties); }

    get SocketBonus() { return EnchantmentRegistry.ref(this, this.row.socketBonus); }

    get DisplayInfo() {
        return ItemDisplayinfoRegistry.ref(this
            , new MulticastCell(this, [
                this.row.displayid, this.dbc.DisplayInfoID
            ])
        )
    }

    get PageText() { return PageTextRegistry.ref(this, this.row.PageText)}
    get PageMaterial() { return new PageMaterialCell(this, this.row.PageMaterial); }

    get AmmoType() {
        return makeEnumCell(ItemAmmoType,this, this.row.ammo_type);
    }

    /** Note: This field seem to have loads of data for >cata in the docs, so it can be very wrong. */
    get FlagsExtra() {
        return makeMaskCell32(ItemFlagsExtra,this, this.row.FlagsExtra);
    }

    get ID() {
        return this.row.entry.get();
    }

    codify(settings: {mod?: string, id?: string, name?: string, create_spells?: boolean, all_locs?: bool} & CodegenSettings)
    {
        const mod = settings.mod || 'mod';
        const id = settings.id || 'id';
        const create_spells = settings.create_spells === undefined ? false : settings.create_spells;
        const all_locs = settings.all_locs === undefined ? false : settings.all_locs

        return GenerateCode(settings,`std.Items.create('${mod}','${id}')`,code=>
        {
            if(all_locs)
            {
                if(settings.name)
                {
                    code.line(`.Name.enGB.set('${settings.name}')`)
                }
                code.loc('Description',this.Description)
                code.loc('ItemSetName',this.ItemSetName)
            }
            else
            {
                if(settings.name)
                {
                    code.line(`.Name.enGB.set('${settings.name}')`)
                }
                else
                {
                    code.line(`.Name.enGB.set('${this.Name.enGB.get().split("'").join("\\'")}')`)
                }
                if(this.Description.enGB.get().length > 0)
                {
                    code.line(`.Description.enGB.set('${this.Description.enGB.get().split("'").join("\\'")}')`)
                }
                if(this.ItemSetName.enGB.get().length > 0)
                {
                    code.line(`.ItemSetName.enGB.set('${this.ItemSetName.enGB.get().split("'").join("\\'")}')`)
                }
            }
            code.non_zero_enum('Bonding',this.Bonding)
            code.non_zero_enum('AmmoType',this.AmmoType)
            code.enum_line('Class',this.Class)
            code.non_zero_enum('FoodType',this.FoodType)
            code.non_zero_enum('InventoryType',this.InventoryType)
            code.non_zero_enum('Material',this.Material)
            code.non_zero_enum('SheatheType',this.SheatheType)
            code.non_zero_enum('TotemCategory',this.TotemCategory)
            code.non_zero_enum('Quality',this.Quality)

            code.non_zero_bitmask('BagFamily',this.BagFamily)
            if(this.ClassMask.get() !== 0xffffffff)
            {
                code.non_zero_bitmask('ClassMask',this.ClassMask)
            }
            if(this.RaceMask.get() !== 0xffffffff)
            {
                code.non_zero_bitmask('RaceMask',this.RaceMask)
            }
            code.non_zero_bitmask('Flags',this.Flags)
            code.non_zero_bitmask('FlagsCustom',this.FlagsCustom)
            code.non_zero_bitmask('FlagsExtra',this.FlagsExtra)

            code.non_def_num('Area',this.Area)
            code.non_def_num('Armor',this.Armor)
            code.non_def_num('Block',this.Block)
            code.non_def_num('BonusArmor',this.BonusArmor)
            code.non_def_num('ContainerSlots',this.ContainerSlots)
            this.Damage.forEach(x=>{
                if(!x.isClear())
                {
                    code.line(`.Damage.add('${x.School.objectify()}',${x.Min.get()},${x.Max.get()})`)
                }
            })

            code.non_def_num('Delay',this.Delay)
            code.non_def_num('Disenchant',this.Disenchant)
            code.non_def_num('Durability',this.Durability)
            code.non_def_num('Price.PlayerBuyPrice',this.Price.PlayerBuyPrice)
            code.non_def_num('Price.PlayerSellPrice',this.Price.PlayerSellPrice)
            code.non_def_num('Price.BuyCount',this.Price.BuyCount)
            code.non_def_num('RequiredLevel',this.RequiredLevel)
            code.non_def_num('RequiredDisenchantSkill',this.RequiredDisenchantSkill,-1)
            code.non_def_num('Duration',this.Duration)
            if(this.GemProperties.get())
            {
                code.line(`// Warning: Ignoring field "GemProperties" (gems will not work)`)
            }
            code.non_def_num('Holiday',this.Holiday)
            code.non_def_num('ItemLevel',this.ItemLevel)
            code.non_def_num('ItemSet',this.ItemSet)
            code.non_def_num('Lock',this.Lock)
            code.non_def_num('Map',this.Map)
            code.non_def_num('MaxCount',this.MaxCount)
            code.non_def_num('MaxStack',this.MaxStack)
            if(this.MoneyLoot.Min.get() !== 0 || this.MoneyLoot.Max.get() !== 0)
            {
                code.line(`.MoneyLoot.set(${this.MoneyLoot.Min.get()},${this.MoneyLoot.Max.get()})`)
            }
            code.non_def_num('ScriptName',this.ScriptName)
            code.non_def_num('Sheath',this.Sheath)
            code.non_def_num('SocketBonus',this.SocketBonus)
            code.non_def_num('StartQuest',this.StartQuest)
            code.non_def_num('SoundOverride',this.SoundOverride)

            this.Socket.forEach(x=>{
                if(!x.isClear())
                {
                    code.line(`.Socket.add('${x.Color.objectify()}',${x.Content.get()})`)
                }
            })

            this.Stats.forEach((x,i)=>{
                if(!x.isClear())
                {
                    code.line(`.Stats.add('${x.Type.objectify()}',${x.Value.get()})`)
                }
            })

            this.Spells.forEach((x,i)=>
            {
                if(!x.Spell.get())
                {
                    return;
                }

                code.begin_block('.Spells.addMod(x=>x')
                if(create_spells)
                {
                    code.begin_block(`.Spell.modRefCopy('${mod}','${id}_spell_${i}',x=>x`)
                    code.substruct(x.Spell.getRef(),settings);
                    code.end_block(')')
                }
                else
                {
                    code.non_def_num('Spell',x.Spell)
                }
                code.non_def_num('Category',x.Category)
                code.non_def_num('CategoryCooldown',x.CategoryCooldown)
                code.non_def_num('Charges.Raw',x.Charges.Raw)
                code.non_def_num('Cooldown',x.Cooldown)
                code.non_def_num('ProcsPerMinute',x.ProcsPerMinute)
                code.enum_line('Trigger',x.Trigger)
                code.end_block(`)`)
            })

            if(this.Loot.get().rows.length > 0)
            {
                code.begin_block(`.Loot.mod(x=>x`)
                this.Loot.get().codify(settings);
                code.end_block(`)`)
            }

            if(this.DisplayInfo.get())
            {
                code.begin_block(`.DisplayInfo.modRefCopy('${mod}','${id}_display',x=>x`)
                code.substruct(this.DisplayInfo.getRef(),Object.assign(settings,{mod:mod,id:id+'_display'}));
                code.end_block(`)`)
            }
        })
    }

        // custom additions
    IsWeapon() : bool {
        return this.Class.getClass() == 2
    }

    Is2hWeapon() : bool {
        return this.InventoryType.TWOHAND.is()
    }

    IsRanged() : bool {
        return this.InventoryType.RANGED.is() || this.InventoryType.THROWN.is() || this.InventoryType.WAND_GUN.is();
    }

    ItemValue: float = 0
    SecondaryValue: float = 0
    MainStat: Stat = 0

    clearStats() : ItemTemplate {
        this.Stats.clearAll()
        return this
    }

    fixSpeed() : ItemTemplate {
        if (this.IsWeapon()) {
            let Delay : uint32 = this.Delay.getAsMilliseconds()
            let Recalc = Delay

            if (this.Is2hWeapon()) {
                Recalc = Math.max(3600, Delay)
            } else if (this.Class.getSubclass() == 19 || this.Class.getSubclass() == 16) {
                Recalc = Math.max(3000, Delay)
            } else if (this.IsRanged()) {
                Recalc = Math.max(3000, Delay)
            } else if (Delay > 1999) {
                Recalc = Math.max(2600, Delay)
            } else
                Recalc = Math.min(1500, Delay)

            this.Delay.set(Recalc, 'MILLISECONDS')
        }

        return this.fixDPS()
    }

    fixDPS() : ItemTemplate {
        if (this.IsWeapon()) {
            let iLvl = this.ItemLevel.get()
            let DPS = this.Is2hWeapon() ? .85*iLvl : this.Class.getSubclass() == 19 ? iLvl : iLvl * .56
            if (this.Quality.get() < 2)
                DPS /= 2


            if (this.MainStat == Stat.INTELLECT) {
                DPS /= 2
                let SP = 6*DPS
                this.Stats.addSpellPower(SP)
                this.FlagsExtra.CASTER_WEAPON.set(true)
            }

            let Avg = DPS*this.Delay.getAsSeconds()
            let Variance = .24 * Avg
            this.Damage.forEach((IDam) => {
                if (IDam.School.PHYSICAL.is()) {
                    IDam.Min.set(Avg - Variance)
                    IDam.Max.set(Avg + Variance)
                }
            })
        }
        return this
    }

    setStats(Main: [number, float], ...Secondary: [number, float][]): ItemTemplate {
        this.clearStats()

        const iLvl = this.ItemLevel.get()
        const SlotMod = this.GetSlotMod()
        const isJewelry = this.InventoryType.FINGER.is() || this.InventoryType.NECK.is()
        const isTrinket = this.InventoryType.TRINKET.is()

        const MainStatMod = isJewelry ? 0 : (isTrinket ? .9 : .5259)
        const StaminaMod = isTrinket ? (Secondary.length < 0) ? .666 : 0 : this.IsWeapon() ? 0 : isJewelry ? .5259 : .7889
        const SecondaryMod = isJewelry ? 1.75 : isTrinket ? .666 : .7
        
        // What the item should look like at ilvl 52
        const BudgetForIlvl = SlotMod * (iLvl > 65 ? 32*(1.24**((iLvl-52)/30))/.7 : .625*iLvl-1.15);
        const SecondaryBudget = SlotMod * (iLvl > 65 ? 54*(1.15**((iLvl-52)/30))/.7 : 2*iLvl);

        const Q = this.Quality.get()
        let QualMod = 1.0
        switch(Q) {
            case ItemQuality.PURPLE:
            case ItemQuality.ORANGE:
                break
            case ItemQuality.BLUE:
                QualMod = .85
                break
            default:
                QualMod = .7
                break
        }
        
        this.MainStat = Main[0]
        const MainOverride = Main[1]

        if (MainStatMod) {
            let Amount = MainOverride * BudgetForIlvl * MainStatMod
            this.Stats.add(this.MainStat, Amount)
        }

        if (StaminaMod) {
            let Amount = BudgetForIlvl * StaminaMod
            this.Stats.addStamina(Amount)
        }

        const AmountForSecondary = SecondaryBudget * SecondaryMod
        Secondary.forEach(([Sec, Pct]) =>[
            this.Stats.add(Sec, Pct*AmountForSecondary)
        ])

        return this
    }

    GetSlotMod() : float {
        if (this.InventoryType.TWOHAND.is())
            return 1.0
        else if (this.InventoryType.WEAPON.is())
            return 1/2
        else {
            let Slot = this.InventoryType.get()
            switch(Slot) {
                case (ItemInventoryType.HEAD):
                case (ItemInventoryType.ROBE):
                case (ItemInventoryType.CHEST):
                case (ItemInventoryType.LEGS):
                    return 1.0
                case (ItemInventoryType.SHOULDER):
                case (ItemInventoryType.HANDS): 
                case (ItemInventoryType.FEET): 
                case (ItemInventoryType.WAIST): 
                case (ItemInventoryType.TRINKET):
                    return 12/16
                case (ItemInventoryType.WRISTS): 
                case (ItemInventoryType.NECK): 
                case (ItemInventoryType.FINGER): 
                case (ItemInventoryType.BACK):  
                    return 9/16
                case (ItemInventoryType.OFFHAND):
                case (ItemInventoryType.SHIELD):
                case (ItemInventoryType.THROWN): 
                    return 1/2
                case (ItemInventoryType.RANGED): 
                case (ItemInventoryType.WAND_GUN):
                    return 1
            }
        }
    }

    GenMainStat() {
        const Class = this.Class.getSubclass()
        const Roll = Math.random() * 100
        if (this.IsWeapon()) {
            switch (Class) {
                case 0: // ITEM_SUBCLASS_WEAPON_AXE
                case 16: // ITEM_SUBCLASS_WEAPON_THROWN
                case 1: // ITEM_SUBCLASS_WEAPON_AXE2
                case 5: // ITEM_SUBCLASS_WEAPON_MACE2
                    this.MainStat = Roll > 50 ? Stat.STRENGTH : Stat.AGILITY
                    break
                case 7: // ITEM_SUBCLASS_WEAPON_SWORD
                case 4: // ITEM_SUBCLASS_WEAPON_MACE
                    let Any = [Stat.AGILITY, Stat.STRENGTH, Stat.INTELLECT]
                    this.MainStat = Any[Math.floor(Math.random() * Any.length)]
                    break
                case 10: // ITEM_SUBCLASS_WEAPON_STAFF
                    this.MainStat = Roll > 80 ? Stat.AGILITY : Stat.INTELLECT
                    break;
                case 15: // ITEM_SUBCLASS_WEAPON_DAGGER
                    this.MainStat = Roll > 80 ? Stat.INTELLECT : Stat.AGILITY
                    break
                case 2: // ITEM_SUBCLASS_WEAPON_BOW
                case 18: // ITEM_SUBCLASS_WEAPON_CROSSBOW
                case 3: // ITEM_SUBCLASS_WEAPON_GUN
                case 6: // ITEM_SUBCLASS_WEAPON_POLEARM
                case 13: // ITEM_SUBCLASS_WEAPON_FIST
                    this.MainStat = Stat.AGILITY
                    break
                case 19: // ITEM_SUBCLASS_WEAPON_WAND
                    this.MainStat = Stat.INTELLECT
                    break
                case 8: // ITEM_SUBCLASS_WEAPON_SWORD2
                    this.MainStat = Stat.STRENGTH
                    break
            }
        } else {
            switch (Class) {
                case 0: // Jewelry
                    let Any = [Stat.AGILITY, Stat.STRENGTH, Stat.INTELLECT]
                    this.MainStat = Any[Math.floor(Math.random() * Any.length)]
                    break
                case 1: // Cloth
                    this.MainStat = Stat.INTELLECT
                    break
                case 2: // leather
                    this.MainStat = Roll > 80 ? Stat.INTELLECT : Stat.AGILITY
                    break
                case 3: // mail
                    this.MainStat = Roll > 60 ? Stat.INTELLECT : Stat.AGILITY
                    break
                case 4: // plate
                    this.MainStat = Roll > 70 ? Stat.INTELLECT : Stat.STRENGTH
                    break
                case 6: // shield
                    this.MainStat = Roll > 50 ? Stat.STAMINA : Stat.INTELLECT
                    break
            }
        }
    }

    WeaponHasAllowableMainStat() : bool {
        let Suggestion = this.MainStat
        if (this.IsWeapon()) {
            const Class = this.Class.getSubclass()
            switch (Class) {
                case 0: // ITEM_SUBCLASS_WEAPON_AXE
                case 16: // ITEM_SUBCLASS_WEAPON_THROWN
                case 1: // ITEM_SUBCLASS_WEAPON_AXE2
                case 5: // ITEM_SUBCLASS_WEAPON_MACE2
                    return Suggestion == Stat.STRENGTH || Suggestion == Stat.AGILITY
                case 7: // ITEM_SUBCLASS_WEAPON_SWORD
                case 4: // ITEM_SUBCLASS_WEAPON_MACE
                    return Suggestion == Stat.INTELLECT || Suggestion == Stat.AGILITY || Suggestion == Stat.STRENGTH
                case 10: // ITEM_SUBCLASS_WEAPON_STAFF
                case 15: // ITEM_SUBCLASS_WEAPON_DAGGER
                    return Suggestion == Stat.AGILITY || Suggestion == Stat.INTELLECT
                case 2: // ITEM_SUBCLASS_WEAPON_BOW
                case 18: // ITEM_SUBCLASS_WEAPON_CROSSBOW
                case 3: // ITEM_SUBCLASS_WEAPON_GUN
                case 6: // ITEM_SUBCLASS_WEAPON_POLEARM
                case 13: // ITEM_SUBCLASS_WEAPON_FIST
                    return Suggestion == Stat.AGILITY
                case 19: // ITEM_SUBCLASS_WEAPON_WAND
                    return Suggestion == Stat.INTELLECT
                case 8: // ITEM_SUBCLASS_WEAPON_SWORD2
                    return Suggestion == Stat.STRENGTH
                default:
                    return false
            }
        }
        return false;
    }
}

export class ItemTemplateRegistryClass
extends RegistryStatic<ItemTemplate,item_templateRow,item_templateQuery> {
    protected Clone(mod: string, id: string, r: ItemTemplate, parent: ItemTemplate): void {
        let dbc = DBC.Item.findById(parent.ID);
        r.row.GemProperties.set(0);
        if(dbc) {
            dbc.clone(r.ID);
        }
    }
    protected Table(): Table<any, item_templateQuery, item_templateRow> & { add: (id: number) => item_templateRow; } {
        return SQL.item_template
    }
    protected IDs(): StaticIDGenerator {
        return Ids.item_template
    }
    protected Entity(r: item_templateRow): ItemTemplate {
        return new ItemTemplate(r)
    }
    protected FindByID(id: number): item_templateRow {
        return SQL.item_template.query({entry:id});
    }
    protected EmptyQuery(): item_templateQuery {
        return {}
    }
    ID(e: ItemTemplate): number {
        return e.ID;
    }
    Clear(r: ItemTemplate) {
        r.AmmoType.NONE.set()
         .Area.set(0)
         .Armor.set(0)
         .BagFamily.set(0)
         .Block.set(0)
         .Bonding.NO_BOUNDS.set()
         .Class.JUNK.set()
         .ClassMask.set(-1)
         .RaceMask.set(-1)
         .ContainerSlots.set(0)
         .Damage.clearAll()
         .Delay.set(0)
         .Description.clear()
         .Disenchant.set(0)
         .DisplayInfo.set(0)
         .Durability.set(0)
         .Duration.set(0)
         .Flags.clearAll()
         .FlagsCustom.clearAll()
         .FlagsExtra.clearAll()
         .FoodType.set(0)
         .Holiday.set(0)
         .InventoryType.set(0)
         .ItemLevel.set(0)
         .ItemSet.set(0)
         .Lock.set(0)
         .Map.set(0)
         .Material.set(0)
         .MaxCount.set(0)
         .MaxStack.set(1)
         .MoneyLoot.set(0,0)
         .Name.clear()
         .Price.set(0,0,1)
         .Quality.set(0)
         .RandomProperty.set(0)
         .RandomSuffix.set(0)
         .RangeMod.set(0)
         .RequiredDisenchantSkill.set(0)
         .RequiredFaction.set(0,0)
         .RequiredHonorRank.set(0)
         .RequiredLevel.set(1)
         .RequiredSpell.set(0)
         .Requirements.Skill.clear()
         .Requirements.clearAll()
         .Resistances.clearAll()
         .ScalingStats.set(0,0)
         .ScriptName.set('')
         .Sheath.NONE.set()
         .Socket.clearAll()
         .SoundOverride.set(0)
         .Spells.clearAll()
         .StartQuest.set(0)
         .Stats.clearAll()
         .TotemCategory.set(0)
         .row.GemProperties.set(0)
    }
}

export const ItemTemplateRegistry = new ItemTemplateRegistryClass();