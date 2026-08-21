#include "shell.hpp"

#include "backbuffer.h"

static size_t line_length(const char* s)
{
    size_t n = 0;

    while (s[n] != '\0')
        ++n;

    return n;
}

static size_t copy_line(char* dst, const char* src)
{
    size_t n = 0;

    while (src[n] != '\0' && n + 1 < Shell::LINE_MAX)
    {
        dst[n] = src[n];
        ++n;
    }

    dst[n] = '\0';

    return n;
}

static bool lines_equal(const char* a, const char* b)
{
    size_t i = 0;

    while (a[i] != '\0' && a[i] == b[i])
        ++i;

    return a[i] == b[i];
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

void Shell::submit()
{
    if (out_)
    {
        out_->print(PROMPT);
        out_->print(line_);
        out_->newline();
    }

    if (len_ > 0)
    {
        const size_t newest =
            (history_head_ + HISTORY_MAX - 1) % HISTORY_MAX;

        const bool duplicate =
            history_count_ > 0 && lines_equal(history_[newest], line_);

        if (!duplicate)
        {
            copy_line(history_[history_head_], line_);

            history_head_ = (history_head_ + 1) % HISTORY_MAX;

            if (history_count_ < HISTORY_MAX)
                history_count_++;
        }
    }

    if (handler_ && out_)
        handler_(line_, *out_);

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
    const size_t prompt_len = line_length(PROMPT);

    size_t col = 0;

    for (size_t i = 0; PROMPT[i] != '\0' && col < columns; ++i, ++col)
    {
        bb_draw_char(
            backbuf,
            fb_width,
            (uint32_t) (x_ + (int) (col * Console::GLYPH_W)),
            (uint32_t) y_,
            PROMPT[i],
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
