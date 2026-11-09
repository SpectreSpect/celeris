#pragma once

#include <algorithm>
#include <functional>
#include <map>
#include <span>
#include <utility>
#include <vector>

#include "../trajectory_cache_concept.h"
#include "component_contract_test_support.h"

namespace celeris::tests {
    /*
        Переиспользуемый поведенческий набор. Попадание проверяется на правильное
        значение, промах разрешён даже после store: не навязываем сохранение
        записей. Проверяются только переданные значения/истории, не все Y/T.
        equivalent задаёт смысловое равенство Y и не меняет объекты.
    */
    template<class Y, class Factory, class Equivalent = std::equal_to<>>
    void test_trajectory_cache_contract(
        Factory make_cache,
        std::span<const std::pair<simulation::Timestamp, Y>> samples,
        Equivalent equivalent = {})
    {
        validate_samples(samples, equivalent);
        auto cache = make_cache();
        static_assert(trajectory_cache<decltype(cache), Y>);

        using Timestamp = simulation::Timestamp;
        std::map<Timestamp, Y> expected;
        std::vector<Timestamp> keys;

        for (const auto& [time, value] : samples) {
            keys.push_back(time);
        }

        keys.push_back(Timestamp::min());
        keys.push_back(Timestamp::max());

        auto verify = [&] {
            const auto& readable = cache;

            for (auto time : keys) {
                auto actual = readable.find(time);
                auto it = expected.find(time);

                if (actual) {
                    require(it != expected.end(), "Cache returned an absent/invalidated entry");
                    require(
                        std::invoke(equivalent, *actual, it->second),
                        "Cache returned a stale or fabricated value"
                    );
                }
            }
        };

        auto store = [&](Timestamp time, const Y& value) {
            cache.store(time, Y(value));
            expected.erase(time);
            expected.emplace(time, value);
            verify();
        };

        auto invalidate = [&](const InvalidationRegionInterface& region) {
            cache.invalidate(region);

            for (auto it = expected.begin(); it != expected.end();) {
                if (region(it->first)) {
                    it = expected.erase(it);
                } else {
                    ++it;
                }
            }

            verify();
        };

        verify();

        for (const auto& [time, value] : samples) {
            store(time, value);
        }

        store(samples[0].first, samples[1].second);
        store(Timestamp::min(), samples[0].second);
        store(Timestamp::max(), samples[1].second);

        PredicateInvalidationRegion none([](Timestamp) { return false; });
        invalidate(none);
        PredicateInvalidationRegion selected(
            [&](Timestamp time) {
                return time == samples[0].first || time == samples[2].first;
            });
        invalidate(selected);
        store(samples[0].first, samples[0].second);

        const auto low = std::min(samples[0].first, samples[1].first);
        const auto high = std::max(samples[0].first, samples[1].first);
        IntervalInvalidationRegion intervals({{low, high}, {Timestamp::max(), std::nullopt}});
        invalidate(intervals); // Динамический базовый интерфейс сохраняет уточнение.
        invalidate(intervals); // Повторное удаление допустимо.
        store(samples[1].first, samples[1].second);

        IntervalInvalidationRegion all({TimeInterval{}});
        invalidate(all);
    }
}
