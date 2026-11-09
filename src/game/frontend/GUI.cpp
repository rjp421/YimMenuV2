#include "core/frontend/manager/UIManager.hpp"
#include "GUI.hpp"
#include "Menu.hpp"
#include "ESP.hpp"
#include "Overlay.hpp"
#include "core/backend/ScriptMgr.hpp"
#include "core/commands/HotkeySystem.hpp"
#include "core/renderer/Renderer.hpp"
#include "core/scripting/LuaManager.hpp"
#include "core/scripting/LuaScript.hpp"
#include "core/frontend/Notifications.hpp"
#include "game/frontend/ChatDisplay.hpp"
#include "game/gta/Natives.hpp"
#include "types/pad/ControllerInputs.hpp"

namespace YimMenu
{
	void GUI::InitImpl()
	{
		Menu::Init();

		Renderer::AddWindowProcedureCallback([this](HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
			GUI::WndProc(hwnd, msg, wparam, lparam);
		});

		Renderer::AddWindowProcedureCallback([this](HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
			LuaManager::DispatchEvent(MenuEvent::WndProc, [hwnd, msg, wparam, lparam](lua_State* state)
			{
				lua_pushinteger(state, reinterpret_cast<uintptr_t>(hwnd));
				lua_pushinteger(state, msg);
				lua_pushinteger(state, wparam);
				lua_pushinteger(state, lparam);

				return 4;
			});
		});

		Renderer::AddRendererCallback(
		    [&] {
			    Notifications::Draw();
		    },
		    -2);
		Renderer::AddRendererCallback(
		    [&] {
			    ESP::Draw();
		    },
		    -3);
		Renderer::AddRendererCallback(
		    [&] {
			    ChatDisplay::Draw();
		    },
		    -5);
		Renderer::AddRendererCallback(
		    [&] {
			    Overlay::Draw();
		    },
		    -6);
		Renderer::AddRendererCallback(
		    [] {
			    LuaManager::ForAllLoadedScripts([](std::shared_ptr<LuaScript>& script) {
				    script->GetUserInterface().DrawAlwaysDrawImGuiCallbacks();
			    });
		    },
		    -7);
		Renderer::AddRendererCallback(
		    [] {
			    if (!GUI::IsOpen())
				    return;
			    LuaManager::ForAllLoadedScripts([](std::shared_ptr<LuaScript>& script) {
				    script->GetUserInterface().DrawImGuiCallbacks();
			    });
		    },
		    -4);
		
		Renderer::SetSafeToRender();
	}

	void GUI::ToggleMouse()
	{
		if (!ImGui::GetCurrentContext())
			return;
		auto& io = ImGui::GetIO();
		const bool enabled = IsOpen() || IsOnboarding() || IsMouseOverridden();
		io.MouseDrawCursor = enabled;
		if (enabled)
			io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
		else
			io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
	}

	void GUI::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		// Mouse buttons can be bound too, and they don't send WM_KEYUP
		int releasedKey;
		switch (msg)
		{
		case WM_KEYUP:
		case WM_SYSKEYUP: releasedKey = static_cast<int>(wparam); break;
		case WM_RBUTTONUP: releasedKey = VK_RBUTTON; break;
		case WM_MBUTTONUP: releasedKey = VK_MBUTTON; break;
		case WM_XBUTTONUP: releasedKey = GET_XBUTTON_WPARAM(wparam) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2; break;
		default: return;
		}

		// Ctrl + \ always toggles the menu, so a bad toggle key binding can't lock the user out
		const bool fallbackKey = msg == WM_KEYUP && releasedKey == VK_OEM_5 && (GetKeyState(VK_CONTROL) & 0x8000) != 0;

		if (fallbackKey || g_HotkeySystem.IsMenuToggleRelease(releasedKey))
		{
			// Persist and restore the cursor position between menu instances
			static POINT CursorCoords{};
			if (m_IsOpen)
			{
				GetCursorPos(&CursorCoords);
			}
			else if (CursorCoords.x + CursorCoords.y)
			{
				SetCursorPos(CursorCoords.x, CursorCoords.y);
			}
			if (!GUI::IsOnboarding())
				Toggle();
			ToggleMouse(); 
		}
	}
	
	void GUI::SetOnboardingImpl(bool state)
	{
		m_Onboarding = state;
		ToggleMouse();
	}

	void GUI::RunScriptImpl()
	{
		while (g_Running)
		{
			if (Renderer::IsInitialized() && (GUI::IsOpen() || GUI::IsMouseOverridden()))
			{
				if (UIManager::ShowingContentWindow() || GUI::IsMouseOverridden())
				{
					if (GUI::IsUsingKeyboard() && PAD::IS_USING_KEYBOARD_AND_MOUSE(0))
					{
						PAD::DISABLE_ALL_CONTROL_ACTIONS(0);
					}
					else
					{
						static constexpr auto controls = std::to_array<ControllerInputs>({ControllerInputs::INPUT_LOOK_LR, ControllerInputs::INPUT_LOOK_UD, ControllerInputs::INPUT_ATTACK, ControllerInputs::INPUT_AIM, ControllerInputs::INPUT_DUCK, ControllerInputs::INPUT_SELECT_WEAPON, ControllerInputs::INPUT_VEH_AIM, ControllerInputs::INPUT_VEH_ATTACK, ControllerInputs::INPUT_VEH_ATTACK2, ControllerInputs::INPUT_VEH_NEXT_RADIO, ControllerInputs::INPUT_VEH_PASSENGER_AIM, ControllerInputs::INPUT_VEH_PASSENGER_ATTACK, ControllerInputs::INPUT_VEH_SELECT_NEXT_WEAPON, ControllerInputs::INPUT_VEH_SELECT_PREV_WEAPON, ControllerInputs::INPUT_VEH_MOUSE_CONTROL_OVERRIDE, ControllerInputs::INPUT_MELEE_ATTACK_ALTERNATE, ControllerInputs::INPUT_FRONTEND_Y, ControllerInputs::INPUT_ATTACK2, ControllerInputs::INPUT_PREV_WEAPON, ControllerInputs::INPUT_NEXT_WEAPON, ControllerInputs::INPUT_VEH_DRIVE_LOOK, ControllerInputs::INPUT_VEH_DRIVE_LOOK2});

						for (const auto& control : controls)
							PAD::DISABLE_CONTROL_ACTION(0, static_cast<int>(control), true);
					}
				}
			}

			ScriptMgr::Yield();
		}
	}
}
