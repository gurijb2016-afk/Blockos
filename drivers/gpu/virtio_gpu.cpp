#include "virtio_gpu.hpp"
#include "../virtio_common.hpp"
#include "../virtqueue_ops.hpp"
#include "../dma.hpp"
#include "../pci.hpp"
#include <string.h>

namespace virtio_gpu {
namespace {

constexpr uint32_t CMD_GET_DISPLAY_INFO = 0x0100;
constexpr uint32_t CMD_RESOURCE_CREATE_2D = 0x0101;
constexpr uint32_t CMD_RESOURCE_UNREF = 0x0102;
constexpr uint32_t CMD_SET_SCANOUT = 0x0103;
constexpr uint32_t CMD_RESOURCE_FLUSH = 0x0104;
constexpr uint32_t CMD_TRANSFER_TO_HOST_2D = 0x0105;
constexpr uint32_t CMD_RESOURCE_ATTACH_BACKING = 0x0106;
constexpr uint32_t CMD_RESOURCE_DETACH_BACKING = 0x0107;
constexpr uint32_t RESP_OK_NODATA = 0x1100;
constexpr uint32_t RESP_OK_DISPLAY_INFO = 0x1101;
constexpr uint32_t RESP_ERR = 0x1200;
constexpr uint32_t FORMAT_B8G8R8A8_UNORM = 1;
constexpr uint16_t DESC_F_NEXT = 1;
constexpr uint16_t DESC_F_WRITE = 2;
constexpr uint16_t QUEUE_SIZE = 8;
constexpr uint32_t RESOURCE_ID = 1;
constexpr uint32_t MAX_SCANOUTS = 16;

#pragma pack(push,1)
struct CtrlHdr { uint32_t type, flags; uint64_t fence_id; uint32_t ctx_id, ring_idx; };
struct Rect { uint32_t x,y,width,height; };
struct ResourceCreate2D { CtrlHdr h; uint32_t resource_id, format, width, height; };
struct MemEntry { uint64_t addr; uint32_t length; uint32_t padding; };
struct ResourceAttach { CtrlHdr h; uint32_t resource_id, nr_entries; MemEntry entry; };
struct SetScanout { CtrlHdr h; Rect r; uint32_t scanout_id, resource_id; };
struct Transfer2D { CtrlHdr h; Rect r; uint64_t offset; uint32_t resource_id, padding; };
struct ResourceFlush { CtrlHdr h; Rect r; uint32_t resource_id, padding; };
struct ResourceUnref { CtrlHdr h; uint32_t resource_id, padding; };
struct DisplayOne { Rect r; uint32_t enabled; uint32_t flags; };
struct DisplayInfoResp { CtrlHdr h; DisplayOne pmodes[MAX_SCANOUTS]; };
struct Resp { CtrlHdr h; };
#pragma pack(pop)

struct State {
    bool ready=false;
    virtio_common::DeviceHandle dev{};
    VirtQueueView queue{};
    void* queue_mem=nullptr;
    uint32_t queue_mem_size=0;
    void* cmd=nullptr;
    void* resp=nullptr;
    void* framebuffer=nullptr;
    size_t framebuffer_bytes=0;
    uint32_t width=0, height=0;
    uint32_t scanout=0;
    uint32_t resource=RESOURCE_ID;
    uint64_t fence=1;
};
State g{};

static uint32_t queue_mem_size(uint16_t n) {
    size_t desc=sizeof(VirtqDesc)*n;
    size_t avail=sizeof(uint16_t)*2+sizeof(uint16_t)*n;
    size_t p=desc+avail;
    p=(p+4095)&~size_t(4095);
    return static_cast<uint32_t>(p+sizeof(uint16_t)*2+sizeof(VirtqUsedElem)*n+4095);
}

static Status submit(const void* request, size_t request_len, void* response, size_t response_len) {
    if (!g.ready || !request || !response || request_len == 0 || response_len == 0)
        return Status::InvalidArgument;
    if (request_len > 4096 || response_len > 4096)
        return Status::InvalidArgument;

    memcpy(g.cmd, request, request_len);
    memset(g.resp, 0, response_len);
    virtqueue_ops::set_descriptor(&g.queue, 0, reinterpret_cast<uint64_t>(g.cmd),
                                  static_cast<uint32_t>(request_len), DESC_F_NEXT, 1);
    virtqueue_ops::set_descriptor(&g.queue, 1, reinterpret_cast<uint64_t>(g.resp),
                                  static_cast<uint32_t>(response_len), DESC_F_WRITE, 0);
    virtqueue_ops::submit_descriptor(&g.queue, 0);
    __asm__ volatile("sfence" ::: "memory");
    if (!virtio_common::notify_queue(&g.dev, 0)) return Status::HardwareError;

    for (uint32_t i = 0; i < 1000000; i++) {
        uint32_t id = 0, len = 0;
        if (virtqueue_ops::try_dequeue_used(&g.queue, &id, &len)) {
            __asm__ volatile("lfence" ::: "memory");
            if (id != 0 || len < sizeof(CtrlHdr)) return Status::HardwareError;
            memcpy(response, g.resp, response_len);
            uint32_t type = reinterpret_cast<const Resp*>(response)->h.type;
            return type == RESP_OK_NODATA || type == RESP_OK_DISPLAY_INFO
                       ? Status::Ok
                       : Status::HardwareError;
        }
        __asm__ volatile("pause");
    }
    return Status::Timeout;
}

static Status create_resource() {
    ResourceCreate2D r{}; r.h.type=CMD_RESOURCE_CREATE_2D; r.h.fence_id=g.fence++; r.resource_id=g.resource; r.format=FORMAT_B8G8R8A8_UNORM; r.width=g.width; r.height=g.height;
    Resp out{}; return submit(&r,sizeof(r),&out,sizeof(out));
}

static Status attach_backing() {
    ResourceAttach r{}; r.h.type=CMD_RESOURCE_ATTACH_BACKING; r.h.fence_id=g.fence++; r.resource_id=g.resource; r.nr_entries=1; r.entry.addr=reinterpret_cast<uint64_t>(g.framebuffer); r.entry.length=static_cast<uint32_t>(g.framebuffer_bytes);
    Resp out{}; return submit(&r,sizeof(r),&out,sizeof(out));
}

static Status set_scanout_internal() {
    SetScanout r{}; r.h.type=CMD_SET_SCANOUT; r.h.fence_id=g.fence++; r.r={0,0,g.width,g.height}; r.scanout_id=g.scanout; r.resource_id=g.resource;
    Resp out{}; return submit(&r,sizeof(r),&out,sizeof(out));
}

static Status transfer() {
    Transfer2D r{}; r.h.type=CMD_TRANSFER_TO_HOST_2D; r.h.fence_id=g.fence++; r.r={0,0,g.width,g.height}; r.offset=0; r.resource_id=g.resource;
    Resp out{}; return submit(&r,sizeof(r),&out,sizeof(out));
}

static Status flush_resource() {
    ResourceFlush r{}; r.h.type=CMD_RESOURCE_FLUSH; r.h.fence_id=g.fence++; r.r={0,0,g.width,g.height}; r.resource_id=g.resource;
    Resp out{}; return submit(&r,sizeof(r),&out,sizeof(out));
}

} // namespace

bool init() {
    if(g.ready) return true;
    if(!virtio_common::probe_device(virtio_common::DeviceType::GPU,&g.dev)) return false;
    if(!virtio_common::device_init(&g.dev,0)) return false;
    uint16_t maxq=virtio_common::queue_max_size(&g.dev,0);
    if(maxq<2) return false;
    uint16_t q=maxq>QUEUE_SIZE?QUEUE_SIZE:maxq;
    g.queue_mem_size=queue_mem_size(q);
    g.queue_mem=dma::alloc(g.queue_mem_size,4096);
    g.cmd=dma::alloc(4096,64);
    g.resp=dma::alloc(4096,64);
    if(!g.queue_mem||!g.cmd||!g.resp) return false;
    memset(g.queue_mem,0,g.queue_mem_size);
    if(!virtio_common::setup_queue(&g.dev,0,g.queue_mem,q,&g.queue)) return false;
    uint8_t st=virtio_common::get_device_status(&g.dev);
    virtio_common::set_device_status(&g.dev, static_cast<uint8_t>(st | 0x04));
    if((virtio_common::get_device_status(&g.dev)&0x84)!=0x04) return false;
    g.ready = true;
    DisplayInfo info{};
    uint32_t scanout = 0;
    uint32_t width = 1024;
    uint32_t height = 768;
    if (get_display_info(info) == Status::Ok && info.enabled && info.width > 0 && info.height > 0) {
        scanout = info.scanout;
        width = info.width;
        height = info.height;
    }
    if (set_scanout(scanout, width, height) != Status::Ok) {
        g.ready = false;
        return false;
    }
    return true;
}

bool available(){ return g.ready; }

Status get_display_info(DisplayInfo& out){
    if(!g.ready) return Status::NotFound;
    CtrlHdr req{}; req.type=CMD_GET_DISPLAY_INFO; req.fence_id=g.fence++;
    DisplayInfoResp resp{};
    Status s=submit(&req,sizeof(req),&resp,sizeof(resp));
    if(s!=Status::Ok) return s;
    for(uint32_t i=0;i<MAX_SCANOUTS;i++) {
        if(resp.pmodes[i].enabled) {
            out={resp.pmodes[i].r.width,resp.pmodes[i].r.height,i,true};
            return Status::Ok;
        }
    }
    return Status::NotFound;
}

Status set_scanout(uint32_t scanout,uint32_t width,uint32_t height){
    if(!g.ready||width==0||height==0||width>16384||height>16384||scanout>=MAX_SCANOUTS)
        return Status::InvalidArgument;
    size_t bytes=static_cast<size_t>(width)*height*4;
    void* fb=dma::alloc(bytes,4096); if(!fb) return Status::HardwareError;
    memset(fb,0,bytes);
    if(g.framebuffer){
        ResourceUnref u{}; u.h.type=CMD_RESOURCE_UNREF; u.h.fence_id=g.fence++; u.resource_id=g.resource; Resp out{}; (void)submit(&u,sizeof(u),&out,sizeof(out));
        dma::free(g.framebuffer);
        g.framebuffer=nullptr;
    }
    g.framebuffer=fb; g.framebuffer_bytes=bytes; g.width=width; g.height=height; g.scanout=scanout;
    Status s=create_resource(); if(s!=Status::Ok) return s;
    s=attach_backing(); if(s!=Status::Ok) return s;
    return set_scanout_internal();
}

Status flush(uint32_t scanout,const void* framebuffer,size_t bytes){
    if(!g.ready||!framebuffer||scanout!=g.scanout) return Status::InvalidArgument;
    size_t need=static_cast<size_t>(g.width)*g.height*4;
    if(bytes<need) return Status::InvalidArgument;
    memcpy(g.framebuffer,framebuffer,need);
    Status s=transfer(); if(s!=Status::Ok) return s;
    return flush_resource();
}

} // namespace virtio_gpu
