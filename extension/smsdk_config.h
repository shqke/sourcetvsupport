#ifndef _INCLUDE_SOURCETV_SUPPORT_CONFIG_H_
#define _INCLUDE_SOURCETV_SUPPORT_CONFIG_H_

// Configuration
#define SMEXT_CONF_NAME			"SourceTV Support"
#define SMEXT_CONF_DESCRIPTION	"Restores broadcasting/recording SourceTV features in Left 4 Dead engine"
#define SMEXT_CONF_VERSION		"0.9.15"
#define SMEXT_CONF_AUTHOR		"Evgeniy \"shqke\" Kazakov"
#define SMEXT_CONF_URL			"https://github.com/shqke/sourcetvsupport"
#define SMEXT_CONF_LOGTAG		"STVS"
#define SMEXT_CONF_LICENSE		"GPL"
#define SMEXT_CONF_DATESTRING	__DATE__

#define GAMEDATA_FILE			"sourcetvsupport"

#define PLUGIN_LOG_PREFIX		"[" SMEXT_CONF_LOGTAG "] "

#define SMEXT_CONF_METAMOD
#define SMEXT_ENABLE_PLAYERHELPERS
#define SMEXT_ENABLE_GAMECONF
#define SMEXT_ENABLE_GAMEHELPERS

#define SMEXT_LINK(name) SDKExtension *g_pExtensionIface = name;

#endif // _INCLUDE_SOURCETV_SUPPORT_CONFIG_H_