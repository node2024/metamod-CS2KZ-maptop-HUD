#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace maptop {
// CS_UM_CustomHudClicked in the pinned CS2 SDK: uint32 layout = 1, string button_id = 2.
// Inspect only the Studio background event; all other messages remain with their original handlers.
inline bool StudioBackgroundClick(int type, const void* buffer, uint32_t size) {
    if (type != 390 || !buffer || size == 0 || size > 256) return false;
    const auto* bytes = static_cast<const uint8_t*>(buffer);
    size_t at = 0;
    auto varint = [&]() -> std::optional<uint64_t> {
        uint64_t value = 0;
        for (int i = 0; i < 10; ++i) {
            if (at == size) return {};
            const uint8_t byte = bytes[at++];
            if (i == 9 && (byte & 0xfe)) return {};
            value |= uint64_t(byte & 0x7f) << (i * 7);
            if (!(byte & 0x80)) return value;
        }
        return {};
    };
    uint32_t layout = 0;
    std::string_view button;
    while (at < size) {
        const auto key = varint();
        if (!key || (*key >> 3) == 0 || (*key >> 3) > 0x1fffffff) return false;
        const auto field = *key >> 3;
        switch (*key & 7) {
        case 0: {
            const auto value = varint();
            if (!value) return false;
            if (field == 1) {
                if (*value > UINT32_MAX) return false;
                layout = static_cast<uint32_t>(*value);
            }
            break;
        }
        case 1: case 5: {
            const size_t length = (*key & 7) == 1 ? 8 : 4;
            if (length > size - at) return false;
            at += length;
            break;
        }
        case 2: {
            const auto length = varint();
            if (!length || *length > size - at) return false;
            if (field == 2) button = {reinterpret_cast<const char*>(bytes + at), static_cast<size_t>(*length)};
            at += static_cast<size_t>(*length);
            break;
        }
        default: return false;
        }
    }
    return layout != 0 && button == "cs_stage";
}
} // namespace maptop
