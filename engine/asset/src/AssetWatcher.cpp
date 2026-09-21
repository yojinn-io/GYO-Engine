#include "engine/asset/hot_reload/AssetWatcher.hpp"

#include <chrono>
#include <filesystem>
#include <system_error>

namespace Engine::Asset::HotReload {

namespace fs = std::filesystem;

static std::uint64_t MsToNs(std::uint64_t ms) noexcept { return ms * 1'000'000ull; }

// file_time_type -> system_clock ns（C++17 best-effort）
static std::uint64_t FileTimeToSystemNs(const fs::file_time_type& ft) {
    // file_clock と system_clock の差分で変換する（よくある手法）
    using namespace std::chrono;
    const auto nowFile = fs::file_time_type::clock::now();
    const auto nowSys  = system_clock::now();

    const auto sysTp = time_point_cast<system_clock::duration>(ft - nowFile + nowSys);
    return static_cast<std::uint64_t>(
        duration_cast<nanoseconds>(sysTp.time_since_epoch()).count()
    );
}

AssetWatcher::AssetWatcher(Options opt) : opt_(opt) {}

void AssetWatcher::SetOptions(Options opt) { opt_ = opt; }
const AssetWatcher::Options& AssetWatcher::GetOptions() const noexcept { return opt_; }

void AssetWatcher::Watch(const AssetId& id, std::string resolvedPath) {
    auto& w = watched_[id];
    w.resolvedPath = std::move(resolvedPath);

    // 初回登録時点の状態をスナップショット
    bool exists = false;
    std::uint64_t writeNs = 0;
    if (ProbeFile(w.resolvedPath, exists, writeNs)) {
        w.existed = exists;
        w.lastWriteTimeNs = exists ? writeNs : 0;
    } else {
        // エラーは黙殺（次回 Poll で再試行）
        w.existed = false;
        w.lastWriteTimeNs = 0;
    }
}

void AssetWatcher::Unwatch(const AssetId& id) {
    watched_.erase(id);
}

void AssetWatcher::Clear() {
    watched_.clear();
}

bool AssetWatcher::IsWatching(const AssetId& id) const noexcept {
    return watched_.find(id) != watched_.end();
}

const AssetWatcher::WatchedInfo* AssetWatcher::FindWatched(const AssetId& id) const noexcept {
    auto it = watched_.find(id);
    return (it == watched_.end()) ? nullptr : &it->second;
}

std::vector<AssetChange> AssetWatcher::Poll() {
    std::vector<AssetChange> out;
    if (watched_.empty()) return out;

    const std::uint64_t nowNs = NowNs();
    const std::uint64_t debounceNs = MsToNs(opt_.debounceMs);

    for (auto it = watched_.begin(); it != watched_.end(); ) {
        const AssetId id = it->first;
        WatchedInfo& w = it->second;

        bool exists = false;
        std::uint64_t writeNs = 0;

        const bool probedOk = ProbeFile(w.resolvedPath, exists, writeNs);
        if (!probedOk) {
            // probe 失敗は「何もしない」：OSエラーや一時的ロックを想定
            ++it;
            continue;
        }

        // --- removed ---
        if (w.existed && !exists) {
            if (opt_.emitRemoved) {
                AssetChange c;
                c.id = id;
                c.kind = AssetChangeKind::Removed;
                c.resolvedPath = w.resolvedPath;
                c.writeTimeNs = 0;
                c.detectedNs = nowNs;
                c.seq = ++seq_;
                out.push_back(std::move(c));
            }
            w.existed = false;
            w.lastWriteTimeNs = 0;
            w.lastEventNs = nowNs;

            // keepWatchingMissing=false なら削除
            if (!opt_.keepWatchingMissing) {
                it = watched_.erase(it);
                continue;
            }

            ++it;
            continue;
        }

        // --- added ---
        if (!w.existed && exists) {
            if (opt_.emitAdded) {
                AssetChange c;
                c.id = id;
                c.kind = AssetChangeKind::Added;
                c.resolvedPath = w.resolvedPath;
                c.writeTimeNs = writeNs;
                c.detectedNs = nowNs;
                c.seq = ++seq_;
                out.push_back(std::move(c));
            }
            w.existed = true;
            w.lastWriteTimeNs = writeNs;
            w.lastEventNs = nowNs;
            ++it;
            continue;
        }

        // --- modified ---
        if (w.existed && exists) {
            const bool changed = (writeNs != 0 && writeNs != w.lastWriteTimeNs);

            if (changed) {
                const bool passDebounce =
                    (debounceNs == 0) || (nowNs >= w.lastEventNs + debounceNs);

                if (passDebounce && opt_.emitModified) {
                    AssetChange c;
                    c.id = id;
                    c.kind = AssetChangeKind::Modified;
                    c.resolvedPath = w.resolvedPath;
                    c.writeTimeNs = writeNs;
                    c.detectedNs = nowNs;
                    c.seq = ++seq_;
                    out.push_back(std::move(c));
                    w.lastEventNs = nowNs;
                }

                // debounce で抑制しても “最新 writeTime” は追従させる
                w.lastWriteTimeNs = writeNs;
            }
        }

        ++it;
    }

    return out;
}

std::uint64_t AssetWatcher::NowNs() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    return static_cast<std::uint64_t>(duration_cast<nanoseconds>(now.time_since_epoch()).count());
}

bool AssetWatcher::ProbeFile(const std::string& path, bool& existsOut, std::uint64_t& writeNsOut) {
    std::error_code ec;

    existsOut = fs::exists(path, ec);
    if (ec) return false;

    if (!existsOut) {
        writeNsOut = 0;
        return true;
    }

    auto ft = fs::last_write_time(path, ec);
    if (ec) return false;

    writeNsOut = FileTimeToSystemNs(ft);
    return true;
}

} // namespace Engine::Asset::HotReload
