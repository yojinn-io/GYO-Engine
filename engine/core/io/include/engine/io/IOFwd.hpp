#pragma once

namespace GYO::Engine::IO {

    namespace Path {
        class Path;
        struct Uri;
    } // namespace Path

    namespace Stream {
        class IStream;
    } // namespace Stream

    namespace FS {
        struct FileInfo;
        struct DirectoryEntry;

        class IFileSystem;
        class IFileWatcher;   // optional
        class MountTable;     // optional
        class Vfs;            // optional
    } // namespace FS

} // namespace GYO::Engine::IO
