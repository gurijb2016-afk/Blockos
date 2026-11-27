#pragma once

#include <stddef.h>
#include <stdint.h>

class Console
{
   public:
    static constexpr size_t ROWS = 72;
    static constexpr size_t COLS = 120;

    // Retained lines, including those scrolled off the top of the view
    static constexpr size_t SCROLLBACK = 512;

    static constexpr uint32_t GLYPH_W = 8;
    static constexpr uint32_t GLYPH_H = 8;
    static constexpr uint32_t LINE_H = 10;

    void attach(int x, int y, int w, int h);
    void set_colors(uint32_t fg, uint32_t bg);
    void set_current_directory(const char* path);

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

    size_t visible_rows() const;
    size_t visible_cols() const;

    const char* get_current_directory() const
    {
        return current_directory;
    }

   private:
    // Ring slot holding absolute line number line
    char* slot(size_t line);
    const char* slot(size_t line) const;

    // Lines held in the ring, counting the one currently being written
    size_t retained() const;

    // The initializer is required. Leaving any member uninitialized causes g++ to guard it
    // with __cxa_guard_acquire, which this kernel never defines
    char current_directory[256] = {};

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

    // Per-cell colour index (0 = default fg_), set by the ANSI SGR sequences ESC[...m
    uint8_t attrs_[SCROLLBACK][COLS] = {};
    uint8_t cur_attr_ = 0;
    // ANSI parser: 0 = normal, 1 = after ESC, 2 = inside CSI
    uint8_t esc_state_ = 0;
    uint32_t esc_params_[4] = {};
    uint8_t esc_count_ = 0;
    void apply_sgr();

    // Screen mode (alternate screen, used by full-screen programs such as nano):
    // the visible rows are screen_top_ .. screen_top_ + visible_rows() - 1
    size_t screen_top_ = 0;
    bool screen_mode_ = false;
    bool esc_private_ = false;   // '?' prefix seen in the current CSI
    size_t saved_line_ = 0;      // ESC 7 / ESC[s cursor save
    size_t saved_col_ = 0;
    size_t hw_line_ = 0;         // highest line ever used (bottom of the content)

    size_t screen_base() const;
    void goto_line(size_t line);
    void clear_line_range(size_t line);
    void screen_enter();
    void screen_leave();
    void csi_dispatch(char final_byte);
    void erase_display(uint32_t mode);
    void erase_line(uint32_t mode);
    void cursor_to(uint32_t row, uint32_t col);
};
