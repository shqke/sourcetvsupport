#ifndef _INCLUDE_GAMEDATA_H_
#define _INCLUDE_GAMEDATA_H_

#include "config.h"
#include <strtools.h>
#include <checksum_crc.h>
#include <iserver.h>

// Updated SDK
#include "sdk/public/steam/steam_gameserver.h"

#include <IGameConfigs.h>
using namespace SourceMod;

class CHLTVServer;
class CClientFrameManager;

class CGameData
{
public:
	CGameData()
	{
		shookid_CBaseServer_ReplyChallenge = 0;
		shookid_SteamGameServer_LogOff = 0;
	}

	bool SetupFromGameConfig(IGameConfig *gc, char *error, int maxlength);

	bool SetupFromSteamAPILibrary(char *error, int maxlength);

public: // Steam API
	void *InvokeCreateInterface(const char *ver)
	{
		return reinterpret_cast<decltype(SteamInternal_CreateInterface) *>(pfn_SteamInternal_CreateInterface)(ver);
	}

	HSteamPipe InvokeGetHSteamPipe()
	{
		return reinterpret_cast<decltype(SteamGameServer_GetHSteamPipe) *>(pfn_SteamGameServer_GetHSteamPipe)();
	}

	HSteamUser InvokeGetHSteamUser()
	{
		return reinterpret_cast<decltype(SteamGameServer_GetHSteamUser) *>(pfn_SteamGameServer_GetHSteamUser)();
	}

protected:
	void *pfn_SteamInternal_CreateInterface;
	void *pfn_SteamGameServer_GetHSteamPipe;
	void *pfn_SteamGameServer_GetHSteamUser;
	void *pfn_SteamInternal_GameServer_Init;
	void *pfn_SteamGameServer_Shutdown;

public:
	CRC32_t &ref_stringTableCRC(IServer *pServer) {
		return *(CRC32_t *)((byte *)pServer + property_CBaseServer_stringTableCRC);
	}

	CClientFrameManager &ref_CClientFrameManager(CHLTVServer *pHLTVServer) {
		return *(CClientFrameManager *)((byte *)pHLTVServer + offset_CHLTVServer_CClientFrameManager);
	}

protected:
	int property_CBaseServer_stringTableCRC;
	int offset_CHLTVServer_CClientFrameManager;

protected:
	void *pfn_CHLTVDemoRecorder_RecordStringTables;
	void *pfn_CBaseServer_IsExclusiveToLobbyConnections;
	void *pfn_CBaseClient_SendFullConnectEvent;
	void *pfn_CSteam3Server_NotifyClientDisconnect;
	void *pfn_CHLTVServer_AddNewFrame;

protected:
	int vtblindex_CBaseServer_GetChallengeNr;
	int vtblindex_CBaseServer_GetChallengeType;
	int vtblindex_CBaseServer_ReplyChallenge;

public:
	int shookid_CBaseServer_ReplyChallenge;
	int shookid_SteamGameServer_LogOff;
};

#endif // _INCLUDE_GAMEDATA_H_
