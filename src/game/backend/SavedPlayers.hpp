#pragma once
#include "game/backend/SavedPlayerData.hpp"
#include "game/gta/Player.hpp"

namespace YimMenu
{
	class SavedPlayers
	{
		static constexpr auto PLAYERS_PER_BUCKET = 100;
		static constexpr auto AUTO_REFRESH_TIME = 3min;

		std::unordered_map<std::uint64_t, SavedPlayerData> m_SavedPlayers;
		bool m_FetchingPlayerInfo;
		std::filesystem::path m_PlayersFile;
		std::chrono::system_clock::time_point m_LastPlayerInfoFetch;
		// Block Join flags for the network thread, refreshed on every load and save
		std::unordered_map<std::uint64_t, int> m_BlockedJoins;
		std::mutex m_BlockedJoinsMutex;

		SavedPlayers();
		~SavedPlayers();

		void UpdateFetchedData(SavedPlayerData& saved_data, const FetchedPlayerData& fetched_data);

		void RunScriptImpl();
		void FetchPlayerInfoImpl(bool tracked_only = false);
		SavedPlayerData* GetPlayerDataImpl(std::uint64_t id);
		SavedPlayerData* GetPlayerDataImpl(Player player, bool create = true);
		void AddPlayerDataImpl(std::uint64_t id, std::string_view username);
		void UpdateRockstarIdImpl(std::uint64_t id, std::uint64_t new_id);
		void RemovePlayerDataImpl(std::uint64_t id);
		void LoadImpl();
		void SaveImpl();
		void OnPlayerJoinImpl(Player player);
		void RefreshBlockedJoins();
		std::optional<int> GetBlockJoinReasonImpl(std::uint64_t rid);

		static SavedPlayers& GetInstance()
		{
			static SavedPlayers instance;
			return instance;
		}

	public:
		static void RunScript()
		{
			GetInstance().RunScriptImpl();
		}

		static void FetchPlayerInfo(bool tracked_only = false)
		{
			GetInstance().FetchPlayerInfoImpl(tracked_only);
		}

		static SavedPlayerData* GetPlayerData(std::uint64_t id)
		{
			return GetInstance().GetPlayerDataImpl(id);
		}

		static SavedPlayerData* GetPlayerData(Player player, bool create = true)
		{
			return GetInstance().GetPlayerDataImpl(player, create);
		}

		static void AddPlayerData(std::uint64_t id, std::string_view username)
		{
			GetInstance().AddPlayerDataImpl(id, username);
		}

		// TODO: this cannot be safely called outside the saved players frontend menu. Invalidates all SavedPlayerData pointers due to potential map reallocation
		static void UpdateRockstarId(std::uint64_t old_id, std::uint64_t new_id)
		{
			GetInstance().UpdateRockstarIdImpl(old_id, new_id);
		}

		// TODO: this cannot be safely called outside the saved players frontend menu
		static void RemovePlayerData(std::uint64_t id)
		{
			GetInstance().RemovePlayerDataImpl(id);
		}

		static std::unordered_map<std::uint64_t, SavedPlayerData>& GetSavedPlayers()
		{
			return GetInstance().m_SavedPlayers;
		}

		static void Save()
		{
			GetInstance().SaveImpl();
		}

		static void OnPlayerJoin(Player player)
		{
			GetInstance().OnPlayerJoinImpl(player);
		}

		// thread safe
		static bool IsJoinBlocked(std::uint64_t rid)
		{
			return GetInstance().GetBlockJoinReasonImpl(rid).has_value();
		}

		// thread safe, nullopt when they aren't blocked
		static std::optional<int> GetBlockJoinReason(std::uint64_t rid)
		{
			return GetInstance().GetBlockJoinReasonImpl(rid);
		}
	};
}