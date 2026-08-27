#include "protocol.hpp"
#include <string>
#include <vector>

namespace blockos::bx11::server {

bool Protocol::dispatch(Server &server, uint32_t client_id, ipc::Channel &channel)
{
    ipc::MessageHeader hdr{};
    if(!channel.recv(&hdr,sizeof(hdr))) return false;
    if(hdr.length < sizeof(hdr) || hdr.length > (1u<<20)) return false;
    std::vector<uint8_t> body(hdr.length-sizeof(hdr));
    if(!body.empty() && !channel.recv(body.data(),body.size())) return false;

    switch(static_cast<ipc::MessageType>(hdr.type)) {
    case ipc::MessageType::CreateWindow: {
        if(body.size()!=sizeof(CreateWindowRequest)) return false;
        auto r=*reinterpret_cast<const CreateWindowRequest*>(body.data());
        uint32_t id=server.create_window(client_id,r.parent,r.x,r.y,r.width,r.height,r.border,r.background);
        ipc::MessageHeader reply{sizeof(hdr)+sizeof(id),static_cast<uint16_t>(ipc::MessageType::Reply),0,hdr.sequence};
        channel.send(&reply,sizeof(reply)); channel.send(&id,sizeof(id)); return true;
    }
    case ipc::MessageType::MapWindow:
    case ipc::MessageType::UnmapWindow:
    case ipc::MessageType::DestroyWindow: {
        if(body.size()!=sizeof(WindowIdRequest)) return false;
        uint32_t id=reinterpret_cast<const WindowIdRequest*>(body.data())->window; bool ok=false;
        if(static_cast<ipc::MessageType>(hdr.type)==ipc::MessageType::MapWindow) ok=server.map_window(id);
        if(static_cast<ipc::MessageType>(hdr.type)==ipc::MessageType::UnmapWindow) ok=server.unmap_window(id);
        if(static_cast<ipc::MessageType>(hdr.type)==ipc::MessageType::DestroyWindow) ok=server.destroy_window(id);
        uint32_t result=ok?0u:1u; ipc::MessageHeader reply{sizeof(hdr)+sizeof(result),static_cast<uint16_t>(ipc::MessageType::Reply),0,hdr.sequence}; channel.send(&reply,sizeof(reply));channel.send(&result,sizeof(result)); return true;
    }
    case ipc::MessageType::MoveResizeWindow: {
        if(body.size()!=sizeof(MoveResizeRequest)) return false;
        auto r=*reinterpret_cast<const MoveResizeRequest*>(body.data()); uint32_t result=server.move_resize(r.window,r.x,r.y,r.width,r.height)?0u:1u;
        ipc::MessageHeader reply{sizeof(hdr)+sizeof(result),static_cast<uint16_t>(ipc::MessageType::Reply),0,hdr.sequence};channel.send(&reply,sizeof(reply));channel.send(&result,sizeof(result));return true;
    }
    case ipc::MessageType::SelectInput: {
        if(body.size()!=sizeof(SelectInputRequest)) return false;
        auto r=*reinterpret_cast<const SelectInputRequest*>(body.data()); uint32_t result=server.select_input(r.window,r.mask)?0u:1u;
        ipc::MessageHeader reply{sizeof(hdr)+sizeof(result),static_cast<uint16_t>(ipc::MessageType::Reply),0,hdr.sequence};channel.send(&reply,sizeof(reply));channel.send(&result,sizeof(result));return true;
    }
    default: return false;
    }
}
}
