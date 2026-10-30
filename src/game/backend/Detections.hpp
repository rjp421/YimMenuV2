#pragma once
#include "game/gta/Player.hpp"

namespace YimMenu
{
	// How sure a detection is that the sender uses a mod menu. The level is also its number of points: a player is marked as a modder
	// at 3 points, so one certain detection is enough, while weaker ones have to repeat (the same detection counts once per 10 seconds)
	enum class DetectionLevel
	{
		None = 0,    // blocked, but normal players send it too, so it is not evidence
		Weak = 1,    // mod menu features that the game can still send in rare situations (e.g. force teleport)
		Strong = 2,  // almost certainly modified, e.g. a spoofed script event sender
		Certain = 3, // impossible for an unmodified game: crash and kick payloads, deleting our player, invalid object types
		Modder = 4,  // proof of a modified game that is not an attack on us (the game's own anticheat reported them): marks them, but the
		             // attack reactions (Host Kick Attackers, Block Join Attackers) only follow certain detections
	};

	class Detections
	{
	public:
		// thread safe, can be called from the network hooks
		static void Add(Player player, DetectionLevel level, std::string_view reason);
		// thread safe: a sign that isn't evidence on its own (e.g. spectating while alive), shown in the player list tooltip
		// without points or reactions
		static void Note(Player player, std::string_view reason);
		// the detections of this game session, most certain first, e.g. "network bail kick (certain, 2x)"
		static std::vector<std::string> Describe(std::uint64_t rid);

		// script thread (natives): an EntityDamage network event's data (sEntityDamagedData, 13 values)
		static void OnEntityDamage(const std::int64_t* data);
	};
}
