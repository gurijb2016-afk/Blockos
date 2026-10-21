#!/usr/bin/env python3
"""Create a deterministic-enough BlockOS newc CPIO initramfs from rootfs/.

Usage: python3 scripts/make_initramfs.py rootfs build/initramfs.cpio

Regular files and symlinks to files inside rootfs are stored as regular files.
This matches BlockOS's current VFS, which does not implement symlink nodes.
Directories are included explicitly. Device nodes, sockets, FIFOs, and symlinked
folders are rejected rather than silently omitted.
"""
from __future__ import annotations

import os
import stat
import sys
import tempfile
from pathlib import Path
from typing import NamedTuple

HEADER_SIZE = 110
MAGIC = b"070701"
TRAILER = "TRAILER!!!"
MAX_VFS_PATH_BYTES = 255  # absolute path, excluding its terminating NUL


class Entry(NamedTuple):
    name: str
    mode: int
    mtime: int
    data: bytes


def fail(message: str) -> "NoReturn":
    print(f"[initramfs][ERROR] {message}", file=sys.stderr)
    raise SystemExit(1)


def align4(value: int) -> int:
    return (value + 3) & ~3


def encode_header(*, ino: int, mode: int, mtime: int,
                  filesize: int, namesize: int) -> bytes:
    # newc has 13 eight-character hexadecimal fields after the 6-byte magic.
    fields = (
        ino, mode, 0, 0, 1, mtime, filesize,
        0, 0, 0, 0, namesize, 0,
    )
    header = MAGIC + b"".join(f"{value:08x}".encode("ascii") for value in fields)
    if len(header) != HEADER_SIZE:
        raise AssertionError(f"internal error: newc header is {len(header)} bytes")
    return header


def append_entry(out: bytearray, entry: Entry, ino: int) -> None:
    name = entry.name.encode("utf-8")
    if b"\0" in name:
        fail(f"NUL in archive path: {entry.name!r}")
    namesize = len(name) + 1
    out.extend(encode_header(
        ino=ino,
        mode=entry.mode,
        mtime=entry.mtime,
        filesize=len(entry.data),
        namesize=namesize,
    ))
    out.extend(name)
    out.append(0)
    out.extend(b"\0" * (align4(len(out)) - len(out)))
    out.extend(entry.data)
    out.extend(b"\0" * (align4(len(out)) - len(out)))


def collect_entries(root: Path) -> list[Entry]:
    try:
        root = root.resolve(strict=True)
    except OSError as exc:
        fail(f"rootfs does not exist: {root}: {exc}")
    if not root.is_dir():
        fail(f"rootfs is not a directory: {root}")

    entries: list[Entry] = []
    for path in root.rglob("*"):
        rel = path.relative_to(root).as_posix()
        if not rel or rel.startswith("/") or any(p in ("", ".", "..") for p in rel.split("/")):
            fail(f"unsafe path in rootfs: {rel!r}")
        if len(("/" + rel).encode("utf-8")) > MAX_VFS_PATH_BYTES:
            fail(f"path exceeds BlockOS VFS path limit: /{rel}")

        if path.is_symlink():
            try:
                target = path.resolve(strict=True)
            except (OSError, RuntimeError) as exc:
                fail(f"broken or invalid symlink: {path}: {exc}")
            try:
                target.relative_to(root)
            except ValueError:
                fail(f"symlink points outside rootfs: {path} -> {target}")
            if not target.is_file():
                fail(f"symlink target is not a regular file (VFS has no symlink nodes): {path} -> {target}")
            st = target.stat()
            mode = stat.S_IFREG | (stat.S_IMODE(st.st_mode) & 0o777)
            try:
                data = target.read_bytes()
            except OSError as exc:
                fail(f"cannot read symlink target {target}: {exc}")
        elif path.is_dir():
            st = path.stat()
            mode = stat.S_IFDIR | (stat.S_IMODE(st.st_mode) & 0o777)
            data = b""
        elif path.is_file():
            st = path.stat()
            mode = stat.S_IFREG | (stat.S_IMODE(st.st_mode) & 0o777)
            try:
                data = path.read_bytes()
            except OSError as exc:
                fail(f"cannot read file {path}: {exc}")
        else:
            fail(f"unsupported special file in rootfs: {path}")

        entries.append(Entry(rel, mode, max(0, int(st.st_mtime)), data))

    # Parent directories appear before their children; stable path order makes
    # the output reproducible apart from source file timestamps.
    entries.sort(key=lambda e: (e.name.count("/"), e.name))
    # The initramfs is a filesystem image, not a shell builder. Do not require
    # /bin/sh here: images used for kernel, driver, and userspace development
    # must still be buildable when no shell binary has been installed yet.
    return entries


def build_archive(entries: list[Entry]) -> bytes:
    out = bytearray()
    for ino, entry in enumerate(entries, start=1):
        append_entry(out, entry, ino)
    append_entry(out, Entry(TRAILER, 0, 0, b""), len(entries) + 1)
    return bytes(out)


def main(argv: list[str]) -> int:
    root = Path(argv[1]) if len(argv) > 1 else Path("rootfs")
    output = Path(argv[2]) if len(argv) > 2 else Path("build/initramfs.cpio")
    entries = collect_entries(root)
    archive = build_archive(entries)
    output.parent.mkdir(parents=True, exist_ok=True)

    # Write atomically so an interrupted build cannot leave a truncated archive.
    tmp_name: str | None = None
    try:
        with tempfile.NamedTemporaryFile(prefix=output.name + ".", suffix=".tmp",
                                         dir=output.parent, delete=False) as tmp:
            tmp_name = tmp.name
            tmp.write(archive)
        os.replace(tmp_name, output)
    except OSError as exc:
        if tmp_name:
            try:
                os.unlink(tmp_name)
            except OSError:
                pass
        fail(f"cannot write {output}: {exc}")

    dirs = sum(1 for entry in entries if stat.S_ISDIR(entry.mode))
    files = len(entries) - dirs
    print(f"[initramfs] format:     newc CPIO (070701)")
    print(f"[initramfs] entries:    {len(entries)} ({dirs} directories, {files} files)")
    print(f"[initramfs] archive:    {output} ({len(archive)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
