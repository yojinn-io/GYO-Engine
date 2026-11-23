// DatagramAddress parsing and text: no OS call, so it lives in src/common.
#include "engine/os/DatagramSocket.hpp"

#include <string>

namespace GYO::Engine::OS {

Base::Result<DatagramAddress, OSError> DatagramAddress::Parse(const std::string_view text, const std::uint16_t port) {
    const auto invalid = [&] {
        return Base::Err(OSError::Make(OSErrorCode::InvalidAddress, "not a dotted IPv4 address",
            "text_bytes=" + std::to_string(text.size())));
    };
    DatagramAddress address{{}, port};
    std::size_t at = 0;
    for (std::size_t part = 0; part < 4; ++part) {
        if (part > 0) {
            if (at >= text.size() || text[at] != '.') return invalid();
            ++at;
        }
        const std::size_t begin = at;
        unsigned value = 0;
        while (at < text.size() && text[at] >= '0' && text[at] <= '9' && at - begin < 3) {
            value = value * 10 + static_cast<unsigned>(text[at] - '0');
            ++at;
        }
        const std::size_t digits = at - begin;
        if (digits == 0 || value > 255 || (digits > 1 && text[begin] == '0')) return invalid();
        address.octets[part] = static_cast<std::uint8_t>(value);
    }
    if (at != text.size()) return invalid();
    return address;
}

std::string DatagramAddress::ToString() const {
    return std::to_string(octets[0]) + "." + std::to_string(octets[1]) + "." + std::to_string(octets[2]) + "." +
           std::to_string(octets[3]) + ":" + std::to_string(port);
}

} // namespace GYO::Engine::OS
