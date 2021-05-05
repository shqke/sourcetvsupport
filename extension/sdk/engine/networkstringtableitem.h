//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef NETWORKSTRINGTABLEITEM_H
#define NETWORKSTRINGTABLEITEM_H
#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

class CNetworkStringTableItem
{
public:
	struct itemchange_s {
		int				tick;
		int				length;
		unsigned char	*data;
	};

	int RestoreTick(int tick)
	{
		int index = 1;

		int count = m_pChangeList->Count();
		itemchange_s* item = &m_pChangeList->Element(0);

		while (index < count)
		{
			itemchange_s* nextitem = &m_pChangeList->Element(index);

			if (nextitem->tick > tick)
			{
				break;
			}

			item = nextitem;
			index++;
		}

		if (item->tick > tick)
		{
			// this string was created after tick, so ignore it right now
			m_pUserData = NULL;
			m_nUserDataLength = 0;
			m_nTickChanged = 0;
		}
		else
		{
			// restore user data for this string
			m_pUserData = item->data;
			m_nUserDataLength = item->length;
			m_nTickChanged = item->tick;
		}

		return m_nTickChanged;
	}

public:
	unsigned char	*m_pUserData;
	int				m_nUserDataLength;
	int				m_nTickChanged;

#ifndef SHARED_NET_STRING_TABLES
	int				m_nTickCreated;
	CUtlVector< itemchange_s > *m_pChangeList;	
#endif
};

#endif // NETWORKSTRINGTABLEITEM_H
