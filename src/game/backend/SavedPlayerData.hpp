#pragma once

namespace YimMenu
{
	struct FetchedPlayerData
	{
		enum class GameState
		{
			INVALID = -1,
			INVITE_ONLY,
			FRIENDS_ONLY,
			CLOSED_CREW,
			OPEN_CREW,
			JOB_SESSION,
			PUBLIC,
			MAX
		};

		enum class MissionType
		{
			NONE = -1,
			MISSION,
			DEATHMATCH,
			RACE,
			SURVIVAL,
			GANG_ATTACK = 6,
			GOLF = 11,
			TENNIS,
			SHOOTING_RANGE
		};

		GameState m_GameState;
		bool m_HostOfSession;
		bool m_Spectating;
		bool m_InTransition;
		bool m_HostOfTransition;
		MissionType m_MissionType;
		std::optional<std::string> m_MissionName;
		std::string m_MissionId;

		static std::string_view GameStateToString(GameState state);
		static std::string_view MissionTypeToString(MissionType type);
	};

	struct SavedPlayerData
	{
		// stored
		std::string m_Name = "";
		bool m_IsModder = false;
		bool m_TrackPlayer = false;
		bool m_BlockJoin = false;  // refuse their join requests when we are host, host kick them if they get in
		int m_BlockJoinReason = 1; // join response status they are refused with (BlockJoinReasons.hpp)

		// not stored
		std::optional<FetchedPlayerData> m_FetchedData = std::nullopt;

		// with defaults, so older files still load
		NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(SavedPlayerData, m_Name, m_IsModder, m_TrackPlayer, m_BlockJoin, m_BlockJoinReason);
	};
}