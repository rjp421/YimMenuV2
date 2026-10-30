#include "LogHelper.hpp"

#include "LogSink.hpp"

namespace YimMenu
{
	LogHelper::~LogHelper()
	{
		if (g_Running)
			GetInstance().DestroyImpl();
	}

	void LogHelper::Destroy()
	{
		GetInstance().DestroyImpl();
	}

	bool LogHelper::Init(const std::string_view consoleName, const std::filesystem::path& file, const bool attachConsole)
	{
		return GetInstance().InitImpl(consoleName, file, attachConsole);
	}

	void LogHelper::ToggleConsole(bool toggle)
	{
		GetInstance().ToggleConsoleImpl(toggle);
	}

	void LogHelper::DestroyImpl()
	{
		Logger::Destroy();
		CloseOutputStreams();

		if (m_DidConsoleExist)
			SetConsoleMode(m_ConsoleHandle, m_OriginalConsoleMode);

		if (!m_DidConsoleExist && m_AttachConsole)
			FreeConsole();
	}

	bool LogHelper::InitImpl(const std::string_view consoleName, const std::filesystem::path& file, const bool attachConsole)
	{
		m_ConsoleTitle = consoleName;
		m_File = file;
		m_AttachConsole = attachConsole;

		if (m_AttachConsole)
		{
			if (m_DidConsoleExist = ::AttachConsole(GetCurrentProcessId()); !m_DidConsoleExist)
				AllocConsole();

			if (m_ConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE); m_ConsoleHandle != nullptr)
			{
				SetConsoleTitleA(m_ConsoleTitle.data());
				SetConsoleOutputCP(CP_UTF8);

				DWORD consoleMode;
				GetConsoleMode(m_ConsoleHandle, &consoleMode);
				m_OriginalConsoleMode = consoleMode;

				// terminal like behaviour enable full color support
				consoleMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
				consoleMode &= ~(ENABLE_QUICK_EDIT_MODE);

				SetConsoleMode(m_ConsoleHandle, consoleMode);
			}
		}

		BackupLogFile();
		OpenOutputStreams();

		Logger::Init();
		Logger::AddSink([this](LogMessagePtr msg) {
			m_ConsoleOut << LogSink::FormatConsole(msg);

			m_ConsoleOut.flush();
		});
		Logger::AddSink([this](LogMessagePtr msg) {
			m_FileOut << LogSink::FormatFile(msg);

			m_FileOut.flush();
		});

		if (!m_BackupResult.empty())
			LOG(INFO) << m_BackupResult;

		return true;
	}

	// moves the log of the previous run to backup/<name>_<last write time>.log, since opening the log truncates it
	void LogHelper::BackupLogFile()
	{
		std::error_code ec;
		if (!std::filesystem::exists(m_File, ec) || std::filesystem::file_size(m_File, ec) == 0)
			return;

		auto writeTime = std::filesystem::last_write_time(m_File, ec);
		if (ec)
			return;
		auto time = std::chrono::system_clock::to_time_t(std::chrono::clock_cast<std::chrono::system_clock>(writeTime));
		std::tm local{};
		localtime_s(&local, &time);
		char stamp[32];
		std::strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", &local);

		auto folder = m_File.parent_path() / "backup";
		std::filesystem::create_directories(folder, ec);
		auto target = folder / std::format("{}_{}{}", m_File.stem().string(), stamp, m_File.extension().string());

		std::filesystem::rename(m_File, target, ec);
		if (ec)
			m_BackupResult = std::format("Could not back up the previous log to {}: {}", target.string(), ec.message());
		else
			m_BackupResult = std::format("Saved the previous log to {}", target.string());
	}

	void LogHelper::ToggleConsoleImpl(bool toggle)
	{
		if (m_DidConsoleExist)
			return;

		if (m_AttachConsole)
		{
			m_ConsoleOut.close();
			FreeConsole();
		}
		else
		{
			m_ConsoleOut.open("CONOUT$", std::ios_base::out | std::ios_base::app);
			AllocConsole();
		}

		m_AttachConsole = !m_AttachConsole;
	}

	void LogHelper::CloseOutputStreams()
	{
		if (m_AttachConsole)
			m_ConsoleOut.close();
		m_FileOut.close();
	}

	void LogHelper::OpenOutputStreams()
	{
		if (m_AttachConsole)
			m_ConsoleOut.open("CONOUT$", std::ios_base::out | std::ios_base::app);
		m_FileOut.open(m_File, std::ios::out | std::ios::trunc);
	}
}