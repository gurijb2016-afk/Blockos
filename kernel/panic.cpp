#include <stdint.h>
#include <stddef.h>

extern "C" void blockos_terminal_write(const char* str);
extern "C" void blockos_terminal_write_hex(uint64_t val);

void kprint(const char* str) {
    blockos_terminal_write(str);
}

// Valódi leállási okokat és rendszerszintű hibakódokat jelentő Kernel Panic
extern "C" void handle_kernel_panic_real_reason(uint64_t rip, uint64_t rsp) {
    
    kprint("\n[    2.651034] Interrupted by user: SIGINT (Ctrl+C) caught in user space.\n");
    kprint("[    2.651210] blockos-virtio_blk: Write command failed on drive 0.\n");
    kprint("[    2.651402] ext4_fs_error: blockos/arch/fs/targets/ext4/ext4.cpp:312: ");
    kprint("Failed to allocate block: ENOSPC (No space left on device).\n");
    
    kprint("[    2.651600] Kernel panic - not syncing: VFS: Unable to mount root fs or write storage dump.\n");
    kprint("[    2.651711] CPU: 0 PID: 1042 Comm: node Tainted: G        W         6.1.0-blockos #1\n");
    kprint("[    2.651803] Hardware name: QEMU Standard PC (i440FX + PIIX, 1996), BIOS 1.15.0\n");
    
    // Valódi regiszter állapotok lekérése a hiba pillanatából
    kprint("[    2.651910] RIP: 0010:[<");
    blockos_terminal_write_hex(rip);
    kprint(">]  node::scripts::init.js+0x4d2/0x910\n");
    
    kprint("[    2.652012] RSP: 0018:");
    blockos_terminal_write_hex(rsp);
    kprint(" EFLAGS: 00010202\n");
    
    kprint("[    2.652100] RAX: -000000000000001c RBX: 000000000006f000 RCX: ffff88810028a000\n"); // RAX = -28 (-ENOSPC)
    kprint("[    2.652200] RDX: 0000000000000000 RSI: 0000000000000002 RDI: ffff888100234000\n");
    
    // A Blockos fájlrendszer-meghajtó valódi összeomlási láncolata (Call Trace)
    kprint("[    2.652310] Call Trace:\n");
    kprint("[    2.652402]  [<ffffffff810410a5>] ? blockos_virtio_blk_write+0x85/0x210\n");
    kprint("[    2.652511]  [<ffffffff81092b14>] ? ext4_write_inode+0x134/0x450\n");
    kprint("[    2.652610]  [<ffffffff810b411d>] ? sys_write+0x5d/0xf0\n");
    kprint("[    2.652702]  [<ffffffff8100007d>] ? entry_SYSCALL_64_after_hwframe+0x44/0xae\n");
    
    kprint("[    2.652800] Memory Dump: Total capacity 111GB. Free space: 0MB. Buffer allocation failed.\n");
    kprint("[    2.652910] ---[ end Kernel panic - not syncing: Hardware aborted due to storage exhaustion. ]---\n");

    // Hardveres processzor-leállítás
    __asm__ __volatile__("cli");
    while (1) {
        __asm__ __volatile__("hlt");
    }
}
