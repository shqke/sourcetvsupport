#ifndef _INCLUDE_PLUGIN_MM_H_
#define _INCLUDE_PLUGIN_MM_H_

#include "ext_sm.h"

/*
bug#3: "CUtlRBTree overflow!" on level change

bug##: connecting to sourcetv doesnt show any game info in steam client (not connected to a server)

bug#11: missing viewmodel attachment models (arms); world weapon model blinking
	entities, initialized as client ones, are missing:
	"
	if ( !IsServerEntity() )
	{
		m_hNetworkMoveParent = pParentEntity;
	}
	"
	in C_BaseEntity::SetParent (ref: https://github.com/ValveSoftware/source-sdk-2013/blob/0d8dceea4310fde5706b3ce1c70609d72a38efdf/mp/src/game/client/c_baseentity.cpp#L2411 )
	CHLClient::FrameStageNotify(...) sets up g_bEngineIsHLTV for sourcetv demos/broadcasts
	CHLClient::FrameStageNotify(FRAME_NET_UPDATE_POSTDATAUPDATE_END) invokes HLTVCamera()->PostEntityPacketReceived() that sets property "m_bEntityPacketReceived" to true
	CViewRender::SetUpView()
	-> C_HLTVCamera::CalcView(...) (condition "g_bEngineIsHLTV")
	-> C_HLTVCamera::FixupMovmentParents() (condition "m_bEntityPacketReceived")
	-> C_BaseEntity::HierarchyUpdateMoveParent() (on all ents)
	-> C_BaseEntity::HierarchySetParent(m_hNetworkMoveParent)
	it invalidates m_pMoveParent because m_hNetworkMoveParent wasn't set,
	making client entity of C_ViewmodelAttachmentModel to stop following C_BaseViewModel as a child and makes it displaced in 0,0,0

bug##: doors not visible with tv_transmitall 0
bug##: some models stuck midair (helicopter c8m1 post intro)

bug##: avatars aren't shown in demos
*/

class MMSPlugin:
	public SMExtension,
	public ISmmPlugin,
	public IMetamodListener,
	public IConCommandBaseAccessor
{
private:
	bool LoadSMExtension(char *error, int maxlength);
	void BindToSourceMod();

public: // ISmmPlugin
	/**
	* @brief Called on plugin load.
	*
	* This is called as DLLInit() executes - after the parameters are
	* known, but before the original GameDLL function is called.
	* Therefore, you cannot hook it, but you don't need to - Load() is
	* basically your hook.  You can override factories before the engine
	* and gamedll exchange them.  However, take care to note that if your
	* plugin is unloaded, and the gamedll/engine have cached an interface
	* you've passed, something will definitely crash.  Be careful.
	*
	* @param id		Internal id of plugin.  Saved globally by PLUGIN_SAVEVARS()
	* @param ismm		External API for SourceMM.  Saved globally by PLUGIN_SAVEVARS()
	* @param error		Error message buffer
	* @param maxlength	Size of error message buffer
	* @param late		Set to true if your plugin was loaded late (not at server load).
	* @return			True if successful, return false to reject the load.
	*/
	virtual bool Load(PluginId id, ISmmAPI * ismm, char * error, size_t maxlength, bool late);

	/**
	* @brief Called on plugin unload.  You can return false if you know
	* your plugin is not capable of restoring critical states it modifies.
	*
	* @param error		Error message buffer
	* @param maxlen	Size of error message buffer
	* @return			True on success, return false to request no unload.
	*/
	virtual bool Unload(char *error, size_t maxlen);

	/**
	* @brief Called when all plugins have been loaded.
	*
	* This is called after DLLInit(), and thus the mod has been mostly initialized.
	* It is also safe to assume that all other (automatically loaded) plugins are now
	* ready to start interacting, because they are all loaded.
	*/
	virtual void AllPluginsLoaded();

	/** @brief Return author as string */
	virtual const char *GetAuthor() { return GetExtensionAuthor(); }

	/** @brief Return plugin name as string */
	virtual const char *GetName() { return GetExtensionName(); }

	/** @brief Return a description as string */
	virtual const char *GetDescription() { return GetExtensionDescription(); }

	/** @brief Return a URL as string */
	virtual const char *GetURL() { return GetExtensionURL(); }

	/** @brief Return quick license code as string */
	virtual const char *GetLicense() { return CONF_LICENSE; }

	/** @brief Return version as string */
	virtual const char *GetVersion() { return GetExtensionVerString(); }

	/** @brief Return author as string */
	virtual const char *GetDate() { return GetExtensionDateString(); }

	/** @brief Return author as string */
	virtual const char *GetLogTag() { return GetExtensionTag(); }

public: // IMetamodListener
	/**
	* @brief Called when Metamod's own factory is invoked.
	* This can be used to provide interfaces to other plugins.
	*
	* If ret is passed, you should fill it with META_IFACE_OK or META_IFACE_FAILED.
	*
	* @param iface			Interface string.
	* @param ret			Optional pointer to store return code.
	* @return				Generic pointer to the interface, or NULL if not
	* 						found.
	*/
	virtual void *OnMetamodQuery(const char *iface, int *ret);

public: // IConCommandBaseAccessor
	// Flags is a combination of FCVAR flags in cvar.h.
	// hOut is filled in with a handle to the variable.
	virtual bool RegisterConCommandBase(ConCommandBase *pVar);
};

#endif
