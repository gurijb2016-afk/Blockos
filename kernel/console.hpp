#pragma once

#include <stddef.h>
#include <stdint.h>

class Console
{
   public:
    static constexpr size_t ROWS = 24;
    static constexpr size_t COLS = 80;

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

    bool dirty() const { return dirty_; }

    int x() const { return x_; }
    int y() const { return y_; }
    int w() const { return w_; }
    int h() const { return h_; }

    size_t visible_rows() const;
    size_t visible_cols() const;

   private:
    void scroll();

    char lines_[ROWS][COLS + 1] = {};
    size_t row_ = 0;
    size_t col_ = 0;
    bool dirty_ = true;

    int x_ = 0;
    int y_ = 0;
    int w_ = 0;
    int h_ = 0;

    uint32_t fg_ = 0x00000000;
    uint32_t bg_ = 0x00FFFFC0;
};
