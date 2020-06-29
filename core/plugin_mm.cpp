#include "plugin_mm.h"

#include <iclient.h>
#include <inetchannel.h>

// Interfaces
IVEngineServer* engine = NULL;
IServerGameDLL* gamedll = NULL;

INetworkStringTableContainer* networkStringTableContainerServer = NULL;
IHLTVDirector* hltvdirector = NULL;
INetSupport* g_pNetSupport = NULL;
IPlayerInfoManager* playerinfomanager = NULL;

IExtensionManager* smexts = NULL;

MMSPlugin g_Plugin;
PLUGIN_EXPOSE(Extension, g_Plugin);

SMExtension* smext = &g_Plugin;

bool MMSPlugin::LoadSMExtension(char* error, int maxlength)
{
	smexts = static_cast<IExtensionManager*>(g_SMAPI->MetaFactory(SOURCEMOD_INTERFACE_EXTENSIONS, NULL, NULL));
	if (smexts == NULL) {
		V_strncpy(error, SOURCEMOD_INTERFACE_EXTENSIONS " not found", maxlength);

		return false;
	}

	if (smexts->LoadExternal(static_cast<IExtensionInterface*>(this), "addons/sourcetvsupport." PLATFORM_LIB_EXT, "sourcetvsupport." PLATFORM_LIB_EXT, error, maxlength) == NULL) {
		SMExtension::Unload();

		return false;
	}

	return true;
}

void MMSPlugin::BindToSourceMod()
{
	if (myself != NULL) {
		// Don't load twice
		return;
	}

	char error[256];
	if (!LoadSMExtension(error, sizeof(error))) {
		fprintf(stderr, PLUGIN_LOG_PREFIX "Could not load as a SourceMod extension: %s\n", error);
		META_LOG(g_PLAPI, "Could not load as a SourceMod extension: %s", error);

		return;
	}

	Msg(PLUGIN_LOG_PREFIX "Loaded as external SM extension\n");
}

bool MMSPlugin::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer, INTERFACEVERSION_VENGINESERVER);
	GET_V_IFACE_CURRENT(GetEngineFactory, networkStringTableContainerServer, INetworkStringTableContainer, INTERFACENAME_NETWORKSTRINGTABLESERVER);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pNetSupport, INetSupport, INETSUPPORT_VERSION_STRING);

	GET_V_IFACE_CURRENT(GetServerFactory, gamedll, IServerGameDLL, INTERFACEVERSION_SERVERGAMEDLL);
	GET_V_IFACE_CURRENT(GetServerFactory, hltvdirector, IHLTVDirector, INTERFACEVERSION_HLTVDIRECTOR);
	GET_V_IFACE_CURRENT(GetServerFactory, playerinfomanager, IPlayerInfoManager, INTERFACEVERSION_PLAYERINFOMANAGER);

	ismm->AddListener(this, this);

	//ConVar_Register(0, this);

	return true;
}

bool MMSPlugin::Unload(char* error, size_t maxlen)
{
	if (myself != NULL) {
		smexts->UnloadExtension(myself);
	}

	//ConVar_Unregister();

	return true;
}

void MMSPlugin::AllPluginsLoaded()
{
	BindToSourceMod();
}

void* MMSPlugin::OnMetamodQuery(const char* iface, int* ret)
{
	if (strcmp(iface, SOURCEMOD_NOTICE_EXTENSIONS) == 0) {
		BindToSourceMod();
	}

	if (ret != NULL) {
		*ret = IFACE_OK;
	}

	return NULL;
}

bool MMSPlugin::RegisterConCommandBase(ConCommandBase* pVar)
{
	// Notify metamod of ownership
	return META_REGCVAR(pVar);
}
