#include "HotkeySystem.hpp"
#include "core/backend/FiberPool.hpp"
#include "core/backend/ScriptMgr.hpp"
#include "core/renderer/D3D12Hook.hpp"
#include "core/renderer/Renderer.hpp"
#include "Commands.hpp"
#include "LoopedCommand.hpp"
#include "core/util/Joaat.hpp"

// TODO: serialization isn't stable

#include "game/pointers/Pointers.hpp" // game import in core!
#include "game/gta/Natives.hpp"       // game import in core!
#include "game/frontend/GUI.hpp"

namespace YimMenu
{
	// Saved next to the command hotkeys under a numeric key like theirs, so builds without this setting skip it
	static constexpr auto MenuToggleId = "menutoggle"_J;

	HotkeySystem::HotkeySystem() :
	    IStateSerializer("hotkeys")
	{
		m_MenuToggle.m_Chain.push_back(VK_INSERT);
	}

	// Called with each key the game window sees released
	bool HotkeySystem::IsMenuToggleRelease(int key)
	{
		const auto& chain = m_MenuToggle.m_Chain;
		if (m_BeingModified || std::find(chain.begin(), chain.end(), key) == chain.end())
			return false;

		// A key that types a character must not close the menu while the user types into a text box
		if (ImGui::GetCurrentContext() && GUI::IsUsingKeyboard() && MapVirtualKeyA(key, MAPVK_VK_TO_CHAR) != 0)
			return false;

		for (int other : chain)
			if (other != key && !(GetKeyState(other) & 0x8000))
				return false;

		return true;
	}

	void HotkeySystem::RegisterCommands()
	{
		auto& cmds = Commands::GetCommands();

		for (auto& [hash, cmd] : cmds)
		{
			CommandLink link;
			m_CommandHotkeys.insert(std::make_pair(hash, link));
		}

		m_CommandHotkeys.at("chathelper"_J).m_Chain.clear(); // ensure chat is always bound
		m_CommandHotkeys.at("chathelper"_J).m_Chain.push_back(0x54);
	}

	bool HotkeySystem::ListenAndApply(int& Hotkey, std::vector<int> Blacklist)
	{
		static auto IsKeyBlacklisted = [Blacklist](int Key) -> bool {
			for (auto Key_ : Blacklist)
				if (Key_ == Key)
					return true;

			return false;
		};

		// VK_OEM_CLEAR Is about the limit in terms of virtual key codes
		for (int i = 0; i < VK_OEM_CLEAR; i++)
		{
			if ((GetKeyState(i) & 0x8000) && i != 1 && !IsKeyBlacklisted(i))
			{
				Hotkey = i;

				return true;
			}
		}

		return false;
	}

	// Will return the keycode if there are no labels
	std::string HotkeySystem::GetHotkeyLabel(int HotkeyModifier)
	{
		char KeyName[32];
		LONG KeyParam = MapVirtualKey(HotkeyModifier, MAPVK_VK_TO_VSC) << 16;

		// These keys share their scan codes with numpad keys; without the extended key flag, Insert is named "Num 0"
		switch (HotkeyModifier)
		{
		case VK_INSERT:
		case VK_DELETE:
		case VK_HOME:
		case VK_END:
		case VK_PRIOR:
		case VK_NEXT:
		case VK_LEFT:
		case VK_RIGHT:
		case VK_UP:
		case VK_DOWN:
		case VK_DIVIDE:
		case VK_NUMLOCK:
		case VK_RCONTROL:
		case VK_RMENU:
		case VK_LWIN:
		case VK_RWIN:
		case VK_APPS:
			KeyParam |= 1 << 24;
			break;
		}

		GetKeyNameTextA(KeyParam, KeyName, 32);

		if (std::string(KeyName).empty())
			strcpy(KeyName, std::to_string(HotkeyModifier).data());

		return KeyName;
	}

	// Meant to be called in a loop
	void HotkeySystem::CreateHotkey(std::vector<int>& chain)
	{
		static auto is_key_unique = [this](int Key, std::vector<int> List) -> bool {
			for (auto& _key : List)
				if (_key == Key)
					return false;

			return true;
		};

		int pressed_key = 0;
		if (ListenAndApply(pressed_key, chain))
		{
			MarkStateDirty();

			if (is_key_unique(pressed_key, chain))
			{
				chain.push_back(pressed_key);
			}
		}
	}

	void HotkeySystem::RunScriptImpl()
	{
		while (g_Running)
		{
			if (GetForegroundWindow() == D3D12Hook::GetWindow() && !HUD::IS_PAUSE_MENU_ACTIVE() && !HUD::IS_SOCIAL_CLUB_ACTIVE() && !m_BeingModified && (Renderer::IsInitialized() && !GUI::IsUsingKeyboard()))
			{
				for (auto& [hash, link] : m_CommandHotkeys)
				{
					if (link.m_Chain.empty())
						continue;

					bool all_keys_pressed = true;

					for (auto modifier : link.m_Chain)
					{
						if (!(GetAsyncKeyState(modifier) & 0x8000))
						{
							all_keys_pressed = false;
						}
					}

					if (all_keys_pressed && std::chrono::system_clock::now() - m_LastHotkeyTriggerTime > 100ms)
					{
						auto command = Commands::GetCommand(hash);
						if (command)
						{
							// TODO: this is the only way I can prevent chat from blocking the main loop while keeping everything else fast
							if (hash != "chathelper"_J)
								command->Call();
							else
							{
								FiberPool::Push([command] {
									command->Call();
								});
							}
						}
						m_LastHotkeyTriggerTime = std::chrono::system_clock::now();
					}
				}
			}
			ScriptMgr::Yield();
		}
	}

	void HotkeySystem::RunScript()
	{
		g_HotkeySystem.RunScriptImpl();
	}

	void HotkeySystem::SaveStateImpl(nlohmann::json& state)
	{
		for (const auto& hotkey : m_CommandHotkeys)
		{
			std::string key = std::to_string(hotkey.first);

			if (!hotkey.second.m_Chain.empty())
			{
				state[key] = hotkey.second.m_Chain;
			}
			else if (state.contains(key))
			{
				state.erase(key);
			}
		}

		// Saved even when empty, so a cleared toggle key doesn't come back as Insert on the next start
		state[std::to_string(MenuToggleId)] = m_MenuToggle.m_Chain;
	}

	void HotkeySystem::LoadStateImpl(nlohmann::json& state)
	{
		for (auto& [key, value] : state.items())
		{
			if (m_CommandHotkeys.contains(static_cast<uint32_t>(std::stoul(key.data()))))
				m_CommandHotkeys[static_cast<uint32_t>(std::stoul(key.data()))].m_Chain = value.get<std::vector<int>>();
			else if (static_cast<uint32_t>(std::stoul(key.data())) == MenuToggleId)
				m_MenuToggle.m_Chain = value.get<std::vector<int>>();
		}
	}

	void HotkeySystem::SetBeingModifed(bool being_modified)
	{
		g_HotkeySystem.m_BeingModified = being_modified;
	}
}
