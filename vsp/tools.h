#ifndef _INCLUDE_PLUGIN_VSP_TOOLS_H_
#define _INCLUDE_PLUGIN_VSP_TOOLS_H_

#include <stdint.h>
#include <stddef.h>
#include <sm_platform.h>

#if defined PLATFORM_POSIX
#include <string.h>

#include <fcntl.h>
#include <link.h>
#include <sys/mman.h>

#define PAGE_SIZE			4096
#define PAGE_ALIGN_UP(x)	((x + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

#define	PAGE_EXECUTE_READWRITE	PROT_READ|PROT_WRITE|PROT_EXEC
#define	PAGE_EXECUTE_READ	PROT_READ|PROT_EXEC
#endif

struct signature_t
{
	constexpr signature_t() :
		pattern(nullptr),
		length(0),
		offset(0)
	{
	}

	template<size_t N>
	constexpr signature_t(const char(&pattern)[N], int offset = 0) :
		pattern(pattern),
		length(N - 1),
		offset(offset)
	{
	}

	const char* const pattern;
	const size_t length;
	const int offset;
};

void* GetLibHandle(const char* libraryName);
void ProtectMemory(void* addr, int length, int prot);
bool GetLibInfo(void* libPtr, uintptr_t& baseAddr, size_t& librarySize);
bool ScanForSignature(const char* libraryName, const char* pattern, size_t len, char*& resultAddr);

#endif // _INCLUDE_PLUGIN_VSP_TOOLS_H_