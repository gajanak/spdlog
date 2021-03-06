//
// Copyright(c) 2015 Gabi Melman.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)
//

#pragma once


// create logger with given name, sinks and the default pattern formatter
// all other ctors will call this one
template<typename It>
inline spdlog::logger::logger(std::string logger_name, const It &begin, const It &end)
    : name_(std::move(logger_name))
    , sinks_(begin, end)
    , level_(level::info)
    , flush_level_(level::off)
    , last_err_time_(0)
    , msg_counter_(1) // message counter will start from 1. 0-message id will be
                      // reserved for controll messages
{
    err_handler_ = [this](const std::string &msg) { this->default_err_handler_(msg); };
}

// ctor with sinks as init list
inline spdlog::logger::logger(std::string logger_name, sinks_init_list sinks_list)
    : logger(std::move(logger_name), sinks_list.begin(), sinks_list.end())
{
}

// ctor with single sink
inline spdlog::logger::logger(std::string logger_name, spdlog::sink_ptr single_sink)
    : logger(std::move(logger_name), {std::move(single_sink)})
{
}

inline spdlog::logger::~logger() = default;

inline void spdlog::logger::set_formatter(std::unique_ptr<spdlog::formatter> f)
{
    for (auto &sink : sinks_)
    {
        sink->set_formatter(f->clone());
    }
}

inline void spdlog::logger::set_pattern(std::string pattern, pattern_time_type time_type)
{
    set_formatter(std::unique_ptr<spdlog::formatter>(new pattern_formatter(std::move(pattern), time_type)));
}

template<typename... Args>
inline void spdlog::logger::log(level::level_enum lvl, const char *fmt, const Args &... args)
{
    if (!should_log(lvl))
    {
        return;
    }

    try
    {
        details::log_msg log_msg(&name_, lvl);
        fmt::format_to(log_msg.raw, fmt, args...);
        sink_it_(log_msg);
    }
    SPDLOG_CATCH_AND_HANDLE
}

template<typename... Args>
inline void spdlog::logger::log(level::level_enum lvl, const char *msg)
{
    if (!should_log(lvl))
    {
        return;
    }
    try
    {
        details::log_msg log_msg(&name_, lvl);
        fmt::format_to(log_msg.raw, "{}", msg);
        sink_it_(log_msg);
    }
    SPDLOG_CATCH_AND_HANDLE
}

template<typename T>
inline void spdlog::logger::log(level::level_enum lvl, const T &msg)
{
    if (!should_log(lvl))
    {
        return;
    }
    try
    {
        details::log_msg log_msg(&name_, lvl);
        fmt::format_to(log_msg.raw, "{}", msg);
        sink_it_(log_msg);
    }
    SPDLOG_CATCH_AND_HANDLE
}

template<typename... Args>
inline void spdlog::logger::trace(const char *fmt, const Args &... args)
{
    log(level::trace, fmt, args...);
}

template<typename... Args>
inline void spdlog::logger::debug(const char *fmt, const Args &... args)
{
    log(level::debug, fmt, args...);
}

template<typename... Args>
inline void spdlog::logger::info(const char *fmt, const Args &... args)
{
    log(level::info, fmt, args...);
}

template<typename... Args>
inline void spdlog::logger::warn(const char *fmt, const Args &... args)
{
    log(level::warn, fmt, args...);
}

template<typename... Args>
inline void spdlog::logger::error(const char *fmt, const Args &... args)
{
    log(level::err, fmt, args...);
}

template<typename... Args>
inline void spdlog::logger::critical(const char *fmt, const Args &... args)
{
    log(level::critical, fmt, args...);
}

template<typename T>
inline void spdlog::logger::trace(const T &msg)
{
    log(level::trace, msg);
}

template<typename T>
inline void spdlog::logger::debug(const T &msg)
{
    log(level::debug, msg);
}

template<typename T>
inline void spdlog::logger::info(const T &msg)
{
    log(level::info, msg);
}

template<typename T>
inline void spdlog::logger::warn(const T &msg)
{
    log(level::warn, msg);
}

template<typename T>
inline void spdlog::logger::error(const T &msg)
{
    log(level::err, msg);
}

template<typename T>
inline void spdlog::logger::critical(const T &msg)
{
    log(level::critical, msg);
}

#ifdef SPDLOG_WCHAR_TO_UTF8_SUPPORT
template<typename... Args>
inline void spdlog::logger::log(level::level_enum lvl, const wchar_t *fmt, const Args &... args)
{
    if (!should_log(lvl))
    {
        return;
    }

    decltype(wstring_converter_)::byte_string utf8_string;

    try
    {
        {
            lock_guard<mutex> lock(wstring_converter_mutex_);
            utf8_string = wstring_converter_.to_bytes(fmt);
        }
        log(lvl, utf8_string.c_str(), args...);
    }
    SPDLOG_CATCH_AND_HANDLE
}

template<typename... Args>
inline void spdlog::logger::trace(const wchar_t *fmt, const Args &... args)
{
    log(level::trace, fmt, args...);
}

template<typename... Args>
inline void spdlog::logger::debug(const wchar_t *fmt, const Args &... args)
{
    log(level::debug, fmt, args...);
}

template<typename... Args>
inline void spdlog::logger::info(const wchar_t *fmt, const Args &... args)
{
    log(level::info, fmt, args...);
}

template<typename... Args>
inline void spdlog::logger::warn(const wchar_t *fmt, const Args &... args)
{
    log(level::warn, fmt, args...);
}

template<typename... Args>
inline void spdlog::logger::error(const wchar_t *fmt, const Args &... args)
{
    log(level::err, fmt, args...);
}

template<typename... Args>
inline void spdlog::logger::critical(const wchar_t *fmt, const Args &... args)
{
    log(level::critical, fmt, args...);
}

#endif // SPDLOG_WCHAR_TO_UTF8_SUPPORT

//
// name and level
//
inline const std::string &spdlog::logger::name() const
{
    return name_;
}

inline void spdlog::logger::set_level(spdlog::level::level_enum log_level)
{
    level_.store(log_level);
}

inline void spdlog::logger::set_error_handler(spdlog::log_err_handler err_handler)
{
    err_handler_ = std::move(err_handler);
}

inline spdlog::log_err_handler spdlog::logger::error_handler()
{
    return err_handler_;
}

inline void spdlog::logger::flush()
{
    try
    {
        flush_();
    }
    SPDLOG_CATCH_AND_HANDLE
}

inline void spdlog::logger::flush_on(level::level_enum log_level)
{
    flush_level_.store(log_level);
}

inline bool spdlog::logger::should_flush_(const details::log_msg &msg)
{
    auto flush_level = flush_level_.load(std::memory_order_relaxed);
    return (msg.level >= flush_level) && (msg.level != level::off);
}

inline spdlog::level::level_enum spdlog::logger::level() const
{
    return static_cast<spdlog::level::level_enum>(level_.load(std::memory_order_relaxed));
}

inline bool spdlog::logger::should_log(spdlog::level::level_enum msg_level) const
{
    return msg_level >= level_.load(std::memory_order_relaxed);
}

//
// protected virtual called at end of each user log call (if enabled) by the
// line_logger
//
inline void spdlog::logger::sink_it_(details::log_msg &msg)
{
#if defined(SPDLOG_ENABLE_MESSAGE_COUNTER)
    incr_msg_counter_(msg);
#endif
    for (auto &sink : sinks_)
    {
        if (sink->should_log(msg.level))
        {
            sink->log(msg);
        }
    }

    if (should_flush_(msg))
    {
        flush();
    }
}

inline void spdlog::logger::flush_()
{
    for (auto &sink : sinks_)
    {
        sink->flush();
    }
}

inline void spdlog::logger::default_err_handler_(const std::string &msg)
{
    auto now = time(nullptr);
    if (now - last_err_time_ < 60)
    {
        return;
    }
    last_err_time_ = now;
    auto tm_time = details::os::localtime(now);
    char date_buf[100];
    std::strftime(date_buf, sizeof(date_buf), "%Y-%m-%d %H:%M:%S", &tm_time);
    fmt::print(stderr, "[*** LOG ERROR ***] [{}] [{}] {}\n", date_buf, name(), msg);
}

inline void spdlog::logger::incr_msg_counter_(details::log_msg &msg)
{
    msg.msg_id = msg_counter_.fetch_add(1, std::memory_order_relaxed);
}

inline const std::vector<spdlog::sink_ptr> &spdlog::logger::sinks() const
{
    return sinks_;
}

inline std::vector<spdlog::sink_ptr> &spdlog::logger::sinks()
{
    return sinks_;
}

#ifdef SPDLOG_ENABLE_LOG_ATTRIBUTES

template<typename T>
inline void spdlog::logger::log(level::level_enum lvl, attributes_type &attrs, const T &msg)
{
    if (!should_log(lvl)) return;
    try
    {
        details::log_msg log_msg(&name_, lvl, std::move(attrs));
        log_msg.raw << msg;
        sink_it_(log_msg);
    }
    catch (const std::exception &ex)
    {
        err_handler_(ex.what());
    }
    catch (...)
    {
        err_handler_("Unknown exception in logger " + name_);
        throw;
    }
}

template <typename... Args>
inline void spdlog::logger::log(level::level_enum lvl, attributes_type &attrs, const char *fmt, const Args &... args)
{
    if (!should_log(lvl)) return;

    try
    {
        details::log_msg log_msg(&name_, lvl, std::move(attrs));

        fmt::format_to(log_msg.raw, fmt, args...);
        //log_msg.raw.write(fmt, args...);
        sink_it_(log_msg);
    }
    catch (const std::exception &ex)
    {
        err_handler_(ex.what());
    }
    catch(...)
    {
        err_handler_("Unknown exception in logger " + name_);
        throw;
    }
}

template <typename... Args>
inline void spdlog::logger::log(level::level_enum lvl, attributes_type &attrs, const char *msg)
{
    if (!should_log(lvl)) return;
    try
    {
        details::log_msg log_msg(&name_, lvl, std::move(attrs));
        fmt::format_to(log_msg.raw, "{}", msg);
        sink_it_(log_msg);
    }
    catch (const std::exception &ex)
    {
        err_handler_(ex.what());
    }
    catch (...)
    {
        err_handler_("Unknown exception in logger " + name_);
        throw;
    }
}

template <typename... Args>
inline void spdlog::logger::trace(attributes_type &attrs, const char *fmt, const Args &... args)
{
    log(level::trace, attrs, fmt, args...);
}

template <typename... Args>
inline void spdlog::logger::debug(attributes_type &attrs, const char *fmt, const Args &... args)
{
    log(level::debug, attrs, fmt, args...);
}

template <typename... Args>
inline void spdlog::logger::info(attributes_type &attrs, const char *fmt, const Args &... args)
{
    log(level::info, attrs, fmt, args...);
}

template <typename... Args>
inline void spdlog::logger::warn(attributes_type &attrs, const char *fmt, const Args &... args)
{
    log(level::warn, attrs, fmt, args...);
}

template <typename... Args>
inline void spdlog::logger::error(attributes_type &attrs, const char *fmt, const Args &... args)
{
    log(level::err, attrs, fmt, args...);
}

template <typename... Args>
inline void spdlog::logger::critical(attributes_type &attrs, const char *fmt, const Args &... args)
{
    log(level::critical, attrs, fmt, args...);
}

template<typename T>
inline void spdlog::logger::trace(attributes_type &attrs, const T &msg)
{
    log(level::trace, attrs, msg);
}

template<typename T>
inline void spdlog::logger::debug(attributes_type &attrs, const T &msg)
{
    log(level::debug, attrs, msg);
}

template<typename T>
inline void spdlog::logger::info(attributes_type &attrs, const T &msg)
{
    log(level::info, attrs, msg);
}

template<typename T>
inline void spdlog::logger::warn(attributes_type &attrs, const T &msg)
{
    log(level::warn, attrs, msg);
}

template<typename T>
inline void spdlog::logger::error(attributes_type &attrs, const T &msg)
{
    log(level::err, attrs, msg);
}

template<typename T>
inline void spdlog::logger::critical(attributes_type &attrs, const T &msg)
{
    log(level::critical, attrs, msg);
}

#ifdef SPDLOG_WCHAR_TO_UTF8_SUPPORT

template <typename... Args>
inline void spdlog::logger::log(level::level_enum lvl, attributes_type &attrs, const wchar_t *msg)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t> > conv;

    log(lvl, attrs, conv.to_bytes(msg));
}

template <typename... Args>
inline void spdlog::logger::log(level::level_enum lvl, attributes_type &attrs, const wchar_t *fmt, const Args &... args)
{
    fmt::WMemoryWriter wWriter;

    wWriter.write(fmt, args...);
    log(lvl, attrs, wWriter.c_str());
}

template <typename... Args>
inline void spdlog::logger::trace(attributes_type &attrs, const wchar_t *fmt, const Args &... args)
{
    log(level::trace, attrs, fmt, args...);
}

template <typename... Args>
inline void spdlog::logger::debug(attributes_type &attrs, const wchar_t *fmt, const Args &... args)
{
    log(level::debug, attrs, fmt, args...);
}

template <typename... Args>
inline void spdlog::logger::info(attributes_type &attrs, const wchar_t *fmt, const Args &... args)
{
    log(level::info, attrs, fmt, args...);
}


template <typename... Args>
inline void spdlog::logger::warn(attributes_type &attrs, const wchar_t *fmt, const Args &... args)
{
    log(level::warn, attrs, fmt, args...);
}

template <typename... Args>
inline void spdlog::logger::error(attributes_type &attrs, const wchar_t *fmt, const Args &... args)
{
    log(level::err, attrs, fmt, args...);
}

template <typename... Args>
inline void spdlog::logger::critical(attributes_type &attrs, const wchar_t *fmt, const Args &... args)
{
    log(level::critical, attrs, fmt, args...);
}

#endif // SPDLOG_WCHAR_TO_UTF8_SUPPORT

#endif // SPDLOG_ENABLE_LOG_ATTRIBUTES
