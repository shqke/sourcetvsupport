//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef HLTVDEMO_H
#define HLTVDEMO_H
#ifdef _WIN32
#pragma once
#endif

#include <filesystem.h>

#include "demo.h"
#include "demofile.h"

class CHLTVDemoRecorder : public IDemoRecorder
{
public:
	virtual ~CHLTVDemoRecorder();

private:
	CDemoFile		m_DemoFile;
	bool			m_bIsRecording;
	int				m_nFrameCount;
	float			m_nStartTick;
	int				m_SequenceInfo;
	int				m_nDeltaTick;
	int				m_nSignonTick;
	bf_write		m_MessageData; // temp buffer for all network messages
};



#endif // HLTVDEMO_H
