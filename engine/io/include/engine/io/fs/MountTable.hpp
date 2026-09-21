#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "engine/io/IoError.hpp"

#include "engine/io/IoFwd.hpp" // Path::Uri
#include "engine/io/fs/IFileSystem.hpp"
#include "engine/io/stream/FileOpenMode.hpp"
#include "engine/io/fs/MountPoint.hpp"

namespace Engine::IO::FS {

using IoError = Engine::Base::Error<Engine::IO::IoErrorCode>;
template<class T>
using IoResult = Engine::Base::Result<T, IoError>;
using IoResultVoid = Engine::Base::Result<void, IoError>;

namespace detail {

    // ---- Uri adapter（ここだけ合わせればOK）----
    inline std::string UriText(const Engine::IO::Path::Uri& u) {
        return u.ToString();
    }
    inline Engine::IO::Path::Uri UriParse(std::string_view s) {
        return Engine::IO::Path::ParseUriLoose(s);
    }

    // "scheme://rest" の scheme を抽出（見つからなければ空）
    inline std::string_view ExtractScheme(std::string_view uri) {
        const auto pos = uri.find("://");
        if (pos == std::string_view::npos) return {};
        return uri.substr(0, pos);
    }

    // "scheme://rest" の rest（先頭スラッシュは除去）を抽出
    inline std::string_view ExtractAfterScheme(std::string_view uri) {
        const auto pos = uri.find("://");
        if (pos == std::string_view::npos) {
            return std::move(uri);
        }
        std::string_view rest = uri.substr(pos + 3);
        while (!rest.empty() && (rest.front() == '/' || rest.front() == '\\')) {
            rest.remove_prefix(1);
        }
        return std::move(rest);
    }

    // rootUri + "/" + rel を string で合成（スキーム等は root 側に従う）
    inline Engine::IO::Path::Uri JoinRootAndRel(const Engine::IO::Path::Uri& root, std::string_view rel) {
        std::string r = UriText(root);
        if (!r.empty() && r.back() != '/') r.push_back('/');
        // rel 先頭スラッシュを除去
        while (!rel.empty() && (rel.front() == '/' || rel.front() == '\\')) rel.remove_prefix(1);
        r.append(rel.data(), rel.size());
        return UriParse(r);
    }

    inline bool IsNotFound(const IoError& e) noexcept {
        return e.code == Engine::IO::IoErrorCode::NotFound;
    }

    inline bool WantsWrite(Engine::IO::Stream::FileOpenMode m) noexcept {
        using Engine::IO::Stream::FileOpenMode;
        // Write/Append/Truncate/Create のどれかが立っていれば “書き込み系” とみなす
        return Engine::IO::Stream::Has(m, FileOpenMode::Write)
            || Engine::IO::Stream::Has(m, FileOpenMode::Append)
            || Engine::IO::Stream::Has(m, FileOpenMode::Truncate)
            || Engine::IO::Stream::Has(m, FileOpenMode::CreateIfMissing);
    }

    } // namespace detail

    /// Mount の解決結果
    struct ResolvedMount final {
        const MountPoint* mp = nullptr;
        Engine::IO::Path::Uri nativeUri; // 下位 FS に渡す URI（root + rel）
    };

    /// mount 管理
    /// - 追加/削除
    /// - scheme で候補を取り出し、priority 降順で並べる
    class MountTable final {
    public:
        IoResultVoid Mount(MountPoint mp) {
            if (!mp.fs) {
                return IoResultVoid::Err(IoError::Make(
                    Engine::IO::IoErrorCode::InvalidPath,
                    "MountTable: fs is null"));
            }
            if (mp.name.empty()) {
                return IoResultVoid::Err(IoError::Make(
                    Engine::IO::IoErrorCode::InvalidPath,
                    "MountTable: mount name is empty"));
            }
            mounts_.push_back(std::move(mp));
            SortByPriority();
            return IoResultVoid::Ok();
        }

        bool Unmount(std::string_view name) {
            const auto it = std::remove_if(mounts_.begin(), mounts_.end(),
                [&](const MountPoint& m) { return m.name == name; });
            const bool removed = (it != mounts_.end());
            mounts_.erase(it, mounts_.end());
            return removed;
        }

        void Clear() { mounts_.clear(); }

        const std::vector<MountPoint>& All() const noexcept { return mounts_; }

        /// scheme に一致する mount を priority 降順で返す
        std::vector<const MountPoint*> Candidates(const Engine::IO::Path::Uri& vfsUri) const {
            const std::string u = detail::UriText(vfsUri);
            const std::string_view scheme = detail::ExtractScheme(u);

            std::vector<const MountPoint*> out;
            out.reserve(mounts_.size());

            for (const auto& m : mounts_) {
                const std::string mu = detail::UriText(m.mountUri);
                const std::string_view ms = detail::ExtractScheme(mu);
                if (!scheme.empty() && ms == scheme) {
                    out.push_back(&m);
                }
            }
            // mounts_ 自体が priority 降順なので out もその順
            return out;
        }

        /// 読み取り向け解決：存在する（または open/stat できる） mount を上から探すのは Vfs 側で行う
        IoResult<ResolvedMount> Resolve(const MountPoint& mp, const Engine::IO::Path::Uri& vfsUri) const {
            const std::string u = detail::UriText(vfsUri);
            const std::string_view rel = detail::ExtractAfterScheme(u);
            ResolvedMount r;
            r.mp = &mp;
            r.nativeUri = detail::JoinRootAndRel(mp.rootUri, rel);
            return IoResult<ResolvedMount>::Ok(std::move(r));
        }

    private:
        void SortByPriority() {
            std::stable_sort(mounts_.begin(), mounts_.end(),
                [](const MountPoint& a, const MountPoint& b) {
                    if (a.priority != b.priority) return a.priority > b.priority; // 高いほど先
                    // 同 priority なら preferWrite を先に
                    if (a.preferWrite != b.preferWrite) return a.preferWrite && !b.preferWrite;
                    return a.name < b.name;
                });
        }

    private:
        std::vector<MountPoint> mounts_;
    };

} // namespace Engine::IO::VFS
