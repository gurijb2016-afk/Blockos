#!/usr/bin/env python3

import random

OUTPUT = "init.data"
SIZE = 1024 * 1024   # 1 MB


patterns = [
    b"\x00",   # 00
    b"\x22",   # 22
    b"sb"      # 73 62 hex
]


with open(OUTPUT, "wb") as f:
    data = bytearray()

    while len(data) < SIZE:
        data.extend(random.choice(patterns))

    f.write(data[:SIZE])


print(f"Kesz: {OUTPUT}")
print(f"Meret: {SIZE} byte")
