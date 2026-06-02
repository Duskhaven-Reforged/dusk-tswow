import { makeEnumCell } from "../../../../data/cell/cells/EnumCell";
import { SpellImplicitTarget } from "../SpellImplicitTarget";
import { SpellRadiusRegistry } from "../SpellRadius";
import { EffectTemplate } from "./EffectTemplate";

export class TargetBase extends EffectTemplate {
    /**
     * Generic Target type.
     */
    get ImplicitTargetA() {
        return makeEnumCell(SpellImplicitTarget, this, this.wrap(this.owner.ImplicitTargetARaw));
    }

    /**
     * Generic Target type. Value depends on TargetA
     */
     get ImplicitTargetB() {
        return makeEnumCell(SpellImplicitTarget, this, this.wrap(this.owner.ImplicitTargetBRaw));
    }

    /**
     * Generic radius. Value depends on TargetA/TargetB
     */
    get Radius() {
        return SpellRadiusRegistry.ref(this, this.wrap(this.owner.Radius));
    }

    /**
     * How many units can be chained by this spell
     */
    get ChainTargets() { return this.wrap(this.owner.ChainTarget); }

    get ChainAmplitude() { return this.wrap(this.owner.ChainAmplitude); }
}
