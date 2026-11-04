#include "PlayerHelp.hpp"
#include "core/backend/ScriptMgr.hpp"
#include "core/commands/BoolCommand.hpp"
#include "game/backend/PlayerData.hpp"
#include "game/backend/Players.hpp"
#include "game/gta/Natives.hpp"
#include "game/gta/data/Weapons.hpp"
#include "game/pointers/Pointers.hpp"
#include "types/script/ScriptEvent.hpp"
#include "types/script/globals/GPBD_FM_3.hpp"

namespace YimMenu::Features
{
	// legacy's ammo pickup rewards, without the vehicle and mission variants
	// clang-format off
	static constexpr joaat_t g_AmmoRewards[] = {
	    "REWARD_AMMO_ADVANCEDRIFLE"_J, "REWARD_AMMO_APPISTOL"_J, "REWARD_AMMO_ASSAULTRIFLE"_J, "REWARD_AMMO_ASSAULTSHOTGUN"_J,
	    "REWARD_AMMO_ASSAULTSMG"_J, "REWARD_AMMO_AUTOSHOTGUN"_J, "REWARD_AMMO_BULLPUPRIFLE"_J, "REWARD_AMMO_BULLPUPSHOTGUN"_J,
	    "REWARD_AMMO_CARBINERIFLE"_J, "REWARD_AMMO_COMBATMG"_J, "REWARD_AMMO_COMBATPDW"_J, "REWARD_AMMO_COMBATPISTOL"_J,
	    "REWARD_AMMO_COMPACTLAUNCHER"_J, "REWARD_AMMO_COMPACTRIFLE"_J, "REWARD_AMMO_DBSHOTGUN"_J, "REWARD_AMMO_DOUBLEACTION"_J,
	    "REWARD_AMMO_FIREWORK"_J, "REWARD_AMMO_FLAREGUN"_J, "REWARD_AMMO_GRENADE"_J, "REWARD_AMMO_GRENADELAUNCHER"_J,
	    "REWARD_AMMO_GUSENBERG"_J, "REWARD_AMMO_HEAVYPISTOL"_J, "REWARD_AMMO_HEAVYSHOTGUN"_J, "REWARD_AMMO_HEAVYSNIPER"_J,
	    "REWARD_AMMO_HOMINGLAUNCHER"_J, "REWARD_AMMO_MACHINEPISTOL"_J, "REWARD_AMMO_MARKSMANPISTOL"_J, "REWARD_AMMO_MARKSMANRIFLE"_J,
	    "REWARD_AMMO_MG"_J, "REWARD_AMMO_MICROSMG"_J, "REWARD_AMMO_MINIGUN"_J, "REWARD_AMMO_MINISMG"_J, "REWARD_AMMO_MOLOTOV"_J,
	    "REWARD_AMMO_MUSKET"_J, "REWARD_AMMO_PETROLCAN"_J, "REWARD_AMMO_PIPEBOMB"_J, "REWARD_AMMO_PISTOL"_J, "REWARD_AMMO_PISTOL50"_J,
	    "REWARD_AMMO_PROXMINE"_J, "REWARD_AMMO_PUMPSHOTGUN"_J, "REWARD_AMMO_RAILGUN"_J, "REWARD_AMMO_RAYPISTOL"_J,
	    "REWARD_AMMO_REVOLVER"_J, "REWARD_AMMO_RPG"_J, "REWARD_AMMO_SAWNOFFSHOTGUN"_J, "REWARD_AMMO_SMG"_J,
	    "REWARD_AMMO_SMOKEGRENADE"_J, "REWARD_AMMO_SNIPERRIFLE"_J, "REWARD_AMMO_SNSPISTOL"_J, "REWARD_AMMO_SPECIALCARBINE"_J,
	    "REWARD_AMMO_STICKYBOMB"_J, "REWARD_AMMO_STUNGUN"_J, "REWARD_AMMO_VINTAGEPISTOL"_J,
	};
	// clang-format on

	void PlayerHelp::GiveReward(int player_bits, std::uint32_t reward)
	{
		if (Pointers.GivePickupRewards && player_bits)
			Pointers.GivePickupRewards(player_bits, reward);
	}

	// each reward is a network event, so space them out like legacy does
	void PlayerHelp::GiveHealth(int player_bits)
	{
		// legacy gives it twice
		GiveReward(player_bits, "REWARD_HEALTH"_J);
		ScriptMgr::Yield(20ms);
		GiveReward(player_bits, "REWARD_HEALTH"_J);
	}

	void PlayerHelp::GiveArmor(int player_bits)
	{
		GiveReward(player_bits, "REWARD_ARMOUR"_J);
	}

	void PlayerHelp::GiveAmmo(int player_bits)
	{
		for (auto reward : g_AmmoRewards)
		{
			GiveReward(player_bits, reward);
			ScriptMgr::Yield(20ms);
		}
	}

	void PlayerHelp::FixVehicle(int player_bits)
	{
		GiveReward(player_bits, "REWARD_VEHICLE_FIX"_J);
	}

	void PlayerHelp::GiveWeapons(Player player)
	{
		if (!player.IsValid())
			return;

		for (auto weapon : g_WeaponHashes)
			player.GetPed().GiveWeapon(weapon);
	}

	// both events carry the receiver's own replay counter, so they are sent to one player at a time
	static int ReplayCounter(int id)
	{
		auto fm3 = GPBD_FM_3::Get();
		return fm3 ? fm3->Entries[id].ScriptEventReplayProtectionCounter : 0;
	}

	void PlayerHelp::ClearWanted(int player_bits)
	{
		for (int id = 0; id < 32; id++)
		{
			if (!(player_bits & (1 << id)))
				continue;
			SCRIPT_EVENT_CLEAR_WANTED_LEVEL event;
			event.ReplayProtectionValue = ReplayCounter(id);
			event.Send(id);
		}
	}

	void PlayerHelp::GiveOffRadar(int player_bits)
	{
		for (int id = 0; id < 32; id++)
		{
			if (!(player_bits & (1 << id)))
				continue;
			SCRIPT_EVENT_REMOTE_OFF_RADAR event;
			event.StartTime = NETWORK::GET_NETWORK_TIME() + 1;
			event.Enable = true;
			event.ReplayProtectionValue = ReplayCounter(id);
			event.Send(id);
		}
	}

	int PlayerHelp::GetBits(const std::vector<Player>& players)
	{
		int bits = 0;
		for (auto player : players)
			if (player.IsValid())
				bits |= 1 << player.GetId();
		return bits;
	}

	static bool IsVehicleDamaged(int vehicle)
	{
		return VEHICLE::GET_VEHICLE_ENGINE_HEALTH(vehicle) < 1000.0f || VEHICLE::GET_VEHICLE_BODY_HEALTH(vehicle) < 1000.0f
		    || VEHICLE::GET_VEHICLE_PETROL_TANK_HEALTH(vehicle) < 1000.0f;
	}

	// the same help as the per-player toggles, for everyone in the session except us
	static BoolCommand _SemiGodmodeAll{"semigodmodeall", "Semi Godmode (All)", "Refills everyone's health and armor when they drop. A big hit can still kill them"};
	static BoolCommand _AutoFixVehicleAll{"autofixvehicleall", "Auto Fix Vehicles (All)", "Repairs everyone's vehicle when it is damaged"};
	static BoolCommand _NeverWantedAll{"neverwantedall", "Never Wanted (All)", "Clears everyone's wanted level when they get one"};
	static BoolCommand _OffRadarAll{"offradarall", "Off The Radar (All)", "Keeps everyone off the radar. Some menus flag this as griefing"};

	// sends a reward only when it is needed, at most every 250 ms
	void PlayerHelp::RunScript()
	{
		while (g_Running)
		{
			// one reward event can target several players, so a whole session needs at most one of each per pass
			int healthBits = 0, armorBits = 0, fixBits = 0;
			// script events go to one player at a time, so a whole session is spread over several passes
			int scriptEventsLeft = 4;

			for (auto& [id, player] : Players::GetPlayers())
			{
				if (!player.IsValid() || player.IsLocal())
					continue;

				auto& data = player.GetData();
				const bool semiGodmode = data.m_SemiGodmode || _SemiGodmodeAll.GetState();
				const bool autoFixVehicle = data.m_AutoFixVehicle || _AutoFixVehicleAll.GetState();
				const bool neverWanted = data.m_NeverWanted || _NeverWantedAll.GetState();
				const bool offRadar = data.m_OffRadar || _OffRadarAll.GetState();
				if (!semiGodmode && !autoFixVehicle && !neverWanted && !offRadar)
					continue;

				// script events: at most one per second (never wanted) / 20 seconds (off the radar, which lasts longer) per player
				auto now = std::chrono::steady_clock::now();
				if (neverWanted && scriptEventsLeft > 0 && player.GetWantedLevel() > 0 && now - data.m_LastClearWanted > 1s)
				{
					data.m_LastClearWanted = now;
					ClearWanted(1 << player.GetId());
					scriptEventsLeft--;
				}
				if (offRadar && scriptEventsLeft > 0 && now - data.m_LastOffRadar > 20s)
				{
					data.m_LastOffRadar = now;
					GiveOffRadar(1 << player.GetId());
					scriptEventsLeft--;
				}

				auto ped = player.GetPed();
				if (!ped || ped.IsDead())
					continue;

				int bits = 1 << player.GetId();
				if (semiGodmode && ped.GetMaxHealth() > 0)
				{
					if (ped.GetHealth() < ped.GetMaxHealth())
						healthBits |= bits;
					if (ped.GetArmour() < 50)
						armorBits |= bits;
				}

				if (autoFixVehicle)
				{
					auto vehicle = ped.GetVehicle();
					if (vehicle && IsVehicleDamaged(vehicle.GetHandle()))
						fixBits |= bits;
				}
			}

			GiveReward(healthBits, "REWARD_HEALTH"_J);
			GiveReward(armorBits, "REWARD_ARMOUR"_J);
			GiveReward(fixBits, "REWARD_VEHICLE_FIX"_J);

			ScriptMgr::Yield(250ms);
		}
	}
}
