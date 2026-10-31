#include "core/commands/Command.hpp"
#include "game/backend/Self.hpp"
#include "game/gta/Natives.hpp"
#include "types/blip/BlipSprite.hpp"

#include <algorithm>
#include <initializer_list>

namespace YimMenu::Features
{
	// player blips use the plain RADAR_LEVEL circle too
	static bool IsPlayerBlip(Blip blip)
	{
		auto entity = HUD::GET_BLIP_INFO_ID_ENTITY_INDEX(blip);
		return entity && ENTITY::IS_ENTITY_A_PED(entity) && PED::IS_PED_A_PLAYER(entity);
	}

	// closest non-player blip of this sprite, optionally filtered by color
	static bool GetBlipLocationOfType(Vector3& location, int sprite, std::initializer_list<int> colors = {}, std::initializer_list<int> excluded_colors = {})
	{
		rage::fvector3 self_pos = Self::GetPed().GetPosition();
		float closest = -1.0f;

		for (Blip blip = HUD::GET_FIRST_BLIP_INFO_ID(sprite); HUD::DOES_BLIP_EXIST(blip); blip = HUD::GET_NEXT_BLIP_INFO_ID(sprite))
		{
			int color = HUD::GET_BLIP_COLOUR(blip);
			if (colors.size() && std::find(colors.begin(), colors.end(), color) == colors.end())
				continue;

			if (std::find(excluded_colors.begin(), excluded_colors.end(), color) != excluded_colors.end())
				continue;

			if (IsPlayerBlip(blip))
				continue;

			Vector3 coords = HUD::GET_BLIP_COORDS(blip);
			float distance = rage::fvector3(coords).GetDistance(self_pos);
			if (closest < 0.0f || distance < closest)
			{
				closest = distance;
				location = coords;
			}
		}

		if (closest < 0.0f)
			return false;

		location.z += 1.0f;
		return true;
	}

	class TpToObjective : public Command
	{
		using Command::Command;

		static constexpr int BLIP_COLOR_NONE = 0, BLIP_COLOR_RED = 1, BLIP_COLOR_GREEN = 2, BLIP_COLOR_BLUE = 3, BLIP_COLOR_YELLOW_MISSION = 5, BLIP_COLOR_ENEMY = 49, BLIP_COLOR_RED_MISSION = 59, BLIP_COLOR_YELLOW_MISSION_2 = 60, BLIP_COLOR_MISSION = 66;

		virtual void OnCall() override
		{
			Vector3 location;

			// plain circles mark objectives, players and enemies alike, so try the objective colors first
			if (GetBlipLocationOfType(location, static_cast<int>(BlipSprite::RADAR_LEVEL), {BLIP_COLOR_YELLOW_MISSION, BLIP_COLOR_YELLOW_MISSION_2, BLIP_COLOR_MISSION})
			    || GetBlipLocationOfType(location, static_cast<int>(BlipSprite::RADAR_RACEFLAG), {BLIP_COLOR_NONE})
			    || GetBlipLocationOfType(location, static_cast<int>(BlipSprite::RADAR_LEVEL), {BLIP_COLOR_GREEN, BLIP_COLOR_BLUE}))
			{
				Self::GetPed().TeleportTo(location);
				return;
			}

			static constexpr BlipSprite sprites[] = {BlipSprite::RADAR_HIGHER, BlipSprite::RADAR_LOWER, BlipSprite::RADAR_OBJECTIVE_BLUE, BlipSprite::RADAR_OBJECTIVE_GREEN, BlipSprite::RADAR_OBJECTIVE_RED, BlipSprite::RADAR_OBJECTIVE_YELLOW, BlipSprite::RADAR_CONTRABAND, BlipSprite::RADAR_TARGET_A, BlipSprite::RADAR_TARGET_B, BlipSprite::RADAR_TARGET_C, BlipSprite::RADAR_TARGET_D, BlipSprite::RADAR_TARGET_E, BlipSprite::RADAR_TARGET_F, BlipSprite::RADAR_TARGET_G, BlipSprite::RADAR_TARGET_H, BlipSprite::RADAR_PICKUP_MACHINEGUN};
			for (const auto sprite : sprites)
			{
				if (GetBlipLocationOfType(location, static_cast<int>(sprite)))
				{
					Self::GetPed().TeleportTo(location);
					return;
				}
			}

			// last resort: any other plain circle that isn't an enemy
			if (GetBlipLocationOfType(location, static_cast<int>(BlipSprite::RADAR_LEVEL), {}, {BLIP_COLOR_RED, BLIP_COLOR_ENEMY, BLIP_COLOR_RED_MISSION}))
				Self::GetPed().TeleportTo(location);
		}
	};

	static TpToObjective _TpToObjective{"tptoobjective", "Teleport to Objective", "Teleports you to the Objective"};
}
