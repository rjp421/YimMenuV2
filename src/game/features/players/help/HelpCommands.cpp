#include "PlayerHelp.hpp"
#include "game/commands/PlayerCommand.hpp"

namespace YimMenu::Features
{
	// one reward event can target several players, so the "all players" versions send each reward once with all their bits
	class GiveHealth : public PlayerCommand
	{
		using PlayerCommand::PlayerCommand;

		virtual void OnCall(Player player) override
		{
			OnCall(std::vector{player});
		}

		virtual void OnCall(const std::vector<Player>& players) override
		{
			PlayerHelp::GiveHealth(PlayerHelp::GetBits(players));
		}
	};

	class GiveArmor : public PlayerCommand
	{
		using PlayerCommand::PlayerCommand;

		virtual void OnCall(Player player) override
		{
			OnCall(std::vector{player});
		}

		virtual void OnCall(const std::vector<Player>& players) override
		{
			PlayerHelp::GiveArmor(PlayerHelp::GetBits(players));
		}
	};

	class GiveAmmo : public PlayerCommand
	{
		using PlayerCommand::PlayerCommand;

		virtual void OnCall(Player player) override
		{
			OnCall(std::vector{player});
		}

		virtual void OnCall(const std::vector<Player>& players) override
		{
			PlayerHelp::GiveAmmo(PlayerHelp::GetBits(players));
		}
	};

	class FixPlayerVehicle : public PlayerCommand
	{
		using PlayerCommand::PlayerCommand;

		virtual void OnCall(Player player) override
		{
			OnCall(std::vector{player});
		}

		virtual void OnCall(const std::vector<Player>& players) override
		{
			PlayerHelp::FixVehicle(PlayerHelp::GetBits(players));
		}
	};

	class ClearPlayerWanted : public PlayerCommand
	{
		using PlayerCommand::PlayerCommand;

		virtual void OnCall(Player player) override
		{
			OnCall(std::vector{player});
		}

		virtual void OnCall(const std::vector<Player>& players) override
		{
			PlayerHelp::ClearWanted(PlayerHelp::GetBits(players));
		}
	};

	class GivePlayerOffRadar : public PlayerCommand
	{
		using PlayerCommand::PlayerCommand;

		virtual void OnCall(Player player) override
		{
			OnCall(std::vector{player});
		}

		virtual void OnCall(const std::vector<Player>& players) override
		{
			PlayerHelp::GiveOffRadar(PlayerHelp::GetBits(players));
		}
	};

	static ClearPlayerWanted _ClearPlayerWanted{"clearplayerwanted", "Clear Wanted", "Clears their wanted level"};
	static GivePlayerOffRadar _GivePlayerOffRadar{"giveplayerotr", "Give Off The Radar", "Hides them from the radar for a while"};
	static GiveHealth _GiveHealth{"givehealth", "Give Health", "Refills their health"};
	static GiveArmor _GiveArmor{"givearmor", "Give Armor", "Gives them full armor"};
	static GiveAmmo _GiveAmmo{"giveammo", "Give Ammo", "Gives them ammo for every weapon type"};
	static FixPlayerVehicle _FixPlayerVehicle{"fixplayerveh", "Fix Vehicle", "Repairs their vehicle"};
}
