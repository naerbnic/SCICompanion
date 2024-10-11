/***************************************************************************
    Copyright (c) 2024 Brian Chin

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
***************************************************************************/
#pragma once

#include <string_view>
#include <absl/strings/str_format.h>
// A logging class that can be used to log messages to a file. This can also be used to
// report UI messages, although that is not a depencency we provide.
class Logger
{
public:
    enum class Level : uint8_t
    {
        Verbose,
        Debug,
        Info,
        Warning,
        Error,
    };

    // The audience of the message. This can be used to filter messages based on the intended
    // audience, to be presented to the user, or stored for the developer.
    enum class Audience : uint8_t
    {
        User,
        Developer,
    };

    class LogMessage
    {
    public:
        LogMessage(Level level, Audience audience, std::string message)
            : level_(level), audience_(audience), message_(std::move(message))
        {
        }

        Level GetLevel() const { return level_; }
        Audience GetAudience() const { return audience_; }
        const std::string& GetMessage() const { return message_; }

    private:
        Level level_;
        Audience audience_;
        std::string message_;
    };

    class Handler
    {
    public:
        virtual ~Handler() = default;

        virtual void Log(const LogMessage& message) = 0;
    };

    // During the RAII scope of this object, the handler will be used to log messages,
    // unless another scope is set within it.
    class HandlerScope
    {
    public:
        explicit HandlerScope(Handler* handler);
        ~HandlerScope();
    private:
        Handler* old_handler_;
    };

    template <typename... Args>
    static void Log(Level level, Audience audience, const absl::FormatSpec<Args...>& format, Args&&... args);

    template <typename... Args>
    static void UserInfo(const absl::FormatSpec<Args...>& format, Args&&... args);

    template <typename... Args>
    static void UserWarning(const absl::FormatSpec<Args...>& format, Args&&... args);

    template <typename... Args>
    static void UserError(const absl::FormatSpec<Args...>& format, Args&&... args);

    template <typename... Args>
    static void DevInfo(const absl::FormatSpec<Args...>& format, Args&&... args);

private:
    static void LogImpl(const LogMessage& message);
};

// Implementation


template <typename... Args>
void Logger::Log(Level level, Audience audience, const absl::FormatSpec<Args...>& format, Args&&... args)
{
    LogImpl(LogMessage(level, audience, absl::StrFormat(format, std::forward<Args>(args)...)));
}

template <typename... Args>
void Logger::UserInfo(const absl::FormatSpec<Args...>& format, Args&&... args)
{
    Log(Level::Info, Audience::User, format, std::forward<Args>(args)...);
}

template <typename... Args>
void Logger::UserWarning(const absl::FormatSpec<Args...>& format, Args&&... args)
{
    Log(Level::Warning, Audience::User, format, std::forward<Args>(args)...);
}

template <typename... Args>
void Logger::UserError(const absl::FormatSpec<Args...>& format, Args&&... args)
{
    Log(Level::Error, Audience::User, format, std::forward<Args>(args)...);
}

template <typename... Args>
void Logger::DevInfo(const absl::FormatSpec<Args...>& format, Args&&... args)
{
    Log(Level::Error, Audience::Developer, format, std::forward<Args>(args)...);
}

