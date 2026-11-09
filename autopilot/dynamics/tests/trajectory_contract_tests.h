#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "../trajectory_interface.h"

namespace celeris::tests {
    class TrajectoryContractViolation : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

    /*
        make_trajectory() создаёт владеющий указатель на свежую траекторию
        с одними и теми же исходными данными. Экземпляры независимы друг от друга.
        timestamps содержит как минимум два различных времени.
        equivalent сравнивает значения состояния; по умолчанию используется ==.

        Эталон для каждого времени получается на отдельном свежем экземпляре.
        С ним сравниваются последовательные повторы, прямой и обратный обход,
        а также возвраты к ранее запрошенным временам после прогрева кэша.
        Проверка не задаёт саму функцию f и не доказывает контракт для всех историй.

        Хранение эталонов в vector требует перемещаемого/копируемого Y.
        Это ограничение данного помощника, а не TrajectoryInterface.
    */
    template<class Y, class Factory, class Equivalent = std::equal_to<>>
    void test_trajectory_contract(
        Factory make_trajectory,
        std::span<const simulation::Timestamp> timestamps,
        Equivalent equivalent = {})
    {
        if (timestamps.empty() ||
            std::all_of(
                timestamps.begin(),
                timestamps.end(),
                [&](auto timestamp) { return timestamp == timestamps.front(); }
            )) {
            throw std::invalid_argument("Expected at least two distinct timestamps");
        }

        std::vector<Y> expected;
        expected.reserve(timestamps.size());

        for (auto timestamp : timestamps) {
            auto trajectory = make_trajectory();
            expected.push_back((*trajectory)(timestamp));
        }

        auto check = [&](auto& trajectory, std::size_t index) {
            Y actual = (*trajectory)(timestamps[index]);

            if (!std::invoke(equivalent, actual, expected[index])) {
                throw TrajectoryContractViolation(
                    "Trajectory result depends on query history at sample " + std::to_string(index)
                );
            }
        };

        // Разные первые запросы позволяют обнаружить зависимость от прогрева.
        for (bool start_from_end : {false, true}) {
            auto trajectory = make_trajectory();

            for (std::size_t i = 0; i < timestamps.size(); ++i) {
                const auto index = start_from_end ? timestamps.size() - 1 - i : i;
                check(trajectory, index);
                check(trajectory, index);
            }

            // Смена направления и чередование обращений к разным временам.
            for (std::size_t i = timestamps.size(); i > 0; --i) {
                check(trajectory, i - 1);
                check(trajectory, 0);
                check(trajectory, timestamps.size() - 1);
                check(trajectory, i - 1);
            }
        }
    }
}
