#pragma once
#include "rlGamerInfo.hpp"

// layouts verified on Enhanced
namespace rage
{
	// a session member (also players that have no player slot yet); no name, that is in the member's snPeer
	class snPlayer
	{
	public:
		uint64_t m_MsgId;            // 0x00
		rlGamerInfoBase m_GamerInfo; // 0x08
		uint64_t m_HostToken;        // 0xC8
		uint32_t m_PeerId;           // 0xD0 connection peer id (= netEvent::m_PeerId)
	};
	static_assert(offsetof(snPlayer, m_HostToken) == 0xC8);
	static_assert(offsetof(snPlayer, m_PeerId) == 0xD0);

	class snPeer
	{
	public:
		rlGamerInfo m_GamerInfo; // 0x00 (with the name)
	};

	// only the members verified on Enhanced (same offsets as legacy's last-gen layout)
	class snSession
	{
	public:
		char pad_0000[0x8F0]; // 0x0000 (local snPlayer at 0x7F0)
		uint64_t m_HostToken; // 0x08F0
		char pad_08F8[0x2CA0 - 0x8F8];
		snPeer* m_Peers[32]; // 0x2CA0
		int32_t m_PeerCount; // 0x2DA0
		char pad_2DA4[0x4DC0 - 0x2DA4];
		snPlayer* m_Players[32]; // 0x4DC0
		int32_t m_PlayerCount;   // 0x4EC0
	};
	static_assert(offsetof(snSession, m_HostToken) == 0x8F0);
	static_assert(offsetof(snSession, m_Peers) == 0x2CA0);
	static_assert(offsetof(snSession, m_PeerCount) == 0x2DA0);
	static_assert(offsetof(snSession, m_Players) == 0x4DC0);
	static_assert(offsetof(snSession, m_PlayerCount) == 0x4EC0);
}
