const iconPath = "Interface\\Addons\\bug-report-assets\\minimap-icon.blp";
let kinds = {//copied from idtip
    spell: "Spell",
    item: "item_template",
    unit: "creature_template",
    achievement: "AchievementID",
    criteria: "CriteriaID",
    ability: "SkillLine",
    enchant: "EnchantID",
    gem: "GemID",
    macro: "MacroID",
    equipmentset: "EquipmentSetID",
    quest: "quest_template"
}

export function BugUIBindings() {
    _G['BINDING_HEADER_BUGREPORT_CATEGORY'] = "Bug Report";
    _G['BINDING_NAME_SUBMIT_ENTITY_BUGREPORT'] = "Open Bug Report Form";
    _G['HandleBugReportDH'] = setupBugReportUI
    createQuestFrames()
    hooksecurefunc("QuestLog_UpdateQuestDetails", BugReportUI_QuestLog_UpdateQuestDetails);
    QuestLogFrame.HookScript("OnHide", (self) => {
        currentInfo = []
        currentKind = ""
    })
    GameTooltip.HookScript("OnHide", (self) => {
        currentInfo = []
        currentKind = ""
    })
}




let currentKind = "1";
let currentInfo = []

function setupBugReportUI() {
    let tag = "";
    let bugFrame = _G["bugTrackerUIFrame"]
    bugFrame['affID'].SetText("")
    switch (currentKind) {
        case kinds.item://itemName, itemLink
            setupBugReportUISpecial(bugFrame, select(3, strfind(currentInfo[1], "|H(.+)|h")).split(":")[1], tag, `${currentInfo[0]}`, "Items")
            break;
        case kinds.spell://spellID, spellName
            setupBugReportUISpecial(bugFrame, currentInfo[0], tag, `${currentInfo[1]}`, 'Spells')
            break;
        case kinds.unit://id, name, guid
            setupBugReportUISpecial(bugFrame, currentInfo[0], tag, `${currentInfo[1]}`, 'Creatures')
            break;

        case kinds.quest://questID, questLevel, questName
            setupBugReportUISpecial(bugFrame, currentInfo[0], tag, `${(currentInfo[2] as string).replace("|h|r", "")}`, 'Quests')
            break;
        case "":
            //print("No Saved Info Here!")
            break;
    }
    bugFrame.Show();
}

function setupBugReportUISpecial(bugFrame: WoWAPI.Frame, entry: string, tag: string, title: string, category: string) {
    if (_G['tooltipInfo'][currentKind] != null)
        if (_G['tooltipInfo'][currentKind][tonumber(entry)] != null)
            tag = _G['tooltipInfo'][currentKind][tonumber(entry)]
    bugFrame['titleBox'].SetText(title)
    UIDropDownMenu_SetText(bugFrame['catBox'], category);
    bugFrame['catBox']['sel'] = category
    if (tag == "")
        bugFrame['affID'].SetText(entry)
    else
        bugFrame['affID'].SetText(tag)
}

export function updateBindingKey() {
    let key = "F6";
    (GetBindingKey("SUBMIT_ENTITY_BUGREPORT") as string[]).forEach((v) => {
        key = v;
    });
    return key
}

export function buildTooltip() {
    let bindingKey = updateBindingKey();
    return `|T${iconPath}:0|t |caa5596f1<${bindingKey} For Bug Report>`;
}

GameTooltip.HookScript("OnTooltipSetItem", () => {
    GameTooltip.AddLine(buildTooltip())
    const [itemName, itemLink] = GameTooltip.GetItem();
    if (itemLink) {
        currentKind = kinds.item
        currentInfo = [itemName, itemLink]
    }
});

GameTooltip.HookScript("OnTooltipSetSpell", () => {
    GameTooltip.AddLine(buildTooltip())
    const [spellName, _, spellID] = GameTooltip.GetSpell();
    if (spellID) {
        currentKind = kinds.spell
        currentInfo = [spellID, spellName]
    }
});

hooksecurefunc(GameTooltip, "SetAction", function (self, action) {
    let [kind, id, subType, spellID] = GetActionInfo(action)
    if (kind != null && kind == "spell" && spellID != null) {
        let [name] = GetSpellInfo(spellID);
        currentKind = kinds.spell
        currentInfo = [spellID, name]
    }
})

GameTooltip.HookScript("OnTooltipSetUnit", (self: WoWAPI.GameTooltip) => {
    GameTooltip.AddLine(buildTooltip())
    let [name, unit] = self.GetUnit();
    if (unit) {
        if (unit == "player")
            return;
        const guid = UnitGUID(unit as WoWAPI.UnitId);
        const id = parseInt((UnitGUID("mouseover") || "0").substring(6, 12), 16);
        if (id < 0)
            return;
        currentKind = kinds.unit
        currentInfo = [id, name, guid]
    }
});


let BugReportUIQuestLogTip: WoWAPI.Frame = null;
function createQuestFrames() {
    BugReportUIQuestLogTip = CreateFrame("Frame", 'BugReportUIQuestLogTip', QuestLogDetailScrollChildFrame);
    BugReportUIQuestLogTip.SetSize(260, 14);

    BugReportUIQuestLogTip['BugReportUIQuestLogTipLabel'] = BugReportUIQuestLogTip.CreateFontString('BugReportUIQuestLogTipLabel', null, 'QuestFontNormalSmall');
    BugReportUIQuestLogTip['BugReportUIQuestLogTipLabel'].SetText('');
    BugReportUIQuestLogTip['BugReportUIQuestLogTipLabel'].SetJustifyH('LEFT');
    BugReportUIQuestLogTip['BugReportUIQuestLogTipLabel'].SetJustifyV('TOP');
    BugReportUIQuestLogTip['BugReportUIQuestLogTipLabel'].SetTextColor(0, 0, 0);
    BugReportUIQuestLogTip['BugReportUIQuestLogTipLabel'].SetSize(235, 14);
    BugReportUIQuestLogTip['BugReportUIQuestLogTipLabel'].SetPoint('LEFT', BugReportUIQuestLogTip, 'LEFT');
}

function BugReportUI_QuestLog_UpdateQuestDetails() {
    BugReportUIQuestLogTip.Show();
    if (QuestInfoRequiredMoneyText.IsVisible())
        BugReportUIQuestLogTip.SetPoint("TOPLEFT", "QuestInfoRequiredMoneyText", "BOTTOMLEFT", 5, -10);
    else if (GetNumQuestLeaderBoards() > 0 && !QuestInfoGroupSize.IsVisible())
        BugReportUIQuestLogTip.SetPoint("TOPLEFT", "QuestInfoObjective" + GetNumQuestLeaderBoards(), "BOTTOMLEFT", 5, -10);

    else if (QuestInfoGroupSize.IsVisible())
        BugReportUIQuestLogTip.SetPoint("TOPLEFT", "QuestInfoGroupSize", "BOTTOMLEFT", 5, -10);
    else
        BugReportUIQuestLogTip.SetPoint("TOPLEFT", "QuestInfoObjectivesText", "BOTTOMLEFT", 5, -10);
    QuestInfoDescriptionHeader.SetPoint("TOPLEFT", BugReportUIQuestLogTip, "BOTTOMLEFT", -5, -10);
    BugReportUIQuestLogTip['BugReportUIQuestLogTipLabel'].SetText(buildTooltip());

    let questLinkParts = (GetQuestLink(GetQuestLogSelection()) as string).replace("|", "").split(":")
    let parts2 = questLinkParts[2].split("[")
    currentKind = kinds.quest
    currentInfo = [questLinkParts[1], parts2[0], parts2[1].replace("]", "")]
}
