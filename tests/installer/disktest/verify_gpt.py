import struct, sys, uuid, zlib
img = open(sys.argv[1], 'rb').read()
S = 512
total = len(img) // S
ok = True
def check(cond, msg):
    global ok
    print(("OK   " if cond else "FAIL ") + msg)
    ok = ok and cond
mbr = img[0:S]
check(mbr[510:512] == b'\x55\xaa', "MBR signature")
check(mbr[450] == 0xEE, "protective MBR type 0xEE")
check(struct.unpack_from('<I', mbr, 454)[0] == 1, "protective MBR starts at LBA 1")
hdr = img[S:2*S]
check(hdr[0:8] == b'EFI PART', "primary GPT signature")
hsz = struct.unpack_from('<I', hdr, 12)[0]
hcrc = struct.unpack_from('<I', hdr, 16)[0]
tmp = bytearray(hdr[:hsz]); tmp[16:20] = b'\0\0\0\0'
check(zlib.crc32(bytes(tmp)) == hcrc, "primary header CRC32")
my, alt = struct.unpack_from('<QQ', hdr, 24)
first_usable, last_usable = struct.unpack_from('<QQ', hdr, 40)
ent_lba, n, esz, ent_crc = struct.unpack_from('<QIII', hdr, 72)
check(my == 1 and alt == total - 1, f"header MyLBA=1 AlternateLBA={alt} (total {total})")
check(first_usable == 34 and last_usable == total - 34, f"usable {first_usable}..{last_usable}")
check(ent_lba == 2 and n == 128 and esz == 128, "entry array at LBA 2, 128 x 128 bytes")
ents = img[ent_lba*S: ent_lba*S + n*esz]
check(zlib.crc32(ents) == ent_crc, "partition entry array CRC32")
def part(i):
    e = ents[i*128:(i+1)*128]
    t = uuid.UUID(bytes_le=bytes(e[0:16]))
    first, last = struct.unpack_from('<QQ', e, 32)
    name = e[56:].decode('utf-16le').rstrip('\0')
    return t, first, last, name
t0, f0, l0, nm0 = part(0)
t1, f1, l1, nm1 = part(1)
check(str(t0).upper() == 'C12A7328-F81F-11D2-BA4B-00A0C93EC93B', f"partition 1 type EFI System ({nm0})")
check(f0 == 2048 and l0 == 2048 + 262144 - 1, f"partition 1 LBA {f0}..{l0} (128 MiB)")
check(str(t1).upper() == 'EBD0A0A2-B9E5-4433-87C0-68B6B72699C7', f"partition 2 type basic data ({nm1})")
check(f1 == l0 + 1 and l1 == total - 34, f"partition 2 LBA {f1}..{l1}")
for i in range(2, n):
    check(ents[i*128:(i+1)*128] == b'\0' * 128, "unused entries zero") if i == 2 else None
bh = img[(total-1)*S:(total)*S]
check(bh[0:8] == b'EFI PART', "backup GPT header at last sector")
bmy, balt = struct.unpack_from('<QQ', bh, 24)
check(bmy == total-1 and balt == 1, "backup header points back to primary")
b_ent = struct.unpack_from('<Q', bh, 72)[0]
check(b_ent == total - 33, f"backup entries at LBA {b_ent}")
bents = img[b_ent*S: b_ent*S + n*esz]
check(bents == ents, "backup entries identical to primary")
tmp = bytearray(bh[:hsz]); bcrc = struct.unpack_from('<I', bh, 16)[0]; tmp[16:20] = b'\0\0\0\0'
check(zlib.crc32(bytes(tmp)) == bcrc, "backup header CRC32")
check(img[f1*S:f1*S+S] == b'\0'*S, "system partition first sector cleared")
print("GPT ALL OK" if ok else "GPT FAILURES")
