#include "shell.hpp"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "backbuffer.h"

static Args tokenize(char* line)
{
    Args args;

    char* p = line;

    while (args.count < Args::MAX)
    {
        while (isspace((unsigned char) *p))
            ++p;

        if (*p == '\0')
            break;

        args.argv[args.count++] = p;

        while (*p != '\0' && !isspace((unsigned char) *p))
            ++p;

        if (*p == '\0')
            break;

        *p++ = '\0';
    }

    return args;
}

bool Args::uint(size_t index, uint32_t* out) const
{
    if (index >= count || !out)
        return false;

    const char* s = argv[index];

    // Base 10 unless explicitly hex
    const int base = (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) ? 16 : 10;

    char* end = nullptr;

    const unsigned long value = strtoul(s, &end, base);

    if (end == s || *end != '\0')
        return false;

    if (value > 0xFFFFFFFFUL)
        return false;

    *out = (uint32_t) value;

    return true;
}

// Truncating copy that always terminates, returning the length written
static size_t copy_line(char* dst, const char* src)
{
    size_t n = strlen(src);

    if (n > Shell::LINE_MAX - 1)
        n = Shell::LINE_MAX - 1;

    memcpy(dst, src, n);

    dst[n] = '\0';

    return n;
}

void Shell::attach(Console& out, int x, int y, int w, int h)
{
    out_ = &out;
    x_ = x;
    y_ = y;
    w_ = w;
    h_ = h;

    dirty_ = true;
}

void Shell::set_handler(CommandHandler fn)
{
    handler_ = fn;
}

void Shell::set_colors(uint32_t fg, uint32_t bg)
{
    fg_ = fg;
    bg_ = bg;

    dirty_ = true;
}

size_t Shell::page_rows() const
{
    if (!out_)
        return 1;

    const size_t rows = out_->visible_rows();

    return (rows > 1) ? rows - 1 : 1;
}

void Shell::handle(const KeyPress& key)
{
    switch (key.key)
    {
        case NonCharacterKey::Enter:
            submit();
            return;

        case NonCharacterKey::Backspace:
            erase_before_cursor();
            return;

        case NonCharacterKey::Delete:
            if (cursor_ < len_)
            {
                for (size_t i = cursor_; i + 1 < len_; ++i)
                    line_[i] = line_[i + 1];

                len_--;
                line_[len_] = '\0';
                dirty_ = true;
            }
            return;

        case NonCharacterKey::Up:
            recall(-1);
            return;

        case NonCharacterKey::Down:
            recall(+1);
            return;

        case NonCharacterKey::PageUp:
            if (out_)
                out_->scroll_up(page_rows());
            return;

        case NonCharacterKey::PageDown:
            if (out_)
                out_->scroll_down(page_rows());
            return;

        case NonCharacterKey::Left:
            if (cursor_ > 0)
            {
                cursor_--;
                dirty_ = true;
            }
            return;

        case NonCharacterKey::Right:
            if (cursor_ < len_)
            {
                cursor_++;
                dirty_ = true;
            }
            return;

        case NonCharacterKey::Home:
            if (cursor_ > 0)
            {
                cursor_ = 0;
                dirty_ = true;
            }
            return;

        case NonCharacterKey::End:
            if (cursor_ < len_)
            {
                cursor_ = len_;
                dirty_ = true;
            }
            return;

        default:
            break;
    }

    if (key.ch != '\0')
        insert_char(key.ch);
}

size_t Shell::build_prompt(char* buffer, size_t capacity) const
{
    const char* cwd = out_->get_current_directory();

    if (cwd[0] == '\0')
        cwd = "/";

    size_t len = 0;

    for (size_t i = 0; cwd[i] != '\0' && len + 1 < capacity; ++i)
        buffer[len++] = cwd[i];

    for (size_t i = 0; PROMPT[i] != '\0' && len + 1 < capacity; ++i)
        buffer[len++] = PROMPT[i];

    buffer[len] = '\0';

    return len;
}

void Shell::submit()
{
    if (out_)
    {
        // Running a command jumps back to live output so its result is visible
        out_->scroll_to_bottom();

        char prompt[PROMPT_MAX];
        build_prompt(prompt, sizeof(prompt));

        out_->print(prompt);
        out_->print(line_);
        out_->newline();
    }

    if (len_ > 0)
    {
        const size_t newest =
            (history_head_ + HISTORY_MAX - 1) % HISTORY_MAX;

        const bool duplicate =
            history_count_ > 0 && strcmp(history_[newest], line_) == 0;

        if (!duplicate)
        {
            copy_line(history_[history_head_], line_);

            history_head_ = (history_head_ + 1) % HISTORY_MAX;

            if (history_count_ < HISTORY_MAX)
                history_count_++;
        }
    }

    if (handler_ && out_)
    {
        Args args = tokenize(line_);
        handler_(args, *out_);
    }

    history_pos_ = -1;
    len_ = 0;
    cursor_ = 0;
    line_[0] = '\0';
    pending_[0] = '\0';

    dirty_ = true;
}

void Shell::recall(int direction)
{
    if (history_count_ == 0 || direction == 0)
        return;

    int next;
    if (direction < 0)
    {
        // Don't move position past the end of the buffer
        if (history_pos_ + 1 >= (int) history_count_)
            return;

        // Save pending line when moving away from latest user input
        if (history_pos_ < 0)
            copy_line(pending_, line_);

        next = history_pos_ + 1;
    }
    else
    {
        // At the start of the strip, can't go backwards
        if (history_pos_ < 0)
            return;

        next = history_pos_ - 1;
    }

    if (next < 0)
    {
        // Step back out of history
        len_ = copy_line(line_, pending_);
    }
    else
    {
        // Load the next history entry
        const size_t slot =
            (history_head_ + HISTORY_MAX - 1 - (size_t) next) % HISTORY_MAX;

        len_ = copy_line(line_, history_[slot]);
    }

    history_pos_ = next;
    cursor_ = len_;

    dirty_ = true;
}

void Shell::insert_char(char c)
{
    if (len_ + 1 >= LINE_MAX)
        return;

    for (size_t i = len_; i > cursor_; --i)
        line_[i] = line_[i - 1];

    line_[cursor_] = c;

    cursor_++;
    len_++;
    line_[len_] = '\0';

    dirty_ = true;
}

void Shell::erase_before_cursor()
{
    if (cursor_ == 0)
        return;

    for (size_t i = cursor_ - 1; i + 1 < len_; ++i)
        line_[i] = line_[i + 1];

    cursor_--;
    len_--;
    line_[len_] = '\0';

    dirty_ = true;
}

void Shell::render(uint8_t* backbuf, uint32_t fb_width)
{
    if (!backbuf || w_ <= 0 || h_ <= 0)
    {
        dirty_ = false;
        return;
    }

    bb_draw_rect(
        backbuf,
        fb_width,
        (uint32_t) x_,
        (uint32_t) y_,
        (uint32_t) w_,
        (uint32_t) h_,
        bg_);

    const size_t columns = (size_t) w_ / Console::GLYPH_W;

    char prompt[PROMPT_MAX];
    const size_t prompt_len = build_prompt(prompt, sizeof(prompt));

    size_t col = 0;

    for (size_t i = 0; prompt[i] != '\0' && col < columns; ++i, ++col)
    {
        bb_draw_char(
            backbuf,
            fb_width,
            (uint32_t) (x_ + (int) (col * Console::GLYPH_W)),
            (uint32_t) y_,
            prompt[i],
            fg_);
    }

    for (size_t i = 0; i < len_ && col < columns; ++i, ++col)
    {
        bb_draw_char(
            backbuf,
            fb_width,
            (uint32_t) (x_ + (int) (col * Console::GLYPH_W)),
            (uint32_t) y_,
            line_[i],
            fg_);
    }

    // Drawn over the top so it marks the character it sits on
    const size_t cursor_col = prompt_len + cursor_;

    if (cursor_col < columns)
    {
        bb_draw_char(
            backbuf,
            fb_width,
            (uint32_t) (x_ + (int) (cursor_col * Console::GLYPH_W)),
            (uint32_t) y_,
            '_',
            fg_);
    }

    dirty_ = false;
}