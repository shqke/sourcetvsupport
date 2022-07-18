#include "tools.h"

#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#endif

#include <amtl/am-bits.h>

void* GetLibHandle(const char* libraryName)
{
#ifdef PLATFORM_WINDOWS
	return GetModuleHandle(libraryName);
#elif PLATFORM_LINUX
	void* handle = dlopen(libraryName, RTLD_NOW);
	if (handle != NULL)
	{
		dlclose(handle);
	}

	return handle;
#endif
}

void ProtectMemory(void* addr, int length, int prot)
{
#if defined PLATFORM_POSIX
	long pageSize = sysconf(_SC_PAGESIZE);
	void* startPage = ke::AlignedBase(addr, pageSize);
	void* endPage = ke::AlignedBase((void*)((intptr_t)addr + length), pageSize);
	mprotect(startPage, ((intptr_t)endPage - (intptr_t)startPage) + pageSize, prot);
#elif defined PLATFORM_WINDOWS
	DWORD old_prot;
	VirtualProtect(addr, length, prot, &old_prot);
#endif
}

/* https://github.com/alliedmodders/sourcemod/blob/1c30b88069f60f6e0c29195ab6de084d336b1971/core/logic/MemoryUtils.cpp#L441 */
bool GetLibInfo(void* libPtr, uintptr_t& baseAddr, size_t& librarySize)
{
	if (libPtr == NULL)
	{
		return false;
	}

#ifdef PLATFORM_WINDOWS
	const WORD PE_FILE_MACHINE = IMAGE_FILE_MACHINE_I386;
	const WORD PE_NT_OPTIONAL_HDR_MAGIC = IMAGE_NT_OPTIONAL_HDR32_MAGIC;

	MEMORY_BASIC_INFORMATION info;
	IMAGE_DOS_HEADER* dos;
	IMAGE_NT_HEADERS* pe;
	IMAGE_FILE_HEADER* file;
	IMAGE_OPTIONAL_HEADER* opt;

	if (!VirtualQuery(libPtr, &info, sizeof(MEMORY_BASIC_INFORMATION)))
	{
		return false;
	}

	baseAddr = reinterpret_cast<uintptr_t>(info.AllocationBase);

	/* All this is for our insane sanity checks :o */
	dos = reinterpret_cast<IMAGE_DOS_HEADER*>(baseAddr);
	pe = reinterpret_cast<IMAGE_NT_HEADERS*>(baseAddr + dos->e_lfanew);
	file = &pe->FileHeader;
	opt = &pe->OptionalHeader;

	/* Check PE magic and signature */
	if (dos->e_magic != IMAGE_DOS_SIGNATURE || pe->Signature != IMAGE_NT_SIGNATURE || opt->Magic != PE_NT_OPTIONAL_HDR_MAGIC)
	{
		return false;
	}

	/* Check architecture */
	if (file->Machine != PE_FILE_MACHINE)
	{
		return false;
	}

	/* For our purposes, this must be a dynamic library */
	if ((file->Characteristics & IMAGE_FILE_DLL) == 0)
	{
		return false;
	}

	/* Finally, we can do this */
	librarySize = opt->SizeOfImage;

#elif defined PLATFORM_LINUX
	typedef Elf32_Ehdr ElfHeader;
	typedef Elf32_Phdr ElfPHeader;
	const unsigned char ELF_CLASS = ELFCLASS32;
	const uint16_t ELF_MACHINE = EM_386;

	Dl_info info;
	ElfHeader* file;
	ElfPHeader* phdr;
	uint16_t phdrCount;

	if (!dladdr(libPtr, &info))
	{
		return false;
	}

	if (!info.dli_fbase || !info.dli_fname)
	{
		return false;
	}

	/* This is for our insane sanity checks :o */
	baseAddr = reinterpret_cast<uintptr_t>(info.dli_fbase);
	file = reinterpret_cast<ElfHeader*>(baseAddr);

	/* Check ELF magic */
	if (memcmp(ELFMAG, file->e_ident, SELFMAG) != 0)
	{
		return false;
	}

	/* Check ELF version */
	if (file->e_ident[EI_VERSION] != EV_CURRENT)
	{
		return false;
	}

	/* Check ELF endianness */
	if (file->e_ident[EI_DATA] != ELFDATA2LSB)
	{
		return false;
	}

	/* Check ELF architecture */
	if (file->e_ident[EI_CLASS] != ELF_CLASS || file->e_machine != ELF_MACHINE)
	{
		return false;
	}

	/* For our purposes, this must be a dynamic library/shared object */
	if (file->e_type != ET_DYN)
	{
		return false;
	}

	phdrCount = file->e_phnum;
	phdr = reinterpret_cast<ElfPHeader*>(baseAddr + file->e_phoff);

	for (uint16_t i = 0; i < phdrCount; i++)
	{
		ElfPHeader& hdr = phdr[i];

		/* We only really care about the segment with executable code */
		if (hdr.p_type == PT_LOAD && hdr.p_flags == (PF_X | PF_R))
		{
			/* From glibc, elf/dl-load.c:
				* c->mapend = ((ph->p_vaddr + ph->p_filesz + GLRO(dl_pagesize) - 1)
				*             & ~(GLRO(dl_pagesize) - 1));
				*
				* In glibc, the segment file size is aligned up to the nearest page size and
				* added to the virtual address of the segment. We just want the size here.
				*/
			librarySize = PAGE_ALIGN_UP(hdr.p_filesz);
			break;
		}
	}
#endif

	return true;
}

bool ScanForSignature(const char* libraryName, const char* pattern, size_t len, char*& resultAddr)
{
	void* clientModule = GetLibHandle(libraryName);
	if (!clientModule)
	{
		return false;
	}

	uintptr_t baseAddr;
	size_t librarySize;

	if (GetLibInfo(clientModule, baseAddr, librarySize))
	{
		char* ptr = reinterpret_cast<char*>(baseAddr);
		char* end = ptr + librarySize - len;

		bool found;
		while (ptr < end)
		{
			found = true;
			for (size_t i = 0; i < len; i++)
			{
				if (pattern[i] != '\x2A' && pattern[i] != ptr[i])
				{
					found = false;
					break;
				}
			}

			// Translate the found offset into the actual live binary memory space.
			if (found)
			{
				resultAddr = ptr;

				return true;
			}

			ptr++;
		}
	}

	return false;
}
