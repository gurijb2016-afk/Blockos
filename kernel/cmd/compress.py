#!/usr/bin/env python3

import argparse
import hashlib
import os
import struct
import sys
import time
import zlib
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path


MAGIC = b"BFSZ"
VERSION = 2

CHUNK_SIZE = 4 * 1024 * 1024  # 4 MiB

HEADER_FORMAT = "<4sHHIQQQQQ"
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)

CHUNK_FORMAT = "<QII32sI"
CHUNK_ENTRY_SIZE = struct.calcsize(CHUNK_FORMAT)


# ---------------------------------------------------------
# Utility
# ---------------------------------------------------------

def format_size(size):
    units = ["B", "KB", "MB", "GB", "TB"]

    value = float(size)

    for unit in units:
        if value < 1024:
            return f"{value:.2f} {unit}"
        value /= 1024

    return f"{value:.2f} PB"


def sha256(data):
    return hashlib.sha256(data).digest()


def get_files(root):
    for path in root.rglob("*"):
        if path.is_file():
            yield path


def get_total_size(root):
    total = 0

    for path in get_files(root):
        try:
            total += path.stat().st_size
        except OSError:
            pass

    return total


# ---------------------------------------------------------
# Progress bar
# ---------------------------------------------------------

class Progress:

    def __init__(self, total):
        self.total = max(total, 1)
        self.start = time.monotonic()
        self.last = 0

    def update(self, current):

        now = time.monotonic()

        if now - self.last < 0.05 and current < self.total:
            return

        self.last = now

        elapsed = max(
            now - self.start,
            0.000001
        )

        percent = min(
            current / self.total,
            1.0
        )

        speed = current / elapsed

        remaining = self.total - current

        if speed > 0:
            eta = remaining / speed
        else:
            eta = 0

        width = 50

        filled = int(
            width * percent
        )

        bar = (
            "=" * filled
            + "-" * (width - filled)
        )

        sys.stdout.write(
            "\r"
            f"[{bar}] "
            f"{percent * 100:6.2f}% "
            f"{format_size(speed)}/s "
            f"ETA {eta:6.1f}s"
        )

        sys.stdout.flush()

    def finish(self):
        self.update(self.total)
        print()


# ---------------------------------------------------------
# Chunk compression
# ---------------------------------------------------------

def compress_chunk(index, data):

    compressed = zlib.compress(
        data,
        level=9
    )

    return {
        "index": index,
        "compressed": compressed,
        "original_size": len(data),
        "compressed_size": len(compressed),
        "checksum": sha256(data),
        "flags": 0,
    }


# ---------------------------------------------------------
# Create BFSZ
# ---------------------------------------------------------

def create_bfsz(source, output, workers):

    root = Path(source)
    output_path = Path(output)

    if not root.exists():
        raise RuntimeError(
            f"Source does not exist: {root}"
        )

    if not root.is_dir():
        raise RuntimeError(
            f"Source is not a directory: {root}"
        )

    print()
    print("BlockOS BFSZ v2")
    print("================")
    print()

    print("Scanning filesystem...")

    total_size = get_total_size(root)

    print(
        f"Input size: {format_size(total_size)}"
    )

    print(
        f"Workers: {workers}"
    )

    print(
        f"Chunk size: {format_size(CHUNK_SIZE)}"
    )

    print()

    # -----------------------------------------------------
    # Read chunks
    # -----------------------------------------------------

    chunks = []

    progress = Progress(total_size)

    processed = 0

    index = 0

    for path in get_files(root):

        try:
            with path.open("rb") as f:

                while True:

                    data = f.read(
                        CHUNK_SIZE
                    )

                    if not data:
                        break

                    chunks.append(
                        (index, data)
                    )

                    index += 1

                    processed += len(data)

                    progress.update(
                        processed
                    )

        except OSError as e:

            raise RuntimeError(
                f"Cannot read {path}: {e}"
            )

    progress.finish()

    print(
        f"Chunks: {len(chunks):,}"
    )

    print()

    # -----------------------------------------------------
    # Compress in parallel
    # -----------------------------------------------------

    print("Compressing...")

    compressed_chunks = [
        None
    ] * len(chunks)

    progress = Progress(total_size)

    processed = 0

    with ThreadPoolExecutor(
        max_workers=workers
    ) as executor:

        futures = []

        for index, data in chunks:

            futures.append(
                executor.submit(
                    compress_chunk,
                    index,
                    data
                )
            )

        for future in as_completed(
            futures
        ):

            result = future.result()

            compressed_chunks[
                result["index"]
            ] = result

            processed += result[
                "original_size"
            ]

            progress.update(
                processed
            )

    progress.finish()

    # Free original chunk memory.
    del chunks

    print()
    print("Building BFSZ image...")

    chunk_count = len(
        compressed_chunks
    )

    table_offset = HEADER_SIZE

    data_offset = (
        table_offset
        + chunk_count * CHUNK_ENTRY_SIZE
    )

    current_offset = data_offset

    compressed_size = 0

    for chunk in compressed_chunks:

        chunk["offset"] = current_offset

        current_offset += chunk[
            "compressed_size"
        ]

        compressed_size += chunk[
            "compressed_size"
        ]

    header = struct.pack(
        HEADER_FORMAT,

        MAGIC,
        VERSION,

        0,

        CHUNK_SIZE,

        total_size,

        compressed_size,

        chunk_count,

        table_offset,

        data_offset,
    )

    # -----------------------------------------------------
    # Write image
    # -----------------------------------------------------

    with output_path.open(
        "wb",
        buffering=1024 * 1024 * 8
    ) as out:

        out.write(header)

        for chunk in compressed_chunks:

            entry = struct.pack(
                CHUNK_FORMAT,

                chunk["offset"],

                chunk["compressed_size"],

                chunk["original_size"],

                chunk["checksum"],

                chunk["flags"],
            )

            out.write(entry)

        progress = Progress(
            compressed_size
        )

        written = 0

        for chunk in compressed_chunks:

            data = chunk[
                "compressed"
            ]

            out.write(data)

            written += len(data)

            progress.update(
                written
            )

    progress.finish()

    ratio = (
        total_size / compressed_size
        if compressed_size
        else 0
    )

    saving = (
        1 -
        compressed_size / total_size
    ) * 100 if total_size else 0

    print()
    print("================")
    print("BFSZ COMPLETE")
    print("================")
    print()

    print(
        f"Original : {format_size(total_size)}"
    )

    print(
        f"BFSZ     : {format_size(compressed_size)}"
    )

    print(
        f"Ratio    : {ratio:.2f}:1"
    )

    print(
        f"Saved    : {saving:.2f}%"
    )

    print()

    print(
        f"Output: {output_path}"
    )


# ---------------------------------------------------------
# Extract BFSZ
# ---------------------------------------------------------

def extract_bfsz(image, output):

    image_path = Path(image)
    output_path = Path(output)

    output_path.mkdir(
        parents=True,
        exist_ok=True
    )

    with image_path.open(
        "rb",
        buffering=1024 * 1024 * 8
    ) as f:

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
                "Invalid BFSZ magic"
            )

        if version != VERSION:
            raise RuntimeError(
                f"Unsupported BFSZ version: {version}"
            )

        print()
        print("BlockOS BFSZ Recovery")
        print("=====================")
        print()

        print(
            f"Original : {format_size(original_size)}"
        )

        print(
            f"Compressed: {format_size(compressed_size)}"
        )

        print(
            f"Chunks   : {chunk_count:,}"
        )

        print()

        # -------------------------------------------------
        # Read chunk table
        # -------------------------------------------------

        f.seek(table_offset)

        table = []

        for _ in range(chunk_count):

            raw = f.read(
                CHUNK_ENTRY_SIZE
            )

            if len(raw) != CHUNK_ENTRY_SIZE:
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
                raw
            )

            table.append({
                "offset": offset,
                "compressed_size": compressed_len,
                "original_size": original_len,
                "checksum": checksum,
                "flags": chunk_flags,
            })

        # -------------------------------------------------
        # Restore
        # -------------------------------------------------

        output_file = (
            output_path /
            "filesystem.raw"
        )

        progress = Progress(
            original_size
        )

        restored = 0

        with output_file.open(
            "wb",
            buffering=1024 * 1024 * 8
        ) as out:

            for chunk in table:

                f.seek(
                    chunk["offset"]
                )

                compressed = f.read(
                    chunk["compressed_size"]
                )

                if len(compressed) != chunk[
                    "compressed_size"
                ]:

                    raise RuntimeError(
                        "Unexpected end of BFSZ image"
                    )

                raw = zlib.decompress(
                    compressed
                )

                if len(raw) != chunk[
                    "original_size"
                ]:

                    raise RuntimeError(
                        "Chunk size mismatch"
                    )

                if sha256(raw) != chunk[
                    "checksum"
                ]:

                    raise RuntimeError(
                        "Chunk checksum failure"
                    )

                out.write(raw)

                restored += len(raw)

                progress.update(
                    restored
                )

        progress.finish()

    print()
    print(
        f"Restored: {output_file}"
    )


# ---------------------------------------------------------
# CLI
# ---------------------------------------------------------

def main():

    parser = argparse.ArgumentParser(
        description="BlockOS BFSZ filesystem compressor"
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

    create.add_argument(
        "-j",
        "--workers",
        type=int,
        default=os.cpu_count() or 4
    )

    extract = sub.add_parser(
        "extract",
        help="Restore BFSZ image"
    )

    extract.add_argument(
        "image"
    )

    extract.add_argument(
        "output"
    )

    args = parser.parse_args()

    if args.command == "create":

        create_bfsz(
            args.source,
            args.output,
            max(1, args.workers)
        )

    elif args.command == "extract":

        extract_bfsz(
            args.image,
            args.output
        )


if __name__ == "__main__":
    main()
