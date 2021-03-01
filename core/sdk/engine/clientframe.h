//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CLIENTFRAME_H
#define CLIENTFRAME_H
#ifdef _WIN32
#pragma once
#endif

#include <bitvec.h>
#include <const.h>
#include <tier1/mempool.h>

class CFrameSnapshot;

#define MAX_CLIENT_FRAMES	128

class CClientFrame
{
public:
	virtual ~CClientFrame();

	virtual bool		IsMemPoolAllocated() { return true; }

public:

	// State of entities this frame from the POV of the client.
	int					last_entity;	// highest entity index
	int					tick_count;	// server tick of this snapshot

	// Used by server to indicate if the entity was in the player's pvs
	CBitVec<MAX_EDICTS>	transmit_entity; // if bit n is set, entity n will be send to client
	CBitVec<MAX_EDICTS>	*from_baseline;	// if bit n is set, this entity was send as update from baseline
	CBitVec<MAX_EDICTS>	*transmit_always; // if bit is set, don't do PVS checks before sending (HLTV only)

	CClientFrame*		m_pNext;

private:

	// Index of snapshot entry that stores the entities that were active and the serial numbers
	// for the frame number this packed entity corresponds to
	// m_pSnapshot MUST be private to force using SetSnapshot(), see reference counters
	CFrameSnapshot		*m_pSnapshot;
};

// TODO substitute CClientFrameManager with an intelligent structure (Tree, hash, cache, etc)
class CClientFrameManager
{
public:
	virtual ~CClientFrameManager(void) { DeleteClientFrames(-1); }

	void DeleteClientFrames(int nTick);	// -1 for all
	int CountClientFrames(void)	// returns number of frames in list
	{
		int count = 0;

		CClientFrame* f = m_Frames;

		while (f)
		{
			count++;
			f = f->m_pNext;
		}

		return count;
	}

	void RemoveOldestFrame(void)  // removes the oldest frame in list
	{
		CClientFrame* frame = m_Frames; // first

		if (!frame)
			return;	// no frames at all

		m_Frames = frame->m_pNext; // unlink head

								   // deleting frame will decrease global reference counter
		FreeFrame(frame);
	}

private:
	void FreeFrame(CClientFrame* pFrame)
	{
		if (pFrame->IsMemPoolAllocated())
		{
			m_ClientFramePool.Free(pFrame);
		}
		else
		{
			delete pFrame;
		}
	}


	CClientFrame* m_Frames;		// updates can be delta'ed from here
	CClassMemoryPoolExt< CClientFrame >	m_ClientFramePool;
};

#endif // CLIENTFRAME_H
