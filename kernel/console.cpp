#include "console.hpp"

#include "drivers/backbuffer.h"
#include "fs/proc.hpp"

void Console::attach(int x, int y, int w, int h)
{
    x_ = x;
    y_ = y;
    w_ = w;
    h_ = h;
}

void Console::set_colors(uint32_t fg, uint32_t bg)
{
    fg_ = fg;
    bg_ = bg;
}

void Console::clear()
{
    for (size_t r = 0; r < ROWS; ++r)
    {
        for (size_t c = 0; c < COLS; ++c)
            lines_[r][c] = 0;
    }

    row_ = 0;
    col_ = 0;
    dirty_ = true;
}

void Console::newline()
{
    col_ = 0;
    if (row_ + 1 < visible_rows())
        row_++;
    else
        scroll();

    dirty_ = true;
}

void Console::putc(char c)
{
    if (c == '\n')
    {
        newline();
    }

    else if (c >= 0x20 && c <= 0x7E)
    {
        lines_[row_][col_] = c;
        col_++;
    }

    if (col_ == visible_cols())
    {
        newline();
    }

    dirty_ = true;
}

void Console::print(const char* text)
{
    if (text)
        while (*text)
            putc(*text++);
}

void Console::print_uint(uint64_t value)
{
    char buffer[32];
    size_t buffer_size = sizeof(buffer);

    if (value == 0)
    {
        buffer[0] = '0';
        buffer[1] = '\0';
        print(buffer);
        return;
    }

    char tmp[32];
    size_t count = 0;

    while (value != 0 && count < sizeof(tmp))
    {
        tmp[count++] =
            static_cast<char>('0' + (value % 10));

        value /= 10;
    }

    size_t out = 0;

    while (count > 0)
    {
        buffer[out++] = tmp[--count];
    }

    buffer[out] = '\0';

    print(buffer);
}

void Console::scroll()
{
    for (size_t r = 1; r < visible_rows(); ++r)
    {
        for (size_t c = 0; c < visible_cols(); ++c)
            lines_[r - 1][c] = lines_[r][c];
    }
    for (size_t c = 0; c < visible_cols(); ++c)
        lines_[visible_rows() - 1][c] = 0;

    row_ = visible_rows() - 1;
}

size_t Console::visible_rows() const
{
    size_t rows = h_ / LINE_H;
    if (rows > ROWS)
        rows = ROWS;
    return rows;
}

size_t Console::visible_cols() const
{
    size_t cols = w_ / GLYPH_W;
    if (cols > COLS)
        cols = COLS;
    return cols;
}

void Console::render(uint8_t* backbuf, uint32_t fb_width)
{
    bb_draw_rect(
        backbuf,
        fb_width,
        x_,
        y_,
        w_,
        h_,
        bg_);

    for (size_t r = 0; r < visible_rows(); ++r)
    {
        bb_draw_text(
            backbuf,
            fb_width,
            x_,
            y_ + r * LINE_H,
            lines_[r],
            fg_);
    }
    dirty_ = false;
}
