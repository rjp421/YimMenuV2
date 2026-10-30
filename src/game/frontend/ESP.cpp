#include "ESP.hpp"
#include "game/gta/Object.hpp"
#include "common.hpp"
#include "core/commands/BoolCommand.hpp"
#include "core/backend/ScriptMgr.hpp"
#include "core/commands/ColorCommand.hpp"
#include "core/frontend/widgets/imgui_colors.h"
#include "core/util/Math.hpp"
#include "game/backend/Players.hpp"
#include "game/backend/Self.hpp"
#include "game/pointers/Pointers.hpp"
#include "game/gta/Pools.hpp"
#include "game/gta/Scripts.hpp"
#include "game/gta/invoker/Invoker.hpp"
#include "game/gta/Natives.hpp"

namespace
{
	// Human
	constexpr int headBone = 37193; //FB_Brow_Centre_000 is a much better bone to use as head
	constexpr int neckBone = 39317;
	constexpr int torsoBone = 23553;
	constexpr int leftHandBone = 18905;
	constexpr int rightHandBone = 57005;
	constexpr int leftFootBone = 14201;
	constexpr int rightFootBone = 52301;
	constexpr int leftElbowBone = 22711;
	constexpr int rightElbowBone = 2992;
	constexpr int leftKneeBone = 46078;
	constexpr int rightKneeBone = 16335;
	constexpr int leftShoulderBone = 61163; // TODO verify all the bones
	constexpr int rightShoulderBone = 28252;
}

namespace YimMenu::Features
{
	// Players
	BoolCommand _ESPDrawPlayers("espdrawplayers", "Draw Players", "Should the ESP draw players?");
	BoolCommand _ESPDrawDeadPlayers("espdrawdeadplayers", "Draw Dead Players", "Should the ESP draw dead players?");

	BoolCommand _ESPName("espnameplayers", "Show Player Name", "Should the ESP draw player names?");
	BoolCommand _ESPTags("esptagsplayers", "Show Player Tags", "Should the ESP draw player tags?");
	BoolCommand _ESPDistance("espdistanceplayers", "Show Player Distance", "Should the ESP draw player distance?");
	BoolCommand _ESPHealth("esphealthplayers", "Show Player Health", "Should the ESP draw player health?");
	BoolCommand _ESPArmor("esparmorplayers", "Show Player Armor", "Should the ESP draw player armor?");
	BoolCommand _ESPSkeleton("espskeletonplayers", "Show Player Skeleton", "Should the ESP draw player skeletons?");

	ColorCommand _NameColorPlayers("namecolorplayers", "Player Name Color", "Changes the color of the name ESP for players", ImVec4{1.0f, 1.0f, 1.0f, 1.0f});
	ColorCommand _DistanceColorPlayers("distancecolorplayers", "Player Distance Color", "Changes the color of the distance ESP for players", ImVec4{1.0f, 1.0f, 1.0f, 1.0f});
	ColorCommand _SkeletonColorPlayers("skeletoncolorplayers", "Player Skeleton Color", "Changes the color of the skeleton ESP for players", ImVec4{1.0f, 1.0f, 1.0f, 1.0f});

	// Peds
	BoolCommand _ESPDrawPeds("espdrawpeds", "Draw Peds", "Should the ESP draw peds?");
	BoolCommand _ESPDrawDeadPeds("espdrawdeadpeds", "Draw Dead Peds", "Should the ESP draw dead peds?");

	BoolCommand _ESPModelPeds("espmodelspeds", "Show Ped Model", "Should the ESP draw ped models?");
	BoolCommand _ESPNetworkInfoPeds("espnetinfopeds", "Show Ped Network Info", "Should the ESP draw network info?");
	BoolCommand _ESPScriptInfoPeds("espscriptinfopeds", "Show Ped Script Info", "Should the ESP draw script info?");
	BoolCommand _ESPDistancePeds("espdistancepeds", "Show Ped Distance", "Should the ESP draw distance?");
	BoolCommand _ESPSkeletonPeds("espskeletonpeds", "Show Ped Skeleton", "Should the ESP draw the skeleton?");

	ColorCommand _HashColorPeds("hashcolorpeds", "Ped Hash Color", "Changes the color of the hash ESP for peds", ImVec4{1.0f, 1.0f, 1.0f, 1.0f});
	ColorCommand _SkeletonColorPeds("skeletoncolorpeds", "Ped Skeleton Color", "Changes the color of the skeleton ESP for peds", ImVec4{1.0f, 1.0f, 1.0f, 1.0f});

	// Objects
	BoolCommand _ESPDrawObjects("espdrawobjects", "Draw Special Objects", "Should the ESP draw special objects?");
	BoolCommand _ESPNetworkInfoObjects("espnetinfoobjects", "Show Object Network Info", "Should the ESP draw network info?");
	BoolCommand _ESPScriptInfoObjects("espscriptinfoobjects", "Show Object Script Info", "Should the ESP draw script info?");
	BoolCommand _ESPDistanceObjects("espdistanceobjects", "Show Object Distance", "Should the ESP draw distance?");

	ColorCommand _HashColorObjects("hashcolorobjects", "Object Hash Color", "Changes the color of the hash ESP for objects", ImVec4{1.0f, 1.0f, 1.0f, 1.0f});
}

namespace YimMenu
{
	static ImVec4 death_bg = ImVec4(0.117f, 0.113f, 0.172f, .75f);
	static ImVec4 armor_blue_bg = ImVec4(0.36f, 0.71f, 0.89f, .75f);
	static ImVec4 armor_blue = ImVec4(0.36f, 0.71f, 0.89f, 1.f);
	static ImVec4 health_green_bg = ImVec4(0.29f, 0.69f, 0.34f, .75f);
	static ImVec4 health_green = ImVec4(0.29f, 0.69f, 0.34f, 1.f);
	static ImVec4 health_yellow_bg = ImVec4(0.69f, 0.49f, 0.29f, .75f);
	static ImVec4 health_yellow = ImVec4(0.69f, 0.49f, 0.29f, 1.f);
	static ImVec4 health_red_bg = ImVec4(0.69f, 0.29f, 0.29f, .75f);
	static ImVec4 health_red = ImVec4(0.69f, 0.29f, 0.29f, 1.f);
	static ImVec4 Green = ImVec4(0.29f, 0.69f, 0.34f, 1.f);
	static ImVec4 Orange = ImVec4(0.69f, 0.49f, 0.29f, 1.f);
	static ImVec4 Red = ImVec4(0.69f, 0.29f, 0.29f, 1.f);
	static ImVec4 Blue = ImVec4(0.36f, 0.71f, 0.89f, 1.f);
	static ImVec4 White = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

	// entities are read on the game thread (ESP::RunScript), which records what to draw at which world position; the render
	// thread (ESP::Draw) only projects those positions with the current frame's camera and draws them
	struct EspLine
	{
		rage::fvector3 a, b;
		ImU32 color;
	};
	struct EspHead // circle around the head, its size from the head to neck distance on screen
	{
		rage::fvector3 head, neck;
		ImU32 color;
	};
	struct EspText
	{
		rage::fvector3 anchor;
		ImVec2 offset;                                    // pixels from the anchor's screen position
		std::vector<std::pair<std::string, ImU32>> parts; // drawn one after another on one line
	};
	struct EspBar
	{
		rage::fvector3 anchor;
		ImVec2 offset;
		float fraction;
		ImU32 background, foreground;
	};
	struct EspFrame
	{
		std::vector<EspLine> lines;
		std::vector<EspHead> heads;
		std::vector<EspText> texts;
		std::vector<EspBar> bars;
		std::chrono::steady_clock::time_point time{};

		void Clear()
		{
			lines.clear();
			heads.clear();
			texts.clear();
			bars.clear();
		}

		bool Empty() const
		{
			return lines.empty() && heads.empty() && texts.empty() && bars.empty();
		}
	};
	static EspFrame g_Building; // game thread only
	static EspFrame g_Ready;    // guarded by g_ReadyMutex
	static EspFrame g_Drawing;  // render thread only
	static std::mutex g_ReadyMutex;

	static void AddText(rage::fvector3 anchor, ImVec2 offset, ImU32 color, std::string text)
	{
		EspText entry{anchor, offset};
		entry.parts.emplace_back(std::move(text), color);
		g_Building.texts.push_back(std::move(entry));
	}

	// We want worldToScreen to return a bool since it will write {0.f, 0.f} to the screen vector if the entity is off-screen, messing up our draws
	static auto worldToScreen = [](rage::fvector3 coords) -> std::optional<ImVec2> {
		float screen_x{}, screen_y{};
		if (!GRAPHICS::GET_SCREEN_COORD_FROM_WORLD_COORD(coords.x, coords.y, coords.z, &screen_x, &screen_y))
			return std::nullopt;

		// Instead of returning a zero vector if the bool is false, we will return a nullopt
		return ImVec2{screen_x * (*Pointers.ScreenResX), screen_y * (*Pointers.ScreenResY)};
	};

	static void DrawSkeleton(Ped ped, ImU32 color)
	{
		if (!ped.IsValid())
			return;

		auto drawBoneLine = [&](int boneA, int boneB) {
			g_Building.lines.push_back({ped.GetBonePosition(boneA), ped.GetBonePosition(boneB), color});
		};

		// What's the point of drawing a skeleton if we don't draw the most important bone?
		g_Building.heads.push_back({ped.GetBonePosition(headBone), ped.GetBonePosition(neckBone), color});

		drawBoneLine(headBone, neckBone);
		drawBoneLine(neckBone, leftShoulderBone);
		drawBoneLine(leftShoulderBone, leftElbowBone);
		drawBoneLine(leftElbowBone, leftHandBone);
		drawBoneLine(neckBone, rightShoulderBone);
		drawBoneLine(rightShoulderBone, rightElbowBone);
		drawBoneLine(rightElbowBone, rightHandBone);
		drawBoneLine(neckBone, torsoBone);
		drawBoneLine(torsoBone, leftKneeBone);
		drawBoneLine(leftKneeBone, leftFootBone);
		drawBoneLine(torsoBone, rightKneeBone);
		drawBoneLine(rightKneeBone, rightFootBone);
	}

	static ImU32 HealthColor(float fraction, bool background)
	{
		if (fraction <= 0.f)
			return ImGui::ColorConvertFloat4ToU32(death_bg);
		if (fraction < 0.25f)
			return ImGui::ColorConvertFloat4ToU32(background ? health_red_bg : health_red);
		if (fraction < 0.65f)
			return ImGui::ColorConvertFloat4ToU32(background ? health_yellow_bg : health_yellow);
		return ImGui::ColorConvertFloat4ToU32(background ? health_green_bg : health_green);
	}

	//TODO : Very bare bones currently, expand and possibly refactor
	static void DrawPlayer(Player plyr)
	{
		if (!plyr.IsValid() || !plyr.GetPed().IsValid() || plyr == Self::GetPlayer()
		    || !worldToScreen(plyr.GetPed().GetBonePosition(torsoBone))
		    || (plyr.GetPed().IsDead() && !Features::_ESPDrawDeadPlayers.GetState()))
			return;

		auto ped = plyr.GetPed();
		auto head = ped.GetBonePosition(headBone);
		float distanceToPlayer = Self::GetPed().GetPosition().GetDistance(ped.GetBonePosition(torsoBone));
		int alphaBasedOnDistance = 255;
		ImColor colorBasedOnDistance = Red;

		if (distanceToPlayer < 100.f)
			colorBasedOnDistance = Green, alphaBasedOnDistance = 255;
		else if (distanceToPlayer > 100.f && distanceToPlayer < 300.f)
			colorBasedOnDistance = Orange, alphaBasedOnDistance = 200;
		else if (distanceToPlayer > 300.f)
			colorBasedOnDistance = Red, alphaBasedOnDistance = 125;

		bool godmode = plyr.GetData().m_IsGodmode; // same flag as the player list

		if (Features::_ESPName.GetState() || Features::_ESPTags.GetState())
		{
			EspText line{head, {0.f, 0.f}};
			if (Features::_ESPName.GetState())
				line.parts.emplace_back(plyr.GetName(), plyr == Players::GetSelected() ? ImGui::ColorConvertFloat4ToU32(Blue) : ImGui::ColorConvertFloat4ToU32(Features::_NameColorPlayers.GetState()));
			if (Features::_ESPTags.GetState())
			{
				// the same tags as the player list
				if (plyr.IsHost())
					line.parts.emplace_back(" [HOST]", ImGui::Colors::DeepSkyBlue);
				if (plyr.IsModder())
					line.parts.emplace_back(" [MOD]", ImGui::Colors::DeepPink);
				if (godmode)
					line.parts.emplace_back(" [GOD]", ImGui::Colors::Crimson);
			}
			if (!line.parts.empty())
				g_Building.texts.push_back(std::move(line));
		}
		if (Features::_ESPDistance.GetState())
		{
			std::string distanceStr = std::to_string((int)distanceToPlayer) + "m";
			AddText(head, {0.f, 20.f}, colorBasedOnDistance, distanceStr);
		}

		// health and armor bars under the text, not while in godmode
		if (!godmode)
		{
			float barY = Features::_ESPDistance.GetState() ? 40.f : 20.f;
			if (Features::_ESPHealth.GetState())
			{
				auto maxHealth = ped.GetMaxHealth();
				float fraction = maxHealth > 0 ? std::clamp(static_cast<float>(ped.GetHealth()) / maxHealth, 0.f, 1.f) : 0.f;
				g_Building.bars.push_back({head, {0.f, barY}, fraction, HealthColor(fraction, true), HealthColor(fraction, false)});
				barY += 6.f;
			}
			if (Features::_ESPArmor.GetState())
			{
				auto armor = ped.GetArmour();
				auto maxArmor = plyr.GetMaxArmour();
				if (armor > 0 && maxArmor > 0)
					g_Building.bars.push_back({head, {0.f, barY}, std::clamp(static_cast<float>(armor) / maxArmor, 0.f, 1.f), ImGui::ColorConvertFloat4ToU32(armor_blue_bg), ImGui::ColorConvertFloat4ToU32(armor_blue)});
			}
		}

		//TODO Boxes, Distance colors, Friendlies, Tracers

		if (Features::_ESPSkeleton.GetState() /* && !plyr.GetPed().IsAnimal() */) // yes, this is neccesary.
		{
			if (distanceToPlayer < 250.f)
			{
				DrawSkeleton(ped, ImGui::ColorConvertFloat4ToU32(Features::_SkeletonColorPlayers.GetState()));
			}
		}
	}

	static void DrawPed(Ped ped)
	{
		if (!ped.IsValid() || ped.IsPlayer() || ped == Self::GetPlayer().GetPed() || !worldToScreen(ped.GetBonePosition(torsoBone)) || (ped.IsDead() && !Features::_ESPDrawDeadPeds.GetState()))
			return;

		float distanceToPed = 0.0f;

		if (auto local = Self::GetPed())
			distanceToPed = local.GetPosition().GetDistance(ped.GetBonePosition(torsoBone));

		int alphaBasedOnDistance = 255;
		ImColor colorBasedOnDistance = Red;

		if (distanceToPed < 100.f)
			colorBasedOnDistance = Green, alphaBasedOnDistance = 255;
		else if (distanceToPed > 100.f && distanceToPed < 300.f)
			colorBasedOnDistance = Orange, alphaBasedOnDistance = 200;
		else if (distanceToPed > 300.f)
			colorBasedOnDistance = Red, alphaBasedOnDistance = 125;

		std::string info = "";

		if (Features::_ESPModelPeds.GetState())
		{
			//if (auto it = Data::g_PedModels.find(ped.GetModel()); it != Data::g_PedModels.end())
			//	info += std::format("{} ", it->second);
			//else
			info += std::format("0x{:08X} ", (joaat_t)ped.GetModel());
		}

		if (Features::_ESPNetworkInfoPeds.GetState() && ped.IsNetworked())
		{
			auto owner = Player(ped.GetOwner());
			auto id = ped.GetNetworkObjectId();

			info += std::format("{} {} ", id, owner.GetName());
		}

		if (Features::_ESPScriptInfoPeds.GetState())
		{
			if (auto script = ENTITY::GET_ENTITY_SCRIPT(ped.GetHandle(), nullptr))
			{
				info += std::format("{} ", script);
			}
		}

		auto head = ped.GetBonePosition(headBone);

		if (!info.empty())
			AddText(head, {0.f, 0.f}, ImGui::ColorConvertFloat4ToU32(Features::_HashColorPeds.GetState()), info);

		if (Features::_ESPDistancePeds.GetState())
		{
			std::string distanceStr = std::to_string((int)distanceToPed) + "m";
			AddText(head, {0.f, 20.f}, colorBasedOnDistance, distanceStr);
		}

		//TODO Boxes, Distance colors, Tracers, Health bars

		if (Features::_ESPSkeletonPeds.GetState() /* && !ped.IsAnimal() */)
		{
			if (distanceToPed < 250.f)
			{
				DrawSkeleton(ped, ImGui::ColorConvertFloat4ToU32(Features::_SkeletonColorPeds.GetState()));
			}
		}
	}

	static void DrawObject(Object object)
	{
		if (!object.IsValid())
			return;

		bool is_camera = object.IsCamera();
		bool is_signal_jammer = object.IsSignalJammer();
		bool is_mission_object = object.IsMissionEntity();

		if (!is_camera && !is_signal_jammer && !is_mission_object)
			return;

		float distanceToObject = 0.0f;

		if (auto local = Self::GetPed())
			distanceToObject = local.GetPosition().GetDistance(object.GetPosition());

		int alphaBasedOnDistance = 255;
		ImColor colorBasedOnDistance = Red;

		if (distanceToObject < 100.f)
			colorBasedOnDistance = Green, alphaBasedOnDistance = 255;
		else if (distanceToObject > 100.f && distanceToObject < 300.f)
			colorBasedOnDistance = Orange, alphaBasedOnDistance = 200;
		else if (distanceToObject > 300.f)
			colorBasedOnDistance = Red, alphaBasedOnDistance = 125;

		Vector3 coords = object.GetPosition();
		ImColor color = ImGui::ColorConvertFloat4ToU32(Features::_HashColorObjects.GetState());
		std::string info = std::format("0x{:08X} ", (joaat_t)object.GetModel());

		if (Features::_ESPNetworkInfoObjects.GetState() && object.IsNetworked())
		{
			auto owner = Player(object.GetOwner());
			auto id = object.GetNetworkObjectId();

			info += std::format("{} {} ", id, owner.GetName());
		}

		if (Features::_ESPScriptInfoObjects.GetState())
		{
			if (auto script = ENTITY::GET_ENTITY_SCRIPT(object.GetHandle(), nullptr))
			{
				info += std::format("{} ", script);
			}
		}

		if (is_camera)
		{
			color = Red;
			info += " (Camera)";
		}
		else if (is_signal_jammer)
		{
			color = Red;
			info += " (Jammer)";
		}
		else if (is_mission_object)
		{
			info += " (Mission)";
		}

		AddText(coords, {0.f, 0.f}, color, info);

		if (Features::_ESPDistanceObjects.GetState())
		{
			std::string distanceStr = std::to_string((int)distanceToObject) + "m";
			AddText(coords, {0.f, 20.f}, colorBasedOnDistance, distanceStr);
		}
	}

	static void Collect()
	{
		if (Features::_ESPDrawPlayers.GetState())
		{
			auto players = Players::GetPlayers();
			for (auto& [id, player] : players)
				DrawPlayer(player);
		}
		if (Features::_ESPDrawPeds.GetState() && GetPedPool())
		{
			for (Ped ped : Pools::GetPeds())
			{
				if (ped && ped.GetPointer<void*>())
					DrawPed(ped);
			}
		}
		if (Features::_ESPDrawObjects.GetState() && GetObjectPool())
		{
			for (auto obj : Pools::GetObjects())
			{
				if (obj)
					DrawObject(obj.As<Object>());
			}
		}
	}

	static void TryCollect()
	{
		// entities can still go away while we read them
		__try
		{
			Collect();
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}
	}

	void ESP::RunScript()
	{
		while (g_Running)
		{
			g_Building.Clear();
			if ((Features::_ESPDrawPlayers.GetState() || Features::_ESPDrawPeds.GetState() || Features::_ESPDrawObjects.GetState())
			    && NativeInvoker::AreHandlersCached() && !CAMERA::IS_SCREEN_FADED_OUT() && !HUD::IS_WARNING_MESSAGE_ACTIVE()
			    && !HUD::IS_PAUSE_MENU_ACTIVE() && !NETWORK::NETWORK_IS_IN_MP_CUTSCENE())
				TryCollect();
			g_Building.time = std::chrono::steady_clock::now();
			{
				std::lock_guard lock(g_ReadyMutex);
				std::swap(g_Building, g_Ready);
			}
			ScriptMgr::Yield();
		}
	}

	void ESP::Draw()
	{
		// only take the newest frame under the lock: projecting calls into the game, so the game thread must never wait on us
		{
			std::lock_guard lock(g_ReadyMutex);
			if (g_Ready.time > g_Drawing.time)
				std::swap(g_Ready, g_Drawing);
		}
		// nothing to draw, or the game thread stopped updating it (loading screens)
		if (g_Drawing.Empty() || std::chrono::steady_clock::now() - g_Drawing.time > 200ms)
			return;

		const auto originalFontSize = ImGui::GetFont()->Scale;
		auto* currentFont = ImGui::GetFont();
		currentFont->Scale *= 1.2;
		ImGui::PushFont(ImGui::GetFont());

		// projecting only reads the camera (no entities), so it is done here, with the camera of the frame being drawn
		if (const auto drawList = ImGui::GetBackgroundDrawList())
		{
			for (auto& line : g_Drawing.lines)
			{
				auto a = worldToScreen(line.a);
				auto b = worldToScreen(line.b);
				if (a && b)
					drawList->AddLine(*a, *b, line.color, 1.0f);
			}

			for (auto& head : g_Drawing.heads)
			{
				// We don't want the radius of the head circle to be static (too big at long range or too small close up), so let's scale it based on the length of the head to neck vector
				auto headScreen = worldToScreen(head.head);
				auto neckScreen = worldToScreen(head.neck);
				if (!headScreen || !neckScreen)
					continue;
				float dx = headScreen->x - neckScreen->x;
				float dy = headScreen->y - neckScreen->y;
				float radius = std::sqrt(dx * dx + dy * dy) * 0.25f;
				drawList->AddCircle(*headScreen, radius, head.color, 12, 1.5f);
			}

			for (auto& text : g_Drawing.texts)
			{
				auto pos = worldToScreen(text.anchor);
				if (!pos)
					continue;
				ImVec2 cursor{pos->x + text.offset.x, pos->y + text.offset.y};
				for (auto& [part, color] : text.parts)
				{
					drawList->AddText(cursor, color, part.c_str());
					cursor.x += ImGui::CalcTextSize(part.c_str()).x;
				}
			}

			for (auto& bar : g_Drawing.bars)
			{
				auto pos = worldToScreen(bar.anchor);
				if (!pos)
					continue;
				constexpr float width = 60.f;
				ImVec2 start{pos->x + bar.offset.x, pos->y + bar.offset.y};
				drawList->AddLine(start, {start.x + width, start.y}, bar.background, 4.f);
				if (bar.fraction > 0.f)
					drawList->AddLine(start, {start.x + width * bar.fraction, start.y}, bar.foreground, 4.f);
			}
		}

		currentFont->Scale = originalFontSize;
		ImGui::PopFont();
	}
}
