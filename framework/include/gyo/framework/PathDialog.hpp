#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace Gyo::Tools::Framework {

enum class PathDialogKind {
    OpenFile,
    SaveFile,
    OpenFolder,
};

// A file type the dialog offers, as SDL takes it: pattern is extensions
// separated by ';' ("json", "wav;wave") or "*".
struct PathDialogFilter final {
    std::string name;
    std::string pattern;
};

// What to ask the operating system for. The tag is the caller's: the answer
// carries it back, so the caller knows what the path is for (Design Invariant 3).
struct PathDialogRequest final {
    std::uint32_t tag{};
    PathDialogKind kind{};
    std::vector<PathDialogFilter> filters; // unused for OpenFolder
    std::string defaultLocation;           // UTF-8, may be empty
};

enum class PathDialogOutcome {
    Chosen,
    Cancelled,
    Failed, // the platform could not show a dialog; the user types the path
};

struct PathDialogAnswer final {
    std::uint32_t tag{};
    PathDialogOutcome outcome{};
    std::string path;  // UTF-8, set when Chosen
    std::string error; // set when Failed
};

// Answers delivered from any thread, taken by the tool between frames.
class PathDialogInbox final {
public:
    void Post(PathDialogAnswer answer);
    [[nodiscard]] std::vector<PathDialogAnswer> Take();

private:
    std::mutex mutex_;
    std::vector<PathDialogAnswer> answers_;
};

} // namespace Gyo::Tools::Framework
