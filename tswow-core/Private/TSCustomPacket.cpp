#include "TSCustomPacket.h"
#include "TSPlayer.h"
#include "TSEvents.h"
#include "WorldPacket.h"
#include "CustomPacketChunk.h"
#include "Player.h"

#include "TSMap.h"
#include "Map.h"
#include "TSBattleground.h"

TSPacketWrite::TSPacketWrite(std::shared_ptr<CustomPacketWrite>&& write)
	: write(std::move(write))
{}

TSPacketRead::TSPacketRead(CustomPacketRead* read)
	: read(read)
{}

void TSPacketWrite::SendToPlayer(TSPlayer player)
{
	auto & arr = write->buildMessages();
	for (auto & chunk : arr)
	{
		WorldPacket packet(SERVER_TO_CLIENT_OPCODE, chunk.FullSize());
		packet.append((uint8_t*)chunk.Data(), chunk.FullSize());
		player.player->SendDirectMessage(&packet);
	}
	// remove this line if we start sending a raw pointer to worldpacket
	write->Destroy();
}

void TSPacketWrite::SendToNotInWorld(uint32 accountID)
{

	if (WorldSession* session = sWorld->FindSession(accountID))
	{
		auto& arr = write->buildMessages();
		for (auto& chunk : arr)
		{
			WorldPacket packet(SERVER_TO_CLIENT_OPCODE, chunk.FullSize());
			packet.append((uint8_t*)chunk.Data(), chunk.FullSize());
			session->SendPacket(&packet);
		}
		// remove this line if we start sending a raw pointer to worldpacket
		write->Destroy();
	}
}

void TSPacketWrite::BroadcastMap(TSMap map, uint32_t teamOnly)
{
	auto& arr = write->buildMessages();
	for (auto& chunk : arr)
	{
		WorldPacket packet(SERVER_TO_CLIENT_OPCODE, chunk.FullSize());
		packet.append((uint8_t*)chunk.Data(), chunk.FullSize());
		for (auto const& ref : map.map->GetPlayers())
		{
			Player* player = ref.GetSource();
#if TRINITY
			if (teamOnly == 0 || player->GetTeam() == teamOnly)
#endif
			{
				player->SendDirectMessage(&packet);
			}
		}
	}
	// remove this line if we start sending a raw pointer to worldpacket
	write->Destroy();
}

void TSPacketWrite::BroadcastAround(TSWorldObject obj, float range, bool self)
{
	auto& arr = write->buildMessages();
	for (auto& chunk : arr)
	{
		WorldPacket packet(SERVER_TO_CLIENT_OPCODE, chunk.FullSize());
		packet.append((uint8_t*)chunk.Data(), chunk.FullSize());
		obj.obj->SendMessageToSetInRange(&packet, range, self);
	}
	// remove this line if we start sending a raw pointer to worldpacket
	write->Destroy();
}

TSServerBuffer::TSServerBuffer(TSPlayer player)
	: CustomPacketBuffer(
		  MIN_FRAGMENT_SIZE
		, BUFFER_QUOTA
		, MAX_FRAGMENT_SIZE
	)
	, m_player(player)
{
}

TSServerBuffer::TSServerBuffer(uint32 accountID)
	: CustomPacketBuffer(
		MIN_FRAGMENT_SIZE
		, BUFFER_QUOTA
		, MAX_FRAGMENT_SIZE
	)
	, account_id(accountID)
{
}

opcode_t TSServerBuffer::GetOpcode()
{
    CustomPacketRead* value = getCur();
    TSPacketRead read(value);
    opcode_t opcode =  value->Opcode();
    return opcode;
}

void TSServerBuffer::ClearPacket()
{
    clearPacket();
}

void TSServerBuffer::OnPacket(CustomPacketRead* value)
{
	// Expanded FIRE_ID macro because we need to reset the packet
	// reading head between every invocation.
	// Please do not change this to some auto-resetting macro abuse,
	// it would NOT be guaranteed to work in the long term.

	TSPacketRead read(value);
	opcode_t opcode = value->Opcode();

	if (m_player)
	{
        auto& cbs = ts_events.CustomPacket.OnReceive_callbacks;
		for (auto const& cb : cbs.m_cxx_callbacks)
		{
			cb(opcode, read, m_player);
		}

		for (auto const& cb : cbs.m_lua_callbacks)
		{
			cb(opcode, read, m_player);
			value->Reset();
		}

		if (opcode < cbs.m_id_cxx_callbacks.size())
		{
			for (auto const& cb : cbs.m_id_cxx_callbacks[opcode])
			{
				cb(opcode, read, m_player);
				value->Reset();
			}
		}

		if (opcode < cbs.m_id_lua_callbacks.size())
		{
			for (auto const& cb : cbs.m_id_lua_callbacks[opcode])
			{
				cb(opcode, read, m_player);
				value->Reset();
			}
		}
	}else {
        auto& cbs2 = ts_events.CustomPacket.OnReceiveNotInWorld_callbacks;
		for (auto const& cb : cbs2.m_cxx_callbacks)
		{
			cb(opcode, read, account_id);
		}

		for (auto const& cb : cbs2.m_lua_callbacks)
		{
			cb(opcode, read, account_id);
			value->Reset();
		}

		if (opcode < cbs2.m_id_cxx_callbacks.size())
		{
			for (auto const& cb : cbs2.m_id_cxx_callbacks[opcode])
			{
				cb(opcode, read, account_id);
				value->Reset();
			}
		}

		if (opcode < cbs2.m_id_lua_callbacks.size())
		{
			for (auto const& cb : cbs2.m_id_lua_callbacks[opcode])
			{
				cb(opcode, read, account_id);
				value->Reset();
			}
		}
	}
}

void TSServerBuffer::OnError(CustomPacketResult error)
{
	m_player.player->GetSession()->KickPlayer("Custom packet error: "+std::to_string(uint32_t(error)));
}

TSPacketWrite CreateCustomPacket(
		opcode_t opcode
	, totalSize_t size
)
{
	return TSPacketWrite(std::make_shared<CustomPacketWrite>(opcode, MAX_FRAGMENT_SIZE, size));
}
