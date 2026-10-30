#pragma once
#include "game/gta/Player.hpp"

namespace YimMenu
{
	// Keeps the most recent incoming network traffic in memory, and writes it to a file when an attack is blocked or the game crashes
	class NetRecorder
	{
	public:
		enum class Kind : uint8_t
		{
			Message,     // a = message type, b = size in bytes
			Event,       // a = net game event id, b = data size in bits
			Reliables,   // a = flags
			CloneRemove, // a = object id, b = token
			CloneCreate, // a = object id, b = object type
			Blocked,     // a = object id, b = BlockReason
			ScriptEvent, // a = script event index (first argument), b = number of arguments (the first few are stored too)
			Join,        // a = RID
			Leave,
			SyncNode,           // a = sync node id (joaat of its class name), b = a value from it (the model for creation nodes)
			BlockedScriptEvent, // a = script event index
			NetworkEvent,       // our side: a = network script event type, b = data size (in 8 byte units, 0 = unknown), the first values are stored too
			State,              // our side: a = SessionState, b = new value
			ScriptHostAck,      // a = script hash, b = bit 0 "rejected", bit 1 "valid host" (both set = "I am the host"), bits 16-31 host token
			Pickup,             // a = pickup type hash, b = amount (cash); a custom model follows as a SyncNode record
			Explosion,          // a = explosion type, b = ExplosionFlags | nearest player id << 8 | distance to them in 0.1 m << 16; values: see RecordExplosion
		};

		enum ExplosionFlags : uint32_t
		{
			ExplosionOwned = 1 << 0, // addOwnedExplosion (ADD_OWNED_EXPLOSION): carries the script that made it
			ExplosionInvisible = 1 << 1,
			ExplosionAudible = 1 << 2,
			ExplosionHasTarget2 = 1 << 3,
			ExplosionAttached = 1 << 4, // its position was relative to the second target, the recorded one is on the map
		};

		enum class SessionState : uint32_t
		{
			InSession,
			Host,
			InTransition,
			SessionHost,  // player id
			FreemodeHost, // player id
			ConnectedPlayers,
			SimpleInterior, // freemode's simple interior index for us, -1 = none
			Interior,       // the interior our ped is in, 0 = none
		};

		enum class BlockReason : uint32_t
		{
			BadObjectType,
			ObjectTypeMismatch,
			OwnPed,
			OwnVehicle,
			PlayerObjectNotOwnedBySender,
			PlayerObjectRemoval,
			BadSyncData,
			BadSyncDataNotBlocked,
		};

		static void Record(Player sender, Kind kind, uint32_t a, uint32_t b = 0);
		// the arguments are the scripted game event's args (args[0] is the event index)
		static void RecordScriptEvent(Player sender, const std::int64_t* args, std::uint32_t arg_count);
		// values: [0] owner net id, [1] owner object type | owning player id << 8 (0xFF = no such object), [2] target entity | second target << 16,
		// [3..5] position in cm, [6] damage scale * 1000 | camera shake * 1000 << 16 | distance to us in 0.1 m << 32, [7] script hash << 32 | program counter
		static void RecordExplosion(Player sender, uint32_t type, uint32_t b, const std::array<std::int64_t, 8>& values);
		// something our own game saw or did (recorded with the name "(us)")
		static void RecordOwn(Kind kind, uint32_t a, uint32_t b = 0, const std::int64_t* values = nullptr, std::size_t value_count = 0);
		static const char* NetworkEventName(uint32_t type);
		static void Dump(std::string_view reason);
		// dump for a blocked attack. A block that isn't serious evidence (weak or no detection) is often one player spamming it,
		// so it only saves the first time per player (RID) and block, then every 10 minutes
		static void DumpBlocked(std::string_view reason, Player sender, std::string_view what, bool serious);
	};
}
