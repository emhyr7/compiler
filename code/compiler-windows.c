#if defined(_WIN32)

#include "compiler.h"

__declspec(dllimport) HANDLE  __stdcall CreateFileA  (const CHAR*, WORD, WORD, VOID*, WORD, WORD, HANDLE);
__declspec(dllimport) BOOLEAN __stdcall GetFileSizeEx(HANDLE, SIZE *);
__declspec(dllimport) BOOLEAN __stdcall ReadFile     (HANDLE, VOID *, WORD, WORD *, VOID *);
__declspec(dllimport) BOOLEAN __stdcall CloseHandle  (HANDLE);
__declspec(dllimport) VOID    __stdcall GetSystemInfo(VOID *);
__declspec(dllimport) VOID   *__stdcall VirtualAlloc (VOID *, SIZE, WORD, WORD);
__declspec(dllimport) BOOLEAN __stdcall VirtualFree  (VOID *, SIZE, WORD);

HANDLE open_file(const char *path)
{
	HANDLE file = CreateFileA(path, 0x80000000L, 0x00000001, 0, 3, 0x00000080, 0);
	assert(file != -1);
	return file;
}

SIZE get_size_of_file(HANDLE file)
{
	SIZE size;
	assert(GetFileSizeEx(file, &size));
	return size;
}

SIZE read_from_file(VOID *buffer, SIZE size, HANDLE file)
{
	assert(size <= 0xffffffffffffffff);
	assert(ReadFile(file, buffer, size, (WORD *)&size, 0));
	return size;
}

VOID close_file(HANDLE file)
{
	assert(CloseHandle(file));
}

SIZE query_system_page_size(VOID)
{
	union {
		WORD _padding0[16];
		struct {
			BYTE _padding1[4];
			WORD dwPageSize;
		};
	} system_info;
	GetSystemInfo(&system_info);
	return system_info.dwPageSize;
}

VOID *allocate_virtual_memory(SIZE size)
{
	VOID *result = VirtualAlloc(0, size, 0x00001000 | 0x00002000, 0x04);
	assert(result);
	return result;
}

VOID *reserve_virtual_memory(SIZE size)
{
	VOID *result = VirtualAlloc(0, size, 0x00002000, 0x04);
	assert(result);
	return result;
}

VOID commit_virtual_memory(VOID *memory, SIZE size)
{
	assert(VirtualAlloc(memory, size, 0x00001000, 0x04));
}

VOID release_virtual_memory(VOID *memory, SIZE size)
{
	(VOID)size;
	assert(VirtualFree(memory, 0, 0x00008000));
}

#endif
