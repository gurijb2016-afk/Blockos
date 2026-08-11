#!/usr/bin/env python3

import argparse
import hashlib
import os
import struct
import zlib
from pathlib import Path


MAGIC = b"BFSZ"
VERSION = 1

CHUNK_SIZE = 1024 * 1024  # 1 MiB

# Header:
# magic        4 bytes
# version      uint16
# flags        uint16
# chunk_size   uint32
# original     uint64
# compressed   uint64
# chunks       uint64
# table_offset uint64
# data_offset  uint64

HEADER_FORMAT = "<4sHHIQQQQQ"
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)

# Chunk:
# offset       uint64
# compressed   uint32
# original     uint32
# checksum     32 bytes
# flags        uint32

CHUNK_FORMAT = "<QII32sI"
CHUNK_SIZE_ON_DISK = struct.calcsize(CHUNK_FORMAT)


def sha256(data: bytes) -> bytes:
    return hashlib.sha256(data).digest()


def collect_files(root: Path):
    for path in root.rglob("*"):
        if path.is_file():
            yield path


def compress_directory(source: str, output: str):
    root = Path(source)
    output_path = Path(output)

    if not root.is_dir():
        raise RuntimeError(
            f"Not a directory: {root}"
        )

    chunks = []
    original_size = 0

    print("BFSZ: scanning filesystem...")

    for path in collect_files(root):

        print(f"  + {path.relative_to(root)}")

        with path.open("rb") as f:

            while True:

                raw = f.read(CHUNK_SIZE)

                if not raw:
                    break

                compressed = zlib.compress(
                    raw,
                    level=9
                )

                chunks.append({
                    "compressed": compressed,
                    "original_size": len(raw),
                    "checksum": sha256(raw),
                    "flags": 0,
                })

                original_size += len(raw)

    chunk_count = len(chunks)

    table_offset = HEADER_SIZE

    data_offset = (
        table_offset +
        chunk_count * CHUNK_SIZE_ON_DISK
    )

    current_offset = data_offset

    compressed_size = 0

    for chunk in chunks:

        chunk["offset"] = current_offset

        chunk["compressed_size"] = len(
            chunk["compressed"]
        )

        current_offset += len(
            chunk["compressed"]
        )

        compressed_size += len(
            chunk["compressed"]
        )

    header = struct.pack(
        HEADER_FORMAT,
        MAGIC,
        VERSION,
        0,
        CHUNK_SIZE,
        original_size,
        compressed_size,
        chunk_count,
        table_offset,
        data_offset,
    )

    print()
    print("BFSZ:")
    print(f"  Filesystem size : {original_size:,} bytes")
    print(f"  Compressed size : {compressed_size:,} bytes")
    print(f"  Chunks          : {chunk_count:,}")
    print()

    with output_path.open("wb") as out:

        out.write(header)

        # Chunk table
        for chunk in chunks:

            entry = struct.pack(
                CHUNK_FORMAT,
                chunk["offset"],
                chunk["compressed_size"],
                chunk["original_size"],
                chunk["checksum"],
                chunk["flags"],
            )

            out.write(entry)

        # Data
        for chunk in chunks:
            out.write(
                chunk["compressed"]
            )

    print(
        f"BFSZ image created: {output_path}"
    )


def decompress_bfsz(image: str, output: str):

    image_path = Path(image)
    output_path = Path(output)

    output_path.mkdir(
        parents=True,
        exist_ok=True
    )

    with image_path.open("rb") as f:

        header_data = f.read(
            HEADER_SIZE
        )

        if len(header_data) != HEADER_SIZE:
            raise RuntimeError(
                "Invalid BFSZ header"
            )

        (
            magic,
            version,
            flags,
            chunk_size,
            original_size,
            compressed_size,
            chunk_count,
            table_offset,
            data_offset,
        ) = struct.unpack(
            HEADER_FORMAT,
            header_data
        )

        if magic != MAGIC:
            raise RuntimeError(
                "Not a BFSZ image"
            )

        if version != VERSION:
            raise RuntimeError(
                f"Unsupported BFSZ version: {version}"
            )

        print("BFSZ image:")
        print(
            f"  Original : {original_size:,} bytes"
        )
        print(
            f"  Stored   : {compressed_size:,} bytes"
        )
        print(
            f"  Chunks   : {chunk_count:,}"
        )

        f.seek(table_offset)

        chunks = []

        for _ in range(chunk_count):

            entry = f.read(
                CHUNK_SIZE_ON_DISK
            )

            if len(entry) != CHUNK_SIZE_ON_DISK:
                raise RuntimeError(
                    "Corrupted chunk table"
                )

            (
                offset,
                compressed_len,
                original_len,
                checksum,
                chunk_flags,
            ) = struct.unpack(
                CHUNK_FORMAT,
                entry
            )

            chunks.append({
                "offset": offset,
                "compressed_size": compressed_len,
                "original_size": original_len,
                "checksum": checksum,
                "flags": chunk_flags,
            })

        restored = output_path / "filesystem.raw"

        with restored.open("wb") as out:

            for index, chunk in enumerate(chunks):

                print(
                    f"\rRestoring "
                    f"{index + 1}/{chunk_count}",
                    end=""
                )

                f.seek(chunk["offset"])

                compressed = f.read(
                    chunk["compressed_size"]
                )

                raw = zlib.decompress(
                    compressed
                )

                if len(raw) != chunk["original_size"]:
                    raise RuntimeError(
                        "Size verification failed"
                    )

                if sha256(raw) != chunk["checksum"]:
                    raise RuntimeError(
                        "SHA-256 verification failed"
                    )

                out.write(raw)

        print()
        print(
            f"Restored: {restored}"
        )


def main():

    parser = argparse.ArgumentParser(
        description="BlockOS BFSZ filesystem tool"
    )

    sub = parser.add_subparsers(
        dest="command",
        required=True
    )

    create = sub.add_parser(
        "create",
        help="Create BFSZ image"
    )

    create.add_argument(
        "source"
    )

    create.add_argument(
        "output"
    )

    extract = sub.add_parser(
        "extract",
        help="Extract BFSZ image"
    )

    extract.add_argument(
        "image"
    )

    extract.add_argument(
        "output"
    )

    args = parser.parse_args()

    if args.command == "create":

        compress_directory(
            args.source,
            args.output
        )

    elif args.command == "extract":

        decompress_bfsz(
            args.image,
            args.output
        )


if __name__ == "__main__":
    main()
