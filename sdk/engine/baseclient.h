//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef BASECLIENT_H
#define BASECLIENT_H
#ifdef _WIN32
#pragma once
#endif

#include <iclient.h>
#include <igameevents.h>

class CBaseClient : public IGameEventListener2, public IClient, public IClientMessageHandler
{

};

#endif // BASECLIENT_H
