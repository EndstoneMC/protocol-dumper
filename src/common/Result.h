#pragma once

#include <nonstd/expected.hpp>

namespace Bedrock {
class LogLevel {
    enum class Type : uint8_t;
};
enum LogAreaID : unsigned int;

struct CallStack {
    struct Frame {
        uint64_t mFilenameHash;
        std::string_view mFilename;
        uint32_t mLine;
    };
    struct Context {
        std::string mValue;
        std::optional<LogLevel> mLogLevel;
        std::optional<LogAreaID> mLogArea;
    };
    struct FrameWithContext {
        Frame mFrame;
        std::optional<Context> mContext;
    };
    std::vector<FrameWithContext> mFrames;
};

template <typename ErrorType>
struct ErrorInfo {
    std::string message() const;
    ErrorType mError;
    Bedrock::CallStack mCallStack;
    std::vector<ErrorInfo<ErrorType>> mBranches;
};

template <typename T = void, typename ErrorType = std::error_code>
class Result : nonstd::expected_lite::expected<T, Bedrock::ErrorInfo<ErrorType>> {};

}  // namespace Bedrock
