#include "patch_body.h"
#include "../extension/smsdk_config.h"

#include <tier0/dbg.h>
#include <tier0/memalloc.h>
#include <icvar.h>
#include <byteswap.h>

#include <sm_platform.h>
#include <asm/asm.h>

Detour_C_BaseEntity__SetParent_s Detour_C_BaseEntity__SetParent
{
#if SOURCE_ENGINE == SE_LEFT4DEAD
	{ 80, 268, 476 },
	{
		// 83 EC 24 56 57 8B 7C 24 30 85 FF 8B F1 74 ? 
		{ "\x83\xEC\x24\x56\x57\x8B\x7C\x24\x30\x85\xFF\x8B\xF1\x74\x2A", +0xE2 },
	},
#define SetParent_this_reg esi
#elif SOURCE_ENGINE == SE_LEFT4DEAD2
#if defined PLATFORM_WINDOWS
	{ 88, 308, 516 },
	{
		// 55 8B EC 83 EC 24 56 57 8B 7D 08 8B F1 85 FF 74 ? 
		{ "\x55\x8B\xEC\x83\xEC\x24\x56\x57\x8B\x7D\x08\x8B\xF1\x85\xFF\x74\x2A", +0xE4 },
	},
#define SetParent_this_reg esi
#elif defined PLATFORM_LINUX
	{ 72, 292, 500 },
	{
		// 55 B8 FF FF FF FF 89 E5 56 53 83 EC 40 
		{ "\x55\xB8\xFF\xFF\xFF\xFF\x89\xE5\x56\x53\x83\xEC\x40", +0xDD },
	},
#define SetParent_this_reg ebx
#endif
#endif
};

bool Detour_C_BaseEntity__SetParent_s::LoadDetour()
{
#if defined PLATFORM_WINDOWS
	if (Signatures.SetParent.pattern == NULL) {
		Warning(PLUGIN_LOG_PREFIX "Missing pattern\n");

		return false;
	}

	m_patchedAddr = NULL;
	if (!ScanForSignature("client." PLATFORM_LIB_EXT, Signatures.SetParent.pattern, Signatures.SetParent.length, m_patchedAddr)) {
		Warning(PLUGIN_LOG_PREFIX "Unable to resolve address for client.dll\n");

		return false;
	}

	m_patchedAddr += Signatures.SetParent.offset;
	m_patchedBytes = copy_bytes((unsigned char*)m_patchedAddr, NULL, OP_JMP_SIZE);

	ProtectMemory((void*)&Detour_Trampoline_ToOriginal, 20, PAGE_EXECUTE_READWRITE);
	copy_bytes((unsigned char*)m_patchedAddr, (unsigned char*)&Detour_Trampoline_ToOriginal, m_patchedBytes); // copy replaced code from original
	inject_jmp(((unsigned char*)&Detour_Trampoline_ToOriginal) + m_patchedBytes, m_patchedAddr + m_patchedBytes); // append with jmp
	ProtectMemory((void*)&Detour_Trampoline_ToOriginal, 20, PAGE_EXECUTE_READ);

	ProtectMemory(m_patchedAddr, 20, PAGE_EXECUTE_READWRITE);
	fill_nop(m_patchedAddr, m_patchedBytes);
	inject_jmp(m_patchedAddr, (void*)&Detour_Trampoline_Main); // set jmp at start
	ProtectMemory(m_patchedAddr, 20, PAGE_EXECUTE_READ);

	m_bEnabled = true;

	return true;
#else
	return false;
#endif
}

void Detour_C_BaseEntity__SetParent_s::UnloadDetour()
{
#if defined PLATFORM_WINDOWS
	if (m_bEnabled) {
		ProtectMemory(m_patchedAddr, 20, PAGE_EXECUTE_READWRITE);
		memcpy(m_patchedAddr, (void*)&Detour_Trampoline_ToOriginal, m_patchedBytes);
		ProtectMemory(m_patchedAddr, 20, PAGE_EXECUTE_READ);

		m_bEnabled = false;
	}
#endif
}

#if defined PLATFORM_WINDOWS
#pragma region C_BaseEntity::SetParent Detour Body
#if !defined(SetParent_this_reg)
#define SetParent_this_reg ecx
#endif

#define REGTHIS ecx
#define REG1 ebx
#define REG2 edx

// under clang: -masm=intel -fdeclspec -fms-extensions
void __declspec(naked) Detour_Trampoline_Main()
{
	__asm
	{
		push REGTHIS;
		mov REGTHIS, SetParent_this_reg;

		push REG1;
		mov REG1, Detour_C_BaseEntity__SetParent.Offsets.index;

		// Compare C_BaseEntity::index to -1 (client entity index)
		cmp dword ptr [REGTHIS + REG1], -1;

		// Bail out if not a client entity
		jne bailOut;

		push REG2;

		mov REG1, Detour_C_BaseEntity__SetParent.Offsets.m_pMoveParent;

		// Store C_BaseEntity::m_pMoveParent
		mov REG2, dword ptr [REGTHIS + REG1];

		mov REG1, Detour_C_BaseEntity__SetParent.Offsets.m_hNetworkMoveParent;
		lea REG1, [REGTHIS + REG1];

		// Store value of C_BaseEntity::m_pMoveParent into C_BaseEntity::m_hNetworkMoveParent
		mov [REG1], REG2;

		pop REG2;

	bailOut:
		pop REG1;
		pop REGTHIS;

		// Back to original function
		jmp Detour_Trampoline_ToOriginal;
	};
}
#pragma endregion

#pragma region C_BaseEntity::SetParent Trampoline Placeholder
void __declspec(naked) Detour_Trampoline_ToOriginal()
{
	__asm {
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
	};
}
#pragma endregion
#endif
