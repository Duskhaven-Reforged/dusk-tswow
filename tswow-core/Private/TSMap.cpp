/*
 * Copyright (C) 2020 tswow <https://github.com/tswow/>
 * Copyright (C) 2010 - 2016 Eluna Lua Engine <http://emudevs.com/>
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
#include "TSIncludes.h"
#include "TSMap.h"
#include "TSPlayer.h"
#include "TSWorldObject.h"
#include "TSGameObject.h"
#include "TSUnit.h"
#include "TSCreature.h"
#include "TSBattleground.h"
#include "TSInstance.h"
#include "TSGUID.h"
#include "TSWeather.h"

#include "ObjectMgr.h"
#include "CreatureData.h"
#include "ObjectGuid.h"
#include "TemporarySummon.h"
#include "Object.h"
#include "Map.h"
#include "Weather.h"
#include "Corpse.h"
#include "DynamicObject.h"
#include "Pet.h"
#include "WeatherMgr.h"
#include "MapReference.h"
#include "Player.h"

#include <memory.h>

TSMap::TSMap(Map *mapIn)
    : TSEntityProvider(&mapIn->m_tsEntity)
    , TSWorldEntityProvider(&mapIn->m_tsWorldEntity)
    , map(mapIn)
{
}

#ifndef CLASSIC
/**
 * Returns `true` if the [Map] is an arena [BattleGround], `false` otherwise.
 *
 * @return bool isArena
 */
bool TSMap::IsArena()
{
    return map->IsBattleArena();
}
#endif

/**
 * Returns `true` if the [Map] is a non-arena [BattleGround], `false` otherwise.
 *
 * @return bool isBattleGround
 */
bool TSMap::IsBG()
{
#if defined TRINITY
    return map->IsBattleground();
#else
    return map->IsBattleGround();
#endif
}

TSBattleground TSMap::ToBG()
{
    return TSBattleground(map, map->ToBattlegroundMap()->GetBG());
}

/**
 * Returns `true` if the [Map] is a dungeon, `false` otherwise.
 *
 * @return bool isDungeon
 */
bool TSMap::IsDungeon()
{
    return map->IsDungeon();
}

/**
 * Returns `true` if the [Map] has no [Player]s, `false` otherwise.
 *
 * @return bool isEmpty
 */
bool TSMap::IsEmpty()
{
#if TRINITY
    return map->isEmpty();
#endif
}

#ifndef CLASSIC
/**
 * Returns `true` if the [Map] is a heroic, `false` otherwise.
 *
 * @return bool isHeroic
 */
bool TSMap::IsHeroic()
{
    return map->IsHeroic();
}
#endif

/**
 * Returns `true` if the [Map] is a raid, `false` otherwise.
 *
 * @return bool isRaid
 */
bool TSMap::IsRaid()
{
    return map->IsRaid();
}

/**
 * Returns the name of the [Map].
 *
 * @return string mapName
 */
std::string TSMap::GetName()
{
     return map->GetMapName();
}

/**
 * Returns the height of the [Map] at the given X and Y coordinates.
 *
 * In case of no height found nil is returned
 *
 * @param float x
 * @param float y
 * @return float z
 */
TSNumber<float> TSMap::GetHeight(float x,float y,uint32 phasemask)
{
#if (defined(TBC) || defined(CLASSIC))
    return map->GetHeight(x, y, MAX_HEIGHT);
#else
    return map->GetHeight(phasemask, x, y, MAX_HEIGHT);
#endif
}

/**
 * Returns the difficulty of the [Map].
 *
 * Always returns 0 if the expansion is pre-TBC.
 *
 * @return int32 difficulty
 */
TSNumber<int32> TSMap::GetDifficulty()
{
#ifndef CLASSIC
    return map->GetDifficulty();
#else
    return (Difficulty)0;
#endif
}

/**
 * Returns the instance ID of the [Map].
 *
 * @return uint32 instanceId
 */
TSNumber<uint32> TSMap::GetInstanceID()
{
    return map->GetInstanceId();
}

/**
 * Returns the player count currently on the [Map] (excluding GMs).
 *
 * @return uint32 playerCount
 */
TSNumber<uint32> TSMap::GetPlayerCount()
{
    return map->GetPlayersCountExceptGMs();
}

/**
 * Returns the ID of the [Map].
 *
 * @return uint32 mapId
 */
TSNumber<uint32> TSMap::GetMapID()
{
    return map->GetId();
}

/**
 * Returns the area ID of the [Map] at the specified X, Y, and Z coordinates.
 *
 * @param float x
 * @param float y
 * @param float z
 * @param uint32 phasemask = PHASEMASK_NORMAL
 * @return uint32 areaId
 */
TSNumber<uint32> TSMap::GetAreaID(float x,float y,float z,float phasemask)
{
    return map->GetAreaId(phasemask, x, y, z);
}

/**
 * Returns a [WorldObject] by its GUID from the map if it is spawned.
 *
 * @param uint64 guid
 */
TSWorldObject TSMap::GetWorldObject(TSGUID guid)
{

#if defined TRINITY
    switch (GUID_HIPART(guid.asGUID()))
    {
        case HIGHGUID_PLAYER:
            return TSWorldObject(eObjectAccessor()GetPlayer(map, guid.asGUID()));
            break;
        case HIGHGUID_TRANSPORT:
        case HIGHGUID_MO_TRANSPORT:
        case HIGHGUID_GAMEOBJECT:
             return TSWorldObject(map->GetGameObject(guid.asGUID()));
            break;
        case HIGHGUID_VEHICLE:
        case HIGHGUID_UNIT:
             return TSWorldObject(map->GetCreature(guid.asGUID()));
            break;
        case HIGHGUID_PET:
             return TSWorldObject(map->GetPet(guid.asGUID()));
            break;
        case HIGHGUID_DYNAMICOBJECT:
             return TSWorldObject(map->GetDynamicObject(guid.asGUID()));
            break;
        case HIGHGUID_CORPSE:
             return TSWorldObject(map->GetCorpse(guid.asGUID()));
            break;
        default:
            return TSWorldObject(nullptr);
            break;
    }
#else
     return TSWorldObject(map->GetWorldObject(ObjectGuid(guid)));
#endif
}

/**
 * Sets the [Weather] type based on [WeatherType] and grade supplied.
 *
 *     enum WeatherType
 *     {
 *         WEATHER_TYPE_FINE       = 0,
 *         WEATHER_TYPE_RAIN       = 1,
 *         WEATHER_TYPE_SNOW       = 2,
 *         WEATHER_TYPE_STORM      = 3,
 *         WEATHER_TYPE_THUNDERS   = 86,
 *         WEATHER_TYPE_BLACKRAIN  = 90
 *
 * @param uint32 zone : id of the zone to set the weather for
 * @param [WeatherType] type : the [WeatherType], see above available weather types
 * @param float grade : the intensity/grade of the [Weather], ranges from 0 to 1
 */
void TSMap::SetWeather(uint32 zoneId,uint32 weatherType,float grade)
{
    (void)map; // ensure that the variable is referenced in order to pass compiler checks

#if TRINITY
    if (Weather * weather = map->GetOrGenerateZoneDefaultWeather(zoneId))
        weather->SetWeather((WeatherType)weatherType, grade);
#else
    if (Weather::IsValidWeatherType(weatherType))
        map->SetWeather(zoneId, (WeatherType)weatherType, grade, false);
#endif
}

/**
 * Returns [Weather] for a zone or generates one if none exists.
 *
 * @param uint32 zoneId : The ID of the zone.
 * @return [Weather] The weather object of the zone
 */
TSWeather TSMap::GetOrGenerateZoneDefaultWeather(uint32 zoneId)
{
    Weather* weather = map->GetOrGenerateZoneDefaultWeather(zoneId);
    if (weather)
        return TSWeather(weather);
    else
        return TSWeather(nullptr);
}

/**
* Returns a table with all the current [Player]s in the map
*
*     enum TeamId
*     {
*         TEAM_ALLIANCE = 0,
*         TEAM_HORDE = 1,
*         TEAM_NEUTRAL = 2
*
* @param [TeamId] team : optional check team of the [Player], Alliance, Horde or Neutral (All)
* @return table mapPlayers
*/
TSArray<TSPlayer> TSMap::GetPlayers(uint32 team)
{

    TSArray<TSPlayer> tbl;

    Map::PlayerList const& players = map->GetPlayers();
    tbl.vec->reserve(players.getSize());
    for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
    {
#if defined TRINITY
        Player* player = itr->GetSource();
#else
        Player* player = itr->getSource();
#endif
        if (!player)
            continue;

        if (player->GetSession() && (team >= TEAM_NEUTRAL || player->GetTeamId() == team))
        {
            tbl.push(TSPlayer(player));
        }
    }

    return tbl;
}

TSEntity * TSMap::GetData()
{
    return &map->m_tsEntity;
}

TSArray<TSUnit> TSMap::GetUnits()
{
    TSArray<TSUnit> units;
    auto const& players = map->GetPlayers();
    auto const& creatures = map->GetCreatureBySpawnIdStore();
    units.vec->reserve(players.getSize() + creatures.size());
    for (auto& player : players)
    {
        units.push(TSUnit(player.GetSource()));
    }
    for (auto& creature : creatures)
    {
        units.push(TSUnit(creature.second));
    }
    return units;
}

TSArray<TSGameObject> TSMap::GetGameObjects(uint32 entry)
{
    TSArray<TSGameObject> gameobjects;
    if (entry == 0)
    {
        gameobjects.vec->reserve(map->GetGameObjectBySpawnIdStore().size());
    }
    for (auto& val : map->GetGameObjectBySpawnIdStore())
    {
        if (entry == 0 || val.second->GetEntry() == entry)
        {
            gameobjects.push(TSGameObject(val.second));
        }
    }
    return gameobjects;
}

TSArray<TSCreature> TSMap::GetCreatures(uint32 entry)
{
    TSArray<TSCreature> creatures;
    if (entry == 0)
    {
        creatures.vec->reserve(map->GetCreatureBySpawnIdStore().size());
    }
    for (auto& val : map->GetCreatureBySpawnIdStore())
    {
        if (entry == 0 || val.second->GetEntry() == entry)
        {
            creatures.push(TSCreature(val.second));
        }
    }
    return creatures;
}

TSCreature TSMap::GetCreatureByDBGUID(uint32 dbGuid)
{
#if TRINITY
    return TSCreature(map->GetCreatureBySpawnId(dbGuid));
#endif
}

TSGameObject TSMap::GetGameObjectByDBGUID(uint32 dbGuid)
{
#if TRINITY
    return TSGameObject(map->GetGameObjectBySpawnId(dbGuid));
#endif
}

TSCreature TSMap::SpawnCreature(uint32 entry, float x, float y, float z, float o, uint32 despawnTimer, uint32 phase)
{
    return TSCreature(map->SummonCreature(entry, Position(x, y, z, o), nullptr, despawnTimer));
}

TSGameObject TSMap::SpawnGameObject(uint32 entry, float x, float y, float z, float o, uint32 despawnTimer, uint32 phase)
{
    const GameObjectTemplate* objectInfo = eObjectMgr->GetGameObjectTemplate(entry);
    GameObject* object = new GameObject;
    uint32 guidLow = map->GenerateLowGuid<HighGuid::GameObject>();
#if TRINITY
    QuaternionData rot = QuaternionData::fromEulerAnglesZYX(o, 0.f, 0.f);
    if (!object->Create(guidLow, objectInfo->entry, map, phase, Position(x, y, z, o), rot, 0, GO_STATE_READY))
#endif
    {
        delete object;
        return TSGameObject(nullptr);
    }
    if (despawnTimer)
        object->SetRespawnTime(despawnTimer);
    map->AddToMap(object);
    return TSGameObject(object);
}

bool TSMap::IsInstance()
{
    if (InstanceMap * inst = map->ToInstanceMap())
    {
        return inst->GetInstanceScript() != nullptr;
    }
    return false;
}

TSInstance TSMap::ToInstance()
{
    if (InstanceMap* inst = map->ToInstanceMap())
    {
        return TSInstance(map,inst->GetInstanceScript());
    }
    return TSInstance(nullptr,nullptr);
}

void TSMap::DoDelayed(std::function<void(TSMap, TSMainThreadContext)> callback)
{
#if TRINITY
    map->m_delayCallbacks.push_back(callback);
#endif
}

TSCreature TSMap::GetCreature(TSNumber<uint32> guid)
{
    return GetCreature(TSGUID(guid));
}

TSGameObject TSMap::GetGameObject(TSNumber<uint32> guid)
{
    return GetGameObject(TSGUID(guid));
}

TSPlayer TSMap::GetPlayer(TSNumber<uint32> guid)
{
    return GetPlayer(TSGUID(guid));
}

TSCreature TSMap::GetCreature(TSGUID guid)
{
    return TSCreature(map->GetCreature(guid.asGUID()));
}

TSGameObject TSMap::GetGameObject(TSGUID guid)
{
    return TSGameObject(map->GetGameObject(guid.asGUID()));
}

TSPlayer TSMap::GetPlayer(TSGUID guid)
{
    return TSPlayer(ObjectAccessor::GetPlayer(map, guid.asGUID()));
}

bool TSMap::IsInLineOfSight(float x1,float y1,float z1, float x2, float y2, float z2, uint32 phasemask, uint32 checks, uint32 ignoreFlags)
{
    return map->isInLineOfSight( x1,  y1,  z1,  x2,  y2,  z2,  phasemask, static_cast<LineOfSightChecks>(checks), static_cast<VMAP::ModelIgnoreFlags>(ignoreFlags));
}

void TSMap::LDoDelayed(sol::function callback)
{
#if TRINITY
    map->m_delayLuaCallbacks.push_back(callback);
#endif
}

TSLua::Array<TSPlayer> TSMap::LGetPlayers0(uint32 team)
{
    return sol::as_table(*GetPlayers(team).vec);
}
TSLua::Array<TSPlayer> TSMap::LGetPlayers1()
{
    return sol::as_table(*GetPlayers().vec);
}

TSLua::Array<TSUnit> TSMap::LGetUnits()
{
    return sol::as_table(*GetUnits().vec);
}

TSLua::Array<TSGameObject> TSMap::LGetGameObjects0(uint32 entry)
{
    return sol::as_table(*GetGameObjects(entry).vec);
}
TSLua::Array<TSGameObject> TSMap::LGetGameObjects1()
{
    return sol::as_table(*GetGameObjects().vec);
}

TSLua::Array<TSCreature> TSMap::LGetCreatures0(uint32 entry)
{
    return sol::as_table(*GetCreatures(entry).vec);
}
TSLua::Array<TSCreature> TSMap::LGetCreatures1()
{
    return sol::as_table(*GetCreatures().vec);
}


TSPlayer TSMap::LGetPlayer0(TSGUID guid)
{
    return GetPlayer(guid);
}

TSPlayer TSMap::LGetPlayer1(TSNumber<uint32> guid)
{
    return GetPlayer(guid);
}

TSCreature TSMap::LGetCreature0(TSGUID guid)
{
    return GetCreature(guid);
}

TSCreature TSMap::LGetCreature1(TSNumber<uint32> guid)
{
    return GetCreature(guid);
}

TSGameObject TSMap::LGetGameObject0(TSGUID guid)
{
    return GetGameObject(guid);
}

TSGameObject TSMap::LGetGameObject1(TSNumber<uint32> guid)
{
    return GetGameObject(guid);
}

void TSMap::RespawnAll(uint32 type) {
    return map->RespawnAll(SpawnObjectTypeMask(type));
}
TC_GAME_API TSInstance ToInstance(TSMap map)
{
    if (map.map && map->IsInstance())
    {
        InstanceMap* instance = map.map->ToInstanceMap();
        return TSInstance(instance, instance->GetInstanceScript());
    }
    else
    {
        return TSInstance(nullptr,nullptr);
    }
}

TC_GAME_API TSBattleground ToBattleground(TSMap map)
{
    if (map.map && map->IsBG())
    {
        BattlegroundMap* bg = map.map->ToBattlegroundMap();
        return TSBattleground(bg, bg->GetBG());
    }
    else
    {
        return TSBattleground(nullptr, nullptr);
    }
}
