#include "compiler.h"

#pragma comment(lib, "user32.lib")

#include <windows.h>

#include <stdio.h>
#include <io.h>
#include <fcntl.h>

extern int start(int, char *[]);

uint32 system_page_size;

handle stdin_handle;
handle stdout_handle;
handle stderr_handle;

_Noreturn void terminate(int status)
{
	_exit(status);
}

static void win32_display_last_error(void)
{
	DWORD last_error = GetLastError();
	LPVOID message;
	FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 0, last_error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&message, 0, 0);
	MessageBoxW(0, message, L"Error", MB_OK);
	LocalFree(message);
}

#define WIN32_ASSERT(assertion) ({ if(!(assertion)) { win32_display_last_error(); TRAP(); } })

int main(int argc, char *argv[])
{
	SYSTEM_INFO system_info;
	GetSystemInfo(&system_info);
	system_page_size = system_info.dwPageSize;
	
	stdin_handle  = GetStdHandle(STD_INPUT_HANDLE);
	stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	stderr_handle = GetStdHandle(STD_ERROR_HANDLE);
	WIN32_ASSERT(SetConsoleOutputCP(65001)); // enable UTF-8 encoded printing

	return start(argc, argv);
}

void *allocate_memory(uint32 size)
{
	void *memory = VirtualAlloc(0, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	WIN32_ASSERT(memory);
	return memory;
}

void *reserve_memory(uint32 size)
{
	void *memory = VirtualAlloc(0, size, MEM_RESERVE, PAGE_READWRITE);
	WIN32_ASSERT(memory);
	return memory;
}

void commit_memory(void *memory, uint32 size)
{
	WIN32_ASSERT(VirtualAlloc(memory, size, MEM_COMMIT, PAGE_READWRITE));
}

void release_memory(void *memory, uint32 size)
{
	(void)size;
	WIN32_ASSERT(VirtualFree(memory, 0, MEM_RELEASE));
}

handle open_file(const utf8 *path)
{
	WCHAR path16[MAX_PATH];
	MultiByteToWideChar(CP_UTF8, 0, path, -1, path16, sizeof(path16) / sizeof(WCHAR));
	handle file = CreateFileW(path16, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	WIN32_ASSERT(file != INVALID_HANDLE_VALUE);
	return file;
}

void close_file(handle file)
{
	CloseHandle(file);
}

uint64 get_size_of_file(handle file)
{
	LARGE_INTEGER size;
	WIN32_ASSERT(GetFileSizeEx(file, &size));
	return size.QuadPart;
}

uint32 read_from_file(void *buffer, uint32 size, handle file)
{
	WIN32_ASSERT(ReadFile(file, buffer, size, (LPDWORD)&size, 0));
	return size;
}

uint32 write_into_file(const void *buffer, uint32 size, handle file)
{
	WIN32_ASSERT(WriteFile(file, buffer, size, (LPDWORD)&size, 0));
	return size;
}
