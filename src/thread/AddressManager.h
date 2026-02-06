#pragma once

#include "core/Types.h"
#include <array>
#include <cstdint>

namespace mt {

class AddressManager {
public:
    // ML-EID: Mesh-Local EID derived from extended address
    // Uses fd00::/64 prefix + IID from EUI-64
    static std::array<uint8_t, 16> computeMLEID(uint64_t ext_addr);

    // Link-local address from extended address
    static std::array<uint8_t, 16> computeLinkLocal(uint64_t ext_addr);

    // Convert EUI-64 to IID (modified EUI-64 per RFC 4291)
    static uint64_t eui64ToIID(uint64_t eui64);
};

} // namespace mt
