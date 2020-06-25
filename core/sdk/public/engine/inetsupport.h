//===== Copyright c 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef INETSUPPORT_H
#define INETSUPPORT_H

#ifdef _WIN32
#pragma once
#endif

class IMatchFramework;
class IMatchSession;

#include "appframework/IAppSystem.h"

#include <tier1/interface.h>
#include <tier1/KeyValues.h>

class ServerInfo_t;
class ClientInfo_t;

class IMatchAsyncOperationCallback;
class IMatchAsyncOperation;

abstract_class INetSupport : public IAppSystem
{
public:
	// Get engine build number
	virtual int GetEngineBuildNumber() = 0;

	// Get server info
	virtual void GetServerInfo(ServerInfo_t *pServerInfo) = 0;

	// Get client info
	virtual void GetClientInfo(ClientInfo_t *pClientInfo) = 0;

	// Update a local server reservation
	virtual void UpdateServerReservation(uint64 uiReservation) = 0;

	// Update a client reservation before connecting to a server
	virtual void UpdateClientReservation(uint64 uiReservation, uint64 uiMachineIdHost) = 0;

	// Submit a server reservation packet
	virtual void ReserveServer(
		const netadr_s &netAdrPublic, const netadr_s &netAdrPrivate,
		uint64 nServerReservationCookie, KeyValues *pKVGameSettings,
		IMatchAsyncOperationCallback *pCallback, IMatchAsyncOperation **ppAsyncOperation) = 0;

	// When client event is fired
	virtual void OnMatchEvent(KeyValues *pEvent) = 0;

	virtual uint32 CreateChannel(int, netadr_s const&, char const*, INetChannelHandler *);

	// Process incoming net packets on the socket
	virtual void ProcessSocket(int sock, IConnectionlessPacketHandler * pHandler) = 0;

	// Send a network packet
	virtual int SendPacket(
		INetChannel *chan, int sock, const netadr_t &to,
		const void *data, int length,
		bf_write *pVoicePayload = NULL,
		bool bUseCompression = false) = 0;
};

#define INETSUPPORT_VERSION_STRING "INETSUPPORT_001"

#endif // INETSUPPORT_H
