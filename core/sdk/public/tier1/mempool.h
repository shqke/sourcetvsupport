//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//===========================================================================//

#ifndef MEMPOOL_EXT_H
#define MEMPOOL_EXT_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlvector.h"
#include "tier1/utlrbtree.h"

#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Optimized pool memory allocator
//-----------------------------------------------------------------------------

typedef void (*MemoryPoolReportFunc_t)( char const* pMsg, ... );

class CUtlMemoryPool
{
public:
	// Ways the memory pool can grow when it needs to make a new blob.
	enum MemoryPoolGrowType_t
	{
		GROW_NONE = 0,		// Don't allow new blobs.
		GROW_FAST = 1,		// New blob size is numElements * (i+1)  (ie: the blocks it allocates
							// get larger and larger each time it allocates one).
							GROW_SLOW = 2			// New blob size is numElements.
	};

	CUtlMemoryPool(int blockSize, int numElements, int growMode = GROW_FAST, const char *pszAllocOwner = NULL, int nAlignment = 0);
	~CUtlMemoryPool();

	//-----------------------------------------------------------------------------
	// Purpose: Frees a block of memory
	// Input  : *memBlock - the memory to free
	//-----------------------------------------------------------------------------
	void		Free(void* memBlock)
	{
		if (!memBlock)
			return;  // trying to delete NULL pointer, ignore

#ifdef _DEBUG
					 // check to see if the memory is from the allocated range
		bool bOK = false;
		for (CBlob* pCur = m_BlobHead.m_pNext; pCur != &m_BlobHead; pCur = pCur->m_pNext)
		{
			if (memBlock >= pCur->m_Data && (char*)memBlock < (pCur->m_Data + pCur->m_NumBytes))
			{
				bOK = true;
			}
		}
		Assert(bOK);
#endif // _DEBUG

#ifdef _DEBUG	
		// invalidate the memory
		memset(memBlock, 0xDD, m_BlockSize);
#endif

		m_BlocksAllocated--;

		// make the block point to the first item in the list
		*((void**)memBlock) = m_pHeadOfFreeList;

		// the list head is now the new block
		m_pHeadOfFreeList = memBlock;
	}

	// Frees everything
	void		Clear()
	{
		// Free everything..
		CBlob* pNext;
		for (CBlob* pCur = m_BlobHead.m_pNext; pCur != &m_BlobHead; pCur = pNext)
		{
			pNext = pCur->m_pNext;
			free(pCur);
		}
		Init();
	}

protected:
	class CBlob
	{
	public:
		CBlob	*m_pPrev, *m_pNext;
		int		m_NumBytes;		// Number of bytes in this blob.
		char	m_Data[1];
		char	m_Padding[3]; // to int align the struct
	};

	// Resets the pool
	void		Init()
	{
		m_NumBlobs = 0;
		m_BlocksAllocated = 0;
		m_pHeadOfFreeList = 0;
		m_BlobHead.m_pNext = m_BlobHead.m_pPrev = &m_BlobHead;
	}

	int			m_BlockSize;
	int			m_BlocksPerBlob;

	int			m_GrowMode;	// GROW_ enum.

	// FIXME: Change m_ppMemBlob into a growable array?
	void			*m_pHeadOfFreeList;
	int				m_BlocksAllocated;
	int				m_PeakAlloc;
	unsigned short	m_nAlignment;
	unsigned short	m_NumBlobs;
	const char *	m_pszAllocOwner;
	// CBlob could be not a multiple of 4 bytes so stuff it at the end here to keep us otherwise aligned
	CBlob			m_BlobHead;

	static MemoryPoolReportFunc_t g_ReportFunc;
};

//-----------------------------------------------------------------------------
// Wrapper macro to make an allocator that returns particular typed allocations
// and construction and destruction of objects.
//-----------------------------------------------------------------------------
template< class T >
class CClassMemoryPoolExt : public CUtlMemoryPool
{
public:
	CClassMemoryPoolExt(int numElements, int growMode = GROW_FAST, int nAlignment = 0) :
		CUtlMemoryPool(sizeof(T), numElements, growMode, MEM_ALLOC_CLASSNAME(T), nAlignment) {}

	inline void	Clear()
	{
		CUtlRBTree<void *, int> freeBlocks;
		SetDefLessFunc(freeBlocks);

		void *pCurFree = m_pHeadOfFreeList;
		while (pCurFree != NULL)
		{
			freeBlocks.Insert(pCurFree);
			pCurFree = *((void**)pCurFree);
		}

		for (CBlob *pCur = m_BlobHead.m_pNext; pCur != &m_BlobHead; pCur = pCur->m_pNext)
		{
			T *p = (T *)pCur->m_Data;
			T *pLimit = (T *)(pCur->m_Data + pCur->m_NumBytes);
			while (p < pLimit)
			{
				if (freeBlocks.Find(p) == freeBlocks.InvalidIndex())
				{
#if defined(_LINUX) && defined(DEBUG)
#error "Unsupported"
#endif
					Destruct(p);
				}
				p++;
			}
		}

		CUtlMemoryPool::Clear();
	}
};

#include "tier0/memdbgoff.h"

#endif // MEMPOOL_EXT_H
