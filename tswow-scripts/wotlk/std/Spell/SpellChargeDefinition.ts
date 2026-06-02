import { spell_charge_defRow } from "../../sql/spell_charge_def";
import { SQL } from "../../SQLFiles";
import { MaybeSQLEntity } from "../Misc/SQLDBCEntity";
import { Spell } from "./Spell";

export class SpellChargeDefinition<T extends Spell> extends MaybeSQLEntity<T, spell_charge_defRow> {
    protected createSQL(): spell_charge_defRow {
        return SQL.spell_charge_def.add(this.owner.ID)
            .max.set(1)
            .cooldown_ms.set(1)
    }

    protected findSQL(): spell_charge_defRow {
        return SQL.spell_charge_def.query({ spell: this.owner.ID })
    }

    protected isValidSQL(sql: spell_charge_defRow): boolean {
        return sql.spell.get() === this.owner.ID
    }

    get Max() { return this.wrapSQL(1, sql => sql.max); }
    get CooldownMs() { return this.wrapSQL(1, sql => sql.cooldown_ms); }

    set(max: number, cooldownMs: number): T {
        if(max <= 0)
            throw new Error(`SpellCharges max must be greater than 0 for spell ${this.owner.ID}`);

        if(cooldownMs <= 0)
            throw new Error(`SpellCharges cooldownMs must be greater than 0 for spell ${this.owner.ID}`);

        this.Max.set(Math.floor(max));
        this.CooldownMs.set(Math.floor(cooldownMs));
        return this.owner;
    }
}
