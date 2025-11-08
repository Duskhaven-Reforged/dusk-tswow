import { mediumint } from "../../../data/primitives";
import { talent_dependenceRow } from "../../sql/talent_dependence";
import { SQL } from "../../SQLFiles";
import { MaybeSQLEntity } from "../Misc/SQLDBCEntity";
import { Spell } from "./Spell";

export class TalentDependency extends MaybeSQLEntity<Spell,talent_dependenceRow> {
    protected createSQL(): talent_dependenceRow {
        return SQL.talent_dependence.add(this.owner.row.ID.get(), {})
            .dependency.set(0)
    }
    protected findSQL(): talent_dependenceRow {
        return SQL.talent_dependence.query({spell: this.owner.row.ID.get()});
    }
    protected isValidSQL(sql: talent_dependenceRow): boolean {
        return sql.spell.get() === this.owner.row.ID.get();
    }

    set (dependency: mediumint) {
        this.wrapSQL(0, (sql=> sql.dependency)).set(dependency)
    }
}