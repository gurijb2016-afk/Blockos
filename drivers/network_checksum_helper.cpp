// safe checksum over bytes to avoid unaligned 16-bit accesses on packed structs
static uint16_t checksum_bytes(const uint8_t* data, size_t len) {
    uint32_t sum = 0;
    size_t i;
    for (i = 0; i + 1 < len; i += 2) {
        sum += ((uint16_t)data[i] << 8) | (uint16_t)data[i+1];
    }
    if (i < len) {
        sum += ((uint16_t)data[i] << 8);
    }
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}
