#include "level_marker.h"
#include "logger.h"

LevelMarker::LevelMarker(Logger& logger, const MultiColorString& level_name)
    :   logger(&logger) 
{
    logger.push_level(level_name);
}

LevelMarker::~LevelMarker() noexcept {
    logger->try_pop_level();
}
