#pragma once

#include "gyo/model_viewer/ModelViewerError.hpp"

#include "engine/io/helpers/UserFiles.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <vector>

// The viewer's file access through engine/io. Paths are UTF-8.
namespace Gyo::Tools::ModelViewer {

// The loader's own allocation limit (UfbxModelLoader.cpp) bounds what it can
// import; a larger file is refused before it is read whole.
inline constexpr std::size_t kMaxModelBytes = 256U * 1024U * 1024U;
// Events files and catalogs.
inline constexpr std::size_t kMaxTextBytes = 16U * 1024U * 1024U;

// ReadFailed when the file cannot be opened or read, or is larger than maxBytes.
[[nodiscard]] Result<std::vector<std::byte>, ModelViewerError> ReadAll(const std::string& path, std::size_t maxBytes);
// The file's stamp (exists == false for a missing file); ReadFailed otherwise.
[[nodiscard]] Result<Engine::IO::Helpers::FileStamp, ModelViewerError> Probe(const std::string& path);
// engine/io writes beside the target, then renames over it: a reader sees the
// old file or the new one. Missing folders above the target are created.
// WriteFailed when it cannot.
[[nodiscard]] Result<void, ModelViewerError> WriteReplacing(const std::string& path, std::span<const std::byte> bytes);

} // namespace Gyo::Tools::ModelViewer
