//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef NETWORKSTRINGTABLE_H
#define NETWORKSTRINGTABLE_H
#ifdef _WIN32
#pragma once
#endif

#include "networkstringtableitem.h"
#include "checksum_crc.h"
#include "tier1/utldict.h"
#include "tier1/utlbuffer.h"
#include "tier1/bitbuf.h"

class SVC_CreateStringTable;
class CBaseClient;

abstract_class INetworkStringDict
{
public:
	virtual ~INetworkStringDict() {}

	virtual unsigned int Count() = 0;
	virtual void Purge() = 0;
	virtual const char *String( int index ) = 0;
	virtual bool IsValidIndex( int index ) = 0;
	virtual int Insert( const char *pString ) = 0;
	virtual int Find( const char *pString ) = 0;
	virtual void UpdateDictionary( int index ) = 0;
	virtual int DictionaryIndex( int index ) = 0;
	virtual CNetworkStringTableItem	&Element( int index ) = 0;
	virtual const CNetworkStringTableItem &Element( int index ) const = 0;
};

//-----------------------------------------------------------------------------
// Purpose: Client/Server shared string table definition
//-----------------------------------------------------------------------------
class CNetworkStringTable  : public INetworkStringTable
{
public:
	void RestoreTick(int tick)
	{
		// TODO optimize this, most of the time the tables doens't really change

		m_nLastChangedTick = 0;

		int count = m_pItems->Count();

		for (int i = 0; i < count; i++)
		{
			// restore tick in all entries
			int tickChanged = m_pItems->Element(i).RestoreTick(tick);

			if (tickChanged > m_nLastChangedTick)
				m_nLastChangedTick = tickChanged;
		}
	}

	TABLEID					m_id;
	char					*m_pszTableName;
	// Must be a power of 2, so encoding can determine # of bits to use based on log2
	int						m_nMaxEntries;
	int						m_nEntryBits;
	int						m_nTickCount;
	int						m_nLastChangedTick;

	bool					m_bChangeHistoryEnabled : 1;
	bool					m_bLocked : 1;
	bool					m_bAllowClientSideAddString : 1;
	bool					m_bUserDataFixedSize : 1;

	int						m_nFlags; // NSF_*

	int						m_nUserDataSize;
	int						m_nUserDataSizeBits;

	// Change function callback
	pfnStringChanged		m_changeFunc;
	// Optional context/object
	void					*m_pObject;

	// pointer to local backdoor table 
	INetworkStringTable		*m_pMirrorTable;

	INetworkStringDict		*m_pItems;
	INetworkStringDict		*m_pItemsClientSide;	 // For m_bAllowClientSideAddString, these items are non-networked and are referenced by a negative string index!!!
};

#endif // NETWORKSTRINGTABLE_H
