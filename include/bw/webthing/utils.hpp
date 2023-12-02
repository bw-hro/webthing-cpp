// Webthing-CPP
// SPDX-FileCopyrightText: 2023 Benno Waldhauer
// SPDX-License-Identifier: MIT

#pragma once

#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <optional>
#include <random>
#include <sstream>
#include <thread>
#include <time.h>

namespace bw::webthing 
{

namespace details
{
    struct global
    {
        static inline std::optional<std::string> fixed_time;
        static inline std::optional<std::string> fixed_uuid;
    };

    // Generate a ISO8601-formatted local time timestamp
    // and return as std::string
    inline std::string current_ISO8601_time_local(const std::optional<std::string>& fixed_time = std::nullopt)
    {
        if(fixed_time)
            return *fixed_time;

        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);

        // Get the milliseconds part of the current time
        auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;

        // Get the local time
        std::tm local_time;

        // Get the timezone offset
        #ifdef _WIN32
            localtime_s(&local_time, &now_time);
            long timezone_offset = 0;
            int err = _get_timezone(&timezone_offset);
            if(err != 0)
                timezone_offset = 0;
            timezone_offset = -timezone_offset;

            if(local_time.tm_isdst)
                timezone_offset += 3600;
        #else
            localtime_r(&now_time, &local_time);
            long timezone_offset = local_time.tm_gmtoff;
        #endif

        // Calculate the timezone offset manually
        int offset_hours = timezone_offset / 3600;
        int offset_minutes = (std::abs(timezone_offset) % 3600) / 60;

        // Format the time in ISO 8601 format, including milliseconds
        std::ostringstream oss;
        oss << std::put_time(&local_time, "%Y-%m-%dT%H:%M:%S")
            << "." << std::setw(3) << std::setfill('0') << milliseconds;
        if (timezone_offset >= 0)
            oss << "+";
        else
            oss << "-";
        oss << std::setw(2) << std::setfill('0') << std::abs(offset_hours)
            << ":" << std::setw(2) << std::setfill('0') << offset_minutes;

        return oss.str();
    }
} // bw::webthing::details

inline std::string timestamp()
{
    return details::current_ISO8601_time_local(details::global::fixed_time);
}

enum log_level
{
    error = 5000,
    warn  = 4000,
    info  = 3000,
    debug = 2000,
    trace = 1000
};

struct logger
{
    typedef std::function<void (log_level, const std::string&)> log_impl;

    static void error(const std::string& msg)
    {
        log(log_level::error, msg);
    }

    static void warn(const std::string& msg)
    {
        log(log_level::warn, msg);
    }

    static void info(const std::string& msg)
    {
        log(log_level::info, msg);
    }

    static void debug(const std::string& msg)
    {
        log(log_level::debug, msg);
    }

    static void trace(const std::string& msg)
    {
        log(log_level::trace, msg);
    }

    static void log(log_level level, const std::string& msg)
    {
        if(level < custom_log_level)
            return;

        if(custom_log_impl)
            return custom_log_impl(level, msg);

        default_log_impl(level, msg);
    }

    static void register_implementation(log_impl log_impl)
    {
        custom_log_impl = log_impl;
    }

    static void set_level(log_level level)
    {
        custom_log_level = level;
    }

private:
    static void default_log_impl(log_level level, const std::string& msg)
    {
        auto timestamp = details::current_ISO8601_time_local(std::nullopt);
        auto level_str = level == log_level::error ? "E" : 
                         level == log_level::warn  ? "W" : 
                         level == log_level::info  ? "I" : 
                         level == log_level::debug ? "D" : 
                         level == log_level::trace ? "T" : 
                         "L:" + std::to_string(level);

        std::stringstream ss;
        ss << timestamp << " [" << std::this_thread::get_id() << "] " << level_str << " - " << msg;
        
        std::lock_guard<std::mutex> lg(logger::log_mutex);
        switch (level)
        {
        case log_level::error:
        case log_level::warn:
            std::cerr << ss.str() << std::endl;
            break;
        default:
            std::cout << ss.str() << std::endl;
        }
    }

    static inline std::mutex log_mutex;
    static inline log_impl custom_log_impl;
    static inline log_level custom_log_level = log_level::debug;
};

// set a fixed time for timestamp generation
// this is useful for tests
// timestamp must follow ISO8601 format
// e.g. "2023-02-08T01:23:45"
inline void fix_time(std::string timestamp)
{
  details::global::fixed_time = timestamp;
  logger::warn("time fixed to " + bw::webthing::timestamp()); 
}

// unfix the time for timestamp generation
inline void unfix_time()
{
  details::global::fixed_time = std::nullopt;
  logger::warn("time unfixed");
}

// fix the time for the current scope. Not thread safe!!!
struct fix_time_scoped
{
  fix_time_scoped(std::string timestamp)
  {
    fix_time(timestamp);
  }

  ~fix_time_scoped()
  {
    unfix_time();
  }
};

#define FIXED_TIME_SCOPED(timestamp) auto fixed_time_scope_guard = fix_time_scoped(timestamp);

inline std::string generate_uuid()
{
    if(details::global::fixed_uuid)
        return *details::global::fixed_uuid;

    // https://stackoverflow.com/a/58467162
    // TODO: include real uuid generator lib?

    static std::random_device dev;
    static std::mt19937 rng(dev());

    std::uniform_int_distribution<int> dist(0, 15);

    const char *v = "0123456789abcdef";
    const bool dash[] = { 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0 };

    std::string uuid;
    for (int i = 0; i < 16; i++) {
        if (dash[i]) uuid += "-";
        uuid += v[dist(rng)];
        uuid += v[dist(rng)];
    }

    return uuid;
}

// set a fixed uuid for uuid generation
// this is useful for tests
inline void fix_uuid(std::string uuid)
{
  details::global::fixed_uuid = uuid;
  logger::warn("uuid generation fixed to " + bw::webthing::generate_uuid()); 
}

// unfix the time for timestamp generation
inline void unfix_uuid()
{
  details::global::fixed_uuid = std::nullopt;
  logger::warn("uuid generation unfixed");
}

// fix the uuid generation for the current scope. Not thread safe!!!
struct fix_uuid_scoped
{
  fix_uuid_scoped(std::string uuid)
  {
    fix_uuid(uuid);
  }

  ~fix_uuid_scoped()
  {
    unfix_uuid();
  }
};

#define FIXED_UUID_SCOPED(uuid) auto fixed_uuid_scope_guard = fix_uuid_scoped(uuid);

// try to static_cast from a type to another, throws std::bad_cast on error
template<class To, class From>
inline To try_static_cast(const From& value)
{
    try
    {
        if constexpr (std::is_convertible_v<From, To>)
            return static_cast<To>(value);
    }
    catch(...)
    {}

    throw std::bad_cast();
}

} // bw::webthing