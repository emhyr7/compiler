#if !defined(COMPILER_H)
#define COMPILER_H

#include <assert.h>
#define ASSERT(assertion) assert(assertion)

#define TRAP()        __builtin_debugtrap()
#define UNREACHABLE() __builtin_unreachable()

#include <stdarg.h>
typedef va_list vargs;
#define GET_VARGS(vargs, last)    va_start(vargs, last)
#define GET_VARG(type, vargs)     va_arg(vargs, type)
#define COPY_VARGS(output, input) va_copy(output, input)
#define END_VARGS(vargs)          va_end(vargs) 

typedef __UINT8_TYPE__  uint8;
typedef __UINT16_TYPE__ uint16;
typedef __UINT32_TYPE__ uint32;
typedef __UINT64_TYPE__ uint64;

typedef __INT8_TYPE__  sint8;
typedef __INT16_TYPE__ sint16;
typedef __INT32_TYPE__ sint32;
typedef __INT64_TYPE__ sint64;

typedef float  real32;
typedef double real64;

typedef uint8  ubyte;
typedef uint16 uhalf;
typedef uint32 uword;
typedef uint64 ulong;

typedef sint8  sbyte;
typedef sint16 shalf;
typedef sint32 sword;
typedef sint64 slong;

typedef uint8 bit;
typedef uint8 byte;

typedef char   utf8;
typedef uint32 utf32;

extern uint32 system_page_size;

void *allocate_memory(uint32 size);
void *reserve_memory(uint32 size);
void commit_memory(void *memory, uint32 size);
void release_memory(void *memory, uint32 size);

typedef void *handle;

extern handle stdin_handle;
extern handle stdout_handle;
extern handle stderr_handle;

handle open_file(const utf8 *path);
void close_file(handle file);
uint64 get_size_of_file(handle file);
uint32 read_from_file(void *buffer, uint32 size, handle file);
uint32 write_into_file(const void *buffer, uint32 size, handle file);

_Noreturn void terminate(int);

#endif
