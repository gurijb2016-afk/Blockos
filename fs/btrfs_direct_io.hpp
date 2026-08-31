
#include <stdint.h>
#include <stddef.h>

namespace blockos::btrfs {

// ============================================================
// Constants
// ============================================================

static constexpr uint64_t PAGE_SIZE = 4096;
static constexpr uint64_t SECTOR_SIZE = 4096;

static constexpr uint64_t DIRECT_IO_ALIGNMENT = 4096;

static constexpr size_t MAX_IO_SIZE =
    128ULL * 1024ULL * 1024ULL;

// ============================================================
// Error codes
// ============================================================

enum class IoStatus : int {
    Ok                 = 0,
    InvalidArgument    = -1,
    AlignmentError     = -2,
    BoundsError        = -3,
    DeviceError        = -4,
    Unsupported        = -5,
    Busy               = -6,
    NoMemory            = -7,
    PermissionDenied   = -8,
    NotInitialized     = -9,
    IoInProgress       = -10,
    IoCancelled        = -11
};

// ============================================================
// I/O flags
// ============================================================

enum IoFlags : uint32_t {
    IO_NONE        = 0,
    IO_READ        = 1U << 0,
    IO_WRITE       = 1U << 1,
    IO_DIRECT      = 1U << 2,
    IO_SYNC        = 1U << 3,
    IO_BARRIER     = 1U << 4,
    IO_FUA         = 1U << 5
};

// ============================================================
// Physical block device abstraction
// ============================================================

class BlockDevice {
public:
    virtual ~BlockDevice() = default;

    virtual uint64_t size_bytes() const = 0;

    virtual uint32_t logical_block_size() const {
        return 4096;
    }

    virtual uint32_t physical_block_size() const {
        return 4096;
    }

    virtual bool supports_direct_io() const {
        return true;
    }

    virtual bool supports_flush() const {
        return true;
    }

    virtual IoStatus read_direct(
        uint64_t offset,
        void* buffer,
        size_t length
    ) = 0;

    virtual IoStatus write_direct(
        uint64_t offset,
        const void* buffer,
        size_t length
    ) = 0;

    virtual IoStatus flush() {
        return IoStatus::Ok;
    }
};

// ============================================================
// Direct I/O request
// ============================================================

struct DirectIoRequest {
    uint64_t file_offset;
    void* buffer;

    size_t length;

    uint32_t flags;

    uint64_t submitted;
    uint64_t completed;

    IoStatus status;
};

// ============================================================
// Completion callback
// ============================================================

using CompletionCallback =
    void (*)(DirectIoRequest* request);

// ============================================================
// Helpers
// ============================================================

static inline bool is_power_of_two(
    uint64_t value
) {
    return value != 0 &&
           (value & (value - 1)) == 0;
}

static inline bool is_aligned(
    uint64_t value,
    uint64_t alignment
) {
    return alignment != 0 &&
           (value & (alignment - 1)) == 0;
}

static inline bool add_overflow_u64(
    uint64_t a,
    uint64_t b,
    uint64_t& result
) {
    if (b > UINT64_MAX - a)
        return true;

    result = a + b;
    return false;
}

static inline bool is_aligned_ptr(
    const void* pointer,
    uint64_t alignment
) {
    uintptr_t address =
        reinterpret_cast<uintptr_t>(pointer);

    return is_aligned(
        static_cast<uint64_t>(address),
        alignment
    );
}

// ============================================================
// Direct I/O context
// ============================================================

class DirectIoContext {
private:
    BlockDevice* device;

    uint32_t logical_block;
    uint32_t physical_block;

    bool initialized;

    uint64_t bytes_read;
    uint64_t bytes_written;

    uint64_t requests_read;
    uint64_t requests_written;

    uint64_t failed_requests;

public:

    DirectIoContext()
        : device(nullptr),
          logical_block(SECTOR_SIZE),
          physical_block(SECTOR_SIZE),
          initialized(false),
          bytes_read(0),
          bytes_written(0),
          requests_read(0),
          requests_written(0),
          failed_requests(0)
    {
    }

    // ========================================================
    // Initialization
    // ========================================================

    IoStatus initialize(
        BlockDevice* block_device
    ) {
        if (!block_device)
            return IoStatus::InvalidArgument;

        if (!block_device->supports_direct_io())
            return IoStatus::Unsupported;

        uint32_t logical =
            block_device->logical_block_size();

        uint32_t physical =
            block_device->physical_block_size();

        if (!is_power_of_two(logical) ||
            !is_power_of_two(physical)) {

            return IoStatus::Unsupported;
        }

        if (logical > DIRECT_IO_ALIGNMENT ||
            physical > DIRECT_IO_ALIGNMENT) {

            return IoStatus::Unsupported;
        }

        device = block_device;

        logical_block = logical;
        physical_block = physical;

        initialized = true;

        bytes_read = 0;
        bytes_written = 0;

        requests_read = 0;
        requests_written = 0;

        failed_requests = 0;

        return IoStatus::Ok;
    }

    // ========================================================
    // Validation
    // ========================================================

    IoStatus validate_request(
        uint64_t offset,
        const void* buffer,
        size_t length,
        bool write
    ) const {

        if (!initialized)
            return IoStatus::NotInitialized;

        if (!device)
            return IoStatus::NotInitialized;

        if (!buffer)
            return IoStatus::InvalidArgument;

        if (length == 0)
            return IoStatus::InvalidArgument;

        if (length > MAX_IO_SIZE)
            return IoStatus::InvalidArgument;

        // File offset must be aligned.
        if (!is_aligned(
                offset,
                DIRECT_IO_ALIGNMENT)) {

            return IoStatus::AlignmentError;
        }

        // Buffer address must be aligned.
        if (!is_aligned_ptr(
                buffer,
                DIRECT_IO_ALIGNMENT)) {

            return IoStatus::AlignmentError;
        }

        // Length must be aligned.
        if (!is_aligned(
                static_cast<uint64_t>(length),
                DIRECT_IO_ALIGNMENT)) {

            return IoStatus::AlignmentError;
        }

        // Check arithmetic overflow.
        uint64_t end;

        if (add_overflow_u64(
                offset,
                static_cast<uint64_t>(length),
                end)) {

            return IoStatus::BoundsError;
        }

        // Device bounds.
        if (end > device->size_bytes())
            return IoStatus::BoundsError;

        // Write/read policy hook.
        (void)write;

        return IoStatus::Ok;
    }

    // ========================================================
    // Direct read
    // ========================================================

    IoStatus read(
        uint64_t offset,
        void* buffer,
        size_t length
    ) {

        IoStatus validation =
            validate_request(
                offset,
                buffer,
                length,
                false
            );

        if (validation != IoStatus::Ok) {
            ++failed_requests;
            return validation;
        }

        DirectIoRequest request{};

        request.file_offset = offset;
        request.buffer = buffer;
        request.length = length;

        request.flags =
            IO_READ |
            IO_DIRECT;

        request.submitted = 0;
        request.completed = 0;

        request.status =
            IoStatus::IoInProgress;

        /*
         * No bounce buffer here.
         *
         * The user/kernel supplied aligned buffer
         * goes directly to the block device.
         */
        IoStatus result =
            device->read_direct(
                offset,
                buffer,
                length
            );

        request.status = result;

        if (result != IoStatus::Ok) {
            ++failed_requests;
            return result;
        }

        bytes_read += length;
        ++requests_read;

        return IoStatus::Ok;
    }

    // ========================================================
    // Direct write
    // ========================================================

    IoStatus write(
        uint64_t offset,
        const void* buffer,
        size_t length
    ) {

        IoStatus validation =
            validate_request(
                offset,
                buffer,
                length,
                true
            );

        if (validation != IoStatus::Ok) {
            ++failed_requests;
            return validation;
        }

        DirectIoRequest request{};

        request.file_offset = offset;
        request.buffer =
            const_cast<void*>(buffer);

        request.length = length;

        request.flags =
            IO_WRITE |
            IO_DIRECT;

        request.submitted = 0;
        request.completed = 0;

        request.status =
            IoStatus::IoInProgress;

        /*
         * No bounce buffer.
         */
        IoStatus result =
            device->write_direct(
                offset,
                buffer,
                length
            );

        request.status = result;

        if (result != IoStatus::Ok) {
            ++failed_requests;
            return result;
        }

        bytes_written += length;
        ++requests_written;

        return IoStatus::Ok;
    }

    // ========================================================
    // Synchronous write + flush
    // ========================================================

    IoStatus write_sync(
        uint64_t offset,
        const void* buffer,
        size_t length
    ) {

        IoStatus result =
            write(
                offset,
                buffer,
                length
            );

        if (result != IoStatus::Ok)
            return result;

        if (!device->supports_flush())
            return IoStatus::Ok;

        return device->flush();
    }

    // ========================================================
    // Explicit flush
    // ========================================================

    IoStatus flush()
    {
        if (!initialized)
            return IoStatus::NotInitialized;

        if (!device->supports_flush())
            return IoStatus::Unsupported;

        return device->flush();
    }

    // ========================================================
    // Async-like submission wrapper
    //
    // This keeps the API ready for a real async block queue.
    // ========================================================

    IoStatus submit(
        DirectIoRequest* request,
        CompletionCallback callback
    ) {

        if (!request)
            return IoStatus::InvalidArgument;

        bool is_write =
            (request->flags & IO_WRITE) != 0;

        bool is_read =
            (request->flags & IO_READ) != 0;

        if (is_write == is_read)
            return IoStatus::InvalidArgument;

        if (!request->buffer)
            return IoStatus::InvalidArgument;

        request->status =
            IoStatus::IoInProgress;

        IoStatus result;

        if (is_read) {
            result =
                read(
                    request->file_offset,
                    request->buffer,
                    request->length
                );
        }
        else {
            result =
                write(
                    request->file_offset,
                    request->buffer,
                    request->length
                );
        }

        request->status = result;

        if (callback)
            callback(request);

        return result;
    }

    // ========================================================
    // Statistics
    // ========================================================

    uint64_t get_bytes_read() const {
        return bytes_read;
    }

    uint64_t get_bytes_written() const {
        return bytes_written;
    }

    uint64_t get_requests_read() const {
        return requests_read;
    }

    uint64_t get_requests_written() const {
        return requests_written;
    }

    uint64_t get_failed_requests() const {
        return failed_requests;
    }

    uint32_t get_logical_block_size() const {
        return logical_block;
    }

    uint32_t get_physical_block_size() const {
        return physical_block;
    }

    uint64_t get_device_size() const {
        if (!device)
            return 0;

        return device->size_bytes();
    }

    bool is_initialized() const {
        return initialized;
    }
};

// ============================================================
// Btrfs file extent
// ============================================================

struct FileExtent {
    uint64_t file_offset;
    uint64_t disk_bytenr;

    uint64_t length;

    bool compressed;
    bool encrypted;
};

// ============================================================
// Btrfs Direct I/O file
// ============================================================

class DirectIoFile {
private:
    DirectIoContext* io;

    uint64_t file_size;

    bool direct_io_enabled;

public:

    DirectIoFile()
        : io(nullptr),
          file_size(0),
          direct_io_enabled(false)
    {
    }

    IoStatus initialize(
        DirectIoContext* context,
        uint64_t size
    ) {

        if (!context ||
            !context->is_initialized()) {

            return IoStatus::InvalidArgument;
        }

        io = context;
        file_size = size;

        direct_io_enabled = true;

        return IoStatus::Ok;
    }

    // ========================================================
    // Set size
    // ========================================================

    void set_size(
        uint64_t size
    ) {
        file_size = size;
    }

    uint64_t size() const {
        return file_size;
    }

    // ========================================================
    // Direct file read
    // ========================================================

    IoStatus read(
        const FileExtent& extent,
        void* buffer,
        size_t length
    ) const {

        if (!io ||
            !direct_io_enabled) {

            return IoStatus::NotInitialized;
        }

        if (extent.length < length)
            return IoStatus::BoundsError;

        uint64_t file_end;

        if (add_overflow_u64(
                extent.file_offset,
                static_cast<uint64_t>(length),
                file_end)) {

            return IoStatus::BoundsError;
        }

        if (file_end > file_size)
            return IoStatus::BoundsError;

        uint64_t disk_end;

        if (add_overflow_u64(
                extent.disk_bytenr,
                static_cast<uint64_t>(length),
                disk_end)) {

            return IoStatus::BoundsError;
        }

        /*
         * Direct I/O is only used for uncompressed,
         * unencrypted extents.
         *
         * Compressed/encrypted extents require transformation,
         * therefore they cannot go through the raw direct path.
         */
        if (extent.compressed ||
            extent.encrypted) {

            return IoStatus::Unsupported;
        }

        return io->read(
            extent.disk_bytenr,
            buffer,
            length
        );
    }

    // ========================================================
    // Direct file write
    // ========================================================

    IoStatus write(
        const FileExtent& extent,
        const void* buffer,
        size_t length
    ) const {

        if (!io ||
            !direct_io_enabled) {

            return IoStatus::NotInitialized;
        }

        if (extent.length < length)
            return IoStatus::BoundsError;

        uint64_t file_end;

        if (add_overflow_u64(
                extent.file_offset,
                static_cast<uint64_t>(length),
                file_end)) {

            return IoStatus::BoundsError;
        }

        if (file_end > file_size)
            return IoStatus::BoundsError;

        /*
         * Transformation paths cannot use this raw direct
         * route.
         */
        if (extent.compressed ||
            extent.encrypted) {

            return IoStatus::Unsupported;
        }

        return io->write(
            extent.disk_bytenr,
            buffer,
            length
        );
    }

    IoStatus write_sync(
        const FileExtent& extent,
        const void* buffer,
        size_t length
    ) const {

        IoStatus result =
            write(
                extent,
                buffer,
                length
            );

        if (result != IoStatus::Ok)
            return result;

        return io->flush();
    }
};

// ============================================================
// Global BlockOS Btrfs Direct I/O instance
// ============================================================

static DirectIoContext g_direct_io;

// ============================================================
// Public initialization
// ============================================================

IoStatus initialize_direct_io(
    BlockDevice* device
) {
    return g_direct_io.initialize(
        device
    );
}

// ============================================================
// Public raw direct read
// ============================================================

IoStatus direct_read(
    uint64_t offset,
    void* buffer,
    size_t length
) {
    return g_direct_io.read(
        offset,
        buffer,
        length
    );
}

// ============================================================
// Public raw direct write
// ============================================================

IoStatus direct_write(
    uint64_t offset,
    const void* buffer,
    size_t length
) {
    return g_direct_io.write(
        offset,
        buffer,
        length
    );
}

// ============================================================
// Public synchronized write
// ============================================================

IoStatus direct_write_sync(
    uint64_t offset,
    const void* buffer,
    size_t length
) {
    return g_direct_io.write_sync(
        offset,
        buffer,
        length
    );
}

// ============================================================
// Public flush
// ============================================================

IoStatus direct_flush()
{
    return g_direct_io.flush();
}

// ============================================================
// Public statistics
// ============================================================

uint64_t direct_bytes_read()
{
    return g_direct_io.get_bytes_read();
}

uint64_t direct_bytes_written()
{
    return g_direct_io.get_bytes_written();
}

uint64_t direct_failed_requests()
{
    return g_direct_io.get_failed_requests();
}

} // namespace blockos::btrfs
