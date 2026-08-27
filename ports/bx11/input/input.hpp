#pragma once
#include <cstdint>
#include <functional>

namespace blockos::bx11::input {

enum class Type : uint8_t { KeyPressEvent, KeyReleaseEvent, ButtonPressEvent, ButtonReleaseEvent, MotionEvent };

struct Event {
    Type type{};
    uint32_t window{};
    int x{};
    int y{};
    int root_x{};
    int root_y{};
    uint32_t keycode{};
    uint32_t state{};
    uint32_t button{};
};

class Translator {
public:
    void set_cursor(int x, int y) { x_ = x; y_ = y; }
    Event keyboard(bool press, uint32_t window, uint32_t keycode, uint32_t state) const;
    Event mouse_motion(uint32_t window, int dx, int dy, uint32_t state);
    Event mouse_button(uint32_t window, bool press, uint32_t button, uint32_t state) const;
private:
    int x_{0};
    int y_{0};
};

}
