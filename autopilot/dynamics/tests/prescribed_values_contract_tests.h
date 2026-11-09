#pragma once

#include <chrono>
#include <functional>
#include <iterator>
#include <map>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include "../prescribed_values_view.h"
#include "component_contract_test_support.h"

namespace celeris::tests {
    /*
        Содержимое и поиск сравниваются с независимой моделью словаря.
        Позиции никогда не переносятся через изменение source. Проверяются
        пустота, несортированный ввод, замена, повторное удаление, min/max,
        точные/промежуточные границы и динамический read-only мост.
        Проверка гарантии исключений требует дополнительного fixture-а,
        способного внедрять ошибки: данный набор не предполагает его наличия.
    */
    template<class Y, class Factory, class Equivalent = std::equal_to<>>
    void test_prescribed_values_contract(
        Factory make_source,
        std::span<const std::pair<simulation::Timestamp, Y>> samples,
        Equivalent equivalent = {})
    {
        validate_samples(samples, equivalent);
        auto source = make_source();
        static_assert(prescribed_values_source<decltype(source), Y>);

        using Timestamp = simulation::Timestamp;
        std::map<Timestamp, Y> expected;
        std::vector<Timestamp> probes{Timestamp::min(), Timestamp::max(), Timestamp{}};

        for (const auto& [time, value] : samples) {
            probes.push_back(time);
            if (time != Timestamp::min()) {
                probes.push_back(time - simulation::Duration{1});
            }

            if (time != Timestamp::max()) {
                probes.push_back(time + simulation::Duration{1});
            }
        }

        auto verify = [&] {
            const auto& readable = source;
            auto expected_it = expected.begin();

            for (const auto& entry : readable) {
                require(expected_it != expected.end(), "Source has extra or repeated entries");
                require(
                    entry.first == expected_it->first,
                    "Source keys are missing or unordered"
                );
                require(
                    std::invoke(equivalent, entry.second, expected_it->second),
                    "Source graph has a wrong value"
                );
                ++expected_it;
            }

            require(expected_it == expected.end(), "Source lost a prescribed entry");

            PrescribedValuesView<Y, decltype(source)> view(readable);

            auto check_bound = [&](auto actual, auto wanted) {
                require(
                    (actual == std::ranges::end(readable)) == (wanted == expected.end()),
                    "Wrong source boundary end"
                );

                if (wanted != expected.end()) {
                    require((*actual).first == wanted->first, "Wrong source boundary key");
                    require(
                        std::invoke(equivalent, (*actual).second, wanted->second),
                        "Wrong source boundary value"
                    );
                }
            };

            auto check_entry = [&](const auto& actual, auto wanted) {
                require(
                    actual.has_value() == (wanted != expected.end()),
                    "Wrong view presence"
                );

                if (actual) {
                    require(actual->first == wanted->first, "Wrong view boundary key");
                    require(
                        std::invoke(equivalent, actual->second, wanted->second),
                        "Wrong view boundary value"
                    );
                }
            };

            for (auto time : probes) {
                const auto wanted = expected.find(time);

                for (int repeat = 0; repeat < 2; ++repeat) {
                    auto actual = readable.find(time);
                    require(
                        actual.has_value() == (wanted != expected.end()),
                        "Wrong source find presence"
                    );
                    if (actual) {
                        require(
                            std::invoke(equivalent, *actual, wanted->second),
                            "Wrong source find value"
                        );
                    }

                    auto through_view = view.find(time);
                    require(
                        through_view.has_value() == actual.has_value(),
                        "Wrong view find presence"
                    );
                    if (through_view) {
                        require(
                            std::invoke(equivalent, *through_view, *actual),
                            "Wrong view find value"
                        );
                    }
                }

                check_bound(readable.lower_bound(time), expected.lower_bound(time));
                check_bound(readable.upper_bound(time), expected.upper_bound(time));
                check_entry(view.lower_bound(time), expected.lower_bound(time));
                check_entry(view.upper_bound(time), expected.upper_bound(time));

                auto before = expected.upper_bound(time);

                if (before == expected.begin()) {
                    check_entry(view.at_or_before(time), expected.end());
                } else {
                    check_entry(view.at_or_before(time), std::prev(before));
                }
            }
        };

        auto set = [&](Timestamp time, const Y& value) {
            source.set(time, Y(value));
            expected.erase(time);
            expected.emplace(time, value);
            verify();
        };

        auto erase = [&](Timestamp time) {
            source.erase(time);
            expected.erase(time);
            verify();
        };

        verify();

        for (const auto& [time, value] : samples) {
            set(time, value);
        }

        set(samples[0].first, samples[1].second);
        erase(samples[1].first);
        erase(samples[1].first);
        set(samples[1].first, samples[0].second);
        set(Timestamp::min(), samples[0].second);
        set(Timestamp::max(), samples[1].second);
        erase(Timestamp::min());
        erase(Timestamp::max());

        for (const auto& [time, value] : samples) {
            erase(time);
        }
    }
}
