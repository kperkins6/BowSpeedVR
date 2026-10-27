#pragma once
#include <cstdint>
namespace BowSpeed {
inline bool SupportedSKSEVersion(bool editor,std::uint32_t packed) {
    // SKSEVR's packed Steam runtime carries platform flag 1 in the low nibble.
    return !editor && (packed==0x010400F0 || packed==0x010400F1);
}
}
