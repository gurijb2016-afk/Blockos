#include <kdrive-config.h>
#include "blockos.h"

static KdOsFuncs blockosOsFuncs = {
    .Init = blockosOsInputInit,
    .Enable = blockosEnableInput,
    .Disable = blockosDisableInput,
    .SpecialKey = NULL,
    .Fini = blockosFiniInput,
    .pollEvents = blockosPollInput,
    .Bell = NULL,
};

void InitCard(char *name)
{
    (void) name;
    KdCardInfoAdd(&blockosFuncs, NULL);
}

void ddxInit(void)
{
    KdOsInit(&blockosOsFuncs);
}

void InitOutput(int argc, char **argv)
{
    KdInitOutput(argc, argv);
}

void InitInput(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    KdOsAddInputDrivers();
    blockosRegisterInputDevices();
    KdInitInput();
}

void CloseInput(void)
{
    KdCloseInput();
    blockosFiniInput();
}

void ddxGiveUp(void)
{
    blockosFiniInput();
}

void ddxUseMsg(void)
{
    KdUseMsg();
    ErrorF("\nBlockOS KDrive server usage:\n");
    ErrorF("Framebuffer: %s\n", BLOCKOS_DISPLAY_DEVICE_PATH);
    ErrorF("Display info: %s\n", BLOCKOS_DISPLAY_INFO_PATH);
    ErrorF("Input:      %s\n", BLOCKOS_INPUT_DEVICE_PATH);
}

int ddxProcessArgument(int argc, char **argv, int i)
{
    return KdProcessArgument(argc, argv, i);
}
