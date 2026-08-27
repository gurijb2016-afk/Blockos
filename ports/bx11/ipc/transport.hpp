#pragma once
#include <cstdint>
#include <cstddef>

namespace blockos::bx11::ipc {

enum class MessageType : uint16_t {
    Hello = 1,
    HelloReply,
    CreateWindow,
    DestroyWindow,
    MapWindow,
    UnmapWindow,
    MoveResizeWindow,
    SelectInput,
    InternAtom,
    ChangeProperty,
    DeleteProperty,
    GetProperty,
    QueryWindow,
    SendEvent,
    GrabPointer,
    UngrabPointer,
    GrabKeyboard,
    UngrabKeyboard,
    Flush,
    Event,
    Reply,
    Error
};

struct MessageHeader {
    uint32_t length;
    uint16_t type;
    uint16_t flags;
    uint32_t sequence;
};

class Channel {
public:
    virtual ~Channel() = default;
    virtual bool send(const void *data, size_t size) = 0;
    virtual bool recv(void *data, size_t size) = 0;
};

class RingChannel final : public Channel {
public:
    static constexpr size_t Capacity = 64 * 1024;
    RingChannel() = default;
    bool send(const void *data, size_t size) override;
    bool recv(void *data, size_t size) override;
    size_t pending() const { return size_; }
private:
    uint8_t buffer_[Capacity]{};
    size_t read_{0};
    size_t write_{0};
    size_t size_{0};
};

}
