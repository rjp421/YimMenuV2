#include "core/hooking/DetourHook.hpp"
#include "game/backend/SyncProtections.hpp"
#include "game/hooks/Hooks.hpp"

namespace YimMenu::Hooks
{
	// called by the clone create and sync handlers before the received sync tree is applied to the object
	bool Network::CanApplyData(rage::netSyncTree* tree, rage::netObject* object)
	{
		if (SyncProtections::ShouldReject(tree, object))
			return false;

		return BaseHook::Get<Network::CanApplyData, DetourHook<decltype(&Network::CanApplyData)>>()->Original()(tree, object);
	}
}
