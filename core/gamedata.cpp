#include "gamedata.h"

#include <os/am-shared-library.h>
#include <os/am-path.h>

#if defined _WIN32
#define LIBSTEAMAPI_FILE "steam_api.dll"
#else
#define LIBSTEAMAPI_FILE "libsteam_api.so"
#endif

bool CGameData::SetupFromGameConfig(IGameConfig* gc, char* error, int maxlength)
{
	static const struct {
		const char* key;
		int& offset;
	} s_offsets[] = {
		{ "CBaseServer::stringTableCRC", property_CBaseServer_stringTableCRC },
		{ "CHLTVServer::CClientFrameManager", offset_CHLTVServer_CClientFrameManager },
		{ "CHLTVServer::IHLTVServer", offset_CHLTVServer_IHLTVServer },
		{ "CHLTVServer::m_DemoRecorder", offset_CHLTVServer_m_DemoRecorder },
		{ "CBaseServer::GetChallengeNr", vtblindex_CBaseServer_GetChallengeNr },
		{ "CBaseServer::GetChallengeType", vtblindex_CBaseServer_GetChallengeType },
		{ "CBaseServer::ReplyChallenge", vtblindex_CBaseServer_ReplyChallenge },
	};

	for (auto&& el : s_offsets) {
		if (!gc->GetOffset(el.key, &el.offset)) {
			V_snprintf(error, maxlength, "Unable to get offset for \"%s\" from game config (file: \"" GAMEDATA_FILE ".txt\")", el.key);

			return false;
		}
	}

	static const struct {
		const char* key;
		void*& address;
	} s_sigs[] = {
		{ "DataTable_WriteSendTablesBuffer", pfn_DataTable_WriteSendTablesBuffer },
		{ "DataTable_WriteClassInfosBuffer", pfn_DataTable_WriteClassInfosBuffer },
		{ "CBaseServer::IsExclusiveToLobbyConnections", pfn_CBaseServer_IsExclusiveToLobbyConnections },
#if SOURCE_ENGINE == SE_LEFT4DEAD2
		{ "CBaseClient::SendFullConnectEvent", pfn_CBaseClient_SendFullConnectEvent },
#endif
		{ "CSteam3Server::NotifyClientDisconnect", pfn_CSteam3Server_NotifyClientDisconnect },
		{ "CHLTVServer::AddNewFrame", pfn_CHLTVServer_AddNewFrame },
	};

	for (auto&& el : s_sigs) {
		if (!gc->GetMemSig(el.key, &el.address)) {
			V_snprintf(error, maxlength, "Unable to find signature for \"%s\" from game config (file: \"" GAMEDATA_FILE ".txt\")", el.key);

			return false;
		}

		if (el.address == NULL) {
			V_snprintf(error, maxlength, "Sigscan for \"%s\" failed (game config file: \"" GAMEDATA_FILE ".txt\")", el.key);

			return false;
		}
	}

	return true;
}

bool CGameData::SetupFromSteamAPILibrary(char* error, int maxlength)
{
#if SOURCE_ENGINE != SE_LEFT4DEAD
	char path[256];
	ke::path::Format(path, sizeof(path), "bin/" LIBSTEAMAPI_FILE);

	char failReason[512];
	ke::RefPtr<ke::SharedLib> steam_api = ke::SharedLib::Open(path, failReason, sizeof(failReason));
	if (!steam_api) {
		V_snprintf(error, maxlength, "Unable to load library \"%s\" (reason: \"%s\")", path, failReason);

		return false;
	}

	static const struct {
		const char* symbol;
		void*& address;
	} s_symbols[] = {
		{ "SteamInternal_CreateInterface", pfn_SteamInternal_CreateInterface },
		{ "SteamInternal_GameServer_Init", pfn_SteamInternal_GameServer_Init },
		{ "SteamGameServer_GetHSteamPipe", pfn_SteamGameServer_GetHSteamPipe },
		{ "SteamGameServer_GetHSteamUser", pfn_SteamGameServer_GetHSteamUser },
		{ "SteamGameServer_Shutdown", pfn_SteamGameServer_Shutdown },
	};

	for (auto&& el : s_symbols) {
		el.address = steam_api->lookup(el.symbol);
		if (el.address == NULL) {
			V_snprintf(error, maxlength, "Unable to find symbol \"%s\" (file: \"%s\")", el.symbol, path);

			return false;
		}
	}
#endif

	return true;
}
