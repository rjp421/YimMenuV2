#include "GUISettings.hpp"
#include "core/commands/ColorCommand.hpp"
#include "core/commands/Command.hpp"
#include "core/commands/FloatCommand.hpp"
#include "core/frontend/manager/styles/Themes.hpp"
#include <regex>

namespace YimMenu
{
	static std::vector<std::unique_ptr<ColorCommand>> g_ColorCommands;
	static std::unordered_map<std::string, float> g_RoundingValues;
	static std::unordered_map<std::string, std::unique_ptr<FloatCommand>> g_FloatCommands; // Global map for all FloatCommands
	static bool g_ColorInit = false, g_RoundingInit = false;
	static constexpr auto kFontScaleKey = "FontGlobalScale";
	static constexpr float kFontScaleMin = 0.5f, kFontScaleMax = 2.0f;

	static const std::string kSettingsFile = [] {
		char* path = nullptr;
		size_t len = 0;
		_dupenv_s(&path, &len, "APPDATA");
		std::string full = std::string(path ? path : "") + "\\YimMenuV2\\themes.json";
		free(path);
		return full;
	}();

	static std::string PrettyPrintLabel(const std::string& raw)
	{
		std::string out = raw;
		if (out.size() > 2 && out.compare(out.size() - 2, 2, "_X") == 0)
			out.replace(out.size() - 2, 2, " Horizontal");
		else if (out.size() > 2 && out.compare(out.size() - 2, 2, "_Y") == 0)
			out.replace(out.size() - 2, 2, " Vertical");

		std::string spaced;
		spaced.reserve(out.size() + 10);
		for (size_t i = 0; i < out.size(); ++i)
		{
			if (i > 0 && isupper(out[i]) && islower(out[i - 1]))
				spaced += ' ';
			spaced += out[i];
		}

		std::regex uscore_re("_");
		spaced = std::regex_replace(spaced, uscore_re, " ");

		return spaced;
	}

	void SyncColorCommandsToStyle()
	{
		auto& style = ImGui::GetStyle();
		for (int i = 0; i < ImGuiCol_COUNT; i++)
			style.Colors[i] = g_ColorCommands[i]->GetState();
	}

	static void SyncRoundingToStyle()
	{
		auto& s = ImGui::GetStyle();
		for (auto& [k, v] : g_RoundingValues)
			if (k == "WindowRounding")
				s.WindowRounding = v;
			else if (k == "FrameRounding")
				s.FrameRounding = v;
			else if (k == "GrabRounding")
				s.GrabRounding = v;
			else if (k == "ScrollbarRounding")
				s.ScrollbarRounding = v;
			else if (k == "ChildRounding")
				s.ChildRounding = v;
			else if (k == "PopupRounding")
				s.PopupRounding = v;
			else if (k == "TabRounding")
				s.TabRounding = v;
	}

	using StyleFloatFn = void (*)(const std::string& name, float& value, float min, float max);

	static void VisitStyleVec2(StyleFloatFn fn, const char* label, ImVec2& value, float min, float max)
	{
		fn(std::string(label) + "_X", value.x, min, max);
		fn(std::string(label) + "_Y", value.y, min, max);
	}

	static void VisitLayoutFloats(StyleFloatFn fn)
	{
		auto& s = ImGui::GetStyle();
		VisitStyleVec2(fn, "WindowPadding", s.WindowPadding, 0.f, 32.f);
		VisitStyleVec2(fn, "ItemSpacing", s.ItemSpacing, 0.f, 32.f);
		VisitStyleVec2(fn, "ItemInnerSpacing", s.ItemInnerSpacing, 0.f, 32.f);
		VisitStyleVec2(fn, "TouchExtraPadding", s.TouchExtraPadding, 0.f, 32.f);
		VisitStyleVec2(fn, "DisplaySafeAreaPadding", s.DisplaySafeAreaPadding, 0.f, 32.f);

		fn("IndentSpacing", s.IndentSpacing, 0.f, 64.f);
		fn("ColumnsMinSpacing", s.ColumnsMinSpacing, 0.f, 64.f);

		VisitStyleVec2(fn, "WindowTitleAlign", s.WindowTitleAlign, 0.f, 1.f);
		VisitStyleVec2(fn, "ButtonTextAlign", s.ButtonTextAlign, 0.f, 1.f);
		VisitStyleVec2(fn, "SelectableTextAlign", s.SelectableTextAlign, 0.f, 1.f);
	}

	static void VisitBorderFloats(StyleFloatFn fn)
	{
		auto& s = ImGui::GetStyle();
		fn("WindowBorderSize", s.WindowBorderSize, 0.f, 8.f);
		fn("FrameBorderSize", s.FrameBorderSize, 0.f, 8.f);
		fn("TabBorderSize", s.TabBorderSize, 0.f, 8.f);
		fn("PopupBorderSize", s.PopupBorderSize, 0.f, 8.f);
	}

	static void VisitGlobalFloats(StyleFloatFn fn)
	{
		auto& s = ImGui::GetStyle();
		fn("GlobalAlpha", s.Alpha, 0.1f, 1.f);
		fn("DisabledAlpha", s.DisabledAlpha, 0.f, 1.f);
		fn("MouseCursorScale", s.MouseCursorScale, 0.5f, 2.f);
		fn("CurveTessellationTol", s.CurveTessellationTol, 0.1f, 10.f);
	}

	static void VisitAllFloats(StyleFloatFn fn)
	{
		VisitLayoutFloats(fn);
		VisitBorderFloats(fn);
		VisitGlobalFloats(fn);
		fn(kFontScaleKey, ImGui::GetIO().FontGlobalScale, kFontScaleMin, kFontScaleMax);
	}

	static void CreateStyleFloat(const std::string& name, float& value, float min, float max)
	{
		g_FloatCommands[name] = std::make_unique<FloatCommand>(name, name, "Adjust " + name, min, max, value);
	}

	// clamped because an out of range value in the file could leave the menu invisible or unusable, with no way to fix it in game
	static void ApplyStyleFloat(const std::string& name, float& value, float min, float max)
	{
		value = std::clamp(g_FloatCommands[name]->GetState(), min, max);
	}

	static void LoadSettings()
	{
		if (!std::filesystem::exists(kSettingsFile))
			return;
		std::ifstream file(kSettingsFile);
		// a crash during a save can leave the file cut short
		auto json = nlohmann::json::parse(file, nullptr, false);
		if (json.is_discarded())
		{
			LOG(WARNING) << "Couldn't read themes.json, using the default style";
			return;
		}

		// Load colors
		for (int i = 0; i < ImGuiCol_COUNT; ++i)
			if (auto it = json.find(ImGui::GetStyleColorName(i)); it != json.end() && it->is_array())
				g_ColorCommands[i]->SetState(ImVec4((*it)[0], (*it)[1], (*it)[2], (*it)[3]));

		// Load rounding values
		for (const char* key : {"WindowRounding", "FrameRounding", "GrabRounding", "ScrollbarRounding", "ChildRounding", "PopupRounding", "TabRounding"})
			if (auto it = json.find(key); it != json.end())
				g_RoundingValues[key] = *it;

		// Load all float command values (except colors and rounding, which are handled)
		for (auto& [key, cmd] : g_FloatCommands)
		{
			if (auto it = json.find(key); it != json.end() && it->is_number())
			{
				cmd->SetState(it->get<float>());
			}
		}
	}

	static void SaveSettings()
	{
		nlohmann::json json;

		// Save colors
		for (int i = 0; i < ImGuiCol_COUNT; ++i)
		{
			auto c = g_ColorCommands[i]->GetState();
			json[ImGui::GetStyleColorName(i)] = {c.x, c.y, c.z, c.w};
		}

		// Save rounding
		for (auto& [k, v] : g_RoundingValues)
			json[k] = v;

		// Save floats
		for (auto& [k, cmd] : g_FloatCommands)
			json[k] = cmd->GetState();

		std::filesystem::create_directories(std::filesystem::path(kSettingsFile).parent_path());
		std::ofstream(kSettingsFile) << json.dump(4);
	}

	void ApplyThemeToImGui()
	{
		auto& style = ImGui::GetStyle();

		for (int i = 0; i < ImGuiCol_COUNT; ++i)
			style.Colors[i] = g_ColorCommands[i]->GetState();

		style.WindowRounding = g_RoundingValues["WindowRounding"];
		style.FrameRounding = g_RoundingValues["FrameRounding"];
		style.GrabRounding = g_RoundingValues["GrabRounding"];
		style.ScrollbarRounding = g_RoundingValues["ScrollbarRounding"];
		style.ChildRounding = g_RoundingValues["ChildRounding"];
		style.PopupRounding = g_RoundingValues["PopupRounding"];
		style.TabRounding = g_RoundingValues["TabRounding"];

		VisitAllFloats(ApplyStyleFloat);
	}


	void InitializeColorCommands()
	{
		if (g_ColorInit)
			return;
		auto& style = ImGui::GetStyle();
		for (int i = 0; i < ImGuiCol_COUNT; ++i)
			g_ColorCommands.emplace_back(std::make_unique<ColorCommand>(
			    "ColorCommand." + std::string(ImGui::GetStyleColorName(i)),
			    ImGui::GetStyleColorName(i),
			    "Edit color for " + std::string(ImGui::GetStyleColorName(i)),
			    style.Colors[i]));

		if (!g_RoundingInit)
		{
			g_RoundingInit = true;
			g_RoundingValues = {
			    {"WindowRounding", style.WindowRounding},
			    {"FrameRounding", style.FrameRounding},
			    {"GrabRounding", style.GrabRounding},
			    {"ScrollbarRounding", style.ScrollbarRounding},
			    {"ChildRounding", style.ChildRounding},
			    {"PopupRounding", style.PopupRounding},
			    {"TabRounding", style.TabRounding}};
		}

		// made here, not when a tab is first drawn: loading and saving only see commands that exist, so values on
		// tabs not opened yet were never applied and were dropped from the file by the next save
		VisitAllFloats(CreateStyleFloat);

		LoadSettings();
		SyncColorCommandsToStyle();
		SyncRoundingToStyle();
		g_ColorInit = true;
	}

	static void DrawStyleFloat(const std::string& name, float& v, float min, float max)
	{
		float newVal = v;
		if (ImGui::SliderFloat(PrettyPrintLabel(name).c_str(), &newVal, min, max, "%.1f", ImGuiSliderFlags_AlwaysClamp))
		{
			g_FloatCommands[name]->SetState(newVal);
			v = newVal;
			SaveSettings();
		}
	}

	static void DrawColorsTab()
	{
		bool changed = false;
		ImGui::Text("Modify Colors:");
		ImGui::Separator();
		for (int i = 0; i < ImGuiCol_COUNT; ++i)
		{
			auto& cmd = g_ColorCommands[i];
			auto col = cmd->GetState();

			if (ImGui::ColorEdit4(PrettyPrintLabel(ImGui::GetStyleColorName(i)).c_str(), (float*)&col))
			{
				cmd->SetState(col);
				changed = true;
			}
		}
		if (changed)
		{
			SyncColorCommandsToStyle();
			SaveSettings();
		}
	}

	static void DrawRoundingTab()
	{
		bool changed = false;
		ImGui::Text("Adjust Rounding:");
		ImGui::Separator();
		for (auto& [k, v] : g_RoundingValues)
			if (ImGui::SliderFloat(PrettyPrintLabel(k).c_str(), &v, 0.0f, 20.0f, "%.1f"))
				changed = true;

		if (changed)
		{
			SyncRoundingToStyle();
			SaveSettings();
		}
	}

	static void DrawLayoutTab()
	{
		ImGui::Text("Layout & Alignment:");
		ImGui::Separator();
		VisitLayoutFloats(DrawStyleFloat);
	}

	static void DrawBorderTab()
	{
		ImGui::Text("Border Sizes:");
		ImGui::Separator();
		VisitBorderFloats(DrawStyleFloat);
	}

	static void DrawGlobalTab()
	{
		ImGui::Text("Global Settings:");
		ImGui::Separator();
		VisitGlobalFloats(DrawStyleFloat);
	}

	static void DrawFontTab()
	{
		ImGuiIO& io = ImGui::GetIO();
		static float scale = io.FontGlobalScale;
		ImGui::Text("Font Configuration:");
		ImGui::Separator();
		ImGui::Text("Current Scale: %.2f", io.FontGlobalScale);
		ImGui::SliderFloat("Font Scale", &scale, kFontScaleMin, kFontScaleMax, "%.2f", ImGuiSliderFlags_AlwaysClamp);
		if (ImGui::Button("Apply Font Scale"))
		{
			io.FontGlobalScale = scale;
			g_FloatCommands[kFontScaleKey]->SetState(scale);
			SaveSettings();
		}
	}

	std::shared_ptr<Category> DrawGUISettingsMenu()
	{
		InitializeColorCommands();
		auto imGuiCustomStyle = std::make_shared<Category>("Customize");
		imGuiCustomStyle->AddItem(std::make_unique<ImGuiItem>([] {
			ImGui::Text("ImGui Style Editor");
			ImGui::Separator();
			if (ImGui::BeginTabBar("StyleTabs"))
			{
				if (ImGui::BeginTabItem("Colors"))
				{
					DrawColorsTab();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Rounding"))
				{
					DrawRoundingTab();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Layout"))
				{
					DrawLayoutTab();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Border"))
				{
					DrawBorderTab();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Global"))
				{
					DrawGlobalTab();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Fonts"))
				{
					DrawFontTab();
					ImGui::EndTabItem();
				}
				ImGui::EndTabBar();
			}
		}));
		return imGuiCustomStyle;
	}
}
