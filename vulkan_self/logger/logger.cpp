#include "logger.h"

Logger logger;

void Logger::push_level(const MultiColorString& str) {
    level_stack.push_back(str);
}

void Logger::pop_level() {
    if (level_stack.empty()) {
        throw std::runtime_error("Level stack was empty.");
    }

    level_stack.pop_back();
}

void Logger::try_pop_level() noexcept {
    if (!level_stack.empty()) {
        level_stack.pop_back();
    } 
}

ColoredStringsStream Logger::log() const {
    return ColoredStringsStream([this](const MultiColorString& message) {
        if (this)
            this->log<false>(message);
    });
}

ColoredStringsStream Logger::log_error() const {
    return ColoredStringsStream([this](const MultiColorString& error_message) {
        if (this)
            this->log_error<false>(error_message);
    });
}

void Logger::log_traceback() const {
    std::cerr << ColoredString("Traceback: ", LoggerPalette::error) << std::endl; 
    
    for (const auto& s : level_stack) {
        std::cerr << ColoredString("<<<<<< ", LoggerPalette::error) << s << std::endl;
    }
}

void Logger::write(std::ostream& os, const std::string& message, bool new_line) const {
    if (level_stack.empty()) {
        os << message << (new_line ? "\n" : "");
    } else {
        os << level_stack.back() << ": " << message << (new_line ? "\n" : "");
    }
}
