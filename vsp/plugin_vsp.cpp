#include "plugin_vsp.h"
#include <icvar.h>

EXPOSE_SINGLE_INTERFACE(VSPPlugin, IServerPluginCallbacks, INTERFACEVERSION_ISERVERPLUGINCALLBACKS);

bool VSPPlugin::Load(CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory)
{
	g_pCVar = static_cast<ICvar*>(interfaceFactory(CVAR_INTERFACE_VERSION, NULL));
	if (g_pCVar == NULL) {
		fprintf(stderr, PLUGIN_LOG_PREFIX "Couldn't retrieve interface \"" CVAR_INTERFACE_VERSION "\"\n");

		return false;
	}

	static const char* const cvars[] = {
		"tv_enable",
		"tv_maxclients",
		"tv_dispatchmode",
		"tv_transmitall",
		"tv_relayvoice",
		"tv_debug",
		"tv_deltacache",
		"tv_password",
		"tv_overridemaster",
		"tv_autorecord",
		"tv_name",
		"tv_title",

		"tv_autoretry",
		"tv_timeout",
		"tv_snapshotrate",

		"tv_maxrate",
		"tv_relaypassword",
		"tv_chattimelimit",
		"tv_chatgroupsize",

		"tv_allow_camera_man",
		"tv_allow_static_shots",
		"tv_delay",
		//"tv_delaymapchange",

		"sv_hibernate_when_empty",
		"sv_master_share_game_socket",
	};

	size_t handled = 0;
	for (auto&& name : cvars) {
		ConVar* pCvar = g_pCVar->FindVar(name);
		if (pCvar == NULL) {
			fprintf(stderr, PLUGIN_LOG_PREFIX "Couldn't find convar \"%s\"\n", name);

			continue;
		}

		handled++;
		pCvar->RemoveFlags(FCVAR_DEVELOPMENTONLY);
	}

	printf(PLUGIN_LOG_PREFIX "SourceTV related convars (%u out of %u) were successfully exposed. Unloading...\n", handled, NELEMS(cvars));

	return false;
}
