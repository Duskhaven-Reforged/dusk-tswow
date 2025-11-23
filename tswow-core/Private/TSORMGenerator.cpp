/*
 * Copyright (C) 2021 tswow <https://github.com/tswow/>
 *
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

#include "TSORMGenerator.h"
#include "Log.h"

#include <iterator>
#include <algorithm>
#include <string>
#include <stdexcept>

static std::shared_ptr<TSDatabaseResult> query(DatabaseType m_type, std::string const& value)
{
    switch(m_type)
    {
        case DatabaseType::AUTH:
            return QueryAuth(value);
        case DatabaseType::CHARACTERS:
            return QueryCharacters(value);
        case DatabaseType::WORLD:
            return QueryWorld(value);
        default: throw std::out_of_range("DatabaseSpec::m_type");
    }
}

std::string toLower(std::string const& str)
{
    std::string out = str;
    std::transform(
          out.begin()
        , out.end()
        , out.begin()
        , [](unsigned char c) { return std::tolower(c); }
    );
    return out;
}

std::string normalizeTypeName(std::string const& typeName)
{
    std::string normalized = toLower(typeName);
    // Normalize whitespace: collapse multiple spaces to single space
    std::string result;
    bool lastWasSpace = false;
    for (char c : normalized)
    {
        if (c == ' ' || c == '\t')
        {
            if (!lastWasSpace)
            {
                result += ' ';
                lastWasSpace = true;
            }
        }
        else
        {
            result += c;
            lastWasSpace = false;
        }
    }
    // Trim leading/trailing spaces
    if (!result.empty() && result.front() == ' ')
        result.erase(0, 1);
    if (!result.empty() && result.back() == ' ')
        result.erase(result.length() - 1, 1);
    
    // Remove display width from numeric types (e.g., "bigint(20)" -> "bigint", "int(10)" -> "int")
    // MySQL's information_schema doesn't include display widths, so we need to normalize them out
    size_t parenPos = result.find('(');
    if (parenPos != std::string::npos)
    {
        // Check if this is a numeric type with display width
        std::string baseType = result.substr(0, parenPos);
        // Common numeric types that have display widths
        if (baseType == "tinyint" || baseType == "smallint" || baseType == "mediumint" ||
            baseType == "int" || baseType == "integer" || baseType == "bigint" ||
            baseType == "float" || baseType == "double" || baseType == "decimal" ||
            baseType == "numeric")
        {
            // Find the closing parenthesis
            size_t closeParen = result.find(')', parenPos);
            if (closeParen != std::string::npos)
            {
                // Remove the (number) part, but keep everything after it (like "unsigned")
                std::string afterParen = result.substr(closeParen + 1);
                result = baseType;
                if (!afterParen.empty())
                {
                    // Add space before "unsigned" or other modifiers if needed
                    if (afterParen[0] != ' ')
                        result += ' ';
                    result += afterParen;
                }
            }
        }
    }
    
    // Normalize whitespace again after removing display width
    std::string finalResult;
    lastWasSpace = false;
    for (char c : result)
    {
        if (c == ' ' || c == '\t')
        {
            if (!lastWasSpace)
            {
                finalResult += ' ';
                lastWasSpace = true;
            }
        }
        else
        {
            finalResult += c;
            lastWasSpace = false;
        }
    }
    // Trim leading/trailing spaces
    if (!finalResult.empty() && finalResult.front() == ' ')
        finalResult.erase(0, 1);
    if (!finalResult.empty() && finalResult.back() == ' ')
        finalResult.erase(finalResult.length() - 1, 1);
    
    return finalResult;
}

void CreateDatabaseSpec(uint32 type, std::string const& m_dbName, std::string const& m_name, std::vector<FieldSpec> m_fields)
{
    DatabaseType m_type = (DatabaseType)(type);
    std::vector<FieldSpec> effectiveFields = m_fields;
    // Normalize all field names and types for consistent comparison
    for (FieldSpec& spec : effectiveFields)
    {
        spec.m_name = toLower(spec.m_name);
        spec.m_typeName = normalizeTypeName(spec.m_typeName);
    }

    auto oldTableQuery = query(m_type,
        "SELECT COUNT(TABLE_NAME) as TableCount"
        " FROM `information_schema`.`TABLES`"
        " WHERE `TABLE_NAME` = \""
        + m_name +
        "\" and `TABLE_SCHEMA` = \""
        + m_dbName +
        "\";"
    );
    oldTableQuery->GetRow();
    bool hasOldTable = oldTableQuery->GetUInt32(0);
    if (hasOldTable)
    {
        // 1. build old fields
        std::vector<FieldSpec> oldFields;
        auto oldQuery = query(m_type,
            "SELECT `COLUMN_NAME`,`COLUMN_TYPE`,`COLUMN_KEY`,`EXTRA`"
            " FROM `information_schema`.`COLUMNS` "
            " WHERE `TABLE_SCHEMA` = \""
            + m_dbName +
            "\" AND `TABLE_NAME`= \""
            + m_name +
            "\";"
        );
        while (oldQuery->GetRow())
        {
            // these are always lowercase in my installation,
            // but they might not be if we upgrade at some point.
            // we don't want that to break our script
            oldFields.push_back({
                  toLower(oldQuery->GetString(0))
                , normalizeTypeName(oldQuery->GetString(1))
                , toLower(oldQuery->GetString(2)) == "pri"
                , toLower(oldQuery->GetString(3)) == "auto_increment"
                });
        }

        // 2. Find if pks changed
        {
            // Get effective primary keys in order (preserved from class definition)
            // effectiveFields is already normalized, so we can use them directly
            std::vector<FieldSpec> effPk;
            for (FieldSpec const& spec : effectiveFields)
            {
                if (spec.m_isPrimaryKey)
                {
                    effPk.push_back(spec);
                    TS_LOG_INFO(
                          "tswow.orm"
                        , "Effective PK: name='{}', type='{}', autoInc={}"
                        , spec.m_name.c_str()
                        , spec.m_typeName.c_str()
                        , spec.m_autoIncrements
                    );
                }
            }

            // Get old primary keys in correct order from KEY_COLUMN_USAGE
            std::vector<std::pair<std::string, uint32>> oldPkOrdered; // name, ordinal_position
            auto oldPkQuery = query(m_type,
                "SELECT `COLUMN_NAME`, `ORDINAL_POSITION`"
                " FROM `information_schema`.`KEY_COLUMN_USAGE`"
                " WHERE `TABLE_SCHEMA` = \"" + m_dbName + "\""
                " AND `TABLE_NAME` = \"" + m_name + "\""
                " AND `CONSTRAINT_NAME` = 'PRIMARY'"
                " ORDER BY `ORDINAL_POSITION` ASC;"
            );
            while (oldPkQuery->GetRow())
            {
                oldPkOrdered.push_back({
                    toLower(oldPkQuery->GetString(0)),
                    oldPkQuery->GetUInt32(1)
                });
            }

            // Build oldPk vector with FieldSpecs in correct order
            std::vector<FieldSpec> oldPk;
            for (auto const& pkPair : oldPkOrdered)
            {
                TS_LOG_INFO(
                      "tswow.orm"
                    , "Looking for old PK: name='{}', ordinal={}"
                    , pkPair.first.c_str()
                    , pkPair.second
                );
                auto itr = std::find_if(
                    oldFields.begin()
                    , oldFields.end()
                    , [&](FieldSpec const& old) {
                        return old.m_name == pkPair.first;
                    });
                if (itr != oldFields.end())
                {
                    oldPk.push_back(*itr);
                    TS_LOG_INFO(
                          "tswow.orm"
                        , "Found old PK: name='{}', type='{}', autoInc={}"
                        , itr->m_name.c_str()
                        , itr->m_typeName.c_str()
                        , itr->m_autoIncrements
                    );
                }
                else
                {
                    // Primary key column not found in oldFields - this shouldn't happen
                    TS_LOG_INFO(
                          "tswow.orm"
                        , "Primary key column not found in table columns: {}.{}.{}"
                        , m_dbName.c_str()
                        , m_name.c_str()
                        , pkPair.first.c_str()
                    );
                }
            }

            // If we couldn't find all primary key columns, treat as changed
            bool pkChanged = false;
            if (oldPkOrdered.size() != oldPk.size())
            {
                TS_LOG_INFO(
                      "tswow.orm"
                    , "Primary key column mismatch: {}.{} (found {} of {} columns)"
                    , m_dbName.c_str()
                    , m_name.c_str()
                    , oldPk.size()
                    , oldPkOrdered.size()
                );
                pkChanged = true;
            }
            else if (effPk.size() != oldPk.size())
            {
                TS_LOG_INFO(
                      "tswow.orm"
                    , "Primary key count changed: {}.{}"
                    , m_dbName.c_str()
                    , m_name.c_str()
                );

                pkChanged = true;
            }
            else if (effPk.size() == 0)
            {
                // No primary keys in effective fields - this shouldn't happen but handle it
                TS_LOG_INFO(
                      "tswow.orm"
                    , "No primary keys found in effective fields for {}.{}"
                    , m_dbName.c_str()
                    , m_name.c_str()
                );
                pkChanged = true;
            }
            else
            {
                // Compare both names and order
                TS_LOG_INFO(
                      "tswow.orm"
                    , "Starting primary key comparison: {} effective PKs vs {} old PKs"
                    , effPk.size()
                    , oldPk.size()
                );
                for (size_t i = 0; i < effPk.size(); ++i)
                {
                    FieldSpec const& eff = effPk[i];
                    FieldSpec const& old = oldPk[i];

                    TS_LOG_INFO(
                          "tswow.orm"
                        , "Comparing PK[{}]: eff.name='{}' vs old.name='{}', eff.type='{}' vs old.type='{}'"
                        , i
                        , eff.m_name.c_str()
                        , old.m_name.c_str()
                        , eff.m_typeName.c_str()
                        , old.m_typeName.c_str()
                    );

                    if (eff.m_name != old.m_name)
                    {
                        TS_LOG_INFO(
                              "tswow.orm"
                            , "Primary key order or name changed: {}.{}.{} (expected: {}, got: {})"
                            , m_dbName.c_str()
                            , m_name.c_str()
                            , eff.m_name.c_str()
                            , eff.m_name.c_str()
                            , old.m_name.c_str()
                        );
                        pkChanged = true;
                        break;
                    }
                    else if (eff.m_typeName != old.m_typeName || eff.m_autoIncrements != old.m_autoIncrements)
                    {
                        TS_LOG_INFO(
                              "tswow.orm"
                            , "Primary key type changed: {}.{}.{} (eff: '{}' != old: '{}') or autoIncrement (eff: {} != old: {})"
                            , m_dbName.c_str()
                            , m_name.c_str()
                            , eff.m_name.c_str()
                            , eff.m_typeName.c_str()
                            , old.m_typeName.c_str()
                            , eff.m_autoIncrements
                            , old.m_autoIncrements
                        );
                        pkChanged = true;
                        break;
                    }
                }
            }
            if (pkChanged)
            {
                TS_LOG_INFO(
                      "tswow.orm"
                    , "Primary keys changed: {}.{} (must rebuild entire table)"
                    , m_dbName.c_str()
                    , m_name.c_str()
                );
                query(m_type,
                    "DROP TABLE IF EXISTS `"
                    + m_dbName +
                    "`.`"
                    + m_name +
                    "`;"
                );
                goto create;
            }
        }

        // 3. Update old fields
        for (FieldSpec const& old : oldFields)
        {
            if (old.m_autoIncrements) continue; // already checked
            auto itr = std::find_if(
                effectiveFields.begin()
                , effectiveFields.end()
                , [&](FieldSpec const& eff) {
                    return eff.m_name == old.m_name;
                });
            // remove old column
            if (itr == effectiveFields.end())
            {
                TS_LOG_INFO(
                      "tswow.orm"
                    , "Column removed: {}.{}.{}"
                    , m_dbName.c_str()
                    , m_name.c_str()
                    , old.m_name.c_str()
                );
                query(m_type,
                    "ALTER TABLE `"
                    + m_dbName +
                    "`.`"
                    + m_name +
                    "` DROP COLUMN `"
                    + old.m_name
                    + "`;"
                );
            }
            else if (itr->m_typeName != old.m_typeName)
            {
                // update column type
                TS_LOG_INFO(
                    "tswow.orm"
                    , "Column type changed: {}.{}.{} ({} -> {})"
                    , m_dbName.c_str()
                    , m_name.c_str()
                    , old.m_name.c_str()
                    , old.m_typeName.c_str()
                    , itr->m_typeName.c_str()
                );
                query(m_type,
                    "ALTER TABLE `"
                    + m_dbName +
                    "`.`"
                    + m_name +
                    "` MODIFY COLUMN `"
                    + itr->m_name +
                    "` "
                    + itr->m_typeName +
                    ";"
                );
            }
        }

        // 4. Add new fields
        for (FieldSpec const& eff : effectiveFields)
        {
            auto itr = std::find_if(
                oldFields.begin()
                , oldFields.end()
                , [&](FieldSpec const& old) {
                    return eff.m_name == old.m_name;
                });

            if (itr == oldFields.end())
            {
                TS_LOG_INFO(
                    "tswow.orm"
                    , "Column added: {}.{}.{}"
                    , m_dbName.c_str()
                    , m_name.c_str()
                    , eff.m_name.c_str()
                );
                query(m_type,
                    "ALTER TABLE `"
                    + m_dbName +
                    "`.`"
                    + m_name +
                    "` ADD COLUMN `"
                    + eff.m_name +
                    "` "
                    + eff.m_typeName +
                    ";"
                );
            }
        }

        // We *always* reorder to match the memory layout
        // (in case someone starts running manual * queries)
        for (size_t i = 0; i < effectiveFields.size(); ++i)
        {
            auto eff = effectiveFields[i];
            query(m_type,
                "ALTER TABLE `"
                + m_dbName +
                "`.`"
                + m_name +
                "` MODIFY `"
                + eff.m_name +
                "` "
                + eff.m_typeName +
                (eff.m_autoIncrements ? " AUTO_INCREMENT" : "") +
                " "
                + (i == 0 ? "FIRST" : "AFTER " + effectiveFields[i - 1].m_name)
            );
        }
    }
    else
    {
    create:
        std::string createQuery =
            "CREATE TABLE `"
            + m_dbName +
            "`.`"
            + m_name +
            "` ("
            ;

        bool hasPrimaryKeys = false;
        for (int i = 0; i < effectiveFields.size(); ++i)
        {
            FieldSpec& f = effectiveFields[i];
            createQuery +=
                " `"
                + f.m_name +
                "` "
                + f.m_typeName
                + (f.m_autoIncrements ? " AUTO_INCREMENT" : "")
                ;

            if (effectiveFields[i].m_isPrimaryKey) {
                hasPrimaryKeys = true;
            }

            if (i < effectiveFields.size() - 1)
            {
                createQuery += ",";
            }
        }

        if (hasPrimaryKeys)
        {
            createQuery += ", PRIMARY KEY (";
            bool fst = true;
            for (FieldSpec const& field : effectiveFields)
            {
                if (field.m_isPrimaryKey)
                {
                    if (!fst)
                    {
                        createQuery += ",";
                    }
                    createQuery += "`" + field.m_name + "`";
                    TS_LOG_INFO(
                          "tswow.orm"
                        , "Creating PRIMARY KEY with field: {} (order in effectiveFields)"
                        , field.m_name.c_str()
                    );
                    fst = false;
                }
            }
            createQuery += " )";
        }
        createQuery += ");";
        TS_LOG_INFO(
              "tswow.orm"
            , "Table created: {}.{}"
            , m_dbName.c_str()
            , m_name.c_str()
        );
        query(m_type,createQuery);
    }
}

void LCreateDatabaseSpec(uint32 type, std::string const& dbName, std::string const& name, sol::table fields)
{
    std::vector<FieldSpec> vFields;
    for (auto& [_, value] : fields)
    {
        auto col = value.as<sol::table>();
        std::string name = col[1].get<std::string>();
        std::string def = col[2].get<std::string>();
        bool isPk = col[3].get<bool>();
        bool autoIncrement = col[4].get<bool>();
        vFields.push_back(FieldSpec{ name,def,isPk,autoIncrement });
    }
    CreateDatabaseSpec(type, dbName, name, vFields);
}
