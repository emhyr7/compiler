#if !defined(__INCLUDED_COMPILER_H)
#define __INCLUDED_COMPILER_H

#define alignas(x) _Alignas(x)
#define alignof(x) _Alignof(x)

#define assert(x) do { if (!(x)) __builtin_debugtrap(); } while (0)

typedef void VOID;

typedef unsigned char      BYTE;
typedef unsigned short     TINY;
typedef unsigned int       WORD;
typedef unsigned long long LONG;

typedef char      B8;
typedef short     B16;
typedef int       B32;
typedef long long B64;

typedef unsigned char      U8;
typedef unsigned short     U16;
typedef unsigned int       U32;
typedef unsigned long long U64;

typedef signed char      S8;
typedef signed short     S16;
typedef signed int       S32;
typedef signed long long S64;

typedef float  F32;
typedef double F64;

typedef char CHAR;

typedef U64 SIZE;

typedef int BOOLEAN;

typedef B64 ADDRESS;

typedef ADDRESS HANDLE;

HANDLE open_file       (const CHAR *path);
SIZE   get_size_of_file(HANDLE file);
SIZE   read_from_file  (VOID *buffer, SIZE size, HANDLE file);
VOID   close_file      (HANDLE file);

SIZE query_system_page_size(VOID);

VOID *allocate_virtual_memory(SIZE size);
VOID *reserve_virtual_memory (SIZE size);
VOID  commit_virtual_memory  (VOID *memory, SIZE size);
VOID  release_virtual_memory (VOID *memory, SIZE size);

#endif
