#include "thread/AddressManager.h"
#include <cstring>

namespace mt {

uint64_t AddressManager::eui64ToIID(uint64_t eui64) {
    // Modified EUI-64: flip the universal/local bit (bit 1 of first byte)
    return eui64 ^ (static_cast<uint64_t>(0x02) << 56);
}

std::array<uint8_t, 16> AddressManager::computeMLEID(uint64_t ext_addr) {
    std::array<uint8_t, 16> addr{};
    // fd00::/64 prefix (mesh-local)
    addr[0] = 0xfd;
    addr[1] = 0x00;
    // Bytes 2-7: zero

    // IID from modified EUI-64
    uint64_t iid = eui64ToIID(ext_addr);
    for (int i = 0; i < 8; ++i) {
        addr[8 + i] = static_cast<uint8_t>((iid >> (56 - 8 * i)) & 0xFF);
    }
    return addr;
}

std::array<uint8_t, 16> AddressManager::computeLinkLocal(uint64_t ext_addr) {
    std::array<uint8_t, 16> addr{};
    // fe80::/64 prefix
    addr[0] = 0xfe;
    addr[1] = 0x80;
    // Bytes 2-7: zero

    uint64_t iid = eui64ToIID(ext_addr);
    for (int i = 0; i < 8; ++i) {
        addr[8 + i] = static_cast<uint8_t>((iid >> (56 - 8 * i)) & 0xFF);
    }
    return addr;
}

} // namespace mt
