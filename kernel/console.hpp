#pragma once

#include <stddef.h>
#include <stdint.h>

class Console
{
   public:
    static constexpr size_t ROWS = 24;
    static constexpr size_t COLS = 80;

    // Retained lines, including those scrolled off the top of the view
    static constexpr size_t SCROLLBACK = 512;

    static constexpr uint32_t GLYPH_W = 8;
    static constexpr uint32_t GLYPH_H = 8;
    static constexpr uint32_t LINE_H = 10;

    void attach(int x, int y, int w, int h);
    void set_colors(uint32_t fg, uint32_t bg);

    void clear();
    void newline();
    void putc(char c);
    void print(const char* text);
    void print_uint(uint64_t value);

    void render(uint8_t* backbuf, uint32_t fb_width);

    // View movement, in lines, toward older and newer output respectively
    void scroll_up(size_t rows);
    void scroll_down(size_t rows);
    void scroll_to_bottom();

    // Lines the view can still move up; 0 when already at the oldest retained line
    size_t max_scroll() const;

    bool dirty() const { return dirty_; }

    int x() const { return x_; }
    int y() const { return y_; }
    int w() const { return w_; }
    int h() const { return h_; }

    size_t visible_rows() const;
    size_t visible_cols() const;

   private:
    // Ring slot holding absolute line number line
    char* slot(size_t line);
    const char* slot(size_t line) const;

    // Lines held in the ring, counting the one currently being written
    size_t retained() const;

    char lines_[SCROLLBACK][COLS + 1] = {};

    // Absolute index of the line being written; only ever increases
    size_t line_ = 0;
    size_t col_ = 0;

    // Lines the view sits above the newest line; 0 follows live output
    size_t view_offset_ = 0;

    bool dirty_ = true;

    int x_ = 0;
    int y_ = 0;
    int w_ = 0;
    int h_ = 0;

    uint32_t fg_ = 0x00000000;
    uint32_t bg_ = 0x00FFFFC0;
};
