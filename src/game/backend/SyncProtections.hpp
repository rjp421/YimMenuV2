#pragma once
#include "game/gta/Player.hpp"

namespace rage
{
	class netSyncTree;
	class netObject;
}

namespace YimMenu
{
	// Checks the data of incoming clone creates and syncs before the game applies it (port of legacy YimMenu's can_apply_data checks)
	class SyncProtections
	{
	public:
		// Set by the clone create/sync hooks while the game handles the message, so the checks know who sent the data.
		// CanApplyData is also called for things that don't come from the network; those are not checked.
		class SenderScope
		{
		public:
			SenderScope(Player sender);
			~SenderScope();

		private:
			Player m_Previous;
		};

		// returns true if the game must not apply the data
		static bool ShouldReject(rage::netSyncTree* tree, rage::netObject* object);

		// the sender of the clone create/sync being handled on this thread (empty outside of one)
		static Player GetCurrentSender();
	};
}
