#ifndef _INCLUDE_GAMEDATA_H_
#define _INCLUDE_GAMEDATA_H_

// include these early to let use debugging versions of the memory allocators
#include "sdk/public/tier1/mempool.h"
#include "sdk/engine/packed_entity.h"

#include "smsdk/smsdk_ext.h"

#include <checksum_crc.h>

// ref: https://partner.steamgames.com/downloads/list
// Left 4 Dead - sdk v1.06
// Left 4 Dead 2 - sdk v1.37

#if SOURCE_ENGINE == SE_LEFT4DEAD2
#include "steamworks_sdk_137/public/steam/steam_gameserver.h"
#endif

class ISteamClient;
class CHLTVServer;
class CClientFrameManager;
class CHLTVDemoRecorder;
class ServerClass;
class bf_write;

void DataTable_WriteSendTablesBuffer(ServerClass* pClasses, bf_write* buf);
void DataTable_WriteClassInfosBuffer(ServerClass* pClasses, bf_write* buf);

class CGameData
{
public:
	bool SetupFromGameConfig(IGameConfig* gc, char* error, int maxlength);
	bool SetupFromSteamAPILibrary(char* error, int maxlength);

	void InvokeDataTable_WriteSendTablesBuffer(ServerClass* pClasses, bf_write* buf)
	{
		return reinterpret_cast<decltype(DataTable_WriteSendTablesBuffer)*>(pfn_DataTable_WriteSendTablesBuffer)(pClasses, buf);
	}

	void InvokeDataTable_WriteClassInfosBuffer(ServerClass* pClasses, bf_write* buf)
	{
		return reinterpret_cast<decltype(DataTable_WriteClassInfosBuffer)*>(pfn_DataTable_WriteClassInfosBuffer)(pClasses, buf);
	}

public: // Steam API
#if SOURCE_ENGINE == SE_LEFT4DEAD2
	void* InvokeCreateInterface(const char* ver)
	{
		return reinterpret_cast<decltype(SteamInternal_CreateInterface)*>(pfn_SteamInternal_CreateInterface)(ver);
	}

	HSteamPipe InvokeGetHSteamPipe()
	{
		return reinterpret_cast<decltype(SteamGameServer_GetHSteamPipe)*>(pfn_SteamGameServer_GetHSteamPipe)();
	}

	HSteamUser InvokeGetHSteamUser()
	{
		return reinterpret_cast<decltype(SteamGameServer_GetHSteamUser)*>(pfn_SteamGameServer_GetHSteamUser)();
	}
#endif

protected:
	void* pfn_SteamInternal_CreateInterface;
	void* pfn_SteamGameServer_GetHSteamPipe;
	void* pfn_SteamGameServer_GetHSteamUser;
	void* pfn_SteamInternal_GameServer_Init;
	void* pfn_SteamGameServer_InitSafe;

public:
	CRC32_t& ref_stringTableCRC_from_CBaseServer(void* pServer)
	{
		return *reinterpret_cast<CRC32_t*>(static_cast<byte*>(pServer) + property_CBaseServer_stringTableCRC);
	}

	CClientFrameManager& ref_CClientFrameManager_from_CHLTVServer(void* pHLTVServer)
	{
		return *reinterpret_cast<CClientFrameManager*>(static_cast<byte*>(pHLTVServer) + offset_CHLTVServer_CClientFrameManager);
	}

	CHLTVServer* ptr_CHLTVServer_from_IHLTVServer(void* pIHLTVServer)
	{
		return reinterpret_cast<CHLTVServer*>(static_cast<byte*>(pIHLTVServer) - offset_CHLTVServer_IHLTVServer);
	}

	CHLTVDemoRecorder* ptr_m_DemoRecorder_from_CHLTVServer(void* pHLTVServer)
	{
		return reinterpret_cast<CHLTVDemoRecorder*>(static_cast<byte*>(pHLTVServer) + offset_CHLTVServer_m_DemoRecorder);
	}

	CHLTVDemoRecorder* ptr_m_DemoRecorder_from_IHLTVServer(void* pIHLTVServer)
	{
		return ptr_m_DemoRecorder_from_CHLTVServer(ptr_CHLTVServer_from_IHLTVServer(pIHLTVServer));
	}

	CClassMemoryPoolExt<PackedEntity>& ref_m_PackedEntitiesPool_from_CFrameSnapshotManager(void* pFrameSnapshotManager)
	{
		return *reinterpret_cast<CClassMemoryPoolExt<PackedEntity>*>(static_cast<byte*>(pFrameSnapshotManager) + offset_CFrameSnapshotManager_m_PackedEntitiesPool);
	}

protected:
	int property_CBaseServer_stringTableCRC;
	int offset_CHLTVServer_CClientFrameManager;
	int offset_CHLTVServer_IHLTVServer;
	int offset_CHLTVServer_m_DemoRecorder;
	int offset_CFrameSnapshotManager_m_PackedEntitiesPool;

protected:
	void* pfn_CHLTVDemoRecorder_RecordStringTables;
	void* pfn_CHLTVDemoRecorder_RecordServerClasses;
	void* pfn_DataTable_WriteSendTablesBuffer;
	void* pfn_DataTable_WriteClassInfosBuffer;
	void* pfn_CBaseServer_IsExclusiveToLobbyConnections;
	void* pfn_CBaseClient_SendFullConnectEvent;
	void* pfn_CSteam3Server_NotifyClientDisconnect;
	void* pfn_CHLTVServer_AddNewFrame;
	void* pfn_CFrameSnapshotManager_LevelChanged;

protected:
	int vtblindex_CBaseServer_GetChallengeNr;
	int vtblindex_CBaseServer_GetChallengeType;
	int vtblindex_CBaseServer_ReplyChallenge;
};

#endif // _INCLUDE_GAMEDATA_H_
