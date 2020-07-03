#include "ext_sm.h"

#include "sdk/engine/baseclient.h"
#include "sdk/public/tier1/mempool.h"
#include "sdk/engine/clientframe.h"

IExtension* myself = NULL;
ISourceMod* g_pSM = NULL;
ISourceMod* smutils = NULL;
IShareSys* sharesys = NULL;

IGameConfigManager* gameconfs = NULL;
IPlayerManager* playerhelpers = NULL;

ISDKTools* sdktools = NULL;
IBinTools* bintools = NULL;

// Cached instances
IServer* sv = NULL;
IHLTVServer* ihltv = NULL;
IServer* hltv = NULL;

// SourceHook
SH_DECL_HOOK1_void(IHLTVDirector, SetHLTVServer, SH_NOATTRIB, 0, IHLTVServer*);
SH_DECL_HOOK0_void(IDemoRecorder, RecordStringTables, SH_NOATTRIB, 0);
SH_DECL_HOOK1_void(IDemoRecorder, RecordServerClasses, SH_NOATTRIB, 0, ServerClass*);
SH_DECL_MANUALHOOK2_void(CBaseServer_ReplyChallenge, 0, 0, 0, netadr_s&, CBitRead&);
SH_DECL_HOOK0(IServer, IsPausable, const, 0, bool);
#if SOURCE_ENGINE != SE_LEFT4DEAD
SH_DECL_HOOK0_void(ISteamGameServer, LogOff, SH_NOATTRIB, 0);
#endif

// Need this for demofile.h
bool CUtlStreamBuffer::IsOpen() const
{
	return m_hFileHandle != NULL;
}

// Detours

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

	CClientFrameManager& hltvFrameManager = smext->ref_CClientFrameManager_from_CHLTVServer(this);

	int nClientFrameCount = hltvFrameManager.CountClientFrames();
	while (nClientFrameCount > numFramesToKeep) {
		hltvFrameManager.RemoveOldestFrame();
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

#if SOURCE_ENGINE != SE_LEFT4DEAD
DETOUR_DECL_STATIC6(Handler_SteamInternal_GameServer_Init, bool, uint32, unIP, uint16, usPort, uint16, usGamePort, uint16, usQueryPort, EServerMode, eServerMode, const char*, pchVersionString)
{
	// bug##: without overriding usQueryPort parm, it uses HLTV port (if having -hltv in launch parameters), which is already bound
	// failing SteamInternal_GameServer_Init also causes game to freeze
	static ConVarRef sv_master_share_game_socket("sv_master_share_game_socket");
	usQueryPort = sv_master_share_game_socket.GetBool() ? MASTERSERVERUPDATERPORT_USEGAMESOCKETSHARE : usPort - 1;

	if (!DETOUR_STATIC_CALL(Handler_SteamInternal_GameServer_Init)(unIP, usPort, usGamePort, usQueryPort, eServerMode, pchVersionString)) {
		return false;
	}

	smext->OnGameServer_Init();

	return true;
}
#endif

DETOUR_DECL_STATIC0(Handler_SteamGameServer_Shutdown, void)
{
	smext->OnGameServer_Shutdown();

	DETOUR_STATIC_CALL(Handler_SteamGameServer_Shutdown)();
}
//

// SMExtension
SMExtension::SMExtension()
{
	m_bIsLoaded = false;

	shookid_CBaseServer_ReplyChallenge = 0;
	shookid_SteamGameServer_LogOff = 0;
	shookid_IServer_IsPausable = 0;
	shookid_IDemoRecorder_RecordStringTables = 0;
	shookid_IDemoRecorder_RecordServerClasses = 0;

	vcall_CBaseServer_GetChallengeNr = NULL;
	vcall_CBaseServer_GetChallengeType = NULL;

	detour_SteamInternal_GameServer_Init = NULL;

	detour_CBaseServer_IsExclusiveToLobbyConnections = NULL;
	detour_CBaseClient_SendFullConnectEvent = NULL;
	detour_CSteam3Server_NotifyClientDisconnect = NULL;
	detour_CHLTVServer_AddNewFrame = NULL;
}

void SMExtension::Load()
{
	if (m_bIsLoaded) {
		return;
	}

	if ((sv = sdktools->GetIServer()) == NULL) {
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

	vcall_CBaseServer_GetChallengeNr = bintools->CreateVCall(vtblindex_CBaseServer_GetChallengeNr, 0, 0, &params[0], &params[1], 1);
	if (vcall_CBaseServer_GetChallengeNr == NULL) {
		smutils->LogError(myself, "Unable to create ICallWrapper for \"CBaseServer::GetChallengeNr\"!");
	}

	vcall_CBaseServer_GetChallengeType = bintools->CreateVCall(vtblindex_CBaseServer_GetChallengeType, 0, 0, &params[0], &params[1], 1);
	if (vcall_CBaseServer_GetChallengeType == NULL) {
		smutils->LogError(myself, "Unable to create ICallWrapper for \"CBaseServer::GetChallengeType\"!");
	}

#if SOURCE_ENGINE != SE_LEFT4DEAD
	detour_SteamInternal_GameServer_Init->EnableDetour();
	detour_CBaseClient_SendFullConnectEvent->EnableDetour();
#endif

	detour_CBaseServer_IsExclusiveToLobbyConnections->EnableDetour();
	detour_CSteam3Server_NotifyClientDisconnect->EnableDetour();
	detour_CHLTVServer_AddNewFrame->EnableDetour();

#if SOURCE_ENGINE == SE_LEFT4DEAD
	shookid_IServer_IsPausable = SH_ADD_HOOK(IServer, IsPausable, sv, SH_MEMBER(this, &SMExtension::Handler_IServer_IsPausable), true);
#endif
	SH_ADD_HOOK(IHLTVDirector, SetHLTVServer, hltvdirector, SH_MEMBER(this, &SMExtension::Handler_IHLTVDirector_SetHLTVServer), true);

	OnSetHLTVServer(hltvdirector->GetHLTVServer());
	OnGameServer_Init();

	m_bIsLoaded = true;
}

void SMExtension::Unload()
{
	if (vcall_CBaseServer_GetChallengeNr != NULL) {
		vcall_CBaseServer_GetChallengeNr->Destroy();
		vcall_CBaseServer_GetChallengeNr = NULL;
	}

	if (vcall_CBaseServer_GetChallengeType != NULL) {
		vcall_CBaseServer_GetChallengeType->Destroy();
		vcall_CBaseServer_GetChallengeType = NULL;
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

	OnGameServer_Shutdown();

	SH_REMOVE_HOOK(IHLTVDirector, SetHLTVServer, hltvdirector, SH_MEMBER(this, &SMExtension::Handler_IHLTVDirector_SetHLTVServer), true);
	OnSetHLTVServer(NULL);

	m_bIsLoaded = false;

	SH_REMOVE_HOOK_ID(shookid_IServer_IsPausable);
	shookid_IServer_IsPausable = 0;

	myself = NULL;
}

bool SMExtension::CreateDetours(char* error, size_t maxlength)
{
#if SOURCE_ENGINE != SE_LEFT4DEAD
	detour_SteamInternal_GameServer_Init = DETOUR_CREATE_STATIC(Handler_SteamInternal_GameServer_Init, pfn_SteamInternal_GameServer_Init);
	if (detour_SteamInternal_GameServer_Init == NULL) {
		V_strncpy(error, "Unable to create a detour for \"SteamInternal_GameServer_Init\"", maxlength);

		return false;
	}

	detour_CBaseClient_SendFullConnectEvent = DETOUR_CREATE_MEMBER(Handler_CBaseClient_SendFullConnectEvent, pfn_CBaseClient_SendFullConnectEvent);
	if (detour_CBaseClient_SendFullConnectEvent == NULL) {
		V_strncpy(error, "Unable to create a detour for \"CBaseClient::SendFullConnectEvent\"", maxlength);

		return false;
	}
#endif

	detour_CBaseServer_IsExclusiveToLobbyConnections = DETOUR_CREATE_MEMBER(Handler_CBaseServer_IsExclusiveToLobbyConnections, pfn_CBaseServer_IsExclusiveToLobbyConnections);
	if (detour_CBaseServer_IsExclusiveToLobbyConnections == NULL) {
		V_strncpy(error, "Unable to create a detour for \"CBaseServer::IsExclusiveToLobbyConnections\"", maxlength);

		return false;
	}

	detour_CSteam3Server_NotifyClientDisconnect = DETOUR_CREATE_MEMBER(Handler_CSteam3Server_NotifyClientDisconnect, pfn_CSteam3Server_NotifyClientDisconnect);
	if (detour_CSteam3Server_NotifyClientDisconnect == NULL) {
		V_strncpy(error, "Unable to create a detour for \"CSteam3Server::NotifyClientDisconnect\"", maxlength);

		return false;
	}

	detour_CHLTVServer_AddNewFrame = DETOUR_CREATE_MEMBER(Handler_CHLTVServer_AddNewFrame, pfn_CHLTVServer_AddNewFrame);
	if (detour_CHLTVServer_AddNewFrame == NULL) {
		V_strncpy(error, "Unable to create a detour for \"CHLTVServer::AddNewFrame\"", maxlength);

		return false;
	}

	return true;
}

void SMExtension::OnGameServer_Init()
{
	OnGameServer_Shutdown();

#if SOURCE_ENGINE != SE_LEFT4DEAD
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
	if (pIHLTVServer != ihltv) {
		ihltv = pIHLTVServer;

		SH_REMOVE_HOOK_ID(shookid_CBaseServer_ReplyChallenge);
		shookid_CBaseServer_ReplyChallenge = 0;

		SH_REMOVE_HOOK_ID(shookid_IDemoRecorder_RecordStringTables);
		shookid_IDemoRecorder_RecordStringTables = 0;

		SH_REMOVE_HOOK_ID(shookid_IDemoRecorder_RecordServerClasses);
		shookid_IDemoRecorder_RecordServerClasses = 0;

		hltv = NULL;
		if (pIHLTVServer != NULL) {
			hltv = pIHLTVServer->GetBaseServer();
			if (hltv != NULL) {
				shookid_CBaseServer_ReplyChallenge = SH_ADD_MANUALVPHOOK(CBaseServer_ReplyChallenge, hltv, SH_MEMBER(this, &SMExtension::Handler_CBaseServer_ReplyChallenge), false);
			}
		}
	}

	if (hltv != NULL) {
		IDemoRecorder* recorder = ptr_m_DemoRecorder_from_IHLTVServer(ihltv)->GetDemoRecorder();
		shookid_IDemoRecorder_RecordStringTables = SH_ADD_HOOK(IDemoRecorder, RecordStringTables, recorder, SH_MEMBER(this, &SMExtension::Handler_IDemoRecorder_RecordStringTables), false);
		shookid_IDemoRecorder_RecordServerClasses = SH_ADD_HOOK(IDemoRecorder, RecordServerClasses, recorder, SH_MEMBER(this, &SMExtension::Handler_IDemoRecorder_RecordServerClasses), false);

		// bug##: in CHLTVServer::StartMaster, bot is executing "spectate" command which does nothing and it keeps him in unassigned team (index 0)
		// bot's going to fall under CDirectorSessionManager::UpdateNewPlayers's conditions to be auto-assigned to some playable team
		// enforce team change here to spectator (index 1)
		IGamePlayer* pl = playerhelpers->GetGamePlayer(pIHLTVServer->GetHLTVSlot() + 1);
		if (pl != NULL && pl->IsSourceTV()) {
			IPlayerInfo* plInfo = pl->GetPlayerInfo();
			if (plInfo != NULL && plInfo->GetTeamIndex() != TEAM_SPECTATOR) {
				Msg(PLUGIN_LOG_PREFIX "Moving \"%s\" to spectators team\n", pl->GetName());

				plInfo->ChangeTeam(TEAM_SPECTATOR);
			}
		}

		// bug#1: stringTableCRC are not set in CHLTVServer::StartMaster
		// client doesn't allow stringTableCRC to be empty
		// hltv instance must copy property stringTableCRC from sv instance
		ref_stringTableCRC_from_CBaseServer(hltv) = ref_stringTableCRC_from_CBaseServer(sv);
	}
}

int SMExtension::GetChallengeNr(IServer* pServer, netadr_t& adr)
{
	struct {
		IServer* pServer;
		netadr_s* adr;
	} stack{ pServer, &adr };

	int ret;
	vcall_CBaseServer_GetChallengeNr->Execute(&stack, &ret);

	return ret;
}

int SMExtension::GetChallengeType(IServer* pServer, netadr_t& adr)
{
	struct {
		IServer* pServer;
		netadr_s* adr;
	} stack{ pServer, &adr };

	int ret;
	vcall_CBaseServer_GetChallengeType->Execute(&stack, &ret);

	return ret;
}

void SMExtension::Handler_IHLTVDirector_SetHLTVServer(IHLTVServer* pHLTVServer)
{
	OnSetHLTVServer(pHLTVServer);
}

void SMExtension::Handler_IDemoRecorder_RecordStringTables()
{
	// bug#2
	// insufficient buffer size in CHLTVDemoRecorder::RecordStringTables, overflowing it with stringtables data (starting at CHLTVDemoRecorder::RecordStringTables)
	// stringtables wont be saved properly, causing demo file to be corrupted
	static byte s_buffer[DEMO_RECORD_BUFFER_SIZE];
	bf_write buf(s_buffer, sizeof(s_buffer));

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
		Warning("unable to record string tables\n");
	}

	IDemoRecorder* recorder = META_IFACEPTR(IDemoRecorder);
	recorder->GetDemoFile()->WriteStringTables(&buf, recorder->GetRecordingTick());

	RETURN_META(MRES_SUPERCEDE);
}

void SMExtension::Handler_IDemoRecorder_RecordServerClasses(ServerClass* pClasses)
{
	static byte s_buffer[DEMO_RECORD_BUFFER_SIZE];
	bf_write buf(s_buffer, sizeof(s_buffer));

	// Send SendTable info.
	InvokeDataTable_WriteSendTablesBuffer(pClasses, &buf);

	// Send class descriptions.
	InvokeDataTable_WriteClassInfosBuffer(pClasses, &buf);

	if (buf.IsOverflowed()) {
		Warning("unable to record server classes\n");
	}

	IDemoRecorder* recorder = META_IFACEPTR(IDemoRecorder);
	recorder->GetDemoFile()->WriteNetworkDataTables(&buf, recorder->GetRecordingTick());

	RETURN_META(MRES_SUPERCEDE);
}

void SMExtension::Handler_CBaseServer_ReplyChallenge(netadr_s& adr, CBitRead& inmsg)
{
	IServer* pServer = META_IFACEPTR(IServer);
	if (!pServer->IsHLTV()) {
		RETURN_META(MRES_IGNORED);
	}

	if (vcall_CBaseServer_GetChallengeNr == NULL || vcall_CBaseServer_GetChallengeType == NULL) {
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
	// rww: look up importance of CMaster::ShutdownConnection
	if (sv != NULL && sv->IsActive()) {
		Msg(PLUGIN_LOG_PREFIX "Game server still active - ignoring ISteamGameServer::LogOff\n");

		RETURN_META(MRES_SUPERCEDE);
	}

	RETURN_META(MRES_IGNORED);
}

bool SMExtension::Handler_IServer_IsPausable() const
{
	SET_META_RESULT(MRES_SUPERCEDE);

	static ConVarRef sv_pausable("sv_pausable");
	return sv_pausable.GetBool();
}

bool SMExtension::OnExtensionLoad(IExtension* me, IShareSys* sys, char* error, size_t maxlength, bool late)
{
	sharesys = sys;
	myself = me;

	SM_GET_IFACE(SOURCEMOD, g_pSM);
	smutils = g_pSM;
	SM_GET_IFACE(GAMECONFIG, gameconfs);
	SM_GET_IFACE(PLAYERMANAGER, playerhelpers);

	// Game config is never used by detour class to handle errors ourselves
	CDetourManager::Init(smutils->GetScriptingEngine(), NULL);

	IGameConfig* gc = NULL;
	if (!gameconfs->LoadGameConfigFile(GAMEDATA_FILE, &gc, error, maxlength)) {
		V_strncpy(error, "Unable to load a gamedata file \"" GAMEDATA_FILE ".txt\"", maxlength);

		return false;
	}

	if (!CGameData::SetupFromGameConfig(gc, error, maxlength)) {
		gameconfs->CloseGameConfigFile(gc);

		return false;
	}

	gameconfs->CloseGameConfigFile(gc);

	SH_MANUALHOOK_RECONFIGURE(CBaseServer_ReplyChallenge, vtblindex_CBaseServer_ReplyChallenge, 0, 0);

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

void SMExtension::OnExtensionUnload()
{
	Unload();
}

void SMExtension::OnExtensionsAllLoaded()
{
	SM_GET_LATE_IFACE(SDKTOOLS, sdktools);
	SM_GET_LATE_IFACE(BINTOOLS, bintools);

	if (sdktools != NULL && bintools != NULL) {
		Load();
	}
}

bool SMExtension::QueryInterfaceDrop(SMInterface* pInterface)
{
	return pInterface != bintools && pInterface != sdktools;
}

void SMExtension::NotifyInterfaceDrop(SMInterface* pInterface)
{
	OnExtensionUnload();
}

bool SMExtension::QueryRunning(char* error, size_t maxlength)
{
	SM_CHECK_IFACE(SDKTOOLS, sdktools);
	SM_CHECK_IFACE(BINTOOLS, bintools);

	return true;
}
