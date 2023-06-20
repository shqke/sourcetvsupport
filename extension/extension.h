#ifndef _INCLUDE_SOURCETV_SUPPORT_H_
#define _INCLUDE_SOURCETV_SUPPORT_H_

// include these before smsdk_ext.h to let use debugging versions of the memory allocators (Windows only)
// sourcetvsupport/issues/6, sourcetvsupport/issues/14
#include "sdk/public/tier1/mempool.h"
#include "sdk/engine/packed_entity.h"

#include <smsdk_ext.h>

#include <stdlib.h>
#include <stdio.h>

//#define TV_RELAYTEST

// SDK
#define TEAM_SPECTATOR			1	// spectator team

#include <netadr.h>
#include <bitbuf.h>
#include <ihltv.h>

#include <os/am-shared-library.h>
#include <os/am-path.h>

// Metamod Source
#include <sourcehook.h>

#if defined _WIN32
#	define LIBSTEAMAPI_FILE "steam_api.dll"
#else
#	define LIBSTEAMAPI_FILE "libsteam_api.so"
#endif

#include "sdk/engine/hltvdemo.h"
#include "sdk/common/netmessages.h"

enum ESocketIndex_t
{
	NS_INVALID = -1,

	NS_CLIENT = 0,	// client socket
	NS_SERVER,	// server socket
	NS_HLTV,
};

class IClient;

#define CONNECTIONLESS_HEADER			0xFFFFFFFF	// all OOB packet start with this sequence
#define S2C_CHALLENGE			'A' // + challenge value
#define S2C_CONNREJECT			'9'  // Special protocol for rejected connections.
#define PORT_SERVER			27015	// Default server port, UDP/TCP

class SMExtension :
	public SDKExtension,
	public IConCommandBaseAccessor
{
public:
	void Load();
	void Unload();

private:
	bool SetupFromGameConfig(IGameConfig* gc, char* error, int maxlength);
	bool SetupFromSteamAPILibrary(char* error, int maxlength);
	bool CreateDetours(char* error, size_t maxlength);

public:
	void OnGameServer_Init();
	void OnGameServer_Shutdown();
	void OnSetHLTVServer(IHLTVServer* pIHLTVServer);

public: // SourceHook callbacks
	void Handler_CHLTVDirector_SetHLTVServer(IHLTVServer* pHLTVServer);
	void Handler_CHLTVDemoRecorder_RecordStringTables();
	void Handler_CHLTVDemoRecorder_RecordServerClasses(ServerClass* pClasses);
	void Handler_CHLTVServer_ReplyChallenge(netadr_s& adr, CBitRead& inmsg);
	void Handler_ISteamGameServer_LogOff();
	bool Handler_CGameServer_IsPausable() const;
	void Handler_CHLTVServer_FillServerInfo(SVC_ServerInfo& serverinfo);
	void Handler_CServerGameEnts_CheckTransmit(CCheckTransmitInfo* pInfo, const unsigned short* pEdictIndices, int nEdicts);
	IClient* Handler_CHLTVServer_ConnectClient(netadr_t& adr, int protocol, int challenge, int authProtocol, const char* name,
		const char* password, const char* hashedCDkey, int cdKeyLen, CUtlVector<NetMessageCvar_t>& splitScreenClients, bool isClientLowViolence);

public: // SDKExtension
	/**
	 * @brief This is called after the initial loading sequence has been processed.
	 *
	 * @param error		Error message buffer.
	 * @param maxlen	Size of error message buffer.
	 * @param late		Whether or not the module was loaded after map load.
	 * @return			True to succeed loading, false to fail.
	 */
	bool SDK_OnLoad(char* error, size_t maxlen, bool late) override;

	/**
	 * @brief This is called right before the extension is unloaded.
	 */
	void SDK_OnUnload() override;

	/**
	 * @brief This is called once all known extensions have been loaded.
	 * Note: It is is a good idea to add natives here, if any are provided.
	 */
	void SDK_OnAllLoaded() override;

	/**
	 * @brief Called when Metamod is attached, before the extension version is called.
	 *
	 * @param error			Error buffer.
	 * @param maxlength		Maximum size of error buffer.
	 * @param late			Whether or not Metamod considers this a late load.
	 * @return				True to succeed, false to fail.
	 */
	bool SDK_OnMetamodLoad(ISmmAPI* ismm, char* error, size_t maxlength, bool late) override;

public: // IExtensionInterface
	/**
	 * @brief Asks the extension whether it's safe to remove an external
	 * interface it's using.  If it's not safe, return false, and the
	 * extension will be unloaded afterwards.
	 *
	 * NOTE: It is important to also hook NotifyInterfaceDrop() in order to clean
	 * up resources.
	 *
	 * @param pInterface		Pointer to interface being dropped.  This
	 * 							pointer may be opaque, and it should not
	 *							be queried using SMInterface functions unless
	 *							it can be verified to match an existing
	 *							pointer of known type.
	 * @return					True to continue, false to unload this
	 * 							extension afterwards.
	 */
	bool QueryInterfaceDrop(SMInterface* pInterface) override;

	/**
	 * @brief Notifies the extension that an external interface it uses is being removed.
	 *
	 * @param pInterface		Pointer to interface being dropped.  This
	 * 							pointer may be opaque, and it should not
	 *							be queried using SMInterface functions unless
	 *							it can be verified to match an existing
	 */
	void NotifyInterfaceDrop(SMInterface* pInterface) override;

	/**
	 * @brief Return false to tell Core that your extension should be considered unusable.
	 *
	 * @param error				Error buffer.
	 * @param maxlength			Size of error buffer.
	 * @return					True on success, false otherwise.
	 */
	bool QueryRunning(char* error, size_t maxlength) override;

public: // IConCommandBaseAccessor
	bool RegisterConCommandBase(ConCommandBase* pVar) override;
};

#endif // _INCLUDE_SOURCETV_SUPPORT_H_
