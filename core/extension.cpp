#include "extension.h"
#include "wrappers.h"

#include "sdk/public/engine/inetsupport.h"
#include "sdk/engine/networkstringtable.h"

#include <am-string.h>
#include <checksum_crc.h>
#include <extensions/IBinTools.h>
#include <extensions/ISDKTools.h>

// Interfaces
INetworkStringTableContainer* networkStringTableContainerServer = NULL;
IHLTVDirector* hltvdirector = NULL;
INetSupport* g_pNetSupport = NULL;
IPlayerInfoManager* playerinfomanager = NULL;
IServerGameEnts* gameents = NULL;
IBinTools* bintools = NULL;
ISDKTools* sdktools = NULL;

IServer* g_pGameIServer = NULL;

int CBasePlayer::sendprop_m_fFlags = 0;
int CBaseServer::offset_stringTableCRC = 0;
int CBaseServer::vtblindex_GetChallengeNr = 0;
int CBaseServer::vtblindex_GetChallengeType = 0;
int CBaseServer::vtblindex_ReplyChallenge = 0;
int CBaseServer::vtblindex_FillServerInfo = 0;
void* CBaseServer::pfn_IsExclusiveToLobbyConnections = NULL;
CDetour* CBaseServer::detour_IsExclusiveToLobbyConnections = NULL;
ICallWrapper* CBaseServer::vcall_GetChallengeNr = NULL;
ICallWrapper* CBaseServer::vcall_GetChallengeType = NULL;
int CHLTVServer::offset_m_DemoRecorder = 0;
int CHLTVServer::offset_CClientFrameManager = 0;
int CHLTVServer::vtblindex_FillServerInfo = 0;
int CHLTVServer::shookid_ReplyChallenge = 0;
int CHLTVServer::shookid_FillServerInfo = 0;
int CHLTVServer::shookid_thunk_FillServerInfo = 0;
void* CHLTVServer::pfn_AddNewFrame = NULL;
CDetour* CHLTVServer::detour_AddNewFrame = NULL;
int CGameServer::shookid_IsPausable = 0;
void* CBaseClient::pfn_SendFullConnectEvent = NULL;
CDetour* CBaseClient::detour_SendFullConnectEvent = NULL;
void* CSteam3Server::pfn_NotifyClientDisconnect = NULL;
CDetour* CSteam3Server::detour_NotifyClientDisconnect = NULL;
int CFrameSnapshotManager::offset_m_PackedEntitiesPool = 0;
void* CFrameSnapshotManager::pfn_LevelChanged = NULL;
CDetour* CFrameSnapshotManager::detour_LevelChanged = NULL;

int shookid_CHLTVDemoRecorder_RecordStringTables = 0;
int shookid_CHLTVDemoRecorder_RecordServerClasses = 0;
int shookid_SteamGameServer_LogOff = 0;
int shookid_CNetworkStringTable_GetStringUserData = 0;

void* pfn_DataTable_WriteSendTablesBuffer = NULL;
void* pfn_SteamGameServer_GetHSteamPipe = NULL;
void* pfn_SteamGameServer_GetHSteamUser = NULL;
void* pfn_SteamInternal_CreateInterface = NULL;
void* pfn_SteamInternal_GameServer_Init = NULL;

CDetour* detour_SteamInternal_GameServer_Init = NULL;

// SourceHook
SH_DECL_HOOK1_void(IHLTVDirector, SetHLTVServer, SH_NOATTRIB, 0, IHLTVServer*);
SH_DECL_HOOK0_void(CHLTVDemoRecorder, RecordStringTables, SH_NOATTRIB, 0);
SH_DECL_HOOK1_void(CHLTVDemoRecorder, RecordServerClasses, SH_NOATTRIB, 0, ServerClass*);
SH_DECL_MANUALHOOK2_void(CBaseServer_ReplyChallenge, 0, 0, 0, netadr_s&, CBitRead&);
SH_DECL_MANUALHOOK1_void(CBaseServer_FillServerInfo, 0, 0, 0, SVC_ServerInfo&);
SH_DECL_MANUALHOOK1_void(CHLTVServer_FillServerInfo, 0, 0, 0, SVC_ServerInfo&);
SH_DECL_HOOK0(IServer, IsPausable, const, 0, bool);
#if SOURCE_ENGINE == SE_LEFT4DEAD2
SH_DECL_HOOK0_void(ISteamGameServer, LogOff, SH_NOATTRIB, 0);
#endif
SH_DECL_HOOK2(INetworkStringTable, GetStringUserData, const, 0, const void*, int, int*);

// Detours
#include <CDetour/detours.h>

// sm1.8 support
#if !defined(DETOUR_DECL_STATIC6)
#define DETOUR_DECL_STATIC6(name, ret, p1type, p1name, p2type, p2name, p3type, p3name, p4type, p4name, p5type, p5name, p6type, p6name) \
ret (*name##_Actual)(p1type, p2type, p3type, p4type, p5type, p6type) = NULL; \
ret name(p1type p1name, p2type p2name, p3type p3name, p4type p4name, p5type p5name, p6type p6name)
#endif

// Need this for demofile.h to not link tier2
bool CUtlStreamBuffer::IsOpen() const
{
	return m_hFileHandle != NULL;
}

SMExtension g_Extension;
SMEXT_LINK(&g_Extension);

// bug#X: hltv clients are sending "player_full_connect" event
// user ids of hltv clients can collide with user ids of sv
// event "player_full_connect" fires with userid of hltv client
DETOUR_DECL_MEMBER0(Handler_CBaseClient_SendFullConnectEvent, void)
{
	CBaseClient* _this = reinterpret_cast<CBaseClient*>(this);

	IServer* pServer = _this->GetServer();
	if (pServer != NULL && pServer->IsHLTV()) {
		// CBaseClient::SendFullConnectEvent for HLTV client - intercept
		return;
	}

	DETOUR_MEMBER_CALL(Handler_CBaseClient_SendFullConnectEvent)();
}

DETOUR_DECL_MEMBER0(Handler_CBaseServer_IsExclusiveToLobbyConnections, bool)
{
	CBaseServer* _this = reinterpret_cast<CBaseServer*>(this);
	if (_this->IsHLTV()) {
		return false;
	}

	return DETOUR_MEMBER_CALL(Handler_CBaseServer_IsExclusiveToLobbyConnections)();
}

DETOUR_DECL_MEMBER1(Handler_CHLTVServer_AddNewFrame, CClientFrame*, CClientFrame*, clientFrame)
{
	CHLTVServer* _this = reinterpret_cast<CHLTVServer*>(this);

	// bug##: hibernation causes to leak memory when adding new frames to hltv
	// forcefully remove oldest frames
	CClientFrame* pFrame = DETOUR_MEMBER_CALL(Handler_CHLTVServer_AddNewFrame)(clientFrame);

	// Only keep the number of packets required to satisfy tv_delay at our tv snapshot rate
	static ConVarRef tv_delay("tv_delay"), tv_snapshotrate("tv_snapshotrate");

	int numFramesToKeep = 2 * ((1 + MAX(1.0f, tv_delay.GetFloat())) * tv_snapshotrate.GetInt());
	if (numFramesToKeep < MAX_CLIENT_FRAMES) {
		numFramesToKeep = MAX_CLIENT_FRAMES;
	}

	CClientFrameManager& frameManager = _this->GetClientFrameManager();

	int nClientFrameCount = frameManager.CountClientFrames();
	while (nClientFrameCount > numFramesToKeep) {
		frameManager.RemoveOldestFrame();
		--nClientFrameCount;
	}

	return pFrame;
}

// bug#8: ticket auth (authprotocol = 2) with hltv clients crashes server in steamclient.so on disconnect
// wrong steamid of unauthentificated hltv client passed to CSteamGameServer012::EndAuthSession
DETOUR_DECL_MEMBER1(Handler_CSteam3Server_NotifyClientDisconnect, void, CBaseClient*, client)
{
	if (!client->IsConnected() || client->IsFakeClient()) {
		return;
	}

	if (client->GetServer()->IsHLTV()) {
		return;
	}

	DETOUR_MEMBER_CALL(Handler_CSteam3Server_NotifyClientDisconnect)(client);
}

#if SOURCE_ENGINE == SE_LEFT4DEAD2
DETOUR_DECL_STATIC6(Handler_SteamInternal_GameServer_Init, bool, uint32, unIP, uint16, usPort, uint16, usGamePort, uint16, usQueryPort, EServerMode, eServerMode, const char*, pchVersionString)
{
	// bug##: without overriding usQueryPort parm, it uses HLTV port (if having -hltv in launch parameters), which is already bound
	// failing SteamInternal_GameServer_Init also causes game to freeze
	static ConVarRef sv_master_share_game_socket("sv_master_share_game_socket");
	usQueryPort = sv_master_share_game_socket.GetBool() ? MASTERSERVERUPDATERPORT_USEGAMESOCKETSHARE : usPort - 1;

	if (!DETOUR_STATIC_CALL(Handler_SteamInternal_GameServer_Init)(unIP, usPort, usGamePort, usQueryPort, eServerMode, pchVersionString)) {
		return false;
	}

	g_Extension.OnGameServer_Init();

	return true;
}
#endif

DETOUR_DECL_MEMBER0(Handler_CFrameSnapshotManager_LevelChanged, void)
{
	CFrameSnapshotManager* _this = reinterpret_cast<CFrameSnapshotManager*>(this);

	// bug##: Underlying method CClassMemoryPool::Clear creates CUtlRBTree with insufficient iterator size (unsigned short)
	// memory object of which fails to iterate over more than 65535 of packed entities
	_this->m_PackedEntitiesPool().Clear();

	// CFrameSnapshotManager::m_PackedEntitiesPool shouldn't have elements to free from now on
	DETOUR_MEMBER_CALL(Handler_CFrameSnapshotManager_LevelChanged)();
}
//

// SMExtension
void SMExtension::Load()
{
	if ((g_pGameIServer = sdktools->GetIServer()) == NULL) {
		smutils->LogError(myself, "Unable to retrieve sv instance pointer!");

		return;
	}

	SourceMod::PassInfo params[] = {
#if SMINTERFACE_BINTOOLS_VERSION == 4
		{ PassType_Basic, PASSFLAG_BYVAL, sizeof(int), NULL, 0 },
		{ PassType_Basic, PASSFLAG_BYVAL, sizeof(netadr_t*), NULL, 0 },
#else
		// sm1.9- support
		{ PassType_Basic, PASSFLAG_BYVAL, sizeof(int) },
		{ PassType_Basic, PASSFLAG_BYVAL, sizeof(netadr_t*) },
#endif
	};

	CBaseServer::vcall_GetChallengeNr = bintools->CreateVCall(CBaseServer::vtblindex_GetChallengeNr, 0, 0, &params[0], &params[1], 1);
	if (CBaseServer::vcall_GetChallengeNr == NULL) {
		smutils->LogError(myself, "Unable to create virtual call for \"CBaseServer::GetChallengeNr\"!");

		return;
	}

	CBaseServer::vcall_GetChallengeType = bintools->CreateVCall(CBaseServer::vtblindex_GetChallengeType, 0, 0, &params[0], &params[1], 1);
	if (CBaseServer::vcall_GetChallengeType == NULL) {
		smutils->LogError(myself, "Unable to create virtual call for \"CBaseServer::GetChallengeType\"!");

		return;
	}

	CBaseServer::detour_IsExclusiveToLobbyConnections->EnableDetour();
	CSteam3Server::detour_NotifyClientDisconnect->EnableDetour();
	CHLTVServer::detour_AddNewFrame->EnableDetour();
	CFrameSnapshotManager::detour_LevelChanged->EnableDetour();
#if SOURCE_ENGINE == SE_LEFT4DEAD2
	detour_SteamInternal_GameServer_Init->EnableDetour();
	CBaseClient::detour_SendFullConnectEvent->EnableDetour();
#endif

#if SOURCE_ENGINE == SE_LEFT4DEAD
	CGameServer::shookid_IsPausable = SH_ADD_HOOK(IServer, IsPausable, g_pGameIServer, SH_MEMBER(this, &SMExtension::Handler_CGameServer_IsPausable), true);
#endif
	SH_ADD_HOOK(IHLTVDirector, SetHLTVServer, hltvdirector, SH_MEMBER(this, &SMExtension::Handler_CHLTVDirector_SetHLTVServer), true);

	OnSetHLTVServer(hltvdirector->GetHLTVServer());
	OnGameServer_Init();

	// Let plugins know when it's safe to use SourceTV features
	sharesys->RegisterLibrary(myself, "sourcetvsupport");
}

void SMExtension::Unload()
{
	if (CBaseServer::vcall_GetChallengeNr != NULL) {
		CBaseServer::vcall_GetChallengeNr->Destroy();
		CBaseServer::vcall_GetChallengeNr = NULL;
	}

	if (CBaseServer::vcall_GetChallengeType != NULL) {
		CBaseServer::vcall_GetChallengeType->Destroy();
		CBaseServer::vcall_GetChallengeType = NULL;
	}

	if (CBaseServer::detour_IsExclusiveToLobbyConnections != NULL) {
		CBaseServer::detour_IsExclusiveToLobbyConnections->Destroy();
		CBaseServer::detour_IsExclusiveToLobbyConnections = NULL;
	}

	if (CHLTVServer::detour_AddNewFrame != NULL) {
		CHLTVServer::detour_AddNewFrame->Destroy();
		CHLTVServer::detour_AddNewFrame = NULL;
	}

	if (CBaseClient::detour_SendFullConnectEvent != NULL) {
		CBaseClient::detour_SendFullConnectEvent->Destroy();
		CBaseClient::detour_SendFullConnectEvent = NULL;
	}

	if (detour_SteamInternal_GameServer_Init != NULL) {
		detour_SteamInternal_GameServer_Init->Destroy();
		detour_SteamInternal_GameServer_Init = NULL;
	}

	if (CSteam3Server::detour_NotifyClientDisconnect != NULL) {
		CSteam3Server::detour_NotifyClientDisconnect->Destroy();
		CSteam3Server::detour_NotifyClientDisconnect = NULL;
	}

	if (CFrameSnapshotManager::detour_LevelChanged != NULL) {
		CFrameSnapshotManager::detour_LevelChanged->Destroy();
		CFrameSnapshotManager::detour_LevelChanged = NULL;
	}

	OnGameServer_Shutdown();

	SH_REMOVE_HOOK(IHLTVDirector, SetHLTVServer, hltvdirector, SH_MEMBER(this, &SMExtension::Handler_CHLTVDirector_SetHLTVServer), true);
	OnSetHLTVServer(NULL);

	SH_REMOVE_HOOK_ID(CGameServer::shookid_IsPausable);
	CGameServer::shookid_IsPausable = 0;
}

bool SMExtension::SetupFromGameConfig(IGameConfig* gc, char* error, int maxlength)
{
	static const struct {
		const char* key;
		int& offset;
	} s_offsets[] = {
		{ "CBaseServer::stringTableCRC", CBaseServer::offset_stringTableCRC },
		{ "CHLTVServer::CClientFrameManager", CHLTVServer::offset_CClientFrameManager },
		{ "CHLTVServer::m_DemoRecorder", CHLTVServer::offset_m_DemoRecorder },
		{ "CFrameSnapshotManager::m_PackedEntitiesPool", CFrameSnapshotManager::offset_m_PackedEntitiesPool },
		{ "CBaseServer::GetChallengeNr", CBaseServer::vtblindex_GetChallengeNr },
		{ "CBaseServer::GetChallengeType", CBaseServer::vtblindex_GetChallengeType },
		{ "CBaseServer::ReplyChallenge", CBaseServer::vtblindex_ReplyChallenge },
		{ "CBaseServer::FillServerInfo", CBaseServer::vtblindex_FillServerInfo },
		{ "CHLTVServer::FillServerInfo", CHLTVServer::vtblindex_FillServerInfo },
	};

	for (auto&& el : s_offsets) {
		if (!gc->GetOffset(el.key, &el.offset)) {
			ke::SafeSprintf(error, maxlength, "Unable to get offset for \"%s\" from game config (file: \"" GAMEDATA_FILE ".txt\")", el.key);

			return false;
		}
	}

	static const struct {
		const char* key;
		void*& address;
	} s_sigs[] = {
		{ "DataTable_WriteSendTablesBuffer", pfn_DataTable_WriteSendTablesBuffer },
		{ "CBaseServer::IsExclusiveToLobbyConnections", CBaseServer::pfn_IsExclusiveToLobbyConnections },
		{ "CSteam3Server::NotifyClientDisconnect", CSteam3Server::pfn_NotifyClientDisconnect },
		{ "CHLTVServer::AddNewFrame", CHLTVServer::pfn_AddNewFrame },
		{ "CFrameSnapshotManager::LevelChanged", CFrameSnapshotManager::pfn_LevelChanged },
#if SOURCE_ENGINE == SE_LEFT4DEAD2
		{ "CBaseClient::SendFullConnectEvent", CBaseClient::pfn_SendFullConnectEvent },
#endif
	};

	for (auto&& el : s_sigs) {
		if (!gc->GetMemSig(el.key, &el.address)) {
			ke::SafeSprintf(error, maxlength, "Unable to find signature for \"%s\" from game config (file: \"" GAMEDATA_FILE ".txt\")", el.key);

			return false;
		}

		if (el.address == NULL) {
			ke::SafeSprintf(error, maxlength, "Sigscan for \"%s\" failed (game config file: \"" GAMEDATA_FILE ".txt\")", el.key);

			return false;
		}
	}

	return true;
}

bool SMExtension::SetupFromSteamAPILibrary(char* error, int maxlength)
{
#if SOURCE_ENGINE == SE_LEFT4DEAD2
	char path[256];
	ke::path::Format(path, sizeof(path), "bin/" LIBSTEAMAPI_FILE);

	char libError[512];
	ke::RefPtr<ke::SharedLib> steam_api = ke::SharedLib::Open(path, libError, sizeof(libError));
	if (!steam_api) {
		ke::SafeSprintf(error, maxlength, "Unable to load library \"%s\" (reason: \"%s\")", path, libError);

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
	};

	for (auto&& el : s_symbols) {
		el.address = steam_api->lookup(el.symbol);
		if (el.address == NULL) {
			ke::SafeSprintf(error, maxlength, "Unable to find symbol \"%s\" (file: \"%s\")", el.symbol, path);

			return false;
		}
	}
#endif

	return true;
}

bool SMExtension::CreateDetours(char* error, size_t maxlength)
{
	// Game config is never used by detour class to handle errors ourselves
	CDetourManager::Init(smutils->GetScriptingEngine(), NULL);

#if SOURCE_ENGINE == SE_LEFT4DEAD2
	detour_SteamInternal_GameServer_Init = DETOUR_CREATE_STATIC(Handler_SteamInternal_GameServer_Init, pfn_SteamInternal_GameServer_Init);
	if (detour_SteamInternal_GameServer_Init == NULL) {
		ke::SafeStrcpy(error, maxlength, "Unable to create a detour for \"SteamInternal_GameServer_Init\"");

		return false;
	}

	CBaseClient::detour_SendFullConnectEvent = DETOUR_CREATE_MEMBER(Handler_CBaseClient_SendFullConnectEvent, CBaseClient::pfn_SendFullConnectEvent);
	if (CBaseClient::detour_SendFullConnectEvent == NULL) {
		ke::SafeStrcpy(error, maxlength, "Unable to create a detour for \"CBaseClient::SendFullConnectEvent\"");

		return false;
	}
#endif

	CBaseServer::detour_IsExclusiveToLobbyConnections = DETOUR_CREATE_MEMBER(Handler_CBaseServer_IsExclusiveToLobbyConnections, CBaseServer::pfn_IsExclusiveToLobbyConnections);
	if (CBaseServer::detour_IsExclusiveToLobbyConnections == NULL) {
		ke::SafeStrcpy(error, maxlength, "Unable to create a detour for \"CBaseServer::IsExclusiveToLobbyConnections\"");

		return false;
	}

	CSteam3Server::detour_NotifyClientDisconnect = DETOUR_CREATE_MEMBER(Handler_CSteam3Server_NotifyClientDisconnect, CSteam3Server::pfn_NotifyClientDisconnect);
	if (CSteam3Server::detour_NotifyClientDisconnect == NULL) {
		ke::SafeStrcpy(error, maxlength, "Unable to create a detour for \"CSteam3Server::NotifyClientDisconnect\"");

		return false;
	}

	CHLTVServer::detour_AddNewFrame = DETOUR_CREATE_MEMBER(Handler_CHLTVServer_AddNewFrame, CHLTVServer::pfn_AddNewFrame);
	if (CHLTVServer::detour_AddNewFrame == NULL) {
		ke::SafeStrcpy(error, maxlength, "Unable to create a detour for \"CHLTVServer::AddNewFrame\"");

		return false;
	}

	CFrameSnapshotManager::detour_LevelChanged = DETOUR_CREATE_MEMBER(Handler_CFrameSnapshotManager_LevelChanged, CFrameSnapshotManager::pfn_LevelChanged);
	if (CFrameSnapshotManager::detour_LevelChanged == NULL) {
		ke::SafeStrcpy(error, maxlength, "Unable to create a detour for \"CFrameSnapshotManager::LevelChanged\"");

		return false;
	}

	return true;
}

void SMExtension::OnGameServer_Init()
{
	OnGameServer_Shutdown();

#if SOURCE_ENGINE == SE_LEFT4DEAD2
	HSteamPipe hSteamPipe = InvokeGetHSteamPipe();
	if (hSteamPipe == 0) {
		return;
	}

	ISteamClient* pSteamClient = static_cast<ISteamClient*>(InvokeCreateInterface(STEAMCLIENT_INTERFACE_VERSION));
	if (pSteamClient == NULL) {
		return;
	}

	HSteamUser hSteamUser = InvokeGetHSteamUser();
	ISteamGameServer* pSteamGameServer = pSteamClient->GetISteamGameServer(hSteamUser, hSteamPipe, STEAMGAMESERVER_INTERFACE_VERSION);
	if (pSteamGameServer == NULL) {
		return;
	}

	shookid_SteamGameServer_LogOff = SH_ADD_HOOK(ISteamGameServer, LogOff, pSteamGameServer, SH_MEMBER(this, &SMExtension::Handler_ISteamGameServer_LogOff), false);
#endif
}

void SMExtension::OnGameServer_Shutdown()
{
	SH_REMOVE_HOOK_ID(shookid_SteamGameServer_LogOff);
	shookid_SteamGameServer_LogOff = 0;
}

void SMExtension::OnSetHLTVServer(IHLTVServer* pIHLTVServer)
{
	SH_REMOVE_HOOK_ID(CHLTVServer::shookid_ReplyChallenge);
	CHLTVServer::shookid_ReplyChallenge = 0;

	SH_REMOVE_HOOK_ID(CHLTVServer::shookid_thunk_FillServerInfo);
	CHLTVServer::shookid_thunk_FillServerInfo = 0;

	SH_REMOVE_HOOK_ID(CHLTVServer::shookid_FillServerInfo);
	CHLTVServer::shookid_FillServerInfo = 0;

	SH_REMOVE_HOOK_ID(shookid_CHLTVDemoRecorder_RecordStringTables);
	shookid_CHLTVDemoRecorder_RecordStringTables = 0;

	SH_REMOVE_HOOK_ID(shookid_CHLTVDemoRecorder_RecordServerClasses);
	shookid_CHLTVDemoRecorder_RecordServerClasses = 0;

	SH_REMOVE_HOOK_ID(shookid_CNetworkStringTable_GetStringUserData);
	shookid_CNetworkStringTable_GetStringUserData = 0;

	if (pIHLTVServer == NULL) {
		return;
	}

	CBaseServer* pServer = CBaseServer::FromIHLTVServer(pIHLTVServer);
	if (pServer == NULL) {
		return;
	}

	CHLTVServer::shookid_ReplyChallenge = SH_ADD_MANUALHOOK(CBaseServer_ReplyChallenge, pServer, SH_MEMBER(this, &SMExtension::Handler_CHLTVServer_ReplyChallenge), false);
	CHLTVServer::shookid_thunk_FillServerInfo = SH_ADD_MANUALHOOK(CBaseServer_FillServerInfo, pServer, SH_MEMBER(this, &SMExtension::Handler_CHLTVServer_FillServerInfo), true);
	
	CHLTVServer* pHLTVServer = CHLTVServer::FromBaseServer(pServer);
	CHLTVServer::shookid_FillServerInfo = SH_ADD_MANUALHOOK(CHLTVServer_FillServerInfo, pHLTVServer, SH_MEMBER(this, &SMExtension::Handler_CHLTVServer_FillServerInfo), true);

	CHLTVDemoRecorder& demoRecorder = pHLTVServer->GetDemoRecorder();
	shookid_CHLTVDemoRecorder_RecordStringTables = SH_ADD_HOOK(CHLTVDemoRecorder, RecordStringTables, &demoRecorder, SH_MEMBER(this, &SMExtension::Handler_CHLTVDemoRecorder_RecordStringTables), false);
	shookid_CHLTVDemoRecorder_RecordServerClasses = SH_ADD_HOOK(CHLTVDemoRecorder, RecordServerClasses, &demoRecorder, SH_MEMBER(this, &SMExtension::Handler_CHLTVDemoRecorder_RecordServerClasses), false);

	INetworkStringTable* pStringTableGameRules = pServer->m_StringTables()->FindTable("GameRulesCreation");
	if (pStringTableGameRules != NULL) {
		shookid_CNetworkStringTable_GetStringUserData = SH_ADD_HOOK(INetworkStringTable, GetStringUserData, pStringTableGameRules, SH_MEMBER(this, &SMExtension::Handler_CNetworkStringTable_GetStringUserData), false);
	}

	// bug##: in CHLTVServer::StartMaster, bot is executing "spectate" command which does nothing and it keeps him in unassigned team (index 0)
	// bot's going to fall under CDirectorSessionManager::UpdateNewPlayers's conditions to be auto-assigned to some playable team
	// enforce team change here to spectator (index 1)
	CBasePlayer* pPlayer = UTIL_PlayerByIndex(pIHLTVServer->GetHLTVSlot() + 1);
	if (pPlayer != NULL && pPlayer->IsHLTV()) {
		pPlayer->AddFlag(FL_FAKECLIENT);
		pPlayer->ChangeTeam(TEAM_SPECTATOR);
	}

	// bug#1: stringTableCRC are not set in CHLTVServer::StartMaster
	// client doesn't allow stringTableCRC to be empty
	// CHLTVServer instance must copy property stringTableCRC from CGameServer instance
	pServer->stringTableCRC() = CBaseServer::FromIServer(g_pGameIServer)->stringTableCRC();
}

void SMExtension::Handler_CHLTVDirector_SetHLTVServer(IHLTVServer* pIHLTVServer)
{
	OnSetHLTVServer(pIHLTVServer);
}

void SMExtension::Handler_CHLTVDemoRecorder_RecordStringTables()
{
	IDemoRecorder* _this = META_IFACEPTR(IDemoRecorder);

	// bug#2
	// insufficient buffer size in CHLTVDemoRecorder::RecordStringTables, overflowing it with stringtables data (starting at CHLTVDemoRecorder::RecordStringTables)
	// stringtables wont be saved properly, causing demo file to be corrupted
	CUtlBuffer bigBuffer;
	bigBuffer.EnsureCapacity(DEMO_RECORD_BUFFER_SIZE);
	bf_write buf(bigBuffer.Base(), DEMO_RECORD_BUFFER_SIZE);

	int numTables = networkStringTableContainerServer->GetNumTables();
	buf.WriteByte(numTables);
	for (int i = 0; i < numTables; i++) {
		INetworkStringTable* table = networkStringTableContainerServer->GetTable(i);
		buf.WriteString(table->GetTableName());

		int numstrings = table->GetNumStrings();
		buf.WriteWord(numstrings);
		for (int j = 0; j < numstrings; j++) {
			buf.WriteString(table->GetString(j));
			int userDataSize;
			const void* pUserData = table->GetStringUserData(j, &userDataSize);
			if (userDataSize > 0) {
				buf.WriteOneBit(1);
				buf.WriteShort(userDataSize);
				buf.WriteBytes(pUserData, userDataSize);
			}
			else {
				buf.WriteOneBit(0);
			}
		}

		// No client side items on server
		buf.WriteOneBit(0);
	}

	if (buf.IsOverflowed()) {
		smutils->LogError(myself, "Unable to record string tables");
	}

	_this->GetDemoFile()->WriteStringTables(&buf, _this->GetRecordingTick());

	RETURN_META(MRES_SUPERCEDE);
}

void SMExtension::Handler_CHLTVDemoRecorder_RecordServerClasses(ServerClass* pClasses)
{
	IDemoRecorder* _this = META_IFACEPTR(IDemoRecorder);

	CUtlBuffer bigBuffer;
	bigBuffer.EnsureCapacity(DEMO_RECORD_BUFFER_SIZE);
	bf_write buf(bigBuffer.Base(), DEMO_RECORD_BUFFER_SIZE);

	// Send SendTable info.
	InvokeDataTable_WriteSendTablesBuffer(pClasses, &buf);

	// Send class descriptions.
	DataTable_WriteClassInfosBuffer(pClasses, &buf);

	if (buf.IsOverflowed()) {
		smutils->LogError(myself, "Unable to record server classes");
	}

	_this->GetDemoFile()->WriteNetworkDataTables(&buf, _this->GetRecordingTick());

	RETURN_META(MRES_SUPERCEDE);
}

void SMExtension::Handler_CHLTVServer_ReplyChallenge(netadr_s& adr, CBitRead& inmsg)
{
	// rww: check if hooks right instance
	CBaseServer* _this = META_IFACEPTR(CBaseServer);

	char buffer[512];
	bf_write msg(buffer, sizeof(buffer));

	char context[256] = { 0 };
	inmsg.ReadString(context, sizeof(context));

	msg.WriteLong(CONNECTIONLESS_HEADER);
	msg.WriteByte(S2C_CHALLENGE);

	int challengeNr = _this->GetChallengeNr(adr);
	int authprotocol = _this->GetChallengeType(adr);

	msg.WriteLong(challengeNr);
	msg.WriteLong(authprotocol);

	msg.WriteShort(1);
	msg.WriteLongLong(0LL);
	msg.WriteByte(0);

	msg.WriteString(context);

	// bug#6: CBaseServer::ReplyChallenge called on CHLTVServer instance, on reserved server will force join client to a steam lobby
	// joining steam lobby will force a client to connect to game server, instead of HLTV one
	// replying with empty lobby id and no means for lobby requirement
	msg.WriteLong(g_pNetSupport->GetEngineBuildNumber());
	msg.WriteString("");
	msg.WriteByte(0);

	msg.WriteLongLong(0LL);

	g_pNetSupport->SendPacket(NULL, NS_HLTV, adr, msg.GetData(), msg.GetNumBytesWritten());

	RETURN_META(MRES_SUPERCEDE);
}

// bug##: "Connection to Steam servers lost." upon shutting hltv down
// CHLTVServer::Shutdown is invoking CBaseServer::Shutdown which calling SteamGameServer()->LogOff()
// without any condition on whether it was just hltv shut down
// as long as sv instance is still active - prevent ISteamGameServer::LogOff from being invoked
void SMExtension::Handler_ISteamGameServer_LogOff()
{
	if (g_pGameIServer != NULL && g_pGameIServer->IsActive()) {
		// Game server still active - intercept ISteamGameServer::LogOff
		RETURN_META(MRES_SUPERCEDE);
	}

	RETURN_META(MRES_IGNORED);
}

bool SMExtension::Handler_CGameServer_IsPausable() const
{
	static ConVarRef sv_pausable("sv_pausable");
	RETURN_META_VALUE(MRES_SUPERCEDE, sv_pausable.GetBool());
}

const void* SMExtension::Handler_CNetworkStringTable_GetStringUserData(int stringNumber, int* length) const
{
	CNetworkStringTable* _this = META_IFACEPTR(CNetworkStringTable);
	RETURN_META_VALUE(MRES_SUPERCEDE, _this->GetStringUserDataFixed(stringNumber, length));
}

void SMExtension::Handler_CHLTVServer_FillServerInfo(SVC_ServerInfo& serverinfo)
{
	// feature request #12 - allow addons in demos
	serverinfo.m_bIsVanilla = false;
}

bool SMExtension::SDK_OnLoad(char* error, size_t maxlength, bool late)
{
	sm_sendprop_info_t info;
	if (!gamehelpers->FindSendPropInfo("CBasePlayer", "m_fFlags", &info)) {
		ke::SafeStrcpy(error, maxlength, "Unable to find SendProp \"CBasePlayer::m_fFlags\"");

		return false;
	}

	CBasePlayer::sendprop_m_fFlags = info.actual_offset;

	IGameConfig* gc = NULL;
	if (!gameconfs->LoadGameConfigFile(GAMEDATA_FILE, &gc, error, maxlength) || gc == NULL) {
		ke::SafeStrcpy(error, maxlength, "Unable to load a gamedata file \"" GAMEDATA_FILE ".txt\"");

		return false;
	}

	if (!SetupFromGameConfig(gc, error, maxlength)) {
		gameconfs->CloseGameConfigFile(gc);

		return false;
	}

	gameconfs->CloseGameConfigFile(gc);

	SH_MANUALHOOK_RECONFIGURE(CBaseServer_ReplyChallenge, CBaseServer::vtblindex_ReplyChallenge, 0, 0);
	SH_MANUALHOOK_RECONFIGURE(CBaseServer_FillServerInfo, CBaseServer::vtblindex_FillServerInfo, 0, 0);
	SH_MANUALHOOK_RECONFIGURE(CHLTVServer_FillServerInfo, CHLTVServer::vtblindex_FillServerInfo, 0, 0);

	// Retrieve addresses from steam_api shared library
	if (!SetupFromSteamAPILibrary(error, maxlength)) {
		return false;
	}

	if (!CreateDetours(error, maxlength)) {
		return false;
	}

	sharesys->AddDependency(myself, "bintools.ext", true, true);
	sharesys->AddDependency(myself, "sdktools.ext", true, true);

	return true;
}

void SMExtension::SDK_OnUnload()
{
	Unload();
}

void SMExtension::SDK_OnAllLoaded()
{
	SM_GET_LATE_IFACE(SDKTOOLS, sdktools);
	SM_GET_LATE_IFACE(BINTOOLS, bintools);

	if (sdktools != NULL && bintools != NULL) {
		Load();
	}
}

bool SMExtension::SDK_OnMetamodLoad(ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	GET_V_IFACE_CURRENT(GetEngineFactory, networkStringTableContainerServer, INetworkStringTableContainer, INTERFACENAME_NETWORKSTRINGTABLESERVER);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pNetSupport, INetSupport, INETSUPPORT_VERSION_STRING);

	GET_V_IFACE_CURRENT(GetServerFactory, hltvdirector, IHLTVDirector, INTERFACEVERSION_HLTVDIRECTOR);
	GET_V_IFACE_CURRENT(GetServerFactory, playerinfomanager, IPlayerInfoManager, INTERFACEVERSION_PLAYERINFOMANAGER);
	GET_V_IFACE_CURRENT(GetServerFactory, gameents, IServerGameEnts, INTERFACEVERSION_SERVERGAMEENTS);

	// For ConVarRef
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);

	return true;
}

bool SMExtension::QueryInterfaceDrop(SMInterface* pInterface)
{
	return pInterface != bintools && pInterface != sdktools;
}

void SMExtension::NotifyInterfaceDrop(SMInterface* pInterface)
{
	SDK_OnUnload();
}

bool SMExtension::QueryRunning(char* error, size_t maxlength)
{
	SM_CHECK_IFACE(SDKTOOLS, sdktools);
	SM_CHECK_IFACE(BINTOOLS, bintools);

	return true;
}
