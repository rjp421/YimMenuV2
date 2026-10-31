#pragma once
#include <array>
#include <utility>

namespace YimMenu
{
	// join response status codes the joiner's game shows a message for (same as legacy's block_join_reasons.hpp)
	inline constexpr auto g_BlockJoinReasons = std::to_array<std::pair<int, const char*>>({
	    {1, "None"},
	    {5, "Been Voted Out"},
	    {7, "Incompatible Assets"},
	    {8, "Session Full"},
	    {9, "Slot Full"},
	    {10, "No Title Update"},
	    {12, "Invites Disabled"},
	    {13, "Different Targeting Mode"},
	    {14, "Cheater"},
	    {16, "Incompatible DLC"},
	    {17, "Crew Only"},
	    {18, "Bad Sport"},
	    {19, "Bad Sports Only"},
	    {20, "Cheaters Only"},
	    {21, "Session No Longer Exists"},
	    {22, "Invite Only"},
	    {23, "Invite And Friends Only"},
	    {24, "Different Build Type"},
	    {26, "Different Content"},
	    {27, "Friends Only"},
	    {28, "Bad Reputation"},
	    {29, "May Not Exist"},
	    {30, "Premium Race"},
	});

	inline const char* GetBlockJoinReasonName(int reason)
	{
		for (auto& [code, name] : g_BlockJoinReasons)
			if (code == reason)
				return name;
		return "Unknown";
	}
}
