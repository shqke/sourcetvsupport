#ifndef _INCLUDE_SOURCETV_SUPPORT_CONFIG_H_
#define _INCLUDE_SOURCETV_SUPPORT_CONFIG_H_

/*
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

bug##: hltv sends no update for clients
*/

// Configuration
#define SMEXT_CONF_NAME			"SourceTV Support"
#define SMEXT_CONF_DESCRIPTION	"Restores broadcasting/recording SourceTV features in Left 4 Dead engine"
#define SMEXT_CONF_VERSION		"1.0"
#define SMEXT_CONF_AUTHOR		"Evgeniy \"shqke\" Kazakov"
#define SMEXT_CONF_URL			"https://github.com/shqke/sourcetvsupport"
#define SMEXT_CONF_LOGTAG		"STVS"
#define SMEXT_CONF_LICENSE		"GPL"
#define SMEXT_CONF_DATESTRING	__DATE__

#define GAMEDATA_FILE		"sourcetvsupport"

#define PLUGIN_LOG_PREFIX	"[" SMEXT_CONF_LOGTAG "] "

#define SMEXT_CONF_METAMOD
#define SMEXT_ENABLE_PLAYERHELPERS
#define SMEXT_ENABLE_GAMECONF
#define SMEXT_ENABLE_GAMEHELPERS

#define SMEXT_LINK(name) SDKExtension *g_pExtensionIface = name;

#endif // _INCLUDE_SOURCETV_SUPPORT_CONFIG_H_