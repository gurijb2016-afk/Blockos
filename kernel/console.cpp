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

char* Console::slot(size_t line)
{
    return lines_[line % SCROLLBACK];
}

const char* Console::slot(size_t line) const
{
    return lines_[line % SCROLLBACK];
}

size_t Console::retained() const
{
    return (line_ + 1 < SCROLLBACK) ? line_ + 1 : SCROLLBACK;
}

void Console::clear()
{
    for (size_t r = 0; r < SCROLLBACK; ++r)
    {
        for (size_t c = 0; c < COLS; ++c)
            lines_[r][c] = 0;
    }

    line_ = 0;
    col_ = 0;
    view_offset_ = 0;
    dirty_ = true;
}

void Console::newline()
{
    col_ = 0;
    line_++;

    // The ring reuses slots, so the incoming line starts clean
    for (size_t c = 0; c < COLS; ++c)
        slot(line_)[c] = 0;

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
        slot(line_)[col_] = c;
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

size_t Console::max_scroll() const
{
    const size_t rows = visible_rows();
    const size_t held = retained();

    return (held > rows) ? held - rows : 0;
}

void Console::scroll_up(size_t rows)
{
    const size_t limit = max_scroll();

    if (view_offset_ >= limit)
        return;

    view_offset_ = (limit - view_offset_ < rows) ? limit : view_offset_ + rows;
    dirty_ = true;
}

void Console::scroll_down(size_t rows)
{
    if (view_offset_ == 0)
        return;

    view_offset_ = (view_offset_ < rows) ? 0 : view_offset_ - rows;
    dirty_ = true;
}

void Console::scroll_to_bottom()
{
    if (view_offset_ == 0)
        return;

    view_offset_ = 0;
    dirty_ = true;
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

    const size_t rows = visible_rows();

    // attach() can shrink the view out from under an existing offset
    const size_t limit = max_scroll();

    if (view_offset_ > limit)
        view_offset_ = limit;

    const size_t bottom = line_ - view_offset_;
    const size_t first = (bottom + 1 > rows) ? bottom + 1 - rows : 0;

    for (size_t r = 0; first + r <= bottom; ++r)
    {
        bb_draw_text(
            backbuf,
            fb_width,
            x_,
            y_ + r * LINE_H,
            slot(first + r),
            fg_);
    }
    dirty_ = false;
}
