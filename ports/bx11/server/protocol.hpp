#pragma once
#include <cstdint>
#include "../ipc/transport.hpp"
#include "server.hpp"
namespace blockos::bx11::server {
class Protocol {
public:
    static bool dispatch(Server &server, uint32_t client_id, ipc::Channel &channel);
};
#pragma pack(push,1)
struct CreateWindowRequest { uint32_t parent; int32_t x,y; uint32_t width,height,border,background; };
struct WindowIdRequest { uint32_t window; };
struct MoveResizeRequest { uint32_t window; int32_t x,y; uint32_t width,height; };
struct SelectInputRequest { uint32_t window; uint64_t mask; };
struct AtomRequest { uint8_t only_if_exists; uint16_t length; };
#pragma pack(pop)
}
