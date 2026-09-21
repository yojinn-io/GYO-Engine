#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "engine/io/IoError.hpp"

#include "engine/io/IoFwd.hpp"
#include "engine/io/fs/IFileSystem.hpp"
#include "engine/io/fs/DirectoryEntry.hpp"
#include "engine/io/fs/DirectoryIterator.hpp"
#include "engine/io/stream/FileOpenMode.hpp"
#include "engine/io/fs/MountTable.hpp"
#include "engine/io/fs/MountPoint.hpp"

namespace Engine::IO::FS {

using IoError = Engine::Base::Error<Engine::IO::IoErrorCode>;
template<class T>
using IoResult = Engine::Base::Result<T, IoError>;
using IoResultVoid = Engine::Base::Result<void, IoError>;

    /// VFS 本体
    /// - scheme（assets:// 等）で MountTable を検索
    /// - 読み取りは overlay（priority 降順で “見つかったら勝ち”）
    /// - 書き込みは最初に見つかった writable mount に集約
    class Vfs final {
    public:
        MountTable& Mounts() noexcept { return mounts_; }
        const MountTable& Mounts() const noexcept { return mounts_; }

        IoResultVoid Mount(MountPoint mp) { return mounts_.Mount(std::move(mp)); }
        bool Unmount(std::string_view name) { return mounts_.Unmount(name); }

        // ---- IFileSystem 互換の操作群（VFS ルーティング付き） ----

        IoResult<std::unique_ptr<Engine::IO::Stream::IStream>>
        Open(const Engine::IO::Path::Uri& uri, Engine::IO::Stream::FileOpenMode mode) {
            if (!Engine::IO::Stream::IsValid(mode)) {
                return IoResult<std::unique_ptr<Engine::IO::Stream::IStream>>::Err(
                    IoError::Make(Engine::IO::IoErrorCode::InvalidPath,
                                  "Vfs: invalid FileOpenMode"));
            }

            const bool writeReq = detail::WantsWrite(mode);
            const auto cands = mounts_.Candidates(uri);
            if (cands.empty()) {
                return IoResult<std::unique_ptr<Engine::IO::Stream::IStream>>::Err(
                    IoError::Make(Engine::IO::IoErrorCode::NotFound, "Vfs: no mount for scheme"));
            }

            if (writeReq) {
                // writable mount を優先順に探す
                for (const auto* mp : cands) {
                    if (!mp || !mp->fs) continue;
                    if (mp->readOnly) continue;

                    auto rr = mounts_.Resolve(*mp, uri);
                    if (!rr) continue;

                    auto openr = mp->fs->Open(rr.value().nativeUri, mode);
                    if (openr) return openr;

                    // NotFound でも “書き込み” は次候補へ（overlay write の想定）
                    // ただし PermissionDenied 等は即返す
                    if (!detail::IsNotFound(openr.error())) {
                        return IoResult<std::unique_ptr<Engine::IO::Stream::IStream>>::Err(openr.error());
                    }
                }
                return IoResult<std::unique_ptr<Engine::IO::Stream::IStream>>::Err(
                    IoError::Make(Engine::IO::IoErrorCode::PermissionDenied,
                                  "Vfs: no writable mount found"));
            } else {
                // 読みは overlay：上から open を試す
                IoError lastNotFound = IoError::Make(Engine::IO::IoErrorCode::NotFound, "Vfs: not found");
                for (const auto* mp : cands) {
                    if (!mp || !mp->fs) continue;

                    auto rr = mounts_.Resolve(*mp, uri);
                    if (!rr) continue;

                    auto openr = mp->fs->Open(rr.value().nativeUri, mode);
                    if (openr) return openr;

                    if (detail::IsNotFound(openr.error())) {
                        lastNotFound = openr.error();
                        continue;
                    }
                    return IoResult<std::unique_ptr<Engine::IO::Stream::IStream>>::Err(openr.error());
                }
                return IoResult<std::unique_ptr<Engine::IO::Stream::IStream>>::Err(lastNotFound);
            }
        }

        IoResult<bool> Exists(const Engine::IO::Path::Uri& uri) {
            const auto cands = mounts_.Candidates(uri);
            if (cands.empty()) return IoResult<bool>::Ok(false);

            for (const auto* mp : cands) {
                if (!mp || !mp->fs) continue;
                auto rr = mounts_.Resolve(*mp, uri);
                if (!rr) continue;

                auto er = mp->fs->Exists(rr.value().nativeUri);
                if (er) {
                    if (er.value()) return IoResult<bool>::Ok(true);
                    continue;
                }
                if (detail::IsNotFound(er.error())) continue;
                return IoResult<bool>::Err(er.error());
            }
            return IoResult<bool>::Ok(false);
        }

        IoResult<Engine::IO::FS::FileInfo> Stat(const Engine::IO::Path::Uri& uri) {
            const auto cands = mounts_.Candidates(uri);
            if (cands.empty()) {
                return IoResult<Engine::IO::FS::FileInfo>::Err(
                    IoError::Make(Engine::IO::IoErrorCode::NotFound, "Vfs: no mount for scheme"));
            }

            IoError lastNotFound = IoError::Make(Engine::IO::IoErrorCode::NotFound, "Vfs: not found");
            for (const auto* mp : cands) {
                if (!mp || !mp->fs) continue;

                auto rr = mounts_.Resolve(*mp, uri);
                if (!rr) continue;

                auto sr = mp->fs->Stat(rr.value().nativeUri);
                if (sr) return sr;

                if (detail::IsNotFound(sr.error())) {
                    lastNotFound = sr.error();
                    continue;
                }
                return IoResult<Engine::IO::FS::FileInfo>::Err(sr.error());
            }
            return IoResult<Engine::IO::FS::FileInfo>::Err(lastNotFound);
        }

        IoResultVoid CreateDirectories(const Engine::IO::Path::Uri& uri) {
            // 書き込み先 mount を選ぶ（preferWrite / priority 順）
            const auto cands = mounts_.Candidates(uri);
            if (cands.empty()) {
                return IoResultVoid::Err(IoError::Make(
                    Engine::IO::IoErrorCode::NotFound, "Vfs: no mount for scheme"));
            }

            for (const auto* mp : cands) {
                if (!mp || !mp->fs) continue;
                if (mp->readOnly) continue;

                auto rr = mounts_.Resolve(*mp, uri);
                if (!rr) continue;

                auto cr = mp->fs->CreateDirectories(rr.value().nativeUri);
                if (cr) return cr;

                if (detail::IsNotFound(cr.error())) continue;
                return IoResultVoid::Err(cr.error());
            }

            return IoResultVoid::Err(IoError::Make(
                Engine::IO::IoErrorCode::PermissionDenied, "Vfs: no writable mount found"));
        }

        IoResultVoid Remove(const Engine::IO::Path::Uri& uri, const Engine::IO::FS::RemoveOptions& opt = {}) {
            const auto cands = mounts_.Candidates(uri);
            if (cands.empty()) {
                return IoResultVoid::Err(IoError::Make(
                    Engine::IO::IoErrorCode::NotFound, "Vfs: no mount for scheme"));
            }

            // 削除は “見つかった場所” を消すのが自然。上から stat して見つかった mount で remove。
            IoError lastNotFound = IoError::Make(Engine::IO::IoErrorCode::NotFound, "Vfs: not found");
            for (const auto* mp : cands) {
                if (!mp || !mp->fs) continue;
                if (mp->readOnly) continue;

                auto rr = mounts_.Resolve(*mp, uri);
                if (!rr) continue;

                auto sr = mp->fs->Stat(rr.value().nativeUri);
                if (sr) {
                    return mp->fs->Remove(rr.value().nativeUri, opt);
                }
                if (detail::IsNotFound(sr.error())) {
                    lastNotFound = sr.error();
                    continue;
                }
                return IoResultVoid::Err(sr.error());
            }
            return IoResultVoid::Err(lastNotFound);
        }

        IoResultVoid Move(const Engine::IO::Path::Uri& from, const Engine::IO::Path::Uri& to) {
            // 原則：同じ mount（同じ下位 FS）内での Move を想定
            // まず “from が存在する mount” を決め、to も同じ mount に解決して実行する
            const auto cands = mounts_.Candidates(from);
            if (cands.empty()) {
                return IoResultVoid::Err(IoError::Make(
                    Engine::IO::IoErrorCode::NotFound, "Vfs: no mount for scheme"));
            }

            for (const auto* mp : cands) {
                if (!mp || !mp->fs) continue;
                if (mp->readOnly) continue;

                auto rfrom = mounts_.Resolve(*mp, from);
                auto rto   = mounts_.Resolve(*mp, to);
                if (!rfrom || !rto) continue;

                auto sr = mp->fs->Stat(rfrom.value().nativeUri);
                if (!sr) {
                    if (detail::IsNotFound(sr.error())) continue;
                    return IoResultVoid::Err(sr.error());
                }

                return mp->fs->Move(rfrom.value().nativeUri, rto.value().nativeUri);
            }

            return IoResultVoid::Err(IoError::Make(
                Engine::IO::IoErrorCode::NotFound, "Vfs: source not found or no writable mount"));
        }

        IoResultVoid Copy(const Engine::IO::Path::Uri& from, const Engine::IO::Path::Uri& to) {
            // Copy も Move と同様に “from の存在 mount” を決める
            const auto cands = mounts_.Candidates(from);
            if (cands.empty()) {
                return IoResultVoid::Err(IoError::Make(
                    Engine::IO::IoErrorCode::NotFound, "Vfs: no mount for scheme"));
            }

            for (const auto* mp : cands) {
                if (!mp || !mp->fs) continue;
                if (mp->readOnly) continue;

                auto rfrom = mounts_.Resolve(*mp, from);
                auto rto   = mounts_.Resolve(*mp, to);
                if (!rfrom || !rto) continue;

                auto sr = mp->fs->Stat(rfrom.value().nativeUri);
                if (!sr) {
                    if (detail::IsNotFound(sr.error())) continue;
                    return IoResultVoid::Err(sr.error());
                }

                return mp->fs->Copy(rfrom.value().nativeUri, rto.value().nativeUri);
            }

            return IoResultVoid::Err(IoError::Make(
                Engine::IO::IoErrorCode::NotFound, "Vfs: source not found or no writable mount"));
        }

        /// List：複数 mount を **マージ**（同名は priority 高い方が勝つ）
        IoResult<std::vector<Engine::IO::FS::DirectoryEntry>>
        List(const Engine::IO::Path::Uri& uri, const Engine::IO::FS::ListOptions& opt = {}) {
            const auto cands = mounts_.Candidates(uri);
            if (cands.empty()) {
                return IoResult<std::vector<Engine::IO::FS::DirectoryEntry>>::Err(
                    IoError::Make(Engine::IO::IoErrorCode::NotFound, "Vfs: no mount for scheme"));
            }

            std::vector<Engine::IO::FS::DirectoryEntry> out;
            std::unordered_set<std::string> seenNames;
            out.reserve(64);

            bool anyOk = false;
            IoError lastErr = IoError::Make(Engine::IO::IoErrorCode::NotFound, "Vfs: list failed");

            for (const auto* mp : cands) {
                if (!mp || !mp->fs) continue;

                auto rr = mounts_.Resolve(*mp, uri);
                if (!rr) continue;

                auto lr = mp->fs->List(rr.value().nativeUri, opt);
                if (!lr) {
                    lastErr = lr.error();
                    // NotFound は overlay 的に次を見る
                    if (detail::IsNotFound(lr.error())) continue;
                    // その他は即返す（壊れた mount を無視するとデバッグ地獄）
                    return IoResult<std::vector<Engine::IO::FS::DirectoryEntry>>::Err(lr.error());
                }

                anyOk = true;
                for (auto& e : lr.value()) {
                    // priority 高い mount のエントリを優先（先に入ったものを残す）
                    if (seenNames.insert(e.name).second) {
                        // 返す path は “VFS 側の path” に揃えるのが理想だが、
                        // ここでは下位FSの返す path をそのまま許容（必要なら後で変換）
                        out.push_back(std::move(e));
                    }
                }

                // overlay list の場合、上位で見つかったら終える設計もあり得るが
                // ここでは “マージ” をデフォルトにしている
            }

            if (!anyOk) {
                return IoResult<std::vector<Engine::IO::FS::DirectoryEntry>>::Err(lastErr);
            }
            return IoResult<std::vector<Engine::IO::FS::DirectoryEntry>>::Ok(std::move(out));
        }

        /// Iterate：簡易に List を VectorDirectoryIterator に変換（巨大ディレクトリでは platform 実装が欲しい）
        IoResult<std::unique_ptr<Engine::IO::FS::DirectoryIterator>>
        Iterate(const Engine::IO::Path::Uri& uri, const Engine::IO::FS::ListOptions& opt = {}) {
            auto lr = List(uri, opt);
            if (!lr) {
                return IoResult<std::unique_ptr<Engine::IO::FS::DirectoryIterator>>::Err(lr.error());
            }
            auto it = std::make_unique<Engine::IO::FS::VectorDirectoryIterator>(std::move(lr.value()), "VfsIterator");
            return IoResult<std::unique_ptr<Engine::IO::FS::DirectoryIterator>>::Ok(std::move(it));
        }

        IoResult<std::string> ToNativePathString(const Engine::IO::Path::Uri& uri) {
            // “存在する mount” のものを返す（read overlay）
            const auto cands = mounts_.Candidates(uri);
            if (cands.empty()) {
                return IoResult<std::string>::Err(IoError::Make(
                    Engine::IO::IoErrorCode::NotFound, "Vfs: no mount for scheme"));
            }

            IoError lastNotFound = IoError::Make(Engine::IO::IoErrorCode::NotFound, "Vfs: not found");
            for (const auto* mp : cands) {
                if (!mp || !mp->fs) continue;

                auto rr = mounts_.Resolve(*mp, uri);
                if (!rr) continue;

                auto pr = mp->fs->ToNativePathString(rr.value().nativeUri);
                if (pr) return pr;

                if (detail::IsNotFound(pr.error())) {
                    lastNotFound = pr.error();
                    continue;
                }
                return IoResult<std::string>::Err(pr.error());
            }
            return IoResult<std::string>::Err(lastNotFound);
        }

    private:
        MountTable mounts_;
    };

} // namespace Engine::IO::VFS
