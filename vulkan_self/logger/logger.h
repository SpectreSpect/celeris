#pragma once
#include <array>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <concepts>
#include <stdexcept>
#include <type_traits>
#include <string_view>


#include "palette.h"
#include "../colored_string/colored_string.h"
#include "../colored_string/multicolor_string.h"
#include "../colored_string/colored_string_stream.h"

class Logger {
public:
    void push_level(const MultiColorString& str);
    void pop_level();
    void try_pop_level() noexcept;
    
    template <bool new_line = true>
    inline void log(const MultiColorString& message) const {
        write(std::cout, message, new_line);
    }

    template <class Str>
    requires std::constructible_from<MultiColorString, Str&&> &&
             (!std::same_as<std::remove_cvref_t<Str>, MultiColorString>)
    inline void log(Str&& message) const {
        log<true>(MultiColorString(std::forward<Str>(message)));
    }

    ColoredStringsStream log() const;

    template <bool new_line = true>
    inline void log_error(const MultiColorString& error_message) const {
        write(std::cerr, error_message.to_string(LoggerPalette::error), new_line);
    }

    template <class Str>
    requires std::constructible_from<MultiColorString, Str&&> &&
             (!std::same_as<std::remove_cvref_t<Str>, MultiColorString>)
    inline void log_error(Str&& error_message) const {
        log_error<true>(MultiColorString(std::forward<Str>(error_message)));
    }
    
    ColoredStringsStream log_error() const;

    void log_traceback() const;
    
    template<class Exception = std::runtime_error, bool new_line = true>
    requires std::derived_from<Exception, std::exception> &&
             std::constructible_from<Exception, std::string>
    inline void throw_error(const MultiColorString& error_message) const {
        log_traceback();

        std::string error_message_str = error_message.to_string(LoggerPalette::error);

        std::cerr << ColoredString("Error: ", LoggerPalette::error) << error_message_str << (new_line ? "\n" : "");
        throw Exception(error_message.to_monocolor_string());
    }

    template<class Exception = std::runtime_error, bool new_line = true>
    requires std::derived_from<Exception, std::exception> &&
             std::constructible_from<Exception, std::string>
    inline void throw_error(std::string_view error_message) const {
        throw_error<Exception, new_line>(MultiColorString(error_message));
    }

    template<class Exception = std::runtime_error, bool new_line = false>
    requires std::derived_from<Exception, std::exception> &&
             std::constructible_from<Exception, std::string>
    inline ColoredStringsStream throw_error() const {
        return ColoredStringsStream([this](const MultiColorString& error_message) {
            if (this)
                this->throw_error<Exception, new_line>(error_message);
        });
    }

    template<class Exception = std::runtime_error, bool new_line = true>
    requires std::derived_from<Exception, std::exception> &&
             std::constructible_from<Exception, std::string>
    inline void check(bool condition, const MultiColorString& error_message) const {
        if (!condition) {
            throw_error<Exception, new_line>(error_message);
        }
    }

    template<class Exception = std::runtime_error, bool new_line = true>
    requires std::derived_from<Exception, std::exception> &&
             std::constructible_from<Exception, std::string>
    inline void check(bool condition, std::string_view error_message) const {
        check<Exception, new_line>(condition, ColoredString(error_message, LoggerPalette::error));
    }

    template<class Exception = std::runtime_error, bool new_line = false>
    requires std::derived_from<Exception, std::exception> &&
             std::constructible_from<Exception, std::string>
    inline ColoredStringsStream check(bool condition) const {
        return ColoredStringsStream([this, condition](const MultiColorString& error_message) {
            if (this && !condition) {
                throw_error<Exception, new_line>(error_message);
            }
        });
    }

private:
    void write(std::ostream& os, const std::string& message, bool new_line = true) const;
    std::vector<MultiColorString> level_stack;
};
