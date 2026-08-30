#include "drivers/virtio_input.hpp"

void example_virtio_input()
{
    if (!virtio_input::init())
        return;

    VirtioInputEvent ev{};
    while (virtio_input::poll(&ev)) {
        // Handle one event. Real code should dispatch it to the input subsystem.
        (void)ev;
    }
}
