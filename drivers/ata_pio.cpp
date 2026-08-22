#include "ata_pio.hpp"

#include "io.hpp"

namespace
{

constexpr uint16_t REG_DATA = 0;
constexpr uint16_t REG_ERROR = 1; // read
constexpr uint16_t REG_FEATURES = 1; // write
constexpr uint16_t REG_SECTOR_COUNT = 2;
constexpr uint16_t REG_LBA_LOW = 3;
constexpr uint16_t REG_LBA_MID = 4;
constexpr uint16_t REG_LBA_HIGH = 5;
constexpr uint16_t REG_DRIVE = 6;
constexpr uint16_t REG_STATUS = 7; // read
constexpr uint16_t REG_COMMAND = 7; // write

constexpr uint8_t ERR_AMNF = 0x01; // Address Mark Not Found
constexpr uint8_t ERR_TKZNF = 0x02; // Track 0 Not Found
constexpr uint8_t ERR_ABRT = 0x04; // Command Aborted
constexpr uint8_t ERR_MCR = 0x08; // Media Change Request
constexpr uint8_t ERR_IDNF = 0x10; // ID Not Found (sector not found)
constexpr uint8_t ERR_MC = 0x20; // Media Changed
constexpr uint8_t ERR_UNC = 0x40; // Uncorrectable Data Error
constexpr uint8_t ERR_BBK = 0x80; // Bad Block Detected

// Status bits
constexpr uint8_t ST_ERR = 0x01;
constexpr uint8_t ST_DRQ = 0x08;
constexpr uint8_t ST_DF = 0x20;
constexpr uint8_t ST_DRDY = 0x40;
constexpr uint8_t ST_BSY = 0x80;

// Device control bits, written to control_base_
constexpr uint8_t CTRL_NIEN = 0x02;
constexpr uint8_t CTRL_SRST = 0x04;

// Commands
constexpr uint8_t CMD_READ_SECTORS = 0x20;
constexpr uint8_t CMD_WRITE_SECTORS = 0x30;
constexpr uint8_t CMD_FLUSH_CACHE = 0xE7;
constexpr uint8_t CMD_IDENTIFY = 0xEC;

// Drive select: LBA mode plus the two obsolete bits that must be set
constexpr uint8_t DRIVE_LBA_BASE = 0xE0;
constexpr uint8_t DRIVE_SLAVE = 0x10;

// IDENTIFY word 49, bit 9: the drive supports LBA addressing.
constexpr uint16_t IDENT_CAP_LBA = 1 << 9;

constexpr uint32_t POLL_LIMIT = 100000;

} // namespace


bool AtaPio::init(Bus bus, Drive drive)
{
    clear_errors();

    switch (bus)
    {
        case Bus::Primary:
            io_base_ = PRIMARY_IO;
            control_base_ = PRIMARY_CONTROL;
            break;

        case Bus::Secondary:
            io_base_ = SECONDARY_IO;
            control_base_ = SECONDARY_CONTROL;
            break;

        default:
            push_error(Error::BadRequest);
            return false;
    }

    if (drive != Drive::Master && drive != Drive::Slave)
    {
        push_error(Error::BadRequest);
        return false;
    }

    drive_ = drive;
    present_ = false;
    sector_count_ = 0;

    // Nothing in this kernel handles IRQ14 yet, so tell the drive not to raise it
    io::outb(control_base_, CTRL_NIEN);

    select_drive(0);
    uint8_t current_status = status();

    if (current_status == 0xFF || current_status == 0x00)
    {
        push_error(Error::NotPresent);
        return false;
    }

    return identify();
}


uint32_t AtaPio::sector_count() const
{
    return sector_count_;
}


bool AtaPio::read_sectors(uint32_t lba, uint8_t sectors, void* buffer)
{
    clear_errors();

    uint32_t effective_sectors = 0;
    if (!validate_request(lba, sectors, buffer, effective_sectors)) return false;
    if (!issue_command(lba, sectors, CMD_READ_SECTORS)) return false;

    uint8_t* out = static_cast<uint8_t*>(buffer);
    for (uint32_t s = 0; s < effective_sectors; s++)
    {
        if (!wait_drq()) return false;
        io::insw(io_base_ + REG_DATA, out + s * SECTOR_SIZE, SECTOR_WORDS);
        delay_400ns();
    }

    if (!wait_not_busy()) return false;
    return check_status(status());
}


bool AtaPio::write_sectors(uint32_t lba, uint8_t sectors, const void* buffer)
{
    clear_errors();

    uint32_t effective_sectors = 0;
    if (!validate_request(lba, sectors, buffer, effective_sectors)) return false;
    if (!issue_command(lba, sectors, CMD_WRITE_SECTORS)) return false;

    const uint16_t* src = static_cast<const uint16_t*>(buffer);
    for (uint32_t s = 0; s < effective_sectors; s++)
    {
        if (!wait_drq()) return false;

        const uint16_t* sector = src + s * SECTOR_WORDS;
        for (uint32_t word = 0; word < SECTOR_WORDS; word++)
            io::outw(io_base_ + REG_DATA, sector[word]);

        delay_400ns();
    }

    if (!wait_not_busy()) return false;
    if (!check_status(status())) return false;

    return flush();
}


bool AtaPio::flush()
{
    clear_errors();

    select_drive(0);
    if (!wait_ready()) return false;

    io::outb(io_base_ + REG_COMMAND, CMD_FLUSH_CACHE);
    delay_400ns();

    if (!wait_not_busy()) return false;
    return check_status(status());
}


uint8_t AtaPio::device_error() const
{
    return device_error_;
}


const char* AtaPio::error_name(Error error)
{
    switch (error)
    {
        case Error::None:
            return "none";
        case Error::NotPresent:
            return "no device";
        case Error::NotAta:
            return "not an LBA28 ATA device";
        case Error::Timeout:
            return "timeout";
        case Error::AddressMarkNotFound:
            return "address mark not found";
        case Error::TrackZeroNotFound:
            return "track 0 not found";
        case Error::Aborted:
            return "command aborted";
        case Error::MediaChangeRequest:
            return "media change request";
        case Error::IdNotFound:
            return "sector not found";
        case Error::MediaChanged:
            return "media changed";
        case Error::UncorrectableData:
            return "uncorrectable data error";
        case Error::BadBlock:
            return "bad block";
        case Error::DeviceError:
            return "device error";
        case Error::DeviceFault:
            return "device fault";
        case Error::BadRequest:
            return "bad request";
    }

    return "unknown";
}


void AtaPio::delay_400ns() const
{
    // Spins with 4 reads of the alternate status port, which take ~100ns each
    for (int i = 0; i < 4; i++)
        io::inb(control_base_);
}

// Clears pending IRQ on call
uint8_t AtaPio::status() const
{
    return io::inb(io_base_ + REG_STATUS);
}

// Doesn't clear any pending IRQ
uint8_t AtaPio::alt_status() const
{
    return io::inb(control_base_);
}


void AtaPio::select_drive(uint32_t lba_high_nibble)
{
    uint8_t value = DRIVE_LBA_BASE | (drive_ == Drive::Slave ? DRIVE_SLAVE : 0) | static_cast<uint8_t>(lba_high_nibble & 0x0F);
    io::outb(io_base_ + REG_DRIVE, value);
    delay_400ns();
}


bool AtaPio::wait_not_busy()
{
    uint32_t limit = POLL_LIMIT;
    while (limit--)
    {
        if (!(alt_status() & ST_BSY)) return true;
    }

    push_error(Error::Timeout);
    return false;
}

bool AtaPio::wait_ready()
{
    uint32_t limit = POLL_LIMIT;
    while (limit--)
    {
        uint8_t current_status = alt_status();

        if (current_status & ST_BSY) continue;

        if (current_status & (ST_ERR | ST_DF)) return check_status(current_status);
        if (current_status & ST_DRDY) return true;
    }

    push_error(Error::Timeout);
    return false;
}

// BSY clear and DRQ set, bailing early if check_status() reports ERR/DF.
bool AtaPio::wait_drq()
{
    uint32_t limit = POLL_LIMIT;
    while (limit--)
    {
        uint8_t current_status = alt_status();

        if (current_status & ST_BSY) continue;
        if (current_status & (ST_ERR | ST_DF)) return check_status(current_status);
        if (current_status & ST_DRQ) return true;
    }

    push_error(Error::Timeout);
    return false;
}

bool AtaPio::check_status(uint8_t current_status)
{
    if (current_status & ST_ERR)
    {
        device_error_ = io::inb(io_base_ + REG_ERROR);

        if (device_error_ == 0)
            push_error(Error::DeviceError); // ERR set with no bit to explain it
        else
            write_errors_from_reg(device_error_);

        return false;
    }

    if (current_status & ST_DF)
    {
        push_error(Error::DeviceFault);
        return false;
    }

    return true;
}


bool AtaPio::issue_command(uint32_t lba, uint8_t sectors, uint8_t command)
{
    if (!wait_not_busy()) return false;

    select_drive(lba >> 24);

    // Selecting a drive does not make it ready, DRDY has to be set before it will accept a command
    if (!wait_ready()) return false;

    io::outb(io_base_ + REG_SECTOR_COUNT, sectors);
    io::outb(io_base_ + REG_LBA_LOW, (lba >> 0) & 0xFF);
    io::outb(io_base_ + REG_LBA_MID, (lba >> 8) & 0xFF);
    io::outb(io_base_ + REG_LBA_HIGH, (lba >> 16) & 0xFF);

    io::outb(io_base_ + REG_COMMAND, command);
    delay_400ns();

    return true;
}


bool AtaPio::identify()
{
    // Zero high LBA bits of shared registers to clear garbage
    select_drive(0);
    io::outb(io_base_ + REG_SECTOR_COUNT, 0);
    io::outb(io_base_ + REG_LBA_LOW, 0);
    io::outb(io_base_ + REG_LBA_MID, 0);
    io::outb(io_base_ + REG_LBA_HIGH, 0);

    // Write the IDENTIFY command
    io::outb(io_base_ + REG_COMMAND, CMD_IDENTIFY);
    delay_400ns();

    // Read status once
    if (alt_status() == 0) // 0 means no device
    {
        push_error(Error::NotPresent);
        return false;
    }

    if (!wait_not_busy()) return false;

    // Check that ATAPI signature is not present
    uint8_t mid = io::inb(io_base_ + REG_LBA_MID);
    uint8_t high = io::inb(io_base_ + REG_LBA_HIGH);
    if (mid != 0 || high != 0)
    {
        push_error(Error::NotAta);
        return false;
    }

    // Check status
    if (!check_status(status())) return false;

    // Wait for a data request signal
    if (!wait_drq()) return false;

    // Read the identity buffer
    uint16_t buffer[SECTOR_WORDS];
    io::insw(io_base_ + REG_DATA, buffer, SECTOR_WORDS);

    // Extract the sector count
    sector_count_ = static_cast<uint32_t>(buffer[60]) | static_cast<uint32_t>(buffer[61]) << 16;
    if (sector_count_ == 0)
    {
        push_error(Error::NotAta);
        return false;
    }

    present_ = true;
    return true;
}


bool AtaPio::validate_request(uint32_t lba, uint8_t sectors, const void* buffer,
                              uint32_t& effective_sectors)
{
    effective_sectors = (sectors == 0) ? 256u : static_cast<uint32_t>(sectors);

    if (buffer == nullptr || !present_)
    {
        push_error(Error::BadRequest);
        return false;
    }

    if (lba > MAX_LBA || effective_sectors > MAX_LBA - lba + 1)
    {
        push_error(Error::BadRequest);
        return false;
    }

    if (lba >= sector_count_ || effective_sectors > sector_count_ - lba)
    {
        push_error(Error::BadRequest);
        return false;
    }

    return true;
}

void AtaPio::clear_errors()
{
    error_index_ = 0;
    device_error_ = 0;
    for (int i = 0; i < MAX_ERRORS; i++)
        last_errors_[i] = Error::None;
}

void AtaPio::push_error(Error error)
{
    if (error_index_ < MAX_ERRORS)
        last_errors_[error_index_++] = error;
}

void AtaPio::write_errors_from_reg(uint8_t err_reg)
{
    if (err_reg & ERR_AMNF) push_error(Error::AddressMarkNotFound);
    if (err_reg & ERR_TKZNF) push_error(Error::TrackZeroNotFound);
    if (err_reg & ERR_ABRT) push_error(Error::Aborted);
    if (err_reg & ERR_MCR) push_error(Error::MediaChangeRequest);
    if (err_reg & ERR_IDNF) push_error(Error::IdNotFound);
    if (err_reg & ERR_MC) push_error(Error::MediaChanged);
    if (err_reg & ERR_UNC) push_error(Error::UncorrectableData);
    if (err_reg & ERR_BBK) push_error(Error::BadBlock);
}

bool AtaPio::any_error() const
{
    return last_errors_[0] != Error::None;
}
