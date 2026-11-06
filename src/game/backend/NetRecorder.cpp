#include "NetRecorder.hpp"

#include "core/commands/BoolCommand.hpp"
#include "core/commands/Command.hpp"
#include "core/filemgr/FileMgr.hpp"
#include "core/util/Joaat.hpp"
#include "core/logger/ExceptionHandler.hpp"

namespace YimMenu
{
	static BoolCommand _NetRecorder{"netrecorder", "Net Recorder", "Keeps recent network traffic in memory and saves it to the netlogs folder when an attack is blocked or the game crashes", false};

	static constexpr std::size_t MAX_STORED_ARGS = 8;

	struct NetRecord
	{
		std::int64_t m_Time; // ms since epoch
		char m_Name[20];
		std::uint8_t m_PlayerId;
		NetRecorder::Kind m_Kind;
		std::uint32_t m_A;
		std::uint32_t m_B;
		std::uint8_t m_ArgCount;              // only used by script events
		std::int64_t m_Args[MAX_STORED_ARGS]; // args[1..], args[0] is the event index
	};

	static std::array<NetRecord, 32768> g_Records;
	static std::size_t g_Next = 0;
	static std::size_t g_Count = 0;
	static std::mutex g_Mutex;
	static constexpr std::size_t MAX_SESSION_NETLOGS = 30;

	static void Push(Player sender, NetRecorder::Kind kind, uint32_t a, uint32_t b, const std::int64_t* extra, std::size_t extra_count, bool own = false)
	{
		if (!_NetRecorder.GetState())
			return;

		auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

		std::lock_guard lock(g_Mutex);
		auto& record = g_Records[g_Next];
		record.m_Time = now;
		record.m_PlayerId = own ? 254 : sender ? static_cast<std::uint8_t>(sender.GetId()) : 255;
		strncpy_s(record.m_Name, own ? "(us)" : sender ? sender.GetName() : "unknown", _TRUNCATE);
		record.m_Kind = kind;
		record.m_A = a;
		record.m_B = b;
		record.m_ArgCount = static_cast<std::uint8_t>(std::min(extra_count, MAX_STORED_ARGS));
		for (std::size_t i = 0; i < record.m_ArgCount; i++)
			record.m_Args[i] = extra[i];

		g_Next = (g_Next + 1) % g_Records.size();
		g_Count = std::min(g_Count + 1, g_Records.size());
	}

	void NetRecorder::Record(Player sender, Kind kind, uint32_t a, uint32_t b)
	{
		Push(sender, kind, a, b, nullptr, 0);
	}

	void NetRecorder::RecordScriptEvent(Player sender, const std::int64_t* args, std::uint32_t arg_count)
	{
		if (arg_count == 0)
			return;

		Push(sender, Kind::ScriptEvent, static_cast<std::uint32_t>(args[0]), arg_count, args + 1, arg_count - 1);
	}

	void NetRecorder::RecordExplosion(Player sender, uint32_t type, uint32_t b, const std::array<std::int64_t, 8>& values)
	{
		Push(sender, Kind::Explosion, type, b, values.data(), values.size());
	}

	void NetRecorder::RecordOwn(Kind kind, uint32_t a, uint32_t b, const std::int64_t* values, std::size_t value_count)
	{
		Push(nullptr, kind, a, b, values, value_count, true);
	}

	// numbers from legacy's eEventNetworkType (last-gen); not yet verified on enhanced
	const char* NetRecorder::NetworkEventName(uint32_t type)
	{
		switch (type)
		{
		case 153: return "PlayerJoinScript";
		case 154: return "PlayerLeftScript";
		case 155: return "StorePlayerLeft";
		case 156: return "StartSession";
		case 157: return "EndSession";
		case 158: return "StartMatch";
		case 160: return "RemovedFromSessionDueToStall";
		case 161: return "RemovedFromSessionDueToComplaints";
		case 162: return "ConnectionTimeout";
		case 163: return "PedDroppedWeapon";
		case 164: return "PlayerSpawn";
		case 165: return "PlayerCollectedPickup";
		case 166: return "PlayerCollectedAmbientPickup";
		case 167: return "PlayerCollectedPortablePickup";
		case 168: return "PlayerDroppedPortablePickup";
		case 170: return "InviteAccepted";
		case 171: return "InviteConfirmed";
		case 172: return "InviteRejected";
		case 173: return "Summon";
		case 174: return "ScriptEvent";
		case 176: return "SignInStateChanged";
		case 177: return "SignInChangeActioned";
		case 178: return "RosChanged";
		case 179: return "Bail";
		case 180: return "HostMigration";
		case 181: return "FindSession";
		case 182: return "HostSession";
		case 183: return "JoinSession";
		case 184: return "JoinSessionResponse";
		case 185: return "CheatTriggered";
		case 186: return "EntityDamage";
		case 187: return "PlayerArrest";
		case 188: return "TimedExplosion";
		case 193: return "VoiceSessionStarted";
		case 194: return "VoiceSessionEnded";
		case 195: return "VoiceConnectionRequested";
		case 196: return "VoiceConnectionResponse";
		case 197: return "VoiceConnectionTerminated";
		case 198: return "TextMessageReceived";
		case 199: return "CloudFileResponse";
		case 200: return "PickupRespawned";
		case 201: return "PresenceStatUpdate";
		case 202: return "PedLeftBehind";
		case 203: return "InboxMsgReceived";
		case 204: return "AttemptHostMigration";
		case 205: return "IncrementStat";
		case 206: return "SessionEvent";
		case 207: return "TransitionStarted";
		case 208: return "TransitionEvent";
		case 209: return "TransitionMemberJoined";
		case 210: return "TransitionMemberLeft";
		case 211: return "TransitionParameterChanged";
		case 213: return "TransitionStringChanged";
		case 214: return "TransitionGamerInstruction";
		case 215: return "PresenceInvite";
		case 218: return "CashTransactionLog";
		case 220: return "VehicleUndrivable";
		case 221: return "PresenceTriggerEvent";
		case 225: return "SpectateLocal";
		case 226: return "CloudEvent";
		case 227: return "ShopTransaction";
		case 231: return "SystemServiceEvent";
		case 232: return "RequestDelay";
		case 239: return "FiredDummyProjectile";
		case 240: return "PlayerEnteredVehicle";
		case 241: return "PlayerActivatedSpecialAbility";
		case 242: return "PlayerDeactivatedSpecialAbility";
		case 244: return "FiredVehicleProjectile";
		}
		return "?";
	}
	void NetRecorder::DumpBlocked(std::string_view reason, Player sender, std::string_view what, bool serious)
	{
		if (!serious)
		{
			static std::mutex mutex;
			static std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_dumps;
			auto key = std::format("{} {}", sender ? sender.GetRID() : 0, what);
			auto now = std::chrono::steady_clock::now();
			std::lock_guard lock(mutex);
			auto [it, inserted] = last_dumps.try_emplace(std::move(key), now);
			if (!inserted)
			{
				if (now - it->second < std::chrono::minutes(10))
					return;
				it->second = now;
			}
		}
		Dump(reason);
	}

	void NetRecorder::Dump(std::string_view reason)
	{
		// called from the exception handler too, so never block and never re-enter
		static thread_local bool dumping = false;
		static std::chrono::steady_clock::time_point last_crash_dump{}, last_blocked_dump{}, last_session_dump{};

		if (dumping || !_NetRecorder.GetState())
			return;

		std::unique_lock lock(g_Mutex, std::try_to_lock);
		if (!lock.owns_lock() || g_Count == 0)
			return;

		// a crash causes a storm of exceptions, so save at most every 30 seconds per trigger (manual saves are never limited)
		auto last_dump = reason == "crash" ? &last_crash_dump : reason == "manual" ? nullptr : reason == "session ended" ? &last_session_dump : &last_blocked_dump;
		auto now = std::chrono::steady_clock::now();
		if (last_dump)
		{
			if (*last_dump != std::chrono::steady_clock::time_point{} && now - *last_dump < std::chrono::seconds(30))
				return;
			*last_dump = now;
		}
		dumping = true;

		auto local_now = std::chrono::zoned_time{std::chrono::current_zone(), std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())};
		auto path = FileMgr::GetProjectFolder("./netlogs").Path() / std::format("netlog_{:%Y%m%d_%H%M%S}{}.txt", local_now, reason == "session ended" ? "_session" : "");
		std::ofstream file(path);
		file << "reason: " << reason << "\n";
		file << "time         pid name                 what\n";

		auto start = (g_Next + g_Records.size() - g_Count) % g_Records.size();
		for (std::size_t i = 0; i < g_Count; i++)
		{
			const auto& r = g_Records[(start + i) % g_Records.size()];
			auto time = std::chrono::zoned_time{std::chrono::current_zone(), std::chrono::sys_time<std::chrono::milliseconds>(std::chrono::milliseconds(r.m_Time))};

			std::string what;
			switch (r.m_Kind)
			{
			case Kind::Message: what = std::format("message 0x{:X} ({} bytes)", r.m_A, r.m_B); break;
			case Kind::Event: what = std::format("  event {} ({} bits)", r.m_A, r.m_B); break;
			case Kind::Reliables: what = std::format("  reliables flags 0x{:X}", r.m_A); break;
			case Kind::CloneRemove: what = std::format("  clone remove object {} (token {})", r.m_A, r.m_B); break;
			case Kind::CloneCreate: what = std::format("  clone create object {} (type {})", r.m_A, r.m_B); break;
			case Kind::ScriptEvent:
			{
				what = std::format("  scripted game event index {} (0x{:X}) with {} args:", static_cast<std::int32_t>(r.m_A), r.m_A, r.m_B);
				for (std::size_t j = 0; j < r.m_ArgCount; j++)
					what += std::format(" {:X}", r.m_Args[j]);
				break;
			}
			case Kind::Join: what = std::format("JOINED rid {}", r.m_A); break;
			case Kind::Leave: what = "LEFT"; break;
			case Kind::SyncNode:
			{
				const char* name = "node";
				switch (r.m_A)
				{
				case "CPlayerCreationDataNode"_J: name = "player creation, model"; break;
				case "CPlayerAppearanceDataNode"_J: name = "player appearance, model"; break;
				case "CPedCreationDataNode"_J: name = "ped creation, model"; break;
				case "CObjectCreationDataNode"_J: name = "object creation, model"; break;
				case "CDoorCreationDataNode"_J: name = "door creation, model"; break;
				case "CVehicleCreationDataNode"_J: name = "vehicle creation, model"; break;
				case "CPickupCreationDataNode"_J: name = "pickup creation, custom model"; break;
				}
				what = std::format("  sync {} 0x{:08X}", name, r.m_B);
				break;
			}
			case Kind::Blocked:
			{
				static constexpr const char* reasons[] = {"bad object type", "object type mismatch", "targets our ped", "targets our vehicle", "player object not owned by sender", "removal of a player object", "bad sync data", "bad sync data (not blocked, Block Bad Sync Data is off)"};
				what = std::format("  BLOCKED object {}: {}", r.m_A, r.m_B < std::size(reasons) ? reasons[r.m_B] : "unknown");
				break;
			}
			case Kind::BlockedScriptEvent: what = std::format("  BLOCKED scripted game event index {} (0x{:X})", static_cast<std::int32_t>(r.m_A), r.m_A); break;
			case Kind::NetworkEvent:
			{
				what = std::format("OUR network event {} ({}), size {}", r.m_A, NetworkEventName(r.m_A), r.m_B);
				if (r.m_ArgCount)
					what += ":";
				for (std::size_t j = 0; j < r.m_ArgCount; j++)
					what += std::format(" {}", static_cast<std::int32_t>(r.m_Args[j]));
				break;
			}
			case Kind::State:
			{
				static constexpr const char* states[] = {"in session", "we are host", "in transition", "session host", "freemode script host", "connected players", "simple interior", "interior"};
				what = std::format("OUR state: {} = {}", r.m_A < std::size(states) ? states[r.m_A] : "unknown", static_cast<std::int32_t>(r.m_B));
				break;
			}
			case Kind::Pickup:
			{
				// the cash pickups (what money drops use), by name
				const char* name = nullptr;
				switch (r.m_A)
				{
				case "PICKUP_MONEY_CASE"_J: name = "PICKUP_MONEY_CASE"; break;
				case "PICKUP_MONEY_VARIABLE"_J: name = "PICKUP_MONEY_VARIABLE"; break;
				case "PICKUP_MONEY_MED_BAG"_J: name = "PICKUP_MONEY_MED_BAG"; break;
				case "PICKUP_MONEY_PAPER_BAG"_J: name = "PICKUP_MONEY_PAPER_BAG"; break;
				case "PICKUP_MONEY_PURSE"_J: name = "PICKUP_MONEY_PURSE"; break;
				case "PICKUP_MONEY_SECURITY_CASE"_J: name = "PICKUP_MONEY_SECURITY_CASE"; break;
				case "PICKUP_MONEY_WALLET"_J: name = "PICKUP_MONEY_WALLET"; break;
				case "PICKUP_MONEY_DEP_BAG"_J: name = "PICKUP_MONEY_DEP_BAG"; break;
				case "PICKUP_GANG_ATTACK_MONEY"_J: name = "PICKUP_GANG_ATTACK_MONEY"; break;
				}
				what = name ? std::format("  sync pickup creation, type {} (cash), amount {}", name, r.m_B) : std::format("  sync pickup creation, type 0x{:08X}, amount {}", r.m_A, r.m_B);
				break;
			}
			case Kind::ScriptHostAck:
				what = std::format("  script host ack for 0x{:08X}: rejected {}, valid host {}, token {}{}", r.m_A, r.m_B & 1, (r.m_B >> 1) & 1, r.m_B >> 16, (r.m_B & 3) == 3 ? " (\"I am the host\")" : "");
				break;
			case Kind::Explosion:
			{
				if (r.m_ArgCount < 8)
					break;
				const auto* v = r.m_Args;
				auto owner_type = v[1] & 0xFF;
				auto owner_player = (v[1] >> 8) & 0xFF;
				auto nearest = (r.m_B >> 8) & 0xFF;
				what = std::format("  explosion type {} at ({:.1f}, {:.1f}, {:.1f}), nearest player {} at {:.1f} m, us at {:.1f} m, owner object {}",
				    static_cast<std::int32_t>(r.m_A), v[3] / 100.0, v[4] / 100.0, v[5] / 100.0, nearest == 255 ? std::string("none") : std::to_string(nearest),
				    (r.m_B >> 16) / 10.0, ((v[6] >> 32) & 0xFFFFFFFF) / 10.0, v[0]);
				what += owner_type == 0xFF ? std::string(" (unknown)") : std::format(" (type {}, player {})", owner_type, owner_player);
				what += std::format(", target {} / {}{}, damage {:.2f}, shake {:.2f}", v[2] & 0xFFFF, (v[2] >> 16) & 0xFFFF, (r.m_B & ExplosionHasTarget2) ? " (set)" : "",
				    (v[6] & 0xFFFF) / 1000.0, static_cast<std::int16_t>((v[6] >> 16) & 0xFFFF) / 1000.0);
				if (r.m_B & ExplosionAttached)
					what += std::format(", attached to object {}", (v[2] >> 16) & 0xFFFF);
				if (r.m_B & ExplosionOwned)
					what += std::format(", owned, script 0x{:08X} pc 0x{:X}", static_cast<std::uint32_t>(v[7] >> 32), static_cast<std::uint32_t>(v[7]));
				if (r.m_B & ExplosionInvisible)
					what += ", invisible";
				if (!(r.m_B & ExplosionAudible))
					what += ", silent";
				break;
			}
			}

			file << std::format("{:%H:%M:%S}  {:>3} {:<20} {}\n", time, r.m_PlayerId, r.m_Name, what);
		}

		LOG(WARNING) << "Net recorder saved " << g_Count << " records to " << path.string() << " (" << reason << ")";
		file.close();

		// keep the folder from growing without limit. Only the routine end-of-session recordings are ever deleted: recordings of crashes,
		// blocked attacks and manual saves are evidence and stay until the user removes them
		std::vector<std::filesystem::path> files;
		for (auto& entry : std::filesystem::directory_iterator(path.parent_path()))
			if (entry.path().stem().string().ends_with("_session"))
				files.push_back(entry.path());
		std::sort(files.begin(), files.end());
		for (std::size_t i = 0; i + MAX_SESSION_NETLOGS < files.size(); i++)
		{
			std::error_code ec;
			std::filesystem::remove(files[i], ec);
		}

		if (reason == "session ended")
			g_Count = 0;
		dumping = false;
	}

	class DumpNetRecorder : public Command
	{
		using Command::Command;

		virtual void OnCall() override
		{
			NetRecorder::Dump("manual");
		}
	};

	static DumpNetRecorder _DumpNetRecorder{"dumpnetrecorder", "Save Net Recorder", "Saves the recorded network messages to the netlogs folder now"};

	static const bool _RegisterCrashDump = [] {
		g_OnException = [] {
			NetRecorder::Dump("crash");
		};
		return true;
	}();
}
