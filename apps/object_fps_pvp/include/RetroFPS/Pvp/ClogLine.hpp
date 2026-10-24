#pragma once

#include <ios>
#include <iostream>
#include <mutex>
#include <sstream>
#include <streambuf>
#include <string>

namespace fps::pvp {

namespace Detail {
inline std::mutex clogLineMutex;
} // namespace Detail

// Event-log text for std::clog, formatted into its own buffer and handed to
// the stream buffer std::clog had at construction in one sputn when the
// ClogLine is destroyed, under one product-wide lock, so concurrent writers
// never interleave. It replaces std::osyncstream, which Apple libc++ offers
// only with -fexperimental-library (v7.1 C16): the same single transfer, no
// flush requested, and default formatting (not std::clog's flags). Writers
// that bypass ClogLine are not ordered against it.
class ClogLine final {
public:
    ClogLine() = default;
    ClogLine(const ClogLine&) = delete;
    ClogLine& operator=(const ClogLine&) = delete;
    ~ClogLine() {
        try {
            const auto text = buffer_.str();
            std::scoped_lock lock(Detail::clogLineMutex);
            if (target_) target_->sputn(text.data(), static_cast<std::streamsize>(text.size()));
        } catch (...) {
            // A log line is never worth a failure (std::osyncstream's emit
            // does not throw either).
        }
    }

    template <typename T>
    ClogLine& operator<<(const T& value) {
        buffer_ << value;
        return *this;
    }
    // Manipulators such as std::fixed.
    ClogLine& operator<<(std::ios_base& (*manipulator)(std::ios_base&)) {
        buffer_ << manipulator;
        return *this;
    }

private:
    std::streambuf* target_ = std::clog.rdbuf();
    std::ostringstream buffer_;
};

} // namespace fps::pvp
