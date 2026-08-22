#pragma once

#include <stdint.h>

// Polled ATA PIO driver, LBA28 only.
//
// One instance drives one device: a bus (primary at 0x1F0/0x3F6, secondary at
// 0x170/0x376) plus a master/slave selection on that bus. Transfers are
// synchronous and interrupt-free; init() sets nIEN so the drive stops asserting
// IRQ14, which nothing on this kernel is prepared to handle yet.

// ATA I/O port map (per channel)
// -------------------------------
// Command block, offsets from io_base_:
//
//   Offset  Read                        Write                       Width
//   ------  --------------------------  --------------------------  -----
//   +0      Data                        Data                        16-bit
//   +1      Error                       Features                    8-bit
//   +2      Sector Count                Sector Count                8-bit
//   +3      LBA low (bits 0-7)          LBA low (bits 0-7)          8-bit
//   +4      LBA mid (bits 8-15)         LBA mid (bits 8-15)         8-bit
//   +5      LBA high (bits 16-23)       LBA high (bits 16-23)       8-bit
//   +6      Drive/Head                  Drive/Head                  8-bit
//   +7      Status (acks pending IRQ)   Command                    8-bit
//
// Control block (from control_base_) (its own single port, not offset from io_base_):
//
//   Read: Alternate Status
//     - Same bits as Status (+7 above), but does NOT ack a pending IRQ.
//     - Safe to poll repeatedly.
//
//   Write: Device Control
//     - bit 1 (nIEN): set to 1 to disable the drive's IRQ line
//     - bit 2 (SRST): software reset
//
//
//

class AtaPio
{
   public:
    static constexpr uint16_t PRIMARY_IO = 0x1F0;
    static constexpr uint16_t PRIMARY_CONTROL = 0x3F6;

    static constexpr uint16_t SECONDARY_IO = 0x170;
    static constexpr uint16_t SECONDARY_CONTROL = 0x376;

    static constexpr uint32_t SECTOR_SIZE = 512;
    static constexpr uint32_t SECTOR_WORDS = 256;

    static constexpr uint32_t MAX_LBA = 0x0FFFFFFF;

    enum class Bus : uint8_t
    {
        Primary = 0,
        Secondary = 1
    };

    enum class Drive : uint8_t
    {
        Master = 0,
        Slave = 1,
        Uninitialized = 2
    };

    enum class Error : uint8_t
    {
        None = 0,
        NotPresent, // No device found
        NotAta, // Not an ATA device
        Timeout, // BSY or DRQ timed out
        AddressMarkNotFound, // AMNF: address mark not found
        TrackZeroNotFound, // TKZNF: track 0 not found
        Aborted, // ABRT: command aborted/rejected by device
        MediaChangeRequest, // MCR
        IdNotFound, // IDNF: requested sector not found
        MediaChanged, // MC
        UncorrectableData, // UNC: uncorrectable data error
        BadBlock, // BBK: bad block detected
        DeviceError, // ERR set, but none of the above bits matched
        DeviceFault, // DF set in status
        BadRequest, // LBA past the end of the disk or a null buffer
        FlushTimeout // Sectors transferred, but FLUSH CACHE did not complete
    };

    AtaPio() = default;

    AtaPio(const AtaPio&) = delete;
    AtaPio& operator=(const AtaPio&) = delete;

    // Selects the drive, disables interrupts on it, and runs IDENTIFY to
    // confirm it exists and learn its size. Safe to call on an empty bus.
    bool init(Bus bus, Drive drive);

    bool present() const
    {
        return present_;
    }

    // Total addressable sectors, from IDENTIFY words 60-61. Zero until init()
    // succeeds
    uint32_t sector_count() const;

    // sectors == 0 means 256 sectors, matching the hardware register. buffer
    // must hold sectors * SECTOR_SIZE bytes
    bool read_sectors(uint32_t lba, uint8_t sectors, void* buffer);
    bool write_sectors(uint32_t lba, uint8_t sectors, const void* buffer);

    // Pushes the drive's write cache to the backing store. write_sectors() is
    // not durable until this returns true
    bool flush();

    // Raw contents of the error register from the last failed command
    uint8_t device_error() const;

    int error_count() const
    {
        return error_index_;
    }

    Error error_at(int index) const
    {
        return (index >= 0 && index < error_index_) ? last_errors_[index] : Error::None;
    }

    static const char* error_name(Error error);

   private:
    // Reads the alternate status port often enough to cover the ~400ns the
    // drive needs before its status register is meaningful after a select.
    void delay_400ns() const;

    uint8_t status() const;

    // Alternate status: same bits, but reading it does not acknowledge the
    // pending interrupt the way reading the status port does.
    uint8_t alt_status() const;

    // Drive select, optionally carrying the top 4 bits of an LBA28 address.
    // Includes the 400ns settle.
    void select_drive(uint32_t lba_high_nibble);

    bool wait_not_busy();
    bool wait_ready();
    bool wait_drq();

    // Maps ERR/DF in a status byte onto last_errors_, latching the error
    // register when ERR is set. Returns false if either bit was set.
    bool check_status(uint8_t value);

    // Loads the count and LBA registers, then issues command.
    bool issue_command(uint32_t lba, uint8_t sectors, uint8_t command);

    bool identify();

    bool validate_request(uint32_t lba, uint8_t sectors, const void* buffer,
                          uint32_t& effective_sectors);

    // Base of address space for the eight contigious I/O ports
    uint16_t io_base_ = 0;
    // Base of address space for the single control port
    uint16_t control_base_ = 0;

    Drive drive_ = Drive::Uninitialized;

    bool present_ = false;
    uint32_t sector_count_ = 0;

    static constexpr int MAX_ERRORS = 12;
    int error_index_ = 0;
    Error last_errors_[MAX_ERRORS] = {};
    void clear_errors();
    void write_errors_from_reg(uint8_t err_reg);
    void push_error(Error error);
    bool any_error() const;

    uint8_t device_error_ = 0;
};
