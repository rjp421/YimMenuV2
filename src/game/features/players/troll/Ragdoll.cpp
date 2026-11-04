#include "game/commands/PlayerCommand.hpp"
#include "game/pointers/Pointers.hpp"

namespace YimMenu::Features
{
	class Ragdoll : public PlayerCommand
	{
		using PlayerCommand::PlayerCommand;

		virtual void OnCall(Player player) override
		{
			if (auto ped = player.GetPed(); ped && ped.IsNetworked() && Pointers.RequestRagdoll)
				Pointers.RequestRagdoll(ped.GetNetworkObjectId());
		}
	};

	static Ragdoll _Ragdoll{"ragdoll", "Ragdoll", "Knocks the player over"};
}
