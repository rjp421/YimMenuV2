#include "core/commands/LoopedCommand.hpp"
#include "game/backend/Players.hpp"
#include "game/gta/Natives.hpp"

namespace YimMenu::Features
{
	class Spectate : public LoopedCommand
	{
		using LoopedCommand::LoopedCommand;

		Player m_SpectatingPlayer{nullptr};

		// looks only at the pointer: the old player object can be cleared or reused once they are gone
		bool IsSpectatedPlayerGone()
		{
			if (!m_SpectatingPlayer.GetHandle())
				return false;

			for (auto& [idx, player] : Players::GetPlayers())
				if (player == m_SpectatingPlayer)
					return false;

			return true;
		}

		virtual void OnTick() override
		{
			// turn off rather than move on to whoever gets selected next
			if (IsSpectatedPlayerGone())
			{
				SetState(false);
				return;
			}

			auto selected = Players::GetSelected();
			if (selected != m_SpectatingPlayer)
			{
				NETWORK::NETWORK_SET_IN_SPECTATOR_MODE(false, 0);
				HUD::SET_MINIMAP_IN_SPECTATOR_MODE(false, 0);

				m_SpectatingPlayer = selected;
			}

			if (m_SpectatingPlayer)
			{
				NETWORK::NETWORK_SET_IN_SPECTATOR_MODE(true, m_SpectatingPlayer.GetPed().GetHandle());
				HUD::SET_MINIMAP_IN_SPECTATOR_MODE(true, m_SpectatingPlayer.GetPed().GetHandle());
			}
		}

		virtual void OnDisable() override
		{
			NETWORK::NETWORK_SET_IN_SPECTATOR_MODE(false, 0);
			HUD::SET_MINIMAP_IN_SPECTATOR_MODE(false, 0);
			m_SpectatingPlayer = nullptr;
		}
	};

	static Spectate _Spectate{"spectate", "Spectate", "Spectates the selected player"};
}