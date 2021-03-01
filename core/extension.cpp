#include "extension.h"
#include "wrappers.h"

#include "sdk/public/engine/inetsupport.h"

#include "sdk/engine/baseclient.h"
#include "sdk/engine/clientframe.h"
#include "sdk/engine/networkstringtable.h"

#include <am-string.h>

// Interfaces
INetworkStringTableContainer* networkStringTableContainerServer = NULL;
IHLTVDirector* hltvdirector = NULL;
INetSupport* g_pNetSupport = NULL;
IPlayerInfoManager* playerinfomanager = NULL;
IServerGameEnts* gameents = NULL;

ISDKTools* sdktools = NULL;
IBinTools* bintools = NULL;

// Cached instances
IServer* g_pGameServer = NULL;

int CBasePlayer::sendprop_m_fFlags = 0;
int CBaseServer::offset_stringTableCRC = 0;
int CBaseServer::vtblindex_GetChallengeNr = 0;
int CBaseServer::vtblindex_GetChallengeType = 0;
int CBaseServer::vtblindex_ReplyChallenge = 0;
ICallWrapper* CBaseServer::vcall_GetChallengeNr = NULL;
ICallWrapper* CBaseServer::vcall_GetChallengeType = NULL;
int CHLTVServer::offset_m_DemoRecorder = 0;
int CHLTVServer::offset_CClientFrameManager = 0;
int CFrameSnapshotManager::offset_m_PackedEntitiesPool = 0;

// SourceHook
SH_DECL_HOOK1_void(IHLTVDirector, SetHLTVServer, SH_NOATTRIB, 0, IHLTVServer*);
SH_DECL_HOOK0_void(CHLTVDemoRecorder, RecordStringTables, SH_NOATTRIB, 0);
SH_DECL_HOOK1_void(CHLTVDemoRecorder, RecordServerClasses, SH_NOATTRIB, 0, ServerClass*);
SH_DECL_MANUALHOOK2_void(CBaseServer_ReplyChallenge, 0, 0, 0, netadr_s&, CBitRead&);
SH_DECL_HOOK0(IServer, IsPausable, const, 0, bool);
#if SOURCE_ENGINE == SE_LEFT4DEAD2
SH_DECL_HOOK0_void(ISteamGameServer, LogOff, SH_NOATTRIB, 0);
#endif
SH_DECL_HOOK2(CNetworkStringTable, GetStringUserData, const, 0, const void*, int, int*);

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
	const IServer* pServer = reinterpret_cast<CBaseClient*>(this)->GetServer();
	if (pServer != NULL && pServer->IsHLTV()) {
		// rww: progress in loading screen for hltv client
		Msg(PLUGIN_LOG_PREFIX "CBaseClient::SendFullConnectEvent for HLTV client - ignoring\n");

		return;
	}

	DETOUR_MEMBER_CALL(Handler_CBaseClient_SendFullConnectEvent)();
}

DETOUR_DECL_MEMBER0(Handler_CBaseServer_IsExclusiveToLobbyConnections, bool)
{
	const IServer* pServer = reinterpret_cast<IServer*>(this);
	if (pServer->IsHLTV()) {
		return false;
	}

	return DETOUR_MEMBER_CALL(Handler_CBaseServer_IsExclusiveToLobbyConnections)();
}

DETOUR_DECL_MEMBER1(Handler_CHLTVServer_AddNewFrame, CClientFrame*, CClientFrame*, clientFrame)
{
	// bug##: hibernation causes to leak memory when adding new frames to hltv
	// forcefully remove oldest frames
	CClientFrame* pRet = DETOUR_MEMBER_CALL(Handler_CHLTVServer_AddNewFrame)(clientFrame);

	// Only keep the number of packets required to satisfy tv_delay at our tv snapshot rate
	static ConVarRef tv_delay("tv_delay"), tv_snapshotrate("tv_snapshotrate");

	int numFramesToKeep = 2 * ((1 + MAX(1.0f, tv_delay.GetFloat())) * tv_snapshotrate.GetInt());
	if (numFramesToKeep < MAX_CLIENT_FRAMES) {
		numFramesToKeep = MAX_CLIENT_FRAMES;
	}

	CClientFrameManager& frameManager = META_IFACEPTR(CHLTVServer)->GetClientFrameManager();

	int nClientFrameCount = frameManager.CountClientFrames();
	while (nClientFrameCount > numFramesToKeep) {
		frameManager.RemoveOldestFrame();
		--nClientFrameCount;
	}

	return pRet;
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

	return DETOUR_MEMBER_CALL(Handler_CSteam3Server_NotifyClientDisconnect)(client);
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
	// bug##: Underlying method CClassMemoryPool::Clear creates CUtlRBTree with insufficient iterator size (unsigned short)
	// memory object of which fails to iterate over more than 65535 of packed entities
	g_Extension.ref_m_PackedEntitiesPool_from_CFrameSnapshotManager(this).Clear();

	// CFrameSnapshotManager::m_PackedEntitiesPool shouldn't have elements to free from now on
	DETOUR_MEMBER_CALL(Handler_CFrameSnapshotManager_LevelChanged)();
}
//

// SMExtension
SMExtension::SMExtension()
{
	shookid_CHLTVServer_ReplyChallenge = 0;
	shookid_SteamGameServer_LogOff = 0;
	shookid_IServer_IsPausable = 0;
	shookid_CHLTVDemoRecorder_RecordStringTables = 0;
	shookid_CHLTVDemoRecorder_RecordServerClasses = 0;
	shookid_CNetworkStringTable_GetStringUserData = 0;

	detour_SteamInternal_GameServer_Init = NULL;
	detour_CBaseServer_IsExclusiveToLobbyConnections = NULL;
	detour_CBaseClient_SendFullConnectEvent = NULL;
	detour_CSteam3Server_NotifyClientDisconnect = NULL;
	detour_CHLTVServer_AddNewFrame = NULL;
	detour_CFrameSnapshotManager_LevelChanged = NULL;
}

void SMExtension::Load()
{
	if ((g_pGameServer = sdktools->GetIServer()) == NULL) {
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

	detour_CBaseServer_IsExclusiveToLobbyConnections->EnableDetour();
	detour_CSteam3Server_NotifyClientDisconnect->EnableDetour();
	detour_CHLTVServer_AddNewFrame->EnableDetour();
	detour_CFrameSnapshotManager_LevelChanged->EnableDetour();
#if SOURCE_ENGINE == SE_LEFT4DEAD2
	detour_SteamInternal_GameServer_Init->EnableDetour();
	detour_CBaseClient_SendFullConnectEvent->EnableDetour();
#endif

#if SOURCE_ENGINE == SE_LEFT4DEAD
	shookid_IServer_IsPausable = SH_ADD_HOOK(IServer, IsPausable, g_pGameServer, SH_MEMBER(this, &SMExtension::Handler_IServer_IsPausable), true);
#endif
	SH_ADD_HOOK(IHLTVDirector, SetHLTVServer, hltvdirector, SH_MEMBER(this, &SMExtension::Handler_IHLTVDirector_SetHLTVServer), true);

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
		CBaseServer::vcall_GetChallengeNr->Destroy();
		CBaseServer::vcall_GetChallengeNr = NULL;
	}

	if (detour_SteamInternal_GameServer_Init != NULL) {
		detour_SteamInternal_GameServer_Init->Destroy();
		detour_SteamInternal_GameServer_Init = NULL;
	}

	if (detour_CBaseServer_IsExclusiveToLobbyConnections != NULL) {
		detour_CBaseServer_IsExclusiveToLobbyConnections->Destroy();
		detour_CBaseServer_IsExclusiveToLobbyConnections = NULL;
	}

	if (detour_CBaseClient_SendFullConnectEvent != NULL) {
		detour_CBaseClient_SendFullConnectEvent->Destroy();
		detour_CBaseClient_SendFullConnectEvent = NULL;
	}

	if (detour_CSteam3Server_NotifyClientDisconnect != NULL) {
		detour_CSteam3Server_NotifyClientDisconnect->Destroy();
		detour_CSteam3Server_NotifyClientDisconnect = NULL;
	}

	if (detour_CHLTVServer_AddNewFrame != NULL) {
		detour_CHLTVServer_AddNewFrame->Destroy();
		detour_CHLTVServer_AddNewFrame = NULL;
	}

	if (detour_CFrameSnapshotManager_LevelChanged != NULL) {
		detour_CFrameSnapshotManager_LevelChanged->Destroy();
		detour_CFrameSnapshotManager_LevelChanged = NULL;
	}

	OnGameServer_Shutdown();

	SH_REMOVE_HOOK(IHLTVDirector, SetHLTVServer, hltvdirector, SH_MEMBER(this, &SMExtension::Handler_IHLTVDirector_SetHLTVServer), true);
	OnSetHLTVServer(NULL);

	SH_REMOVE_HOOK_ID(shookid_IServer_IsPausable);
	shookid_IServer_IsPausable = 0;
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

	detour_CBaseClient_SendFullConnectEvent = DETOUR_CREATE_MEMBER(Handler_CBaseClient_SendFullConnectEvent, pfn_CBaseClient_SendFullConnectEvent);
	if (detour_CBaseClient_SendFullConnectEvent == NULL) {
		ke::SafeStrcpy(error, maxlength, "Unable to create a detour for \"CBaseClient::SendFullConnectEvent\"");

		return false;
	}
#endif

	detour_CBaseServer_IsExclusiveToLobbyConnections = DETOUR_CREATE_MEMBER(Handler_CBaseServer_IsExclusiveToLobbyConnections, pfn_CBaseServer_IsExclusiveToLobbyConnections);
	if (detour_CBaseServer_IsExclusiveToLobbyConnections == NULL) {
		ke::SafeStrcpy(error, maxlength, "Unable to create a detour for \"CBaseServer::IsExclusiveToLobbyConnections\"");

		return false;
	}

	detour_CSteam3Server_NotifyClientDisconnect = DETOUR_CREATE_MEMBER(Handler_CSteam3Server_NotifyClientDisconnect, pfn_CSteam3Server_NotifyClientDisconnect);
	if (detour_CSteam3Server_NotifyClientDisconnect == NULL) {
		ke::SafeStrcpy(error, maxlength, "Unable to create a detour for \"CSteam3Server::NotifyClientDisconnect\"");

		return false;
	}

	detour_CHLTVServer_AddNewFrame = DETOUR_CREATE_MEMBER(Handler_CHLTVServer_AddNewFrame, pfn_CHLTVServer_AddNewFrame);
	if (detour_CHLTVServer_AddNewFrame == NULL) {
		ke::SafeStrcpy(error, maxlength, "Unable to create a detour for \"CHLTVServer::AddNewFrame\"");

		return false;
	}

	detour_CFrameSnapshotManager_LevelChanged = DETOUR_CREATE_MEMBER(Handler_CFrameSnapshotManager_LevelChanged, pfn_CFrameSnapshotManager_LevelChanged);
	if (detour_CFrameSnapshotManager_LevelChanged == NULL) {
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
	SH_REMOVE_HOOK_ID(shookid_CHLTVServer_ReplyChallenge);
	shookid_CHLTVServer_ReplyChallenge = 0;

	SH_REMOVE_HOOK_ID(shookid_CHLTVDemoRecorder_RecordStringTables);
	shookid_CHLTVDemoRecorder_RecordStringTables = 0;

	SH_REMOVE_HOOK_ID(shookid_CHLTVDemoRecorder_RecordServerClasses);
	shookid_CHLTVDemoRecorder_RecordServerClasses = 0;

	SH_REMOVE_HOOK_ID(shookid_CNetworkStringTable_GetStringUserData);
	shookid_CNetworkStringTable_GetStringUserData = 0;

	if (pIHLTVServer == NULL) {
		return;
	}

	CBaseServer* pBaseServer = CBaseServer::FromIHLTVServer(pIHLTVServer);
	if (pBaseServer == NULL) {
		return;
	}

	shookid_CHLTVServer_ReplyChallenge = SH_ADD_MANUALHOOK(CBaseServer_ReplyChallenge, pBaseServer, SH_MEMBER(this, &SMExtension::Handler_CHLTVServer_ReplyChallenge), false);

	CHLTVDemoRecorder& demoRecorder = CHLTVServer::FromBaseServer(pBaseServer)->GetDemoRecorder();
	shookid_CHLTVDemoRecorder_RecordStringTables = SH_ADD_HOOK(CHLTVDemoRecorder, RecordStringTables, &demoRecorder, SH_MEMBER(this, &SMExtension::Handler_CHLTVDemoRecorder_RecordStringTables), false);
	shookid_CHLTVDemoRecorder_RecordServerClasses = SH_ADD_HOOK(CHLTVDemoRecorder, RecordServerClasses, &demoRecorder, SH_MEMBER(this, &SMExtension::Handler_CHLTVDemoRecorder_RecordServerClasses), false);

	INetworkStringTable* pStringTableGameRules = networkStringTableContainerServer->FindTable("GameRulesCreation");
	if (pStringTableGameRules != NULL) {
		shookid_CNetworkStringTable_GetStringUserData = SH_ADD_VPHOOK(CNetworkStringTable, GetStringUserData, pStringTableGameRules, SH_MEMBER(this, &SMExtension::Handler_CNetworkStringTable_GetStringUserData), false);
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
	pBaseServer->stringTableCRC() = CBaseServer::FromIServer(g_pGameServer)->stringTableCRC();
}

void SMExtension::Handler_IHLTVDirector_SetHLTVServer(IHLTVServer* pHLTVServer)
{
	OnSetHLTVServer(pHLTVServer);
}

void SMExtension::Handler_CHLTVDemoRecorder_RecordStringTables()
{
	// bug#2
	// insufficient buffer size in CHLTVDemoRecorder::RecordStringTables, overflowing it with stringtables data (starting at CHLTVDemoRecorder::RecordStringTables)
	// stringtables wont be saved properly, causing demo file to be corrupted
	CUtlBuffer bigBuff;
	bigBuff.EnsureCapacity(DEMO_RECORD_BUFFER_SIZE);
	bf_write buf(bigBuff.Base(), DEMO_RECORD_BUFFER_SIZE);

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

	IDemoRecorder* pDemoRecorder = META_IFACEPTR(IDemoRecorder);
	pDemoRecorder->GetDemoFile()->WriteStringTables(&buf, pDemoRecorder->GetRecordingTick());

	RETURN_META(MRES_SUPERCEDE);
}

void SMExtension::Handler_CHLTVDemoRecorder_RecordServerClasses(ServerClass* pClasses)
{
	CUtlBuffer bigBuff;
	bigBuff.EnsureCapacity(DEMO_RECORD_BUFFER_SIZE);
	bf_write buf(bigBuff.Base(), DEMO_RECORD_BUFFER_SIZE);

	// Send SendTable info.
	InvokeDataTable_WriteSendTablesBuffer(pClasses, &buf);

	// Send class descriptions.
	DataTable_WriteClassInfosBuffer(pClasses, &buf);

	if (buf.IsOverflowed()) {
		smutils->LogError(myself, "Unable to record server classes");
	}

	IDemoRecorder* pDemoRecorder = META_IFACEPTR(IDemoRecorder);
	pDemoRecorder->GetDemoFile()->WriteNetworkDataTables(&buf, pDemoRecorder->GetRecordingTick());

	RETURN_META(MRES_SUPERCEDE);
}

void SMExtension::Handler_CHLTVServer_ReplyChallenge(netadr_s& adr, CBitRead& inmsg)
{
	// rww: check if hooks right instance
	IServer* pServer = META_IFACEPTR(IServer);
	if (!pServer->IsHLTV()) {
		RETURN_META(MRES_IGNORED);
	}

	char buffer[512];
	bf_write msg(buffer, sizeof(buffer));

	char context[256] = { 0 };
	inmsg.ReadString(context, sizeof(context));

	msg.WriteLong(CONNECTIONLESS_HEADER);
	msg.WriteByte(S2C_CHALLENGE);

	int challengeNr = GetChallengeNr(pServer, adr);
	int authprotocol = GetChallengeType(pServer, adr);

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
	if (g_pGameServer != NULL && g_pGameServer->IsActive()) {
		// Game server still active - intercept ISteamGameServer::LogOff
		RETURN_META(MRES_SUPERCEDE);
	}

	RETURN_META(MRES_IGNORED);
}

bool SMExtension::Handler_IServer_IsPausable() const
{
	static ConVarRef sv_pausable("sv_pausable");
	RETURN_META_VALUE(MRES_SUPERCEDE, sv_pausable.GetBool());
}

const void* SMExtension::Handler_CNetworkStringTable_GetStringUserData(int stringNumber, int* length) const
{
	CNetworkStringTable* _this = META_IFACEPTR(CNetworkStringTable);
	RETURN_META_VALUE(MRES_SUPERCEDE, _this->GetStringUserDataFixed(stringNumber, length));
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

	if (!CGameData::SetupFromGameConfig(gc, error, maxlength)) {
		gameconfs->CloseGameConfigFile(gc);

		return false;
	}

	gameconfs->CloseGameConfigFile(gc);

	SH_MANUALHOOK_RECONFIGURE(CBaseServer_ReplyChallenge, CBaseServer::vtblindex_ReplyChallenge, 0, 0);

	// Retrieve addresses from steam_api shared library
	if (!CGameData::SetupFromSteamAPILibrary(error, maxlength)) {
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
