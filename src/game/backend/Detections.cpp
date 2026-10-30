#include "Detections.hpp"
#include "core/backend/FiberPool.hpp"
#include "core/commands/BoolCommand.hpp"
#include "core/frontend/Notifications.hpp"
#include "game/backend/Players.hpp"
#include "game/backend/SavedPlayers.hpp"
#include "game/backend/Self.hpp"
#include "game/gta/Natives.hpp"
#include "types/script/globals/GlobalPlayerBD.hpp"

namespace YimMenu
{
	static BoolCommand _ModderDetection{"modderdetection", "Modder Detection", "Marks players who send what the normal game can't, like crash and kick attacks", true};

	static BoolCommand _TrustFriends{"modderdetectiontrustfriends", "Trust Friends", "Never marks your friends as modders", true};

	// reactions
	static BoolCommand _SaveModders{"reactionsavemodders", "Save Modders", "Adds marked modders to Saved Players, so they stay marked", true};
	static BoolCommand _HostKickAttackers{"reactionhostkick", "Host Kick Attackers", "As host, kicks players who try to crash or kick you or break the session", true};
	static BoolCommand _BlockJoinAttackers{"reactionblockjoin", "Block Join Attackers", "Turns on Block Join for players who try to crash or kick you or break the session. Needs Save Modders", true};

	static constexpr int MODDER_SCORE = 3;
	static constexpr auto REPEAT_WINDOW = std::chrono::seconds(10); // repeats within this time are one occurrence (attacks come in bursts)

	struct DetectionEntry
	{
		DetectionLevel m_Level = DetectionLevel::None;
		int m_Count = 0;
		std::chrono::steady_clock::time_point m_LastCounted{};
	};

	struct PlayerDetections
	{
		std::string m_Name;
		int m_Score = 0;
		bool m_Flagged = false;
		std::map<std::string, DetectionEntry, std::less<>> m_Entries;
	};

	static std::mutex g_Mutex;
	static std::unordered_map<std::uint64_t, PlayerDetections> g_Players; // by RID, kept for the whole game session so rejoins add up

	static const char* LevelName(DetectionLevel level)
	{
		switch (level)
		{
		case DetectionLevel::Weak: return "weak";
		case DetectionLevel::Strong: return "strong";
		case DetectionLevel::Certain: return "certain";
		case DetectionLevel::Modder: return "modder";
		default: return "none";
		}
	}

	static std::vector<std::string> DescribeLocked(const PlayerDetections& player)
	{
		std::vector<std::pair<const std::string*, const DetectionEntry*>> entries;
		for (auto& [reason, entry] : player.m_Entries)
			entries.emplace_back(&reason, &entry);
		std::stable_sort(entries.begin(), entries.end(), [](auto& a, auto& b) {
			return a.second->m_Level > b.second->m_Level;
		});

		std::vector<std::string> result;
		for (auto [reason, entry] : entries)
			result.push_back(std::format("{} ({}, {}x)", *reason, LevelName(entry->m_Level), entry->m_Count));
		return result;
	}

	// Players::GetByRID dereferences the gamer info of every player, which is null for a player who is leaving
	static Player FindPlayer(std::uint64_t rid)
	{
		for (auto& [id, player] : Players::GetPlayers())
			if (player.IsValid() && player.GetRID() == rid)
				return player;
		return nullptr;
	}

	// natives, so only call this in a fiber
	static bool IsTrustedFriend(Player player)
	{
		if (!_TrustFriends.GetState())
			return false;

		std::uint64_t handle[13]{};
		NETWORK::NETWORK_HANDLE_FROM_PLAYER(player.GetId(), handle, std::size(handle));
		return NETWORK::NETWORK_IS_FRIEND(handle);
	}

	// runs in a fiber: host kicks a player that tried to crash or kick us
	static void HostKickAttacker(std::uint64_t rid, std::string name, std::string reason)
	{
		auto player = FindPlayer(rid);
		if (!player)
		{
			LOGF(INFO, "Not kicking {} for {}: not found in the session", name, reason);
			return;
		}
		if (IsTrustedFriend(player))
		{
			LOGF(INFO, "Not kicking {} for {}: they are your friend (Trust Friends)", name, reason);
			return;
		}

		if (!Self::GetPlayer().IsHost())
		{
			LOGF(INFO, "Not kicking {} for {}: we are not the session host", name, reason);
			return;
		}

		static std::unordered_map<std::uint64_t, std::chrono::steady_clock::time_point> last_kicks; // only used in fibers
		auto now = std::chrono::steady_clock::now();
		if (auto it = last_kicks.find(rid); it != last_kicks.end() && now - it->second < std::chrono::minutes(1))
		{
			LOGF(INFO, "Not kicking {} for {}: already kicked less than a minute ago", name, reason);
			return;
		}
		last_kicks[rid] = now;

		LOGF(WARNING, "Host kicking {} (RID {}) for {}", name, rid, reason);
		Notifications::Show("Host Kick", std::format("Kicked {} for {}", name, reason), NotificationType::Warning);
		NETWORK::NETWORK_SESSION_KICK_PLAYER(player.GetId());
	}

	// runs in a fiber: turns on Block Join for a player that tried to crash or kick us (saving them first if needed)
	static void BlockJoinAttacker(std::uint64_t rid, std::string name, std::string reason)
	{
		auto player = FindPlayer(rid);
		if (player && IsTrustedFriend(player))
		{
			LOGF(INFO, "Not blocking {} from joining for {}: they are your friend (Trust Friends)", name, reason);
			return;
		}

		auto data = SavedPlayers::GetPlayerData(rid);
		if (!data)
		{
			SavedPlayers::AddPlayerData(rid, name);
			data = SavedPlayers::GetPlayerData(rid);
		}
		if (!data || data->m_BlockJoin)
			return;

		data->m_BlockJoin = true;
		SavedPlayers::Save();
		LOGF(WARNING, "Turned on Block Join for {} (RID {}) for {}", name, rid, reason);
		Notifications::Show("Block Join", std::format("{} can't join your sessions anymore ({})", name, reason), NotificationType::Warning);
	}

	// runs in a fiber: marks the player in this session and in the saved players
	static void MarkModder(std::uint64_t rid, std::string name, std::vector<std::string> detections, bool newly_flagged)
	{
		auto player = FindPlayer(rid);
		if (player && IsTrustedFriend(player))
		{
			if (newly_flagged)
				LOGF(WARNING, "Not marking {} as a modder because they are your friend (Trust Friends): {}", name, detections.empty() ? "" : detections.front());
			return;
		}

		if (player)
			player.GetData().m_IsModder = true;

		SavedPlayerData* data = nullptr;
		if (_SaveModders.GetState())
		{
			// the Add to Saved button's method; AddPlayerData if they already left
			data = player ? SavedPlayers::GetPlayerData(player, true) : SavedPlayers::GetPlayerData(rid);
			if (!data)
			{
				SavedPlayers::AddPlayerData(rid, name);
				data = SavedPlayers::GetPlayerData(rid);
			}
		}
		if (data)
		{
			data->m_IsModder = true;
			data->m_Detections = detections;
			SavedPlayers::Save();
		}

		if (newly_flagged)
		{
			LOGF(WARNING, "Marked {} (RID {}) as a modder: {}", name, rid, detections.empty() ? "" : detections.front());
			Notifications::Show("Modder Detected", std::format("{}: {}", name, detections.empty() ? "" : detections.front()), NotificationType::Warning);
		}
	}

	void Detections::Add(Player player, DetectionLevel level, std::string_view reason)
	{
		if (level == DetectionLevel::None || !player || !_ModderDetection.GetState())
			return;
		if (player.GetId() == Self::GetPlayer().GetId())
			return;

		auto rid = player.GetRID();
		if (rid == 0)
			return;

		std::string name = player.GetName();
		bool counted = false, newly_flagged = false, new_reason_of_modder = false;
		int score = 0;
		std::vector<std::string> description;
		{
			std::lock_guard lock(g_Mutex);
			auto& data = g_Players[rid];
			data.m_Name = name;

			auto [it, inserted] = data.m_Entries.try_emplace(std::string(reason));
			auto& entry = it->second;
			entry.m_Level = std::max(entry.m_Level, level);
			entry.m_Count++;

			auto now = std::chrono::steady_clock::now();
			if (inserted || now - entry.m_LastCounted > REPEAT_WINDOW)
			{
				entry.m_LastCounted = now;
				data.m_Score += static_cast<int>(level);
				counted = true;

				if (!data.m_Flagged && data.m_Score >= MODDER_SCORE)
					data.m_Flagged = newly_flagged = true;
				else if (data.m_Flagged && inserted)
					new_reason_of_modder = true;
			}

			score = data.m_Score;
			if (newly_flagged || new_reason_of_modder)
				description = DescribeLocked(data);
		}

		if (counted)
			LOGF(WARNING, "Detection for {}: {} ({}, {} of {} points)", name, reason, LevelName(level), score, MODDER_SCORE);

		if (newly_flagged || new_reason_of_modder)
			FiberPool::Push([rid, name, description, newly_flagged] {
				MarkModder(rid, name, description, newly_flagged);
			});

		// certain detections are crash and kick attempts
		if (counted && level == DetectionLevel::Certain && _HostKickAttackers.GetState())
			FiberPool::Push([rid, name, reason = std::string(reason)] {
				HostKickAttacker(rid, name, reason);
			});
		if (counted && level == DetectionLevel::Certain && _BlockJoinAttackers.GetState() && _SaveModders.GetState())
			FiberPool::Push([rid, name, reason = std::string(reason)] {
				BlockJoinAttacker(rid, name, reason);
			});
	}

	void Detections::Note(Player player, std::string_view reason)
	{
		if (!player || !_ModderDetection.GetState() || player.GetId() == Self::GetPlayer().GetId())
			return;

		auto rid = player.GetRID();
		if (rid == 0)
			return;

		std::lock_guard lock(g_Mutex);
		auto& data = g_Players[rid];
		data.m_Name = player.GetName();
		data.m_Entries[std::string(reason)].m_Count++;
	}

	std::vector<std::string> Detections::Describe(std::uint64_t rid)
	{
		std::lock_guard lock(g_Mutex);
		if (auto it = g_Players.find(rid); it != g_Players.end())
			return DescribeLocked(it->second);
		return {};
	}

	// legacy's ATTACKING_WITH_GODMODE with its exceptions. Strong, not certain: the synced godmode flag is coarser than legacy's
	// damage bits and spawn protection lingers after a respawn, so it has to repeat 10+ seconds later
	void Detections::OnEntityDamage(const std::int64_t* data)
	{
		auto victim = static_cast<int>(data[0]); // script entity handles
		auto damager = static_cast<int>(data[1]);
		auto weapon = static_cast<std::uint32_t>(data[6]);

		// placed explosives and mines hurt people after their owner died or respawned
		switch (weapon)
		{
		case "WEAPON_STICKYBOMB"_J:
		case "VEHICLE_WEAPON_MINE_KINETIC_RC"_J:
		case "VEHICLE_WEAPON_MINE_EMP_RC"_J:
		case "VEHICLE_WEAPON_MINE_KINETIC"_J:
		case "VEHICLE_WEAPON_MINE_EMP"_J:
		case "VEHICLE_WEAPON_MINE_SPIKE"_J: return;
		}

		if (!damager || !victim || !ENTITY::IS_ENTITY_A_PED(damager) || !ENTITY::IS_ENTITY_A_PED(victim) || NETWORK::NETWORK_IS_ACTIVITY_SESSION())
			return;

		for (auto& [id, player] : Players::GetPlayers())
		{
			if (!player.IsValid() || player.IsLocal())
				continue;
			auto ped = player.GetPed();
			if (!ped || ped.GetHandle() != damager)
				continue;

			// RC vehicles, submarines and other vehicles the player isn't cloned in, tutorial sessions, interiors, cutscenes
			if (PLAYER::IS_REMOTE_PLAYER_IN_NON_CLONED_VEHICLE(id))
				return;
			if (auto vehicle = PED::GET_VEHICLE_PED_IS_IN(damager, false))
			{
				auto model = ENTITY::GET_ENTITY_MODEL(vehicle);
				if (model == "rcbandito"_J || model == "minitank"_J || model == "kosatka"_J)
					return;
			}
			if (!NETWORK::NETWORK_ARE_PLAYERS_IN_SAME_TUTORIAL_SESSION(Self::GetPlayer().GetId(), id) || INTERIOR::GET_INTERIOR_FROM_ENTITY(damager) != 0
			    || NETWORK::NETWORK_IS_PLAYER_IN_MP_CUTSCENE(id))
				return;

			if (ped.IsInvincible())
				Add(player, DetectionLevel::Strong, "attacking with godmode");

			// legacy's other two exceptions: freemode's own flags for the player (GlobalPlayerBD)
			if (auto gpbd = GlobalPlayerBD::Get())
			{
				auto& entry = gpbd->Entries[id];
				// only hurting players counts, and not with explosions or vehicles (legacy's exceptions)
				if (entry.IsInvisible && PED::IS_PED_A_PLAYER(victim) && weapon != "WEAPON_EXPLOSION"_J && weapon != "WEAPON_RAMMED_BY_CAR"_J
				    && weapon != "WEAPON_RUN_OVER_BY_CAR"_J)
					Add(player, DetectionLevel::Strong, "attacking while invisible");
				if (entry.CayoPericoFlags & 1)
					Add(player, DetectionLevel::Strong, "attacking while hidden from the player list");
			}
			return;
		}
	}
}
