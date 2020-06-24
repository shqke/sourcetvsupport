#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ISmmPluginExt.h>

#include "../config.h"

#include <sm_platform.h>
#include <os/am-shared-library.h>

#define CORE_L4D_FILE	"sourcetvsupport_mm.2.l4d." PLATFORM_LIB_EXT
#define CORE_L4D2_FILE	"sourcetvsupport_mm.2.l4d2." PLATFORM_LIB_EXT

ke::RefPtr<ke::SharedLib> g_CoreLibrary;

METAMOD_PLUGIN *GetPluginInterface(const char *path)
{
	char error[512];
	ke::RefPtr<ke::SharedLib> sourcetvsupport = ke::SharedLib::Open(path, error, sizeof(error));
	if (!sourcetvsupport) {
		fprintf(stderr, PLUGIN_LOG_PREFIX "Unable to load library \"%s\" (reason: \"%s\")\n", path, error);

		return NULL;
	}

	METAMOD_FN_ORIG_LOAD ld = sourcetvsupport->get<METAMOD_FN_ORIG_LOAD>("CreateInterface");
	if (ld == NULL) {
		fprintf(stderr, PLUGIN_LOG_PREFIX "Unable to find symbol \"CreateInterface\" (file: \"%s\")\n", path);

		return NULL;
	}

	int ret;
	METAMOD_PLUGIN *pl = reinterpret_cast<METAMOD_PLUGIN *>(ld(METAMOD_PLAPI_NAME, &ret));
	if (pl == NULL) {
		fprintf(stderr, PLUGIN_LOG_PREFIX "Unable to retrieve interface \"" METAMOD_PLAPI_NAME "\"\n");

		return NULL;
	}

	g_CoreLibrary = sourcetvsupport.take();

	return pl;
}

PLATFORM_EXTERN_C METAMOD_PLUGIN *CreateInterface_MMS(const MetamodVersionInfo *mvi, const MetamodLoaderInfo *mli)
{
	if (mvi->api_major != 2) {
		fprintf(stderr, PLUGIN_LOG_PREFIX "Only MM:Source API v2 is supported\n");

		return NULL;
	}

	const char *file = NULL;
	switch (mvi->source_engine) {
		case SOURCE_ENGINE_LEFT4DEAD:
			file = CORE_L4D_FILE;
			break;
		case SOURCE_ENGINE_LEFT4DEAD2:
			file = CORE_L4D2_FILE;
			break;
	}

	if (file == NULL) {
		fprintf(stderr, PLUGIN_LOG_PREFIX "Unsupported engine detected\n");

		return NULL;
	}

	char path[256];
	snprintf(path, sizeof(path), "%s" PLATFORM_SEP "%s", mli->pl_path, file);

	return GetPluginInterface(path);
}

PLATFORM_EXTERN_C void UnloadInterface_MMS()
{
	g_CoreLibrary.forget();
}

#if defined _MSC_VER
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
	if (fdwReason == DLL_PROCESS_DETACH) {
		UnloadInterface_MMS();
	}

	return TRUE;
}
#else
__attribute__((destructor)) static void gcc_fini()
{
	UnloadInterface_MMS();
}
#endif
