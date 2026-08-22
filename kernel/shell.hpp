#pragma once

#include <stddef.h>
#include <stdint.h>

#include "console.hpp"
#include "drivers/keymap.hpp"

// A submitted line split on whitespace
struct Args
{
    static constexpr size_t MAX = 16;

    size_t count = 0;
    char* argv[MAX] = {};

    const char* at(size_t index) const
    {
        return index < count ? argv[index] : "";
    }

    // Decimal, or hex with a 0x prefix. False if absent, not numeric, or > 32 bits.
    bool uint(size_t index, uint32_t* out) const;
};

// Line editor and command history for the input strip
class Shell
{
   public:
    static constexpr size_t LINE_MAX = 128;
    static constexpr size_t HISTORY_MAX = 32;

    static constexpr const char* PROMPT = "> ";

    using CommandHandler = void (*)(const Args& args, Console& out);

    void attach(Console& out, int x, int y, int w, int h);
    void set_handler(CommandHandler fn);
    void set_colors(uint32_t fg, uint32_t bg);

    void handle(const KeyPress& key);
    void render(uint8_t* backbuf, uint32_t fb_width);

    bool dirty() const
    {
        return dirty_;
    }

    int x() const
    {
        return x_;
    }
    int y() const
    {
        return y_;
    }
    int w() const
    {
        return w_;
    }
    int h() const
    {
        return h_;
    }

   private:
    void submit();

    // direction < 0 walks toward older entries, > 0 toward newer
    void recall(int direction);

    void insert_char(char c);
    void erase_before_cursor();

    Console* out_ = nullptr;
    CommandHandler handler_ = nullptr;

    // cursor_ <= len_ < LINE_MAX and line_[len_] is always '\0'.
    char line_[LINE_MAX] = {};
    size_t len_ = 0;
    size_t cursor_ = 0;

    char pending_[LINE_MAX] = {};

    // Ring buffer, newest first
    char history_[HISTORY_MAX][LINE_MAX] = {};
    size_t history_count_ = 0;
    size_t history_head_ = 0;

    // -1 while editing fresh input
    int history_pos_ = -1;

    int x_ = 0;
    int y_ = 0;
    int w_ = 0;
    int h_ = 0;

    uint32_t fg_ = 0x00000000;
    uint32_t bg_ = 0x00FFFFC0;

    bool dirty_ = true;
};
