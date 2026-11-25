// The Gateway's scenes' common part (SceneBuilds.hpp): the Frame World's GatewayClient, the player's words for what
// went wrong, and the addresses the Gateway and a room's connect give.

#include "client/SceneBuilds.hpp"

#include <charconv>
#include <cstdint>
#include <utility>

namespace object_fps_pvp::client {

std::optional<GYO::Engine::OS::SocketAddress> ParseMatchAddress(const std::string_view text) {
    const std::size_t colon = text.rfind(':');
    if (colon == std::string_view::npos) return std::nullopt;
    const std::string_view portText = text.substr(colon + 1);
    std::uint16_t port = 0;
    const auto parsed = std::from_chars(portText.data(), portText.data() + portText.size(), port);
    if (parsed.ec != std::errc{} || parsed.ptr != portText.data() + portText.size() || port == 0) return std::nullopt;
    auto address = GYO::Engine::OS::SocketAddress::Parse(text.substr(0, colon), port);
    if (!address) return std::nullopt;
    return *address;
}

std::string GatewayErrorText(const GatewayResult& result, const std::string& gateway) {
    const std::string& code = result.errorCode;
    if (code == "no_device") return "この端末の ID を用意できません" + (result.detail.empty() ? std::string() : "（" + result.detail + "）");
    if (code == "unreachable") return "Gateway（" + gateway + "）に接続できません";
    if (code == "timeout") return "Gateway（" + gateway + "）から応答がありません";
    if (code == "room_full") return "部屋が満員です";
    if (code == "room_started" || code == "room_starting") return "その部屋はもう始まっています";
    if (code == "room_not_found") return "部屋がありません";
    if (code == "not_member") return "この部屋のメンバーではありません";
    if (code == "match_ended") return "対戦は終わりました";
    if (code == "not_owner") return "開始できるのは部屋を作った人だけです";
    if (code == "not_all_ready") return "全員の準備がまだです";
    if (code == "no_match_available" || code == "allocation_failed") return "対戦を始められませんでした。もう一度開始してください";
    if (code == "unknown_map" || code == "unknown_mode") return "Gateway がこのマップかモードを知りません";
    if (code.empty()) return "Gateway が答えられませんでした（HTTP " + std::to_string(result.status) + "）" + (result.detail.empty() ? "" : ": " + result.detail);
    return "Gateway: " + code + (result.detail.empty() ? "" : "（" + result.detail + "）");
}

std::string ModeText(const std::string_view mode) {
    if (mode == "coop") return "協力";
    if (mode == "survival") return "生存";
    return std::string(mode);
}

std::string DeviceIdPath(const std::string& dataDirectory) {
    return dataDirectory.empty() ? std::string() : dataDirectory + "/device-id";
}

void PlaceGatewayClient(K::World& world, const GYO::Engine::OS::SocketAddress& gateway, const std::string& dataDirectory,
                        const bool needed) {
    const GatewayClient* now = world.Resource<GatewayClient>();
    if (now != nullptr && (!now->Offline() || !needed)) return;
    if (!needed) {
        // A scene that does not call the Gateway: no device id is read or made for it.
        world.SetResource(GatewayClient(gateway, std::string(), "the Gateway is not called in this scene"));
        return;
    }
    std::string error;
    std::string device = LoadDeviceId(DeviceIdPath(dataDirectory), error);
    world.SetResource(GatewayClient(gateway, std::move(device), std::move(error)));
}

} // namespace object_fps_pvp::client
