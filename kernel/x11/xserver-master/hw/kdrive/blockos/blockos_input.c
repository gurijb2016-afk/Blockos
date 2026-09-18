#include <kdrive-config.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kdrive.h"
#include "blockos.h"

static KdPointerInfo *g_pointer;
static KdKeyboardInfo *g_keyboard;
static int g_fd = -1;

static Status blockos_pointer_init(KdPointerInfo *pi)
{
    if (!pi)
        return BadValue;

    pi->nAxes = 3;
    pi->nButtons = 5;
    pi->transformCoordinates = FALSE;
    free(pi->name);
    pi->name = strdup("BlockOS Mouse");
    return pi->name ? Success : BadAlloc;
}

static Status blockos_pointer_enable(KdPointerInfo *pi)
{
    (void) pi;
    return Success;
}

static void blockos_pointer_disable(KdPointerInfo *pi) { (void) pi; }
static void blockos_pointer_fini(KdPointerInfo *pi) { (void) pi; }

static KdPointerDriver blockos_pointer_driver = {
    .name = "blockos",
    .Init = blockos_pointer_init,
    .Enable = blockos_pointer_enable,
    .Disable = blockos_pointer_disable,
    .Fini = blockos_pointer_fini,
};

static Bool blockos_keyboard_preinit(KdKeyboardInfo *ki)
{
    (void) ki;
    return TRUE;
}

static Bool blockos_keyboard_init(KdKeyboardInfo *ki)
{
    if (!ki)
        return FALSE;

    ki->minScanCode = 1;
    ki->maxScanCode = 255;
    free(ki->name);
    ki->name = strdup("BlockOS Keyboard");
    return ki->name != NULL;
}

static Bool blockos_keyboard_enable(KdKeyboardInfo *ki)
{
    (void) ki;
    return TRUE;
}

static void blockos_keyboard_leds(KdKeyboardInfo *ki, int leds)
{
    (void) ki;
    (void) leds;
}

static void blockos_keyboard_bell(KdKeyboardInfo *ki, int volume, int frequency, int duration)
{
    (void) ki;
    (void) volume;
    (void) frequency;
    (void) duration;
}

static void blockos_keyboard_disable(KdKeyboardInfo *ki) { (void) ki; }
static void blockos_keyboard_fini(KdKeyboardInfo *ki) { (void) ki; }

static KdKeyboardDriver blockos_keyboard_driver = {
    .name = "blockos",
    .PreInit = blockos_keyboard_preinit,
    .Init = blockos_keyboard_init,
    .Enable = blockos_keyboard_enable,
    .Leds = blockos_keyboard_leds,
    .Bell = blockos_keyboard_bell,
    .Disable = blockos_keyboard_disable,
    .Fini = blockos_keyboard_fini,
};

static void blockos_input_read(void)
{
    BlockOSInputEvent ev[32];
    ssize_t n;
    size_t count;
    size_t i;

    if (g_fd < 0)
        return;

    do {
        n = read(g_fd, ev, sizeof(ev));
        if (n <= 0)
            return;

        count = (size_t) n / sizeof(ev[0]);
        for (i = 0; i < count; ++i) {
            if (ev[i].magic != BLOCKOS_IN_MAGIC ||
                ev[i].version != BLOCKOS_PROTOCOL_VERSION)
                continue;

            if (ev[i].type == BLOCKOS_EVENT_MOUSE && g_pointer) {
                unsigned long flags = KD_MOUSE_DELTA;

                if (ev[i].buttons & 0x01u) flags |= KD_BUTTON_1;
                if (ev[i].buttons & 0x02u) flags |= KD_BUTTON_2;
                if (ev[i].buttons & 0x04u) flags |= KD_BUTTON_3;
                if (ev[i].buttons & 0x08u) flags |= KD_BUTTON_4;
                if (ev[i].buttons & 0x10u) flags |= KD_BUTTON_5;

                KdEnqueuePointerEvent(g_pointer,
                                      flags,
                                      ev[i].dx,
                                      ev[i].dy,
                                      ev[i].wheel);
            }
            else if (ev[i].type == BLOCKOS_EVENT_KEYBOARD && g_keyboard) {
                KdEnqueueKeyboardEvent(g_keyboard,
                                       (unsigned char) ev[i].scancode,
                                       (unsigned char) (!ev[i].pressed));
            }
        }
    } while ((size_t) n == sizeof(ev));
}

void KdOsAddInputDrivers(void)
{
    KdAddPointerDriver(&blockos_pointer_driver);
    KdAddKeyboardDriver(&blockos_keyboard_driver);
}

int blockosOsInputInit(void)
{
    if (g_fd >= 0)
        return TRUE;

    g_fd = open(BLOCKOS_INPUT_DEVICE_PATH, O_RDONLY | O_NONBLOCK);
    if (g_fd < 0)
        return FALSE;

    return TRUE;
}

void blockosEnableInput(void)
{
    /* The BlockOS input device is opened with O_NONBLOCK in Init.
     * Do not depend on a working fcntl syscall here. */
}

void blockosDisableInput(void) { }

void blockosPollInput(void) { blockos_input_read(); }

void blockosFiniInput(void)
{
    if (g_fd >= 0)
        close(g_fd);

    g_fd = -1;
    g_pointer = NULL;
    g_keyboard = NULL;
}

static void blockos_register_input_devices(void)
{
    if (!g_pointer) {
        g_pointer = KdNewPointer();
        if (g_pointer) {
            g_pointer->driver = &blockos_pointer_driver;
            if (KdAddPointer(g_pointer) != Success) {
                KdFreePointer(g_pointer);
                g_pointer = NULL;
            }
        }
    }

    if (!g_keyboard) {
        g_keyboard = KdNewKeyboard();
        if (g_keyboard) {
            g_keyboard->driver = &blockos_keyboard_driver;
            if (KdAddKeyboard(g_keyboard) != Success) {
                KdFreeKeyboard(g_keyboard);
                g_keyboard = NULL;
            }
        }
    }
}

/* Kept as a compatibility wrapper for older local patches. */
void blockosRegisterInputDevices(void)
{
    blockos_register_input_devices();
}
