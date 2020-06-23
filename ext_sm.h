#ifndef _INCLUDE_EXTENSION_SM_H_
#define _INCLUDE_EXTENSION_SM_H_

#include "gamedata.h"
#include "extension.h"

#include <CDetour/detours.h>

class SMExtension :
	public CGameData,
	public IExtensionInterface
{
public:
	SMExtension();

public:
	void Load();
	void Unload();

private:
	bool CreateDetours(char *error, size_t maxlength);

	bool m_bIsLoaded;

	CDetour *detour_SteamInternal_GameServer_Init;
	CDetour *detour_SteamGameServer_Shutdown;

	CDetour *detour_CHLTVDemoRecorder_RecordStringTables;
	CDetour *detour_CBaseServer_IsExclusiveToLobbyConnections;
	CDetour *detour_CBaseClient_SendFullConnectEvent;
	CDetour *detour_CSteam3Server_NotifyClientDisconnect;
	CDetour *detour_CHLTVServer_AddNewFrame;

	ICallWrapper *vcall_CBaseServer_GetChallengeNr;
	ICallWrapper *vcall_CBaseServer_GetChallengeType;

public:
	void OnGameServer_Init();
	void OnGameServer_Shutdown();
	void OnSetHLTVServer(IHLTVServer *pHLTVServer);

public: // Wrappers for call wrappers
	int GetChallengeNr(IServer *sv, netadr_t &adr);
	int GetChallengeType(IServer *sv, netadr_t &adr);

public: // SourceHook callbacks
	void Handler_IHLTVDirector_SetHLTVServer(IHLTVServer *pHLTVServer);
	void Handler_CBaseServer_ReplyChallenge(netadr_s &adr, CBitRead &inmsg);
	void Handler_ISteamGameServer_LogOff();
	void Handler_CFrameSnapshotManager_LevelChanged();

public: // IExtensionInterface
	/**
	* @brief Called when the extension is loaded.
	*
	* @param me		Pointer back to extension.
	* @param sys		Pointer to interface sharing system of SourceMod.
	* @param error		Error buffer to print back to, if any.
	* @param maxlength	Maximum size of error buffer.
	* @param late		If this extension was loaded "late" (i.e. manually).
	* @return			True if load should continue, false otherwise.
	*/
	virtual bool OnExtensionLoad(IExtension *me, IShareSys *sys, char *error, size_t maxlength, bool late);

	/**
	* @brief Called when the extension is about to be unloaded.
	*/
	virtual void OnExtensionUnload();

	/**
	* @brief Called when all extensions are loaded (loading cycle is done).
	* If loaded late, this will be called right after OnExtensionLoad().
	*/
	virtual void OnExtensionsAllLoaded();

	/**
	* @brief Called when your pause state is about to change.
	*
	* @param pause		True if pausing, false if unpausing.
	*/
	virtual void OnExtensionPauseChange(bool pause) {}

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
	virtual bool QueryInterfaceDrop(SMInterface *pInterface);

	/**
	* @brief Notifies the extension that an external interface it uses is being removed.
	*
	* @param pInterface		Pointer to interface being dropped.  This
	* 							pointer may be opaque, and it should not
	*							be queried using SMInterface functions unless
	*							it can be verified to match an existing
	*/
	virtual void NotifyInterfaceDrop(SMInterface *pInterface);

	/**
	* @brief Return false to tell Core that your extension should be considered unusable.
	*
	* @param error				Error buffer.
	* @param maxlength			Size of error buffer.
	* @return					True on success, false otherwise.
	*/
	virtual bool QueryRunning(char *error, size_t maxlength);

	/**
	* @brief For extensions loaded through SourceMod, this should return true
	* if the extension needs to attach to Metamod:Source.  If the extension
	* is loaded through Metamod:Source, and uses SourceMod optionally, it must
	* return false.
	*
	* @return					True if Metamod:Source is needed.
	*/
	virtual bool IsMetamodExtension() { return false; }

	/**
	* @brief Must return a string containing the extension's short name.
	*
	* @return					String containing extension name.
	*/
	virtual const char *GetExtensionName() { return CONF_NAME; }

	/**
	* @brief Must return a string containing the extension's URL.
	*
	* @return					String containing extension URL.
	*/
	virtual const char *GetExtensionURL() { return CONF_URL; }

	/**
	* @brief Must return a string containing a short identifier tag.
	*
	* @return					String containing extension tag.
	*/
	virtual const char *GetExtensionTag() { return CONF_LOGTAG; }

	/**
	* @brief Must return a string containing a short author identifier.
	*
	* @return					String containing extension author.
	*/
	virtual const char *GetExtensionAuthor() { return CONF_AUTHOR; }

	/**
	* @brief Must return a string containing version information.
	*
	* Any version string format can be used, however, SourceMod
	* makes a special guarantee version numbers in the form of
	* A.B.C.D will always be fully displayed, where:
	*
	* A is a major version number of at most one digit.
	* B is a minor version number of at most two digits.
	* C is a minor version number of at most two digits.
	* D is a build number of at most 5 digits.
	*
	* Thus, thirteen characters of display is guaranteed.
	*
	* @return					String containing extension version.
	*/
	virtual const char *GetExtensionVerString() { return CONF_VERSION; }

	/**
	* @brief Must return a string containing description text.
	*
	* The description text may be longer than the other identifiers,
	* as it is only displayed when viewing one extension at a time.
	* However, it should not have newlines, or any other characters
	* which would otherwise disrupt the display pattern.
	*
	* @return					String containing extension description.
	*/
	virtual const char *GetExtensionDescription() { return CONF_DESCRIPTION; }

	/**
	* @brief Must return a string containing the compilation date.
	*
	* @return					String containing the compilation date.
	*/
	virtual const char *GetExtensionDateString() { return CONF_DATESTRING; }
};

extern SMExtension *g_Extension;

#endif
