#pragma once

#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef long fpos_t;

typedef struct BlockOSFileOps
{
    void* (*open)(const char* path, const char* mode);
    int   (*close)(void* handle);

    size_t (*read)(
        void* handle,
        void* buffer,
        size_t bytes
    );

    size_t (*write)(
        void* handle,
        const void* buffer,
        size_t bytes
    );

    int (*seek)(
        void* handle,
        long offset,
        int whence
    );

    long (*tell)(void* handle);

} BlockOSFileOps;


typedef struct FILE
{
    void* handle;

    int eof;
    int error;

    int readable;
    int writable;

    int owns_handle;
} FILE;


extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;


/*
 * BlockOS VFS bekötése.
 */
void blockos_stdio_set_ops(
    const BlockOSFileOps* ops
);


/*
 * Konzol kimenet.
 */
typedef void (*BlockOSConsoleWriteFn)(
    const char* data,
    size_t length
);

void blockos_stdio_set_console(
    BlockOSConsoleWriteFn fn
);


FILE* fopen(const char* path, const char* mode);
int fclose(FILE* stream);

size_t fread(
    void* ptr,
    size_t size,
    size_t count,
    FILE* stream
);

size_t fwrite(
    const void* ptr,
    size_t size,
    size_t count,
    FILE* stream
);

int fseek(FILE* stream, long offset, int whence);
long ftell(FILE* stream);
void rewind(FILE* stream);

int feof(FILE* stream);
int ferror(FILE* stream);

int fflush(FILE* stream);

int fgetc(FILE* stream);
int fputc(int c, FILE* stream);

int printf(const char* format, ...);

int fprintf(
    FILE* stream,
    const char* format,
    ...
);

int sprintf(
    char* buffer,
    const char* format,
    ...
);

int snprintf(
    char* buffer,
    size_t size,
    const char* format,
    ...
);

int vprintf(
    const char* format,
    va_list args
);

int vfprintf(
    FILE* stream,
    const char* format,
    va_list args
);

int vsprintf(
    char* buffer,
    const char* format,
    va_list args
);

int vsnprintf(
    char* buffer,
    size_t size,
    const char* format,
    va_list args
);

#ifdef __cplusplus
}
#endif
