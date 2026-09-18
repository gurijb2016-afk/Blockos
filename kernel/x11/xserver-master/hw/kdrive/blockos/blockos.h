#ifndef _KDRIVE_BLOCKOS_H_
#define _KDRIVE_BLOCKOS_H_

#include "kdrive.h"

#define BLOCKOS_DISPLAY_INFO_PATH "/system/display.info"
#define BLOCKOS_DISPLAY_DEVICE_PATH "/devices/display"
#define BLOCKOS_INPUT_DEVICE_PATH "/devices/x11-input"

#define BLOCKOS_FB_MAGIC 0x424F5346u
#define BLOCKOS_IN_MAGIC 0x424F5349u
#define BLOCKOS_PROTOCOL_VERSION 1u
#define BLOCKOS_EVENT_MOUSE 1u
#define BLOCKOS_EVENT_KEYBOARD 2u

#include <stdint.h>

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint64_t framebuffer_phys;
    uint64_t framebuffer_size;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t bpp;
    uint32_t depth;
} BlockOSDisplayInfo;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t type;
    int32_t dx;
    int32_t dy;
    int32_t wheel;
    uint32_t buttons;
    uint32_t scancode;
    uint32_t pressed;
    uint32_t extended;
} BlockOSInputEvent;

extern KdCardFuncs blockosFuncs;

Bool blockosCardInit(KdCardInfo *card);
Bool blockosScreenInit(KdScreenInfo *screen);
Bool blockosInitScreen(ScreenPtr pScreen);
Bool blockosFinishInitScreen(ScreenPtr pScreen);
Bool blockosCreateResources(ScreenPtr pScreen);
void blockosPreserve(KdCardInfo *card);
Bool blockosEnable(ScreenPtr pScreen);
Bool blockosDPMS(ScreenPtr pScreen, int mode);
void blockosDisable(ScreenPtr pScreen);
void blockosRestore(KdCardInfo *card);
void blockosScreenFini(KdScreenInfo *screen);
void blockosCardFini(KdCardInfo *card);

void KdOsAddInputDrivers(void);
void blockosRegisterInputDevices(void);
int blockosOsInputInit(void);
void blockosEnableInput(void);
void blockosDisableInput(void);
void blockosPollInput(void);
void blockosFiniInput(void);

void InitCard(char *name);
void InitOutput(int argc, char **argv);
void InitInput(int argc, char **argv);
void CloseInput(void);
void ddxInit(void);
void ddxGiveUp(void);
void ddxUseMsg(void);
int ddxProcessArgument(int argc, char **argv, int i);

#endif
