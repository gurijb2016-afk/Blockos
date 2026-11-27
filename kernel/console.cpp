#include "console.hpp"

#include <string.h>

#include "drivers/backbuffer.h"
#include "fs/proc.hpp"

// Colour index -> 0xRRGGBB. 1..8 = SGR 30..37, 9..16 = SGR 90..97. Index 0 is the default fg_.
static const uint32_t kAttrColor[17] = {
    0,
    0x202020, 0xD05050, 0x5CD65C, 0xD8C060, 0x6F8FAF, 0xB070C0, 0x60C0C0, 0xE0E0E0,
    0x707070, 0xFF6060, 0x80FF80, 0xFFFF80, 0x8090FF, 0xFF80FF, 0x80FFFF, 0xFFFFFF,
};

void Console::attach(int x, int y, int w, int h)
{
    x_ = x;
    y_ = y;
    w_ = w;
    h_ = h;
}

void Console::set_current_directory(const char* path)
{
    size_t len = strlen(path);
    if (len >= sizeof(current_directory))
    {
        len = sizeof(current_directory) - 1; // truncate to fit, leaving room for '\0'
    }
    memcpy(current_directory, path, len);
    current_directory[len] = '\0';
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

void Console::mark_full_dirty()
{
    dirty_ = true;
    full_redraw_ = true;
    has_dirty_rows_ = false;
}

void Console::mark_line_dirty(size_t abs_line)
{
    dirty_ = true;
    if (!has_dirty_rows_)
    {
        dirty_row_min_ = abs_line;
        dirty_row_max_ = abs_line;
        has_dirty_rows_ = true;
    }
    else
    {
        if (abs_line < dirty_row_min_) dirty_row_min_ = abs_line;
        if (abs_line > dirty_row_max_) dirty_row_max_ = abs_line;
    }
}

bool Console::dirty_band(int* out_y, int* out_h) const
{
    if (!out_y || !out_h) return false;
    if (full_redraw_ || !has_dirty_rows_)
    {
        *out_y = y_;
        *out_h = h_;
        return false;
    }
    const size_t rows = visible_rows();
    if (dirty_row_max_ < last_first_ || dirty_row_min_ >= last_first_ + rows)
    {
        *out_y = y_;
        *out_h = h_;
        return false;
    }
    size_t r0 = dirty_row_min_ > last_first_ ? (dirty_row_min_ - last_first_) : 0;
    size_t r1 = (dirty_row_max_ - last_first_) + 1;
    if (r1 > rows) r1 = rows;
    *out_y = y_ + static_cast<int>(r0 * LINE_H);
    *out_h = static_cast<int>((r1 - r0) * LINE_H);
    return true;
}

void Console::clear_dirty_band()
{
    full_redraw_ = false;
    has_dirty_rows_ = false;
}

void Console::clear()
{
    for (size_t r = 0; r < SCROLLBACK; ++r)
    {
        for (size_t c = 0; c < COLS; ++c)
            lines_[r][c] = 0;
    }
    for (size_t r = 0; r < SCROLLBACK; ++r)
        for (size_t c = 0; c < COLS; ++c)
            attrs_[r][c] = 0;
    cur_attr_ = 0;
    esc_state_ = 0;

    line_ = 0;
    col_ = 0;
    view_offset_ = 0;
    mark_full_dirty();
}

void Console::newline()
{
    col_ = 0;
    line_++;
    if (line_ > hw_line_)
        hw_line_ = line_;

    // The ring reuses slots, so the incoming line starts clean
    for (size_t c = 0; c < COLS; ++c)
        slot(line_)[c] = 0;
    for (size_t c = 0; c < COLS; ++c)
        attrs_[line_ % SCROLLBACK][c] = 0;

    mark_line_dirty(line_);
}

// First absolute line of the visible screen
size_t Console::screen_base() const
{
    if (screen_mode_)
        return screen_top_;

    const size_t rows = visible_rows();
    return (hw_line_ + 1 > rows) ? hw_line_ + 1 - rows : 0;
}

// Clear one absolute line (text and colours)
void Console::clear_line_range(size_t line)
{
    for (size_t c = 0; c < COLS; ++c)
    {
        slot(line)[c] = 0;
        attrs_[line % SCROLLBACK][c] = 0;
    }
}

// Move the cursor to an absolute line, creating and clearing the lines passed on the way
void Console::goto_line(size_t line)
{
    if (line > line_)
    {
        for (size_t k = line_ + 1; k <= line; ++k)
            clear_line_range(k);
    }
    line_ = line;
    if (line_ > hw_line_)
        hw_line_ = line_;
    dirty_ = true;
}

void Console::screen_enter()
{
    if (screen_mode_)
        return;

    screen_top_ = screen_base();
    screen_mode_ = true;

    // Lines below the cursor inside the screen start blank
    const size_t bottom = screen_top_ + visible_rows() - 1;
    for (size_t k = line_ + 1; k <= bottom; ++k)
        clear_line_range(k);
    dirty_ = true;
}

void Console::screen_leave()
{
    if (!screen_mode_)
        return;

    // Content stays on screen; the next prompt goes on the line below the lowest one used
    if (hw_line_ > line_)
        line_ = hw_line_;
    newline();

    screen_mode_ = false;
    dirty_ = true;
}

void Console::cursor_to(uint32_t row, uint32_t col)
{
    const uint32_t rows = static_cast<uint32_t>(visible_rows());
    if (row < 1) row = 1;
    if (row > rows) row = rows;
    if (col < 1) col = 1;
    if (col > COLS) col = COLS;

    if (!screen_mode_)
        screen_enter();
    goto_line(screen_base() + row - 1);
    col_ = col - 1;
}

void Console::erase_line(uint32_t mode)
{
    size_t from = 0, to = COLS;   // [from, to)
    if (mode == 0) from = col_;
    else if (mode == 1) to = col_ + 1;

    for (size_t c = from; c < to && c < COLS; ++c)
    {
        slot(line_)[c] = 0;
        attrs_[line_ % SCROLLBACK][c] = 0;
    }
    dirty_ = true;
}

void Console::erase_display(uint32_t mode)
{
    const size_t base = screen_base();
    const size_t bottom = base + visible_rows() - 1;

    if (mode == 2 || mode == 3)
    {
        // Whole screen: switch to screen mode so the cleared area is the visible one
        if (!screen_mode_)
            screen_enter();
        const size_t top = screen_top_;
        for (size_t k = top; k <= top + visible_rows() - 1; ++k)
            clear_line_range(k);
        mark_full_dirty();
        return;
    }

    if (mode == 0)
    {
        erase_line(0);
        for (size_t k = line_ + 1; k <= bottom; ++k)
            clear_line_range(k);
    }
    else if (mode == 1)
    {
        erase_line(1);
        for (size_t k = base; k < line_; ++k)
            clear_line_range(k);
    }
    mark_full_dirty();
}

void Console::csi_dispatch(char final_byte)
{
    const uint32_t p0 = esc_params_[0];
    const uint32_t n = p0 ? p0 : 1;   // default count for cursor moves

    switch (final_byte)
    {
    case 'm':
        apply_sgr();
        break;

    case 'H':
    case 'f':
        cursor_to(p0 ? esc_params_[0] : 1, esc_count_ > 1 && esc_params_[1] ? esc_params_[1] : 1);
        break;

    case 'J':
        erase_display(p0);
        break;

    case 'K':
        erase_line(p0);
        break;

    case 'A':   // up
    {
        const size_t top = screen_base();
        const size_t up = (line_ >= top + n) ? n : (line_ - top);
        goto_line(line_ - up);
        break;
    }
    case 'B':   // down
    {
        const size_t limit = screen_base() + visible_rows() - 1;
        size_t target = line_ + n;
        if (target > limit) target = limit;
        goto_line(target);
        break;
    }
    case 'C':   // right
        col_ = (col_ + n >= COLS) ? COLS - 1 : col_ + n;
        break;
    case 'D':   // left
        col_ = (col_ >= n) ? col_ - n : 0;
        break;

    case 's':   // save cursor (ESC[s)
        saved_line_ = line_;
        saved_col_ = col_;
        break;
    case 'u':   // restore cursor (ESC[u)
        line_ = saved_line_;
        col_ = saved_col_;
        dirty_ = true;
        break;

    case 'h':   // set mode: 1049 / 47 = alternate screen
    case 'l':   // reset mode
        if (esc_private_)
        {
            for (uint8_t i = 0; i < esc_count_; ++i)
            {
                if (esc_params_[i] == 1049 || esc_params_[i] == 47)
                {
                    if (final_byte == 'h') screen_enter();
                    else screen_leave();
                }
            }
        }
        break;

    default:
        // cursor visibility, scroll regions, etc.: accepted and ignored
        break;
    }
}

void Console::apply_sgr()
{
    for (uint8_t i = 0; i < esc_count_; ++i)
    {
        uint32_t p = esc_params_[i];
        if (p == 0 || p == 39)
            cur_attr_ = 0;
        else if (p >= 30 && p <= 37)
            cur_attr_ = static_cast<uint8_t>(p - 29);
        else if (p >= 90 && p <= 97)
            cur_attr_ = static_cast<uint8_t>(p - 81);
        // other SGR codes (bold, background, ...) are accepted and ignored
    }
}

void Console::putc(char c)
{
    // ESC sequences: only SGR (ESC [ ... m) changes state, everything else is swallowed
    if (esc_state_ == 1)
    {
        if (c == '[')
        {
            esc_state_ = 2;
            esc_count_ = 1;
            esc_private_ = false;
            for (auto& v : esc_params_)
                v = 0;
        }
        else if (c == '(' || c == ')' || c == '#')
        {
            esc_state_ = 3;   // charset designation: skip one more byte
        }
        else if (c == '7')
        {
            saved_line_ = line_;
            saved_col_ = col_;
            esc_state_ = 0;
        }
        else if (c == '8')
        {
            // restore without clearing: the lines are already there
            line_ = saved_line_;
            col_ = saved_col_;
            dirty_ = true;
            esc_state_ = 0;
        }
        else
        {
            esc_state_ = 0;
        }
        return;
    }
    if (esc_state_ == 3)
    {
        esc_state_ = 0;
        return;
    }
    if (esc_state_ == 2)
    {
        if (c >= '0' && c <= '9')
            esc_params_[esc_count_ - 1] = esc_params_[esc_count_ - 1] * 10 + static_cast<uint32_t>(c - '0');
        else if (c == ';')
        {
            if (esc_count_ < 4)
                esc_count_++;
        }
        else if (c == '?')
        {
            esc_private_ = true;
        }
        else if (c >= 0x20 && c <= 0x3F)
        {
            // other parameter / intermediate bytes: ignored
        }
        else if (c >= 0x40 && c <= 0x7E)
        {
            csi_dispatch(c);
            esc_state_ = 0;
        }
        else
        {
            esc_state_ = 0;
        }
        return;
    }
    if (c == 0x1b)
    {
        esc_state_ = 1;
        return;
    }

    if (c == '\n')
    {
        newline();
    }
    else if (c == '\r')
    {
        col_ = 0;
    }
    else if (c == '\b' || c == 0x7f)
    {
        if (col_ > 0)
            col_--;
    }
    else if (c == '\t')
    {
        size_t next_tab = (col_ + 4) & ~static_cast<size_t>(3);
        while (col_ < next_tab && col_ < visible_cols())
        {
            slot(line_)[col_] = ' ';
            attrs_[line_ % SCROLLBACK][col_] = cur_attr_;
            col_++;
        }
    }
    else if (c >= 0x20 && c <= 0x7E)
    {
        slot(line_)[col_] = c;
        attrs_[line_ % SCROLLBACK][col_] = cur_attr_;
        col_++;
    }

    if (col_ == visible_cols())
    {
        newline();
    }

    mark_line_dirty(line_);
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
    const size_t rows = visible_rows();

    // attach() can shrink the view out from under an existing offset
    const size_t limit = max_scroll();

    if (view_offset_ > limit)
        view_offset_ = limit;

    size_t bottom;
    size_t first;
    if (screen_mode_)
    {
        first = screen_top_;
        bottom = screen_top_ + rows - 1;
    }
    else
    {
        bottom = hw_line_ - view_offset_;
        first = (bottom + 1 > rows) ? bottom + 1 - rows : 0;
    }

    if (first != last_first_)
        full_redraw_ = true;
    last_first_ = first;

    size_t r_start = 0;
    size_t r_end = (bottom >= first) ? (bottom - first) : 0;
    if (full_redraw_ || !has_dirty_rows_)
    {
        bb_draw_rect(backbuf, fb_width, x_, y_, w_, h_, bg_);
        full_redraw_ = true;
    }
    else
    {
        if (dirty_row_min_ > first)
            r_start = dirty_row_min_ - first;
        if (dirty_row_max_ >= first && dirty_row_max_ - first < r_end)
            r_end = dirty_row_max_ - first;
        if (r_start > r_end)
        {
            dirty_ = false;
            return;
        }
        bb_draw_rect(
            backbuf,
            fb_width,
            x_,
            y_ + static_cast<int>(r_start * LINE_H),
            w_,
            static_cast<int>((r_end - r_start + 1) * LINE_H),
            bg_);
    }

    for (size_t r = r_start; r <= r_end && first + r <= bottom; ++r)
    {
        const char* row = slot(first + r);
        const uint8_t* ar = attrs_[(first + r) % SCROLLBACK];

        // Columns past the last written cell are blank
        size_t row_limit = 0;
        for (size_t k = 0; k < COLS; ++k)
            if (row[k])
                row_limit = k + 1;

        // Draw runs of cells with the same colour
        size_t c = 0;
        while (c < row_limit)
        {
            const uint8_t a = ar[c];
            char tmp[COLS + 1];
            size_t n = 0;
            size_t e = c;
            while (e < row_limit && ar[e] == a)
            {
                tmp[n++] = row[e] ? row[e] : ' ';
                ++e;
            }
            tmp[n] = '\0';
            bb_draw_text(
                backbuf,
                fb_width,
                x_ + static_cast<int>(c * GLYPH_W),
                y_ + r * LINE_H,
                tmp,
                a ? kAttrColor[a] : fg_);
            c = e;
        }
    }
    dirty_ = false;
}
