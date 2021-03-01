#ifndef _INCLUDE_SOURCETV_SUPPORT_H_
#define _INCLUDE_SOURCETV_SUPPORT_H_

#include "gamedata.h"

#include <stdlib.h>
#include <stdio.h>

// SDK
#define TEAM_SPECTATOR			1	// spectator team

#include <game/server/iplayerinfo.h>
extern IPlayerInfoManager* playerinfomanager;

#include <networkstringtabledefs.h>
extern INetworkStringTableContainer* networkStringTableContainerServer;

#include <netadr.h>
#include <bitbuf.h>

#include "sdk/engine/hltvdemo.h"

enum ESocketIndex_t
{
	NS_INVALID = -1,

	NS_CLIENT = 0,	// client socket
	NS_SERVER,	// server socket
	NS_HLTV,
};

#define CONNECTIONLESS_HEADER			0xFFFFFFFF	// all OOB packet start with this sequence
#define S2C_CHALLENGE			'A' // + challenge value

// Metamod Source
#include <sourcehook.h>
using namespace SourceHook;

// SourceMod
#include <extensions/ISDKTools.h>
extern ISDKTools* sdktools;

#include <extensions/IBinTools.h>
extern IBinTools* bintools;

class CDetour;

class SMExtension :
	public CGameData,
	public SDKExtension
{
public:
	SMExtension();

public:
	void Load();
	void Unload();

private:
	bool CreateDetours(char* error, size_t maxlength);

private:
	CDetour* detour_CSteam3Server_NotifyClientDisconnect;

public:
	int shookid_CHLTVDemoRecorder_RecordStringTables;
	int shookid_CHLTVDemoRecorder_RecordServerClasses;
	int shookid_SteamGameServer_LogOff;
	int shookid_CGameServer_IsPausable;
	int shookid_CNetworkStringTable_GetStringUserData;

public:
	void OnGameServer_Init();
	void OnGameServer_Shutdown();
	void OnSetHLTVServer(IHLTVServer* pHLTVServer);

public: // Wrappers for call wrappers
	int GetChallengeNr(IServer* sv, netadr_t& adr);
	int GetChallengeType(IServer* sv, netadr_t& adr);

public: // SourceHook callbacks
	void Handler_IHLTVDirector_SetHLTVServer(IHLTVServer* pHLTVServer);
	void Handler_CHLTVDemoRecorder_RecordStringTables();
	void Handler_CHLTVDemoRecorder_RecordServerClasses(ServerClass* pClasses);
	void Handler_CHLTVServer_ReplyChallenge(netadr_s& adr, CBitRead& inmsg);
	void Handler_ISteamGameServer_LogOff();
	bool Handler_CGameServer_IsPausable() const;
	const void* Handler_CNetworkStringTable_GetStringUserData(int stringNumber, int* length) const;

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
	virtual bool QueryInterfaceDrop(SMInterface* pInterface) override;

	/**
	 * @brief Notifies the extension that an external interface it uses is being removed.
	 *
	 * @param pInterface		Pointer to interface being dropped.  This
	 * 							pointer may be opaque, and it should not
	 *							be queried using SMInterface functions unless
	 *							it can be verified to match an existing
	 */
	virtual void NotifyInterfaceDrop(SMInterface* pInterface) override;

	/**
	 * @brief Return false to tell Core that your extension should be considered unusable.
	 *
	 * @param error				Error buffer.
	 * @param maxlength			Size of error buffer.
	 * @return					True on success, false otherwise.
	 */
	virtual bool QueryRunning(char* error, size_t maxlength) override;
};

#endif // _INCLUDE_SOURCETV_SUPPORT_H_
