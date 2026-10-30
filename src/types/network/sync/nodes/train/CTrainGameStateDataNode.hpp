#pragma once
#include "types/network/sync/CProjectBaseSyncDataNode.hpp"

// layout of the game update that legacy calls b3717 (two new fields), verified on enhanced with normal players' trains
class CTrainGameStateDataNode : CSyncDataNodeInfrequent
{
public:
	bool m_IsEngine;               //0x00C0
	bool unk_00C1;                 //0x00C1
	bool m_IsCaboose;              //0x00C2
	bool m_IsMissionTrain;         //0x00C3
	bool m_Direction;              //0x00C4
	bool m_HasPassengerCarriages;  //0x00C5
	bool m_RenderDerailed;         //0x00C6
	bool unk_00C7;                 //0x00C7
	bool unk_00C8;                 //0x00C8
	char pad_00C9[1];              //0x00C9
	uint16_t m_EngineId;           //0x00CA
	int8_t m_TrainConfigIndex;     //0x00CC
	int8_t m_CarriageConfigIndex;  //0x00CD
	int8_t m_TrackId;              //0x00CE
	char pad_00CF[1];              //0x00CF
	float m_DistanceFromEngine;    //0x00D0
	float unk_00D4;                //0x00D4 (same value as the cruise speed in all samples)
	float m_CruiseSpeed;           //0x00D8
	uint16_t m_LinkedToBackwardId; //0x00DC
	uint16_t m_LinkedToForwardId;  //0x00DE
	uint32_t m_TrainState;         //0x00E0
	bool m_ForceDoorsOpen;         //0x00E4
	bool unk_00E5;                 //0x00E5
}; //Size: 0x00E8
static_assert(sizeof(CTrainGameStateDataNode) == 0xE8);
