import { CellSystem } from "../../../data/cell/systems/CellSystem";
import { loc_constructor } from "../../../data/primitives";
import { SkillLineRow } from "../../dbc/SkillLine";
import { DBC } from "../../DBCFiles";
import { SQL } from "../../SQLFiles";
import { ItemTemplate, ItemTemplateRegistry } from "../Item/ItemTemplate";
import { MainEntity } from "../Misc/Entity";
import { Ids } from "../Misc/Ids";
import { SelfRef } from "../Refs/Ref";
import { SkillLine } from "../SkillLines/SkillLine";
import { Spell } from "../Spell/Spell";
import { SpellRegistry } from "../Spell/Spells";
import { TrainerBase } from "../Trainer/Trainer";
import { ProfessionGatheringNodes } from "./ProfessionGatheringNodes";
import { ProfessionGatheringSpells } from "./ProfessionGatheringSpells";
import { ProfessionNameSystem } from "./ProfessionName";
import { ProfessionRecipes } from "./ProfessionRecipe";
import { ProfessionTier, resolveProfessionRank } from "./ProfessionType";

export class Profession extends MainEntity<SkillLineRow> {
    get AsSkillLine() {
        return new SelfRef(this, ()=>new SkillLine(this.row))
    }
    private _cachedApprenticeSpell: Spell|undefined = undefined;
    private _cachedLearnSpells: {[key: number]: Spell[]} = {}

    /** contains all except the apprentice spell */
    private _cachedRanks: Spell[]|undefined = undefined;

    setHasCrafting(value: boolean) {
        this.Ranks.forEach(rank=>{
            let spell = rank.ProfessionSpell();
            if(value) {
                spell.Attributes.IS_HIDDEN_IN_SPELLBOOK.set(false);
                spell.Attributes.UNK41.set(false);
            } else {
                spell.Attributes.UNK41.set(true);
                spell.Attributes.IS_HIDDEN_IN_SPELLBOOK.set(true);
            }
        })
        return this;
    }

    get Recipes() { return new ProfessionRecipes(this); }
    get GatheringNodes() { return new ProfessionGatheringNodes(this); }
    get GatheringSpells() { return new ProfessionGatheringSpells(this); }
    get Name() { return new ProfessionNameSystem(this); }
    get ID() { return this.AsSkillLine.get().ID; }
    readonly Ranks = new ProfessionRanks(this);

    static findApprenticeSpell(thiz: Profession) {
        // cached because it's expensive to find, and shouldn't change
        if(thiz._cachedApprenticeSpell!==undefined) return thiz._cachedApprenticeSpell;
        let spell = DBC.SkillLineAbility.queryAll({SkillLine:thiz.ID})
            .map(x=>SpellRegistry.load(x.Spell.get()))
            [0]

        if(!spell) return undefined;
        let spellRank = SQL.spell_ranks.query({spell_id:spell.ID});
        if(!spellRank) return undefined;
        let firstSpell = SpellRegistry.load(spellRank.first_spell_id.get());
        if(!firstSpell) {
            throw new Error(`Profession ${thiz.AsSkillLine.get().Name.enGB.get()} has an invalid first spell rank in spell_ranks`);
        }
        return thiz._cachedApprenticeSpell = firstSpell;
    }

    static getLearnSpells(profession: Profession, rank: number): Spell[] {
        if(profession._cachedLearnSpells[rank]) {
            return profession._cachedLearnSpells[rank];
        }
        let rankSpell = this.getSkillRank(profession, rank);
        if(!rankSpell) return [];
        // TODO: false positive
        let spells = SpellRegistry.queryAll({Effect:36,EffectTriggerSpell:rankSpell.ID})
        if(spells.length === 0) {
            throw new Error(`Profession ${profession.ID} lacks a learn spell for rank ${rank}!`)
        }
        return (profession._cachedLearnSpells[rank] = spells);
    }

    static getSkillRank(profession: Profession, index: number) {
        if(index==1) {
            return this.findApprenticeSpell(profession);
        }

        if(profession._cachedRanks !== undefined) {
            return profession._cachedRanks[index-2];
        }

        let apprentice = this.findApprenticeSpell(profession);
        if(!apprentice) {
            return undefined;
        }

        let rank = SQL.spell_ranks.query({
              first_spell_id:apprentice.ID
            , rank:index
        });

        if(rank===undefined) {
            return undefined;
        }
        let spl = SpellRegistry.load(rank.spell_id.get());

        if(spl===undefined) {
            throw new Error(`Spell ${profession.AsSkillLine.get().Name.enGB} has an invalid spell at rank ${index}`);
        }
        return spl;
    }

    static setCacheRanks(profession: Profession) {
        profession._cachedRanks = []
    }

    static setCacheNonApprenticeSpell(profession: Profession, spell: Spell) {
        if(profession._cachedRanks !== undefined) {
            profession._cachedRanks.push(spell);
        }
    }

    static setCacheApprenticeSpell(profession: Profession, spell: Spell) {
        profession._cachedApprenticeSpell = spell;
    }

    static setCacheLearnSpells(profession: Profession) {
        profession._cachedLearnSpells = [];
    }
    static addCachedLearnSpell(profession: Profession, rank: number, spell: Spell) {
        if(profession._cachedLearnSpells) {
            (profession._cachedLearnSpells[rank] || (profession._cachedLearnSpells[rank] = []))
                .push(spell);
        }
    }

    static getTiers(profession: Profession) {
        return DBC.SkillTiers.findById(
            profession.AsSkillLine.get().RaceClassInfos.get()[0].SkillTier.get()
        )
    }

    static copyTiers(profession: Profession) {
        profession.AsSkillLine.get().RaceClassInfos
            .get()[0].SkillTier.set
                (
                    this.getTiers(profession)
                        .clone(Ids.SkillTiers.id())
                        .ID.get()
                )
    }

    addToTrainer(trainer: TrainerBase, tier: ProfessionTier, reqSkillValue: number, cost: number = 0, reqLevel: number = 0) {
        let index = resolveProfessionRank(tier);
        let len = this.Ranks.length;
        if(index > len) {
            throw new Error(
              `Attempted to add rank ${resolveProfessionRank(index)}`
            + ` for profession ${this.ID} to a trainer`
            + `, but it only has ${len} ranks`
            )
        }
        trainer.Spells.add(
              this.Ranks.get(index).LearnSpells()[0].ID
            , cost
            , reqLevel
            , this.ID
            , reqSkillValue
            , index == 0 ? [] : [this.Ranks.get(resolveProfessionRank(index)-1).ProfessionSpell().ID]
            )
        return this;
    }

    /**
     * Calculate trivial skill levels based on item level (placeholder implementation)
     * @param itemLevel The item level of the crafted item
     * @returns Object with yellow and gray skill levels
     */
    static calculateTrivialLevels(itemLevel: number): {yellow: number, gray: number} {
        if (itemLevel <= 20) {
            return { yellow: 25, gray: 50 };
        } else if (itemLevel <= 40) {
            return { yellow: 75, gray: 150 };
        } else if (itemLevel <= 60) {
            return { yellow: 150, gray: 300 };
        } else if (itemLevel <= 80) {
            return { yellow: 225, gray: 450 };
        } else {
            return { yellow: 300, gray: 600 };
        }
    }

    /**
     * Helper function to easily add a recipe to a profession
     * @param mod Mod identifier for the recipe spell
     * @param id Unique identifier for the recipe spell
     * @param outputItem The ItemTemplate of the item created by this recipe
     * @param reagents Array of reagent tuples: [ItemTemplate | number, count]
     * @param options Optional configuration for the recipe
     * @param options.trainer Optional trainer to automatically add this recipe to (requires reqSkillRank)
     * @param options.learnOnRank Profession tier to auto-learn this recipe at. Note: Use 'Apprentice' or 0 for rank 1, 'Journeyman' or 1 for rank 2, etc.
     * @param options.reqSkillRank Optional skill rank requirement for trainer. If trainer is provided, recipe will be automatically added to trainer.
     * @returns this for method chaining
     */
    addRecipe(
        mod: string,
        id: string,
        outputItem: ItemTemplate,
        reagents: Array<[ItemTemplate | number, number]>,
        options?: {
            outputCount?: number,
            castTime?: number,
            trivialYellow?: number,
            trivialGray?: number,
            spellFocus?: number,
            totems?: [number, number],
            learnOnRank?: ProfessionTier,
            trainer?: TrainerBase,
            trainerRequiredRank?: number,
            reqSkillRank?: number,
            trainerCost?: number,
            trainerReqLevel?: number
        }
    ): this {
        // Calculate cast time from reagent count (1.5s per reagent entry)
        const reagentCount = reagents.length;
        const calculatedCastTime = reagentCount * 1500;
        const castTime = options?.castTime ?? calculatedCastTime;

        // Get item level and calculate trivial levels
        const itemLevel = outputItem.ItemLevel.get();
        const trivialLevels = Profession.calculateTrivialLevels(itemLevel);
        const yellow = options?.trivialYellow ?? trivialLevels.yellow;
        const gray = options?.trivialGray ?? trivialLevels.gray;

        // Default output count is 1
        const outputCount = options?.outputCount ?? 1;

        let recipeSpellId: number | undefined = undefined;

        this.Recipes.addMod(mod, id, (R) => {
            recipeSpellId = R.ID;
            // Set output item
            R.OutputItem.set(outputItem.ID);
            
            // Set output count
            // Note: ShiftedNumberCell handles conversion based on DieSides value
            // If DieSides > 0, it stores as (count - 1), otherwise stores as-is
            // For count > 1, we set DieSides to 1 to enable the shifted behavior
            if (outputCount > 1) {
                R.AsSpell().Effects.mod(0, eff => eff.PointsDieSides.set(1));
                R.OutputCount.set(outputCount - 1);
            } else {
                // For count = 1, we can leave DieSides at 0 and set BasePoints to 1
                // or set DieSides to 1 and BasePoints to 0. We'll use the latter for consistency.
                R.AsSpell().Effects.mod(0, eff => eff.PointsDieSides.set(1));
                R.OutputCount.set(0);
            }
            
            // Add reagents
            for (const [item, count] of reagents) {
                const itemId = typeof item === 'number' ? item : item.ID;
                R.Reagents.add(itemId, count);
            }
            
            // Set cast time
            R.CastTime.setSimple(castTime, 0, castTime);
            
            // Set trivial skill levels
            R.Ranks.set(yellow, gray);
            
            // Optional: Set spell focus
            if (options?.spellFocus !== undefined) {
                R.SpellFocus.set(options.spellFocus);
            }
            
            // Optional: Set totems
            if (options?.totems !== undefined) {
                R.Totems.setIndex(0, options.totems[0]);
                if (options.totems[1] !== undefined && options.totems[1] !== 0) {
                    R.Totems.setIndex(1, options.totems[1]);
                }
            }
            
            // Optional: Auto-learn at profession rank
            if (options?.learnOnRank !== undefined) {
                R.LearnOnRank(options.learnOnRank);
            }
        });

        // Automatically add recipe to trainer if reqSkillRank is provided
        // IMPORTANT: The trainer option should match the correct trainer rank based on reqSkillRank:
        // - reqSkillRank 1-75: Use Journeyman trainer (teaches Apprentice recipes)
        // - reqSkillRank 76-150: Use Expert trainer (teaches Journeyman recipes)
        // - reqSkillRank 151-225: Use Artisan trainer (teaches Expert recipes)
        // - reqSkillRank 226-300: Use Artisan trainer (teaches Artisan recipes)
        // The trainer option is still required but should be set to the correct rank.
        if (options?.trainer && options?.reqSkillRank !== undefined && recipeSpellId !== undefined) {
            const reqSkillRank = options.reqSkillRank;
            const cost = options.trainerCost ?? 0;
            const reqLevel = options.trainerReqLevel ?? 0;
            
            // Add to the specified trainer
            this.addRecipeToTrainer(
                options.trainer,
                recipeSpellId,
                reqSkillRank,
                cost,
                reqLevel,
                this.ID,
                reqSkillRank
            );
        }

        // Note: If trainer is not provided, use trainer.Spells.add() with the required values,
        // or use the addRecipeToTrainer helper method to add the recipe to a trainer later.

        return this;
    }

    /**
     * Add a recipe to a trainer with the specified required skill rank
     * This helper method uses the trainerRequiredRank value from addRecipe options
     * @param trainer The trainer to add the recipe to
     * @param recipeSpellId The spell ID of the recipe (obtained from SpellRegistry after creating the recipe)
     * @param requiredRank The required skill rank to learn this recipe
     * @param cost The cost to learn the recipe (default: 0)
     * @param reqLevel The required character level (default: 0)
     * @param reqSkillLine The required skill line (default: this profession's skill line)
     * @param reqSkillRank The required skill rank value (default: requiredRank parameter)
     */
    addRecipeToTrainer(
        trainer: TrainerBase,
        recipeSpellId: number,
        requiredRank: number,
        cost: number = 0,
        reqLevel: number = 0,
        reqSkillLine?: number,
        reqSkillRank?: number
    ): this {
        trainer.Spells.add(
            recipeSpellId,
            cost,
            reqLevel,
            this.ID,
            reqSkillRank ?? requiredRank,
            []
        );
        return this;
    }
}

export class ProfessionRankNumber extends CellSystem<Profession> {
    private readonly index: number;

    constructor(owner: Profession, index: number) {
        super(owner);
        this.index = index;
    }

    set(value: number) {
        Profession.getTiers(this.owner).Value.setIndex(this.index,value);
        return this.owner;
    }

    copyTiersAndSet(value: number) {
        Profession.copyTiers(this.owner);
        return this.set(value);
    }
}

export class ProfessionRank extends CellSystem<Profession> {
    private readonly rank;
    constructor(owner: Profession, rank: number) {
        super(owner);
        this.rank = rank;
    }

    get SkillLevel() { return new ProfessionRankNumber(this.owner,this.rank); }
    LearnSpells() { return Profession.getLearnSpells(this.owner, this.rank) }
    ProfessionSpell() { return Profession.getSkillRank(this.owner, this.rank) as any as Spell; }
}

export class ProfessionRanks extends CellSystem<Profession> {
    private cachedLength?: number = undefined;

    static setCached(ranks: ProfessionRanks, length: number) {
        ranks.cachedLength = length;
    }

    clearCache() {
        this.cachedLength = undefined;
    }

    get length() {
        if(this.cachedLength !== undefined) {
            return this.cachedLength;
        }

        let fst = Profession.findApprenticeSpell(this.owner);
        if(fst == undefined) {
            return 0;
        }
        let ranks = SQL.spell_ranks
            .queryAll({first_spell_id:fst.ID})
            .sort((a,b)=>a.rank.get()>b.rank.get() ? -1 : 1)
            .map(x=>x.rank.get())

        if(ranks.length == 0) {
            throw new Error(`Profession ${this.owner.ID} does not have any correct spell ranks`);
        }

        return this.cachedLength = ranks[0];
    }

    forEach(callback: (rank: ProfessionRank)=>void) {
        let len = this.length;
        for(let i=0;i<len; ++i) {
            callback(this.get(i));
        }
        return this.owner;
    }

    add(modid: string, id: string, maxSkill: number, subtext: loc_constructor) {
        this.addGet(modid,id,maxSkill,subtext);
        return this.owner;
    }

    addGet(modid: string, id: string, maxSkill: number, subtext: loc_constructor) {
        let newIndex = this.length;
        let newRank = newIndex + 1
        if(this.cachedLength!==undefined) this.cachedLength++;
        let spell = SpellRegistry.create(modid,id)
            .Name.set(this.owner.AsSkillLine.get().Name.objectify())
            .Subtext.set(subtext)
            .Description.set(this.owner.AsSkillLine.get().Description.objectify())
            .Attributes.IS_ABILITY.set(true)
            .Attributes.NOT_SHAPESHIFTED.set(true)
            .Attributes.CASTABLE_WHILE_MOUNTED.set(true)
            .Attributes.CASTABLE_ON_VEHICLE.set(true)
            .Icon.setPath('Interface\\Icons\\Trade_BlackSmithing')
            .SchoolMask.PHYSICAL.set(true)
            .Visual.set(0)
            .Effects.addMod(eff=>{
                eff.Type.TRADE_SKILL.set()
            })
            .Effects.addMod(eff=>{
                eff.Type.SKILL.set()
                    .SkillTier.set(newIndex)
                    .Skill.set(this.owner.ID)
                    .ImplicitTargetA.set(0)
                    .ImplicitTargetB.set(0)
                    .ChainAmplitude.set(1)
                    .AsEffect.get()
                    .PointsDieSides.set(1)
                    .BonusMultiplier.set(1)
            })
            .SkillLines.addMod(this.owner.ID,undefined,undefined,sla=>{
                sla
                   .ClassMaskForbidden.set(0)
                   .MinSkillRank.set(1)
                   .AcquireMethod.set(0)
                   .RaceMask.set(0)
                   .ClassMask.set(0)
            })

        if(newIndex === 0) {
            Profession.setCacheApprenticeSpell(this.owner, spell);
        } else {
            Profession.setCacheNonApprenticeSpell(this.owner, spell);
        }

        spell.Rank.set(newIndex == 0 ? spell.ID : this.get(0).ProfessionSpell().ID,newRank)
        if(newIndex > 0) {
            this.get(newIndex-1).ProfessionSpell().SkillLines.forEach(x=>{
                x.SupercededBy.set(spell.ID);
            })
        }

        let learnSpell = SpellRegistry.create(modid,`${id}-learn`)
            .Name.set(this.owner.AsSkillLine.get().Name.objectify())
            .Subtext.set(subtext)
            .Attributes.IS_HIDDEN_FROM_LOG.set(true)
            .Attributes.SHEATHE_UNCHANGED.set(true)
            .TargetType.UNIT_ALLY.set(true)
            .SchoolMask.PHYSICAL.set(true)
            .Effects.addMod(effect=>{
                effect.Type.LEARN_SPELL.set()
                    .LearntSpell.set(spell.ID)
            })
            .Effects.addMod(effect=>{
                effect.Type.SKILL_STEP.set()
                        .Skill.set(this.owner.ID)
                        .Tier.set(newIndex)
                        .AsEffect.get()
                        .PointsDieSides.set(1)
            })

        Profession.addCachedLearnSpell(this.owner, newRank, learnSpell);
        Profession.getTiers(this.owner).Value.setIndex(newIndex,maxSkill);
        return new ProfessionRank(this.owner,newRank);
    }

    copyTiersAndAdd(modid: string, id: string, maxSkill: number, subtext: loc_constructor) {
        Profession.copyTiers(this.owner);
        this.add(modid,id,maxSkill,subtext);
        return this.owner;
    }

    get(index: ProfessionTier) {
        return new ProfessionRank(
              this.owner
            , resolveProfessionRank(index)+1
            );
    }
}