#ifndef _INCLUDE_GAMEDATA_H_
#define _INCLUDE_GAMEDATA_H_

// include these early to let use debugging versions of the memory allocators
#include "sdk/public/tier1/mempool.h"
#include "sdk/engine/packed_entity.h"

#include "smsdk/smsdk_ext.h"
#include <ihltv.h>

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

class CGameData
{
public:
	bool SetupFromGameConfig(IGameConfig* gc, char* error, int maxlength);
	bool SetupFromSteamAPILibrary(char* error, int maxlength);

	void InvokeDataTable_WriteSendTablesBuffer(ServerClass* pClasses, bf_write* buf)
	{
		return reinterpret_cast<decltype(DataTable_WriteSendTablesBuffer)*>(pfn_DataTable_WriteSendTablesBuffer)(pClasses, buf);
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

public:
	CClassMemoryPoolExt<PackedEntity>& ref_m_PackedEntitiesPool_from_CFrameSnapshotManager(void* pFrameSnapshotManager)
	{
		return *reinterpret_cast<CClassMemoryPoolExt<PackedEntity>*>(static_cast<byte*>(pFrameSnapshotManager) + offset_CFrameSnapshotManager_m_PackedEntitiesPool);
	}

protected:
	void* pfn_CSteam3Server_NotifyClientDisconnect;
};

#endif // _INCLUDE_GAMEDATA_H_
