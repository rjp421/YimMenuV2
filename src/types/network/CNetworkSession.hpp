#pragma once
#include "snSession.hpp"

// legacy's CNetwork (Pointers.NetworkSession); only the session pointers, verified on Enhanced
class CNetworkSession
{
public:
	char pad_0000[0xAA38];                // 0x0000
	rage::snSession* m_GameSession;       // 0xAA38
	rage::snSession* m_TransitionSession; // 0xAA40
};
static_assert(offsetof(CNetworkSession, m_GameSession) == 0xAA38);
