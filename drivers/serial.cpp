#include "serial.hpp"
#include "io.hpp"
#include "device_manager.hpp"

extern "C" {
#include <efi.h>
#include <efilib.h>
}

namespace serial {
namespace {

constexpr uint16_t REG_DATA      = 0;
constexpr uint16_t REG_IER       = 1;
constexpr uint16_t REG_BAUD_LO   = 0;
constexpr uint16_t REG_BAUD_HI   = 1;
constexpr uint16_t REG_IIR_FCR   = 2;
constexpr uint16_t REG_LCR       = 3;
constexpr uint16_t REG_MCR       = 4;
constexpr uint16_t REG_LSR       = 5;
constexpr uint16_t REG_MSR       = 6;
constexpr uint16_t REG_SCRATCH   = 7;

constexpr uint8_t LCR_DLAB       = 0x80;
constexpr uint8_t LCR_8N1        = 0x03;
constexpr uint8_t FCR_ENABLE     = 0xC7; // Enable FIFO, clear TX/RX, 14-byte threshold
constexpr uint8_t MCR_NORMAL     = 0x0B; // DTR + RTS + OUT2 (user-mode interrupts)
constexpr uint8_t MCR_LOOPBACK   = 0x1E;

constexpr uint8_t LSR_DATA_READY = 0x01;
constexpr uint8_t LSR_TX_EMPTY   = 0x20;

static bool g_com1_ready = false;
static bool g_com2_ready = false;

static bool* ready_ptr(uint16_t port)
{
    if (port == COM1) return &g_com1_ready;
    if (port == COM2) return &g_com2_ready;
    return nullptr;
}

static uint8_t irq_for_port(uint16_t port)
{
    if (port == COM1 || port == COM3) return 4;
    if (port == COM2 || port == COM4) return 3;
    return 0;
}

} // namespace

bool init(uint16_t port, uint32_t baud)
{
    if (baud == 0) baud = 115200;
    const uint16_t divisor = static_cast<uint16_t>(115200u / baud);

    // Disable all UART interrupts
    io::outb(port + REG_IER, 0x00);

    // Enable DLAB (set baud rate divisor)
    io::outb(port + REG_LCR, LCR_DLAB);
    io::outb(port + REG_BAUD_LO, static_cast<uint8_t>(divisor & 0xFF));
    io::outb(port + REG_BAUD_HI, static_cast<uint8_t>((divisor >> 8) & 0xFF));

    // 8 bits, no parity, one stop bit (8N1)
    io::outb(port + REG_LCR, LCR_8N1);

    // Enable FIFO, clear TX/RX FIFO, 14-byte threshold
    io::outb(port + REG_IIR_FCR, FCR_ENABLE);

    // Hardware loopback test to verify UART controller is actually present
    io::outb(port + REG_MCR, MCR_LOOPBACK);
    io::outb(port + REG_DATA, 0xAE);

    if (io::inb(port + REG_DATA) != 0xAE) {
        return false;
    }

    // Restore normal operation mode (DTR, RTS, OUT2)
    io::outb(port + REG_MCR, MCR_NORMAL);

    bool* r = ready_ptr(port);
    if (r) *r = true;

    // Register with BlockOS DeviceManager
    hardware_center.register_device(
        port == COM1 ? "uart-16550-com1" : "uart-16550-com2",
        DEV_TYPE_UNKNOWN,
        port,
        irq_for_port(port)
    );

    Print((CHAR16*)L"serial: 16550 UART port 0x%x initialized at %u baud\r\n", (UINTN)port, (UINTN)baud);
    return true;
}

bool is_present(uint16_t port)
{
    bool* r = ready_ptr(port);
    return r ? *r : false;
}

void write_char(char c, uint16_t port)
{
    // Wait until Transmit Holding Register is empty
    for (uint32_t spin = 0; spin < 100000; ++spin) {
        if (io::inb(port + REG_LSR) & LSR_TX_EMPTY)
            break;
        __asm__ volatile("pause");
    }

    io::outb(port + REG_DATA, static_cast<uint8_t>(c));
}

void write_string(const char* str, uint16_t port)
{
    if (!str) return;
    while (*str) {
        if (*str == '\n')
            write_char('\r', port);
        write_char(*str++, port);
    }
}

void write(const void* data, size_t len, uint16_t port)
{
    if (!data || len == 0) return;
    const char* p = static_cast<const char*>(data);
    for (size_t i = 0; i < len; ++i) {
        write_char(p[i], port);
    }
}

bool has_data(uint16_t port)
{
    return (io::inb(port + REG_LSR) & LSR_DATA_READY) != 0;
}

char read_char(uint16_t port)
{
    while (!has_data(port)) {
        __asm__ volatile("pause");
    }
    return static_cast<char>(io::inb(port + REG_DATA));
}

int16_t read_char_nonblocking(uint16_t port)
{
    if (!has_data(port))
        return -1;
    return static_cast<int16_t>(io::inb(port + REG_DATA));
}

} // namespace serial
