#ifndef _INCLUDE_WRAPPERS_H_
#define _INCLUDE_WRAPPERS_H_

#include <extensions/IBinTools.h>

#include <iserver.h>
#include <ihltvdirector.h>
#include <ihltv.h>
#include <server_class.h>
#include <game/server/iplayerinfo.h>
#include <iclient.h>
#include <igameevents.h>
#include <networkstringtabledefs.h>

extern IServerGameEnts* gameents;
extern IPlayerInfoManager* playerinfomanager;

// ref: https://partner.steamgames.com/downloads/list
// Left 4 Dead - sdk v1.06
// Left 4 Dead 2 - sdk v1.41

#if SOURCE_ENGINE == SE_LEFT4DEAD2
#include "steam/steam_gameserver.h"
#include "steam/steamclientpublic.h"
#else
#include "steam/steamclientpublic.h"
#endif

#include "sdk/engine/demo.h"
#include "sdk/engine/clientframe.h"

#define TICK_INTERVAL			(gpGlobals->interval_per_tick)
#define TIME_TO_TICKS( dt )		( (int)( 0.5f + (float)(dt) / TICK_INTERVAL ) )

class CDetour;

inline CBaseEntity* GetEntityHandle(CBaseHandle& refHandle);
inline void SetEntityHandle(CBaseHandle& refHandle, CBaseEntity* pEntity);

void DataTable_WriteClassInfosBuffer(ServerClass* pClasses, bf_write* pBuf)
{
	int count = 0;
	ServerClass* pClass = pClasses;

	// first count total number of classes in list
	while (pClass != NULL) {
		pClass = pClass->m_pNext;
		count++;
	}

	// write number of classes
	pBuf->WriteShort(count);

	pClass = pClasses; // go back to first class

	// write each class info
	while (pClass != NULL) {
		pBuf->WriteShort(pClass->m_ClassID);
		pBuf->WriteString(pClass->m_pNetworkName);
		pBuf->WriteString(pClass->m_pTable->GetName());
		pClass = pClass->m_pNext;
	}
}

extern int shookid_CHLTVDemoRecorder_RecordStringTables;
extern int shookid_CHLTVDemoRecorder_RecordServerClasses;
extern int shookid_SteamGameServer_LogOff;

extern void* pfn_DataTable_WriteSendTablesBuffer;
extern void* pfn_SteamGameServer_GetHSteamPipe;
extern void* pfn_SteamGameServer_GetHSteamUser;
extern void* pfn_SteamInternal_CreateInterface;
extern void* pfn_SteamInternal_GameServer_Init;
extern void* pfn_OpenSocketInternal;

extern CDetour* detour_SteamInternal_GameServer_Init;

void InvokeDataTable_WriteSendTablesBuffer(ServerClass* pClasses, bf_write* buf)
{
	void DataTable_WriteSendTablesBuffer(ServerClass * pClasses, bf_write * buf);
	return reinterpret_cast<decltype(DataTable_WriteSendTablesBuffer)*>(pfn_DataTable_WriteSendTablesBuffer)(pClasses, buf);
}

// Steam API
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

void InvokeOpenSocketInternal(int nModule, int nSetPort, int nDefaultPort, const char* pName, bool bTryAny)
{
	void OpenSocketInternal(int nModule, int nSetPort, int nDefaultPort, const char* pName, bool bTryAny);
	return reinterpret_cast<decltype(OpenSocketInternal)*>(pfn_OpenSocketInternal)(nModule, nSetPort, nDefaultPort, pName, bTryAny);
}

class CBaseClient :
	public IGameEventListener2,
	public IClient,
	public IClientMessageHandler
{
public:
	static int offset_m_SteamID;

	static void* pfn_SendFullConnectEvent;

	static CDetour* detour_SendFullConnectEvent;

	CSteamID& m_SteamID()
	{
		return *reinterpret_cast<CSteamID*>(reinterpret_cast<byte*>(this) + offset_m_SteamID);
	}
};

class CBaseServer
	: public IServer
{
public:
	static int offset_stringTableCRC;
	static int vtblindex_GetChallengeNr;
	static int vtblindex_GetChallengeType;
	static int vtblindex_ReplyChallenge;
	static int vtblindex_FillServerInfo;
	static int vtblindex_ConnectClient;

	static void* pfn_IsExclusiveToLobbyConnections;

	static CDetour* detour_IsExclusiveToLobbyConnections;

	static ICallWrapper* vcall_GetChallengeNr;
	static ICallWrapper* vcall_GetChallengeType;

	static CBaseServer* FromIServer(IServer* pServer)
	{
		return reinterpret_cast<CBaseServer*>(pServer);
	}

	static CBaseServer* FromIHLTVServer(IHLTVServer* pHLTVServer)
	{
		return FromIServer(pHLTVServer->GetBaseServer());
	}

	CRC32_t& stringTableCRC()
	{
		return *reinterpret_cast<CRC32_t*>(reinterpret_cast<byte*>(this) + offset_stringTableCRC);
	}

	INetworkStringTableContainer* m_StringTables()
	{
		return *reinterpret_cast<INetworkStringTableContainer**>(reinterpret_cast<byte*>(this) + offset_stringTableCRC + 4);
	}

	int GetChallengeNr(netadr_t& adr)
	{
		struct {
			CBaseServer* pServer;
			netadr_s* adr;
		} stack{ this, &adr };

		int ret;
		vcall_GetChallengeNr->Execute(&stack, &ret);

		return ret;
	}

	int GetChallengeType(netadr_t& adr)
	{
		struct {
			CBaseServer* pServer;
			netadr_s* adr;
		} stack{ this, &adr };

		int ret;
		vcall_GetChallengeType->Execute(&stack, &ret);

		return ret;
	}
};

class CHLTVServer
{
public:
	static int offset_m_DemoRecorder;
	static int offset_CClientFrameManager;
	static int offset_CBaseServer;

	static int vtblindex_FillServerInfo;

	static int shookid_ReplyChallenge;
	static int shookid_FillServerInfo;
	static int shookid_hltv_FillServerInfo;
	static int shookid_ConnectClient;

	static void* pfn_AddNewFrame;

	static CDetour* detour_AddNewFrame;

	CHLTVDemoRecorder& m_DemoRecorder()
	{
		return *reinterpret_cast<CHLTVDemoRecorder*>(reinterpret_cast<byte*>(this) + offset_m_DemoRecorder);
	}

	CClientFrameManager& GetClientFrameManager()
	{
		return *reinterpret_cast<CClientFrameManager*>(reinterpret_cast<byte*>(this) + offset_CClientFrameManager);
	}

	static CHLTVServer* FromBaseServer(CBaseServer* pServer)
	{
		return reinterpret_cast<CHLTVServer*>(reinterpret_cast<byte*>(pServer) - offset_CBaseServer);
	}
};

class CGameServer
{
public:
	static int shookid_IsPausable;
};

class CSteam3Server
{
public:
	static void* pfn_NotifyClientDisconnect;

	static CDetour* detour_NotifyClientDisconnect;
};

class CFrameSnapshotManager
{
public:
	static int offset_m_PackedEntitiesPool;

	static void* pfn_RecordStringTables;
	static void* pfn_RecordServerClasses;

	static void* pfn_LevelChanged;

	static CDetour* detour_LevelChanged;

	CClassMemoryPoolExt<PackedEntity>& m_PackedEntitiesPool()
	{
		return *reinterpret_cast<CClassMemoryPoolExt<PackedEntity>*>(reinterpret_cast<byte*>(this) + offset_m_PackedEntitiesPool);
	}
};

class CBaseEntity :
	public IServerEntity
{
public:
	edict_t* edict()
	{
		return gameents->BaseEntityToEdict(this);
	}

	const char* GetClassname()
	{
		return edict()->GetClassName();
	}
};

class CBaseAbility :
	public CBaseEntity
{
public:
	static void* pfn_ShouldTransmit;
	static CDetour* detour_ShouldTransmit;
};

class CBasePlayer :
	public CBaseEntity
{
public:
	static int sendprop_m_fFlags;
	static int sendprop_m_PlayerFog_m_hCtrl;
	static int sendprop_m_hPostProcessCtrl;
	static int sendprop_m_hColorCorrectionCtrl;

	IGamePlayer* GetIGamePlayer()
	{
		return playerhelpers->GetGamePlayer(edict());
	}

	inline int& m_fFlags()
	{
		return *(int*)((byte*)(this) + sendprop_m_fFlags);
	}

	int GetFlags()
	{
		return m_fFlags();
	}

	void AddFlag(int flags)
	{
		m_fFlags() |= flags;

		edict_t* pEdict = this->edict();
		if (pEdict != NULL) {
			gamehelpers->SetEdictStateChanged(pEdict, sendprop_m_fFlags);
		}
	}

	bool IsHLTV()
	{
		IGamePlayer* pGamePlayer = GetIGamePlayer();
		if (pGamePlayer == NULL) {
			return false;
		}

		return pGamePlayer->IsSourceTV();
	}

	int GetUserID()
	{
		return engine->GetPlayerUserId(edict());
	}

	void ChangeTeam(int teamIndex)
	{
		IPlayerInfo* pPlayerInfo = playerinfomanager->GetPlayerInfo(edict());
		if (pPlayerInfo == NULL) {
			return;
		}

		if (pPlayerInfo->GetTeamIndex() == teamIndex) {
			return;
		}

		pPlayerInfo->ChangeTeam(teamIndex);
	}

	void SetController(CBaseEntity* pSetEnt, int iNetpropOffset)
	{
		CBaseHandle& hCtrl = *(CBaseHandle*)((byte*)(this) + iNetpropOffset);
		CBaseEntity *pCtrl = GetEntityHandle(hCtrl);
		if (pCtrl == pSetEnt) {
			return;
		}

		SetEntityHandle(hCtrl, pSetEnt);

		// CBasePlayer::NetworkStateChanged
		gamehelpers->SetEdictStateChanged(edict(), iNetpropOffset);
		/*Msg("Old ctrl: %s (%p), set ctrl: %s (%p). Change edict state.""\n",
			(pCtrl) ? pCtrl->GetClassname() : "Unknown", pCtrl,
			(pSetEnt) ? pSetEnt->GetClassname() : "Unknown", pSetEnt);*/
	}

	inline void ResetControllers()
	{
		SetController(NULL, sendprop_m_PlayerFog_m_hCtrl);
		SetController(NULL, sendprop_m_hPostProcessCtrl);
		SetController(NULL, sendprop_m_hColorCorrectionCtrl);
	}
};

class CTerrorPlayer :
	public CBasePlayer
{
public:
	static void* pfn_UpdateFXVolume;

	static CDetour* detour_UpdateFXVolume;
};

class HitAnnouncement
{
public:
	static void* pfn_ForEachTerrorPlayer;

	static CDetour* detour_ForEachTerrorPlayer;

	static int pzMsgId;

#if defined _WIN32
	static void SetupFromRelativeAddress(ptrdiff_t relative)
	{
		pfn_ForEachTerrorPlayer = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(pfn_ForEachTerrorPlayer) + relative);
	}
#endif

public:
	int m_iEventType;

	CTerrorPlayer* m_pVictim;
	CTerrorPlayer* m_pAttacker;
	CTerrorPlayer* m_pInflictor;

	int m_iDamageAmount;
	bool m_bIgnoreTeamCheck;
};

class CFogVolume
{
public:
	static void* pfn_FindFogVolumeForPosition;

	static CDetour* detour_FindFogVolumeForPosition;

	static CBaseHandle hCurrentUpdatePlayer;

public:
	static inline void SetCurrentPlayer(CTerrorPlayer* pPlayer)
	{
		SetEntityHandle(hCurrentUpdatePlayer, (CBaseEntity*)pPlayer);
	}

	static inline CTerrorPlayer* GetCurrentPlayer()
	{
		return (CTerrorPlayer*)GetEntityHandle(hCurrentUpdatePlayer);
	}
};

inline CBaseEntity* GetEntityHandle(CBaseHandle& refHandle)
{
	edict_t* pEdict = gamehelpers->GetHandleEntity(refHandle);
	if (pEdict != NULL) {
		return gameents->EdictToBaseEntity(pEdict);
	}

	return NULL;
}

inline void SetEntityHandle(CBaseHandle& refHandle, CBaseEntity* pEntity)
{
	if (pEntity == NULL) {
		refHandle.Set(NULL);
		return;
	}

	gamehelpers->SetHandleEntity(refHandle, pEntity->edict());
}

CBasePlayer* UTIL_PlayerByIndex(int playerIndex)
{
	if (playerIndex <= 0 || playerIndex > playerhelpers->GetMaxClients()) {
		return NULL;
	}

	IGamePlayer* pPlayer = playerhelpers->GetGamePlayer(playerIndex);
	if (pPlayer == NULL) {
		return NULL;
	}

	return (CBasePlayer*)gameents->EdictToBaseEntity(pPlayer->GetEdict());
}

#endif // _INCLUDE_WRAPPERS_H_
