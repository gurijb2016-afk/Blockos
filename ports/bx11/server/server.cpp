#include "../include/X11/X.h"
#include "server.hpp"

namespace blockos::bx11::server {

bool Server::init(render::Surface *surface)
{
    if (!surface) return false;
    surface_ = surface;
    windows_.clear(); clients_.clear(); atoms_by_name_.clear(); atom_names_.clear();
    windows_[root_] = Window{root_, 0, 0, 0, surface->framebuffer().width, surface->framebuffer().height, 0, 0xFF202020, true, false, 0};
    atoms_by_name_["PRIMARY"] = XA_PRIMARY; atom_names_[XA_PRIMARY] = "PRIMARY";
    atoms_by_name_["SECONDARY"] = XA_SECONDARY; atom_names_[XA_SECONDARY] = "SECONDARY";
    atoms_by_name_["WM_NAME"] = XA_WM_NAME; atom_names_[XA_WM_NAME] = "WM_NAME";
    atoms_by_name_["WM_CLASS"] = XA_WM_CLASS; atom_names_[XA_WM_CLASS] = "WM_CLASS";
    atoms_by_name_["WM_PROTOCOLS"] = 67; atom_names_[67] = "WM_PROTOCOLS";
    atoms_by_name_["WM_DELETE_WINDOW"] = 68; atom_names_[68] = "WM_DELETE_WINDOW";
    atoms_by_name_["_NET_SUPPORTED"] = 69; atom_names_[69] = "_NET_SUPPORTED";
    atoms_by_name_["_NET_SUPPORTING_WM_CHECK"] = 70; atom_names_[70] = "_NET_SUPPORTING_WM_CHECK";
    return true;
}

uint32_t Server::connect(ipc::Channel *channel)
{
    if (!channel) return 0;
    uint32_t id = next_client_id_++;
    clients_[id] = ClientSlot{id, Client{channel, 0, 0, {}}};
    return id;
}

void Server::disconnect(uint32_t client_id) { clients_.erase(client_id); }

uint32_t Server::create_window(uint32_t, uint32_t parent, int x, int y, uint32_t w, uint32_t h, uint32_t border, uint32_t background)
{
    if (!find_window(parent)) return 0;
    uint32_t id = next_window_id_++;
    windows_[id] = Window{id, parent, x, y, w, h, border, background, false, false, 0};
    return id;
}

Window *Server::find_window(uint32_t id) { auto it=windows_.find(id); return it==windows_.end()?nullptr:&it->second; }
const Window *Server::find_window(uint32_t id) const { auto it=windows_.find(id); return it==windows_.end()?nullptr:&it->second; }

bool Server::map_window(uint32_t id) { auto *w=find_window(id); if(!w) return false; w->mapped=true; return true; }
bool Server::unmap_window(uint32_t id) { auto *w=find_window(id); if(!w) return false; w->mapped=false; return true; }
bool Server::destroy_window(uint32_t id) { if(id==root_) return false; return windows_.erase(id)==1; }
bool Server::move_resize(uint32_t id, int x, int y, uint32_t w, uint32_t h) { auto *win=find_window(id); if(!win||!w||!h)return false; win->x=x;win->y=y;win->width=w;win->height=h;return true; }
bool Server::select_input(uint32_t id, uint64_t mask) { auto *w=find_window(id); if(!w) return false; w->event_mask=mask; return true; }

uint32_t Server::intern_atom(const std::string &name, bool only_if_exists)
{
    auto it=atoms_by_name_.find(name); if(it!=atoms_by_name_.end()) return it->second; if(only_if_exists) return 0;
    uint32_t id=next_atom_id_++; atoms_by_name_[name]=id; atom_names_[id]=name; return id;
}

bool Server::push_input(uint32_t window, const input::Event &event)
{
    for(auto &[cid, slot]: clients_) {
        auto *w=find_window(window); if(!w) continue;
        if(w->event_mask != 0) slot.client.events.push_back(event);
        (void)cid;
    }
    return true;
}

bool Server::pop_event(uint32_t client_id, input::Event &event)
{
    auto it=clients_.find(client_id); if(it==clients_.end()||it->second.client.events.empty()) return false;
    event=it->second.client.events.front(); it->second.client.events.pop_front(); return true;
}

bool Server::process_one(uint32_t) { return false; }

void Server::render()
{
    if(!surface_) return;
    surface_->clear(0xFF1E1E1E);
    for(const auto &[id,w]: windows_) {
        if(id==root_ || !w.mapped) continue;
        surface_->rect(w.x,w.y,static_cast<int>(w.width),static_cast<int>(w.height),w.background);
    }
}

}
