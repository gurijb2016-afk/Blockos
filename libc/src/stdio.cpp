#include "../include/stdio.h"
#include "../include/string.h"
#include "../include/stdlib.h"

#include <stddef.h>
#include <stdint.h>


namespace
{

static BlockOSFileOps g_ops{};
static BlockOSConsoleWriteFn g_console = nullptr;

static FILE g_stdin{};
static FILE g_stdout{};
static FILE g_stderr{};

static char* output_buffer = nullptr;


static size_t append_char(
    char* buffer,
    size_t capacity,
    size_t pos,
    char c)
{
    if (capacity != 0 &&
        pos + 1 < capacity)
    {
        buffer[pos] = c;
    }

    return pos + 1;
}


static size_t append_string(
    char* buffer,
    size_t capacity,
    size_t pos,
    const char* s)
{
    if (!s)
        s = "(null)";

    while (*s)
    {
        pos = append_char(
            buffer,
            capacity,
            pos,
            *s);

        ++s;
    }

    return pos;
}


static size_t append_unsigned(
    char* buffer,
    size_t capacity,
    size_t pos,
    unsigned long long value,
    unsigned base,
    bool upper)
{
    char digits[] =
        "0123456789abcdef";

    if (upper)
    {
        for (int i = 10; i < 16; ++i)
            digits[i] =
                static_cast<char>(
                    'A' + (i - 10));
    }

    char temp[64];
    size_t count = 0;

    if (value == 0)
    {
        temp[count++] = '0';
    }
    else
    {
        while (value)
        {
            temp[count++] =
                digits[value % base];

            value /= base;
        }
    }

    while (count)
    {
        --count;

        pos = append_char(
            buffer,
            capacity,
            pos,
            temp[count]);
    }

    return pos;
}


static size_t append_signed(
    char* buffer,
    size_t capacity,
    size_t pos,
    long long value)
{
    if (value < 0)
    {
        pos = append_char(
            buffer,
            capacity,
            pos,
            '-');

        unsigned long long magnitude =
            static_cast<unsigned long long>(
                -(value + 1));

        ++magnitude;

        return append_unsigned(
            buffer,
            capacity,
            pos,
            magnitude,
            10,
            false);
    }

    return append_unsigned(
        buffer,
        capacity,
        pos,
        static_cast<unsigned long long>(value),
        10,
        false);
}


static int format_to(
    char* buffer,
    size_t capacity,
    const char* format,
    va_list args)
{
    size_t pos = 0;

    while (*format)
    {
        if (*format != '%')
        {
            pos = append_char(
                buffer,
                capacity,
                pos,
                *format);

            ++format;
            continue;
        }

        ++format;

        if (*format == '%')
        {
            pos = append_char(
                buffer,
                capacity,
                pos,
                '%');

            ++format;
            continue;
        }

        bool long_flag = false;
        bool long_long_flag = false;

        if (*format == 'l')
        {
            long_flag = true;
            ++format;

            if (*format == 'l')
            {
                long_long_flag = true;
                ++format;
            }
        }

        switch (*format)
        {
            case 'd':
            case 'i':
            {
                long long value;

                if (long_long_flag)
                    value = va_arg(args, long long);
                else if (long_flag)
                    value = va_arg(args, long);
                else
                    value = va_arg(args, int);

                pos = append_signed(
                    buffer,
                    capacity,
                    pos,
                    value);

                break;
            }

            case 'u':
            {
                unsigned long long value;

                if (long_long_flag)
                    value = va_arg(
                        args,
                        unsigned long long);
                else if (long_flag)
                    value = va_arg(
                        args,
                        unsigned long);
                else
                    value = va_arg(
                        args,
                        unsigned int);

                pos = append_unsigned(
                    buffer,
                    capacity,
                    pos,
                    value,
                    10,
                    false);

                break;
            }

            case 'x':
            case 'X':
            {
                unsigned long long value;

                if (long_long_flag)
                    value = va_arg(
                        args,
                        unsigned long long);
                else if (long_flag)
                    value = va_arg(
                        args,
                        unsigned long);
                else
                    value = va_arg(
                        args,
                        unsigned int);

                pos = append_unsigned(
                    buffer,
                    capacity,
                    pos,
                    value,
                    16,
                    *format == 'X');

                break;
            }

            case 'o':
            {
                unsigned long long value;

                if (long_long_flag)
                    value = va_arg(
                        args,
                        unsigned long long);
                else if (long_flag)
                    value = va_arg(
                        args,
                        unsigned long);
                else
                    value = va_arg(
                        args,
                        unsigned int);

                pos = append_unsigned(
                    buffer,
                    capacity,
                    pos,
                    value,
                    8,
                    false);

                break;
            }

            case 'c':
            {
                int c = va_arg(args, int);

                pos = append_char(
                    buffer,
                    capacity,
                    pos,
                    static_cast<char>(c));

                break;
            }

            case 's':
            {
                const char* s =
                    va_arg(args, const char*);

                pos = append_string(
                    buffer,
                    capacity,
                    pos,
                    s);

                break;
            }

            case 'p':
            {
                uintptr_t value =
                    reinterpret_cast<uintptr_t>(
                        va_arg(args, void*));

                pos = append_string(
                    buffer,
                    capacity,
                    pos,
                    "0x");

                pos = append_unsigned(
                    buffer,
                    capacity,
                    pos,
                    value,
                    16,
                    false);

                break;
            }

            default:
            {
                pos = append_char(
                    buffer,
                    capacity,
                    pos,
                    '%');

                pos = append_char(
                    buffer,
                    capacity,
                    pos,
                    *format);

                break;
            }
        }

        ++format;
    }

    if (capacity != 0)
    {
        size_t end =
            pos < capacity
                ? pos
                : capacity - 1;

        buffer[end] = '\0';
    }

    return static_cast<int>(pos);
}

}


extern "C"
{

FILE* stdin = &g_stdin;
FILE* stdout = &g_stdout;
FILE* stderr = &g_stderr;


void blockos_stdio_set_ops(
    const BlockOSFileOps* ops)
{
    if (ops)
        g_ops = *ops;
    else
        memset(&g_ops, 0, sizeof(g_ops));
}


void blockos_stdio_set_console(
    BlockOSConsoleWriteFn fn)
{
    g_console = fn;
}


FILE* fopen(
    const char* path,
    const char* mode)
{
    if (!path || !mode || !g_ops.open)
        return nullptr;

    void* handle =
        g_ops.open(path, mode);

    if (!handle)
        return nullptr;

    FILE* file =
        static_cast<FILE*>(malloc(sizeof(FILE)));

    if (!file)
    {
        if (g_ops.close)
            g_ops.close(handle);

        return nullptr;
    }

    memset(file, 0, sizeof(FILE));

    file->handle = handle;
    file->owns_handle = 1;
    file->readable =
        (strchr(mode, 'r') != nullptr) ||
        (strchr(mode, '+') != nullptr);

    file->writable =
        (strchr(mode, 'w') != nullptr) ||
        (strchr(mode, 'a') != nullptr) ||
        (strchr(mode, '+') != nullptr);

    return file;
}


int fclose(FILE* stream)
{
    if (!stream)
        return -1;

    int result = 0;

    if (stream->handle &&
        g_ops.close)
    {
        result =
            g_ops.close(stream->handle);
    }

    if (stream != stdin &&
        stream != stdout &&
        stream != stderr)
    {
        free(stream);
    }

    return result;
}


size_t fread(
    void* ptr,
    size_t size,
    size_t count,
    FILE* stream)
{
    if (!ptr ||
        !stream ||
        !stream->handle ||
        !g_ops.read ||
        size == 0 ||
        count == 0)
    {
        return 0;
    }

    if (count >
        SIZE_MAX / size)
    {
        return 0;
    }

    size_t bytes =
        size * count;

    size_t got =
        g_ops.read(
            stream->handle,
            ptr,
            bytes);

    if (got < bytes)
        stream->eof = 1;

    return got / size;
}


size_t fwrite(
    const void* ptr,
    size_t size,
    size_t count,
    FILE* stream)
{
    if (!ptr ||
        !stream ||
        size == 0 ||
        count == 0)
    {
        return 0;
    }

    if (stream == stdout ||
        stream == stderr)
    {
        if (g_console)
        {
            size_t bytes =
                size * count;

            g_console(
                static_cast<const char*>(ptr),
                bytes);

            return count;
        }
    }

    if (!stream->handle ||
        !g_ops.write)
    {
        return 0;
    }

    if (count >
        SIZE_MAX / size)
    {
        return 0;
    }

    size_t bytes =
        size * count;

    size_t written =
        g_ops.write(
            stream->handle,
            ptr,
            bytes);

    return written / size;
}


int fseek(
    FILE* stream,
    long offset,
    int whence)
{
    if (!stream ||
        !stream->handle ||
        !g_ops.seek)
    {
        return -1;
    }

    stream->eof = 0;

    return g_ops.seek(
        stream->handle,
        offset,
        whence);
}


long ftell(FILE* stream)
{
    if (!stream ||
        !stream->handle ||
        !g_ops.tell)
    {
        return -1;
    }

    return g_ops.tell(
        stream->handle);
}


void rewind(FILE* stream)
{
    if (stream)
        fseek(stream, 0, 0);
}


int feof(FILE* stream)
{
    return stream ? stream->eof : 0;
}


int ferror(FILE* stream)
{
    return stream ? stream->error : 1;
}


int fflush(FILE*)
{
    return 0;
}


int fgetc(FILE* stream)
{
    unsigned char c = 0;

    if (fread(&c, 1, 1, stream) != 1)
        return -1;

    return c;
}


int fputc(int c, FILE* stream)
{
    unsigned char ch =
        static_cast<unsigned char>(c);

    return fwrite(
        &ch,
        1,
        1,
        stream) == 1
        ? c
        : -1;
}


int vsnprintf(
    char* buffer,
    size_t size,
    const char* format,
    va_list args)
{
    if (!format)
        return -1;

    return format_to(
        buffer,
        size,
        format,
        args);
}


int snprintf(
    char* buffer,
    size_t size,
    const char* format,
    ...)
{
    va_list args;

    va_start(args, format);

    int result =
        vsnprintf(
            buffer,
            size,
            format,
            args);

    va_end(args);

    return result;
}


int vsprintf(
    char* buffer,
    const char* format,
    va_list args)
{
    return format_to(
        buffer,
        SIZE_MAX,
        format,
        args);
}


int sprintf(
    char* buffer,
    const char* format,
    ...)
{
    va_list args;

    va_start(args, format);

    int result =
        vsprintf(
            buffer,
            format,
            args);

    va_end(args);

    return result;
}


int vfprintf(
    FILE* stream,
    const char* format,
    va_list args)
{
    if (!stream)
        return -1;

    char buffer[1024];

    int result =
        vsnprintf(
            buffer,
            sizeof(buffer),
            format,
            args);

    if (result < 0)
        return result;

    size_t length =
        static_cast<size_t>(result);

    if (length >= sizeof(buffer))
        length = sizeof(buffer) - 1;

    fwrite(
        buffer,
        1,
        length,
        stream);

    return result;
}


int fprintf(
    FILE* stream,
    const char* format,
    ...)
{
    va_list args;

    va_start(args, format);

    int result =
        vfprintf(
            stream,
            format,
            args);

    va_end(args);

    return result;
}


int vprintf(
    const char* format,
    va_list args)
{
    return vfprintf(
        stdout,
        format,
        args);
}


int printf(
    const char* format,
    ...)
{
    va_list args;

    va_start(args, format);

    int result =
        vprintf(
            format,
            args);

    va_end(args);

    return result;
}

}
