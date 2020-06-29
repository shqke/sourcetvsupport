//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef DEMOFILE_H
#define DEMOFILE_H
#ifdef _WIN32
#pragma once
#endif

#include "demo.h"

#include <tier2/utlstreambuffer.h>
#include <tier1/bitbuf.h>

#include <demofile/demoformat.h>

//-----------------------------------------------------------------------------
// Demo file 
//-----------------------------------------------------------------------------
class CDemoFile
{
public:
	bool IsOpen()
	{
		return m_Buffer.IsOpen();
	}

	void WriteRawData(const char* buffer, int length)
	{
		m_Buffer.PutInt(length);
		m_Buffer.Put(buffer, length);
	}

	void WriteCmdHeader(unsigned char cmd, int tick, int playerSlot = 0)
	{
		m_Buffer.PutUnsignedChar(cmd);
		m_Buffer.PutInt(tick);
		m_Buffer.PutUnsignedChar(playerSlot);
	}

	void WriteStringTables(bf_write* buf, int tick)
	{
		if (!IsOpen())
		{
			DevMsg("CDemoFile::WriteStringTables: Haven't opened file yet!\n");
			return;
		}

		WriteCmdHeader(9, tick);

		WriteRawData((char*)buf->GetBasePointer(), buf->GetNumBytesWritten());
	}

	void WriteNetworkDataTables(bf_write* buf, int tick)
	{
		if (!IsOpen())
		{
			DevMsg("CDemoFile::WriteNetworkDataTables: Haven't opened file yet!\n");
			return;
		}

		WriteCmdHeader(dem_datatables, tick, 0);

		WriteRawData((char*)buf->GetBasePointer(), buf->GetNumBytesWritten());
	}

public:
	char			m_szFileName[MAX_PATH];	//name of current demo file
	demoheader_t    m_DemoHeader;  //general demo info

private:
	CUtlStreamBuffer m_Buffer;
};

#define DEMO_RECORD_BUFFER_SIZE 2*1024*1024 // temp buffer big enough to fit both string tables and server classes

#endif // DEMOFILE_H
