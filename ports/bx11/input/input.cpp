#include "input.hpp"
namespace blockos::bx11::input {
Event Translator::keyboard(bool press, uint32_t window, uint32_t keycode, uint32_t state) const
{
    Event e{}; e.type = press ? Type::KeyPressEvent : Type::KeyReleaseEvent; e.window = window; e.keycode = keycode; e.state = state; e.x=x_; e.y=y_; e.root_x=x_; e.root_y=y_; return e;
}
Event Translator::mouse_motion(uint32_t window, int dx, int dy, uint32_t state)
{
    x_ += dx; y_ += dy; Event e{}; e.type=Type::MotionEvent; e.window=window; e.x=x_; e.y=y_; e.root_x=x_; e.root_y=y_; e.state=state; return e;
}
Event Translator::mouse_button(uint32_t window, bool press, uint32_t button, uint32_t state) const
{
    Event e{}; e.type=press?Type::ButtonPressEvent:Type::ButtonReleaseEvent; e.window=window; e.button=button; e.state=state; e.x=x_; e.y=y_; e.root_x=x_; e.root_y=y_; return e;
}
}
