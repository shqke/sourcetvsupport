//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef NETMESSAGES_H
#define NETMESSAGES_H

#include <inetmessage.h>
#include <checksum_crc.h>
#include <const.h>
#include <utlvector.h>
#include <bitbuf.h>
#include <inetchannel.h>
#include <inetmsghandler.h>

#define	MAX_OSPATH		260			// max length of a filesystem pathname

class INetMessageHandler;
class IServerMessageHandler;
class IClientMessageHandler;

class CNetMessageRatelimitPolicy
{
public:
	int unk;
	int m_nBytes;
	double m_unk;
	int unk1;
	int unk2;
	double m_prevTime;

	int unk3;
	int unk4;
	bool m_bunk;
	double m_unkTimeDelta;
	int unk5;
	int unk6;
};

class CNetMessage : public INetMessage
{
public:
	virtual ~CNetMessage() {};

	virtual int		GetGroup() const { return INetChannelInfo::GENERIC; }
	INetChannel		*GetNetChannel() const { return m_NetChannel; }
		
	virtual void	SetReliable( bool state) {m_bReliable = state;};
	virtual bool	IsReliable() const { return m_bReliable; };
	virtual void    SetNetChannel(INetChannel * netchan) { m_NetChannel = netchan; }	
	virtual bool	Process() { Assert( 0 ); return false; };	// no handler set

protected:
	bool				m_bReliable;	// true if message should be send reliable
	INetChannel			*m_NetChannel;	// netchannel this message is from/for

	CNetMessageRatelimitPolicy  m_rateLimitPolicy;
};


///////////////////////////////////////////////////////////////////////////////////////
// server messages:
///////////////////////////////////////////////////////////////////////////////////////

class SVC_ServerInfo : public CNetMessage
{
	IServerMessageHandler* m_pMessageHandler;

public:	// member vars are public for faster handling
	int			m_nProtocol;	// protocol version
	int			m_nServerCount;	// number of changelevels since server start
	bool		m_bIsDedicated;  // dedicated server ?	
	bool		m_bIsVanilla;
	bool		m_bIsHLTV;		// HLTV server ?
	char		m_cOS;
	CRC32_t		m_nMapCRC;
	CRC32_t		m_nClientCRC;
	CRC32_t		m_nStringTableCRC;
	int			m_nMaxClients;
	int			m_nMaxClasses;
	int			m_nPlayerSlot;
	float		m_fTickInterval;
	const char* m_szGameDir;
	const char* m_szMapName;
	const char* m_szSkyName;
	const char* m_szHostName;
	const char* m_szMissionName;
	const char* m_szModeName;

	char		m_szGameDirBuffer[MAX_OSPATH];
	char		m_szMapNameBuffer[MAX_OSPATH];
	char		m_szSkyNameBuffer[MAX_OSPATH];
	char		m_szHostNameBuffer[MAX_OSPATH];
	char		m_szMissionBuffer[MAX_OSPATH];
	char		m_szGamemodeBuffer[MAX_OSPATH];
};

#endif // NETMESSAGES_H
