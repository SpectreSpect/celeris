#include "colored_string_stream.h"
#include "../logger/logger.h"
#include "multicolor_string.h"

ColoredStringsStream::ColoredStringsStream(std::function<void(const MultiColorString&)> destroy_callback) 
    :   destroy_callback(destroy_callback) { }

ColoredStringsStream::~ColoredStringsStream() noexcept {
    if (destroy_callback) {        
        MultiColorString result;
        for (const auto& s : strings) {
            result += s;
        }

        destroy_callback(result);
    }
}
