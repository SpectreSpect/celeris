#pragma once

#include "../../vulkan_self/logger/logger_header.h"
#include "clock.h"

namespace celeris {
    template<class Y>
    class TrajectoryInterface {
    public:
        _XPARENT_NAME(TrajectoryInterface);

        using Timestamp = simulation::Timestamp;

        virtual ~TrajectoryInterface() = default;
        
        /*
            При неизменных данных, определяющих траекторию, результат
            зависит только от timestamp.

            Порядок и количество запросов, а также наличие или отсутствие
            закэшированных значений не должны влиять на результат.

            Вычисление может изменять внутренний кэш, но не должно
            изменять определяемую объектом траекторию.
        */
        virtual Y define_trajectory(Timestamp timestamp) = 0;
    };
}
