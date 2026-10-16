#!/usr/bin/env python3
"""Static consistency checks for the BlockOS syscall ABI.
Run from the BlockOS repository root: python3 cpy/tests/check_syscall_abi.py .
"""
from pathlib import Path
import re
import sys

root = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()

def require(condition, message):
    if not condition:
        print(f"FAIL: {message}", file=sys.stderr)
        raise SystemExit(1)

paths = {
    "numbers": root / "kernel/syscall/syscall_numbers.hpp",
    "dispatcher": root / "kernel/user_syscall.cpp",
    "libc": root / "userspace/libc/include/blockos_syscall.h",
    "posix": root / "posix/include/blockos_syscall.h",
    "entry": root / "arch/86_64x/syscall_entry.S",
    "tables": root / "arch/86_64x/hardware_tables.cpp",
    "vfs_h": root / "fs/vfs.hpp",
    "vfs_cpp": root / "fs/vfs.cpp",
    "process_h": root / "kernel/process.hpp",
    "process_cpp": root / "kernel/process.cpp",
}
for name, path in paths.items():
    require(path.is_file(), f"missing required source file: {path.relative_to(root) if path.is_relative_to(root) else path}")

numbers_text = paths["numbers"].read_text()
dispatch_text = paths["dispatcher"].read_text()
libc_text = paths["libc"].read_text()
posix_text = paths["posix"].read_text()

native = {name: int(number) for name, number in re.findall(r"\b(SYS_[A-Za-z0-9_]+)\s*=\s*(\d+)\s*,", numbers_text)}
require(len(native) == 101, f"expected 101 native syscalls, found {len(native)}")
require(set(native.values()) == set(range(101)), "native syscall numbers must cover 0..100 exactly")
require(len(set(native.values())) == len(native), "duplicate native syscall number detected")

cases = set(re.findall(r"case\s+blockos::syscall::(SYS_[A-Za-z0-9_]+)", dispatch_text))
missing_cases = sorted(set(native) - cases)
require(not missing_cases, "syscalls with no explicit dispatcher case: " + ", ".join(missing_cases))

libc = {name: int(number) for name, number in re.findall(r"__SYS_([A-Za-z0-9_]+)\s*=\s*(\d+)", libc_text)}
missing_libc = sorted(name[4:] for name in native if name[4:] not in libc)
require(not missing_libc, "userspace libc syscall constants missing: " + ", ".join(missing_libc))
libc_mismatch = sorted((name, libc[name[4:]], number) for name, number in native.items() if libc[name[4:]] != number)
require(not libc_mismatch, f"libc/kernel syscall number mismatch: {libc_mismatch}")

posix = {name.lower(): int(number) for name, number in re.findall(r"^\s*#define\s+BOS_SYS_([A-Za-z0-9_]+)\s+(\d+)\s*$", posix_text, re.M)}
missing_posix = sorted(name[4:] for name in native if name[4:] not in posix)
require(not missing_posix, "POSIX syscall constants missing: " + ", ".join(missing_posix))
posix_mismatch = sorted((name, posix[name[4:]], number) for name, number in native.items() if posix[name[4:]] != number)
require(not posix_mismatch, f"POSIX/kernel syscall number mismatch: {posix_mismatch}")

require("call blockos_syscall_dispatch_frame" in paths["entry"].read_text(), "assembly entry does not call active dispatcher")
require("register_interrupt_handler(128,(void*)blockos_syscall_entry,0xEE)" in paths["tables"].read_text().replace(" ", ""), "IDT vector 128 is not connected to syscall entry")
require("bool remove_directory(const char* path);" in paths["vfs_h"].read_text(), "rmdir VFS declaration missing")
require("bool remove_directory(const char* path)" in paths["vfs_cpp"].read_text(), "rmdir VFS implementation missing")
require("bool waiting_child;" in paths["process_h"].read_text(), "wait4 wake state missing from Process")
require("parent->waiting_child&&wait_selector_matches(parent,exiting)" in dispatch_text.replace(" ", ""), "wait4 child-exit wakeup path missing")

print("PASS: 101 BlockOS syscall numbers are unique and unchanged (0..100)")
print("PASS: every native syscall has an explicit dispatcher case")
print("PASS: libc and POSIX syscall constants match the kernel ABI")
print("PASS: IDT 128 -> syscall assembly -> active C++ dispatcher is connected")
print("PASS: wait4 and rmdir integration checks passed")
