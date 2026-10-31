#include "core/commands/Command.hpp"
#include "core/filemgr/FileMgr.hpp"
#include "core/memory/ModuleMgr.hpp"
#include "game/gta/invoker/Invoker.hpp"

namespace YimMenu::Features
{
	class DumpGameModule : public Command
	{
		using Command::Command;

		virtual void OnCall() override
		{
			auto module = ModuleMgr.Get("GTA5_Enhanced.exe"_J);
			if (!module)
			{
				LOG(WARNING) << "Could not find GTA5_Enhanced.exe";
				return;
			}

			auto folder = FileMgr::GetProjectFolder("./dumps").Path();
			std::ofstream image(folder / "GTA5_Enhanced.bin", std::ios::binary | std::ios::trunc);
			if (!image)
			{
				LOG(WARNING) << "Could not create the dump file in " << folder.string();
				return;
			}

			// copy the module image page by page, anything that can't be read is written as zeros
			std::array<char, 0x1000> page;
			for (std::uintptr_t offset = 0; offset < module->Size(); offset += page.size())
			{
				SIZE_T read = 0;
				if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(module->Base() + offset), page.data(), page.size(), &read) || read != page.size())
					page.fill(0);
				image.write(page.data(), page.size());
			}

			std::ofstream info(folder / "GTA5_Enhanced.txt");
			info << std::format("base: 0x{:X}\nsize: 0x{:X}\n", module->Base(), module->Size());

			LOG(INFO) << "Dumped GTA5_Enhanced.exe (base 0x" << std::hex << module->Base() << ") to " << folder.string();
		}
	};

	// index, hash and handler RVA of each native
	class DumpNativeHandlers : public Command
	{
		using Command::Command;

		virtual void OnCall() override
		{
			auto module = ModuleMgr.Get("GTA5_Enhanced.exe"_J);
			if (!module || !NativeInvoker::AreHandlersCached())
			{
				LOG(WARNING) << "Native handlers aren't available yet";
				return;
			}

			auto path = FileMgr::GetProjectFolder("./dumps").Path() / "native_handlers.txt";
			std::ofstream file(path, std::ios::trunc);
			file << "index hash rva\n";
			for (std::size_t i = 0; i < g_Crossmap.size(); i++)
			{
				auto handler = reinterpret_cast<std::uintptr_t>(NativeInvoker::GetNativeHandler(static_cast<NativeIndex>(i)));
				auto rva = handler >= module->Base() && handler < module->Base() + module->Size() ? handler - module->Base() : 0;
				file << std::format("{} {:016X} {:X}\n", i, g_Crossmap[i], rva);
			}

			LOG(INFO) << "Dumped " << g_Crossmap.size() << " native handlers to " << path.string();
		}
	};

	static DumpNativeHandlers _DumpNativeHandlers{"dumpnativehandlers", "Dump Native Handlers", "Writes each native's hash and handler address to the dumps folder"};
	static DumpGameModule _DumpGameModule{"dumpgamemodule", "Dump Game Module", "Writes the game's memory image to the dumps folder"};
}
