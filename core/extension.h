#ifndef _INCLUDE_SOURCETV_SUPPORT_H_
#define _INCLUDE_SOURCETV_SUPPORT_H_

#include "config.h"

#include <stdlib.h>
#include <stdio.h>

// SDK
#define TEAM_SPECTATOR			1	// spectator team

// Must include mathlib.h before utlmemory.h: V_swap is not getting resolved for utlmemory.h and utlmemory.h doesn't include mathlib.h (gcc)
#include <mathlib/mathlib.h>
#include <eiface.h>
extern IVEngineServer* engine;
extern IServerGameDLL* gamedll;

#include <iserver.h>
extern IServer* sv;

#include <ihltv.h>
extern IHLTVServer* ihltv;
extern IServer* hltv;

#include <ihltvdirector.h>
extern IHLTVDirector* hltvdirector;

#include <game/server/iplayerinfo.h>
extern IPlayerInfoManager* playerinfomanager;

#include <networkstringtabledefs.h>
extern INetworkStringTableContainer* networkStringTableContainerServer;

#include <iclient.h>
#include <tier1/interface.h>
#include <tier1/utlbuffer.h>
#include <checksum_crc.h>
#include <netadr.h>
#include <bitbuf.h>

// Engine
#include "sdk/public/engine/inetsupport.h"
extern INetSupport* g_pNetSupport;

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
#include <ISmmPlugin.h>
using namespace SourceMM;

#include <sourcehook.h>
using namespace SourceHook;

PLUGIN_GLOBALVARS();

// SourceMod
#include <sm_platform.h>
#include <ISourceMod.h>
#include <IExtensionSys.h>
using namespace SourceMod;

extern IExtension* myself;
extern ISourceMod* g_pSM;
extern ISourceMod* smutils;

#include <IShareSys.h>
extern IShareSys* sharesys;

#include <IGameConfigs.h>
extern IGameConfigManager* gameconfs;

#include <IPlayerHelpers.h>
extern IPlayerManager* playerhelpers;

#include <IGameHelpers.h>
extern IGameHelpers* gamehelpers;

#include <extensions/ISDKTools.h>
extern ISDKTools* sdktools;

#include <extensions/IBinTools.h>
extern IBinTools* bintools;

#define SM_MKIFACE(name) SMINTERFACE_##name##_NAME, SMINTERFACE_##name##_VERSION

#define SM_GET_LATE_IFACE(prefix, addr) \
	sharesys->RequestInterface(SM_MKIFACE(prefix), myself, (SMInterface **)&addr)

#define SM_CHECK_IFACE(prefix, addr) \
	if (!addr) { \
		if (error != NULL && maxlength > 0) { \
			size_t len = g_SMAPI->Format(error, maxlength, "Could not find interface: %s", SMINTERFACE_##prefix##_NAME); \
			if (len >= maxlength) { \
				error[maxlength - 1] = '\0'; \
			} \
		} \
		return false; \
	}

#define SM_GET_IFACE(prefix, addr) SM_CHECK_IFACE(prefix, SM_GET_LATE_IFACE(prefix, addr))

// SourcePawn
#include <sp_vm_api.h>
using namespace SourcePawn;

#endif // _INCLUDE_SOURCETV_SUPPORT_H_
