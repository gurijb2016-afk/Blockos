#include "gui.hpp"

// Minimal no-op implementations so linking succeeds during build.
void GuiEngine::render() { }
void GuiEngine::draw_rect(int x, int y, int w, int h, uint32_t color) { (void)x; (void)y; (void)w; (void)h; (void)color; }
void GuiEngine::draw_text(int x, int y, const char* text, uint32_t color) { (void)x; (void)y; (void)text; (void)color; }
void GuiEngine::draw_pixel(int x, int y, uint32_t color) { (void)x; (void)y; (void)color; }

GuiEngine desktop;
