#include "Players.hpp"
#include "core/backend/ScriptMgr.hpp"
#include "core/scripting/LuaManager.hpp"
#include "game/backend/SavedPlayers.hpp"
#include "game/backend/Self.hpp"
#include "game/gta/Natives.hpp"
#include "game/pointers/Pointers.hpp"
#include "types/network/CNetGamePlayer.hpp"
#include "types/network/CNetworkPlayerMgr.hpp"
#include "types/network/rlGamerInfo.hpp"

namespace YimMenu
{
	void Players::InitImpl()
	{
		for (uint8_t idx = 0; idx < 32u; idx++)
		{
			if (const auto netPlayer = Pointers.GetNetPlayerFromPid(idx);
			    netPlayer && netPlayer->IsPhysical() && netPlayer == (*Pointers.NetworkPlayerMgr)->m_Players[idx])
			{
				OnPlayerJoin(netPlayer);
			}
		}

		LuaManager::DispatchEvent(MenuEvent::PlayerMgrInit, [](lua_State*){
			return 0;
		});
	}

	void Players::ShutdownImpl()
	{
		m_SelectedPlayer = nullptr;
		m_Players.clear();
		m_PlayerDatas.clear();

		LuaManager::DispatchEvent(MenuEvent::PlayerMgrShutdown, [](lua_State*){
			return 0;
		});
	}

	// the player list and Info page are drawn on the render thread, where natives on other players' peds race with
	// the game thread and can crash, so what they need is read here on the game thread
	void Players::RunScriptImpl()
	{
		std::chrono::steady_clock::time_point last_tags{};
		while (g_Running)
		{
			if (std::chrono::steady_clock::now() - last_tags > 500ms)
			{
				last_tags = std::chrono::steady_clock::now();
				RemoveDroppedPlayers();
				auto players = m_Players;
				for (auto& [idx, player] : players)
				{
					auto ped = player.GetPed();
					// the game makes players invincible inside interiors
					player.GetData().m_IsGodmode = ped && ped.IsInvincible() && INTERIOR::GET_INTERIOR_FROM_ENTITY(ped.GetHandle()) == 0;
				}
			}

			if (auto selected = m_SelectedPlayer; selected.IsValid())
			{
				auto& data = selected.GetData();
				auto ped = selected.GetPed();
				data.m_HasPed = static_cast<bool>(ped);
				if (ped)
				{
					data.m_Health = ped.GetHealth();
					data.m_MaxHealth = ped.GetMaxHealth();
					data.m_Position = ped.GetPosition();
					data.m_Distance = Self::GetPed() ? data.m_Position.GetDistance(Self::GetPed().GetPosition()) : 0.0f;
				}
			}

			ScriptMgr::Yield();
		}
	}

	void Players::OnPlayerJoinImpl(CNetGamePlayer* player)
	{
		auto new_player = Player(player);

		m_Players[player->m_PlayerIndex] = new_player;
		m_PlayerDatas[player->m_PlayerIndex] = PlayerData();
		m_PlayerDatas[player->m_PlayerIndex].m_Name = player->GetName();
		SavedPlayers::OnPlayerJoin(new_player);

		LuaManager::DispatchEvent(MenuEvent::PlayerJoin, [player](lua_State* state){
			lua_pushinteger(state, player->m_PlayerIndex);
			lua_pushstring(state, player->GetName());
			return 2;
		});
	}

	// the game can drop a player who got a slot during a join without the leave that OnPlayerLeave gets. Entries the
	// player manager no longer has in their slot (twice in a row, so a join in progress isn't caught) are removed as if they left
	void Players::RemoveDroppedPlayers()
	{
		static std::array<int, 32> misses{};
		for (auto it = m_Players.begin(); it != m_Players.end();)
		{
			auto idx = it->first;
			auto& player = it->second;
			bool dropped = idx < 32 && (!player.IsValid() || (*Pointers.NetworkPlayerMgr)->m_Players[idx] != player.GetHandle());
			if (idx < 32)
				misses[idx] = dropped ? misses[idx] + 1 : 0;
			if (!dropped || misses[idx] < 2)
			{
				++it;
				continue;
			}

			misses[idx] = 0;
			std::string name = m_PlayerDatas.contains(idx) ? m_PlayerDatas[idx].m_Name : std::string("unknown");
			LOGF(WARNING, "Removed {} (slot {}) from the player list: the game dropped them without a leave (a join that didn't finish)", name, idx);
			if (m_SelectedPlayer == player)
				m_SelectedPlayer = nullptr;
			m_PlayerDatas.erase(idx);
			it = m_Players.erase(it);

			LuaManager::DispatchEvent(MenuEvent::PlayerLeave, [name](lua_State* state) {
				lua_pushstring(state, name.c_str());
				return 1;
			});
		}
	}

	void Players::OnPlayerLeaveImpl(CNetGamePlayer* player)
	{
		if (m_SelectedPlayer == player)
			m_SelectedPlayer = nullptr;
		m_Players.erase(player->m_PlayerIndex);
		m_PlayerDatas.erase(player->m_PlayerIndex);

		LuaManager::DispatchEvent(MenuEvent::PlayerLeave, [player](lua_State* state){
			lua_pushstring(state, player->GetName());
			return 1;
		});
	}

	Player Players::GetByRIDImpl(uint64_t rid)
	{
		for (auto& [idx, player] : Players::GetPlayers())
		{
			if (player.GetGamerInfo()->m_GamerHandle.m_RockstarId == rid)
			{
				return player;
			}
		}

		return nullptr;
	}

	Player Players::GetByHostTokenImpl(uint64_t token)
	{
		for (auto& [idx, player] : Players::GetPlayers())
		{
			if (player.GetGamerInfo()->m_HostToken == token)
			{
				return player;
			}
		}

		return nullptr;
	}

	Player Players::GetByMessageIdImpl(int id)
	{
		for (auto& [idx, player] : Players::GetPlayers())
		{
			if (player.GetHandle()->m_MessageId == id)
			{
				return player;
			}
		}

		return nullptr;
	}
}