import { Table } from "../../../data/table/Table";
import { jump_charge_paramsQuery, jump_charge_paramsRow } from "../../sql/jump_charge_params";
import { SQL } from "../../SQLFiles";
import { MainEntity } from "../Misc/Entity";
import { Ids, StaticIDGenerator } from "../Misc/Ids";
import { RegistryStatic } from "../Refs/Registry";

export class JumpChargeParams extends MainEntity<jump_charge_paramsRow> {
    get ID() { return this.row.id.get(); }
    get Speed() { return this.wrap(this.row.speed); }
    get TreatSpeedAsMoveTimeSeconds() { return this.wrap(this.row.treatSpeedAsMoveTimeSeconds); }
    get UnlimitedSpeed() { return this.wrap(this.row.unlimitedSpeed); }
    get MinHeight() { return this.wrap(this.row.minHeight); }
    get MaxHeight() { return this.wrap(this.row.maxHeight); }
    get SpellVisualId() { return this.wrap(this.row.spellVisualId); }
    get ProgressCurveId() { return this.wrap(this.row.progressCurveId); }
    get ParabolicCurveId() { return this.wrap(this.row.parabolicCurveId); }
    get TriggerSpellId() { return this.wrap(this.row.triggerSpellId); }
}

export class JumpChargeParamsRegistryClass extends RegistryStatic<JumpChargeParams, jump_charge_paramsRow, jump_charge_paramsQuery> {
    protected Table(): Table<any, jump_charge_paramsQuery, jump_charge_paramsRow> & { add: (id: number) => jump_charge_paramsRow; } {
        return SQL.jump_charge_params;
    }

    protected IDs(): StaticIDGenerator {
        return Ids.jump_charge_params;
    }

    Clear(entity: JumpChargeParams): void {
        entity.Speed.set(0)
            .TreatSpeedAsMoveTimeSeconds.set(0)
            .UnlimitedSpeed.set(0)
            .MinHeight.set(0)
            .MaxHeight.set(0)
            .SpellVisualId.set(0)
            .ProgressCurveId.set(0)
            .ParabolicCurveId.set(0)
            .TriggerSpellId.set(0);
    }

    protected FindByID(id: number): jump_charge_paramsRow {
        return SQL.jump_charge_params.query({ id });
    }

    protected EmptyQuery(): jump_charge_paramsQuery {
        return {};
    }

    ID(entity: JumpChargeParams): number {
        return entity.ID;
    }

    protected Entity(row: jump_charge_paramsRow): JumpChargeParams {
        return new JumpChargeParams(row);
    }
}

export const JumpChargeParamsRegistry = new JumpChargeParamsRegistryClass();
