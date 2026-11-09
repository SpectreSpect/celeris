#pragma once

#include <algorithm>
#include <concepts>
#include <functional>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include "clock.h"

namespace celeris {
    /*
        Полуоткрытый интервал [lower, upper). Отсутствие нижней/верхней границы
        означает отсутствие ограничения с этой стороны. Пустые и пересекающиеся
        интервалы допустимы. В частности, [Timestamp::max(), +inf) содержит
        последнюю представимую отметку времени, без прибавления к ней шага.
    */
    struct TimeInterval {
        std::optional<simulation::Timestamp> lower;
        std::optional<simulation::Timestamp> upper;

        [[nodiscard]]
        bool contains(simulation::Timestamp timestamp) const noexcept {
            return (!lower || *lower <= timestamp) && (!upper || timestamp < *upper);
        }
    };

    /*
        Область — тотальный предикат на Timestamp. Для фиксированного объекта
        ответы стабильны и не зависят от порядка/числа обращений. Вызов завершает
        работу, не изменяет источник или траекторию; исключения допускаются.
        При исключении операция инвалидирования может прерваться.

        Предикат может заимствовать данные: владелец обязан сохранять их время
        жизни и логические значения до последнего вызова предиката. В протоколе
        обновления это прежний источник ДО применения изменения.
        Концепт проверяет форму вызова; смысловые законы проверяются отдельно.
    */
    template<class Region>
    concept invalidation_region = requires(const Region& region, simulation::Timestamp timestamp) {
        { region(timestamp) } -> std::same_as<bool>;
    };

    class InvalidationRegionInterface {
    public:
        virtual ~InvalidationRegionInterface() = default;

        [[nodiscard]]
        bool operator()(simulation::Timestamp timestamp) const {
            return contains_impl(timestamp);
        }

    private:
        virtual bool contains_impl(simulation::Timestamp timestamp) const = 0;
    };

    /*
        Усиленный контракт: предикат ТОЧНО совпадает с объединением intervals().
        Интервалы не обязаны быть отсортированы или непересекающимися.
        span и его содержимое действительны и неизменны до уничтожения области.
        Пустой список означает пустую область; {TimeInterval{}} — всё время.
        Интервальное описание позволяет удалять диапазоны без просмотра каждого
        ключа. Реализации без такого описания остаются допустимыми областями.
    */
    template<class Region>
    concept interval_invalidation_region =
        invalidation_region<Region> &&
        requires(const Region& region) {
            { region.intervals() } -> std::same_as<std::span<const TimeInterval>>;
        };

    class IntervalInvalidationRegionInterface : public InvalidationRegionInterface {
    public:
        [[nodiscard]]
        virtual std::span<const TimeInterval> intervals() const = 0;
    };

    class IntervalInvalidationRegion final : public IntervalInvalidationRegionInterface {
    public:
        explicit IntervalInvalidationRegion(std::vector<TimeInterval> intervals)
            :   m_intervals(std::move(intervals)) {}

        std::span<const TimeInterval> intervals() const override {
            return m_intervals;
        }

    private:
        bool contains_impl(simulation::Timestamp timestamp) const override {
            return std::any_of(
                m_intervals.begin(),
                m_intervals.end(),
                [&](const auto& interval) { return interval.contains(timestamp); });
        }

        std::vector<TimeInterval> m_intervals;
    };

    // Мост для обычного предиката; дополнительных возможностей он не заявляет.
    template<invalidation_region Predicate>
    class PredicateInvalidationRegion final : public InvalidationRegionInterface {
    public:
        explicit PredicateInvalidationRegion(Predicate predicate)
            :   m_predicate(std::move(predicate)) {}

    private:
        bool contains_impl(simulation::Timestamp timestamp) const override {
            return std::invoke(m_predicate, timestamp);
        }

        Predicate m_predicate;
    };

    /*
        Используем усиленный статический контракт либо явно заявленный
        динамический интерфейс. Отсутствие описания отличается от пустой области.
        Динамическое обнаружение требует RTTI; статическая ветвь обходится без
        dynamic_cast. Полученный span заимствуется у region.
    */
    template<invalidation_region Region>
    [[nodiscard]]
    std::optional<std::span<const TimeInterval>> invalidation_intervals(const Region& region) {
        if constexpr (interval_invalidation_region<Region>) {
            return region.intervals();
        } else if constexpr (std::is_polymorphic_v<Region>) {
            if (const auto* intervals =
                dynamic_cast<const IntervalInvalidationRegionInterface*>(&region)) {
                return intervals->intervals();
            }
        }

        return std::nullopt;
    }
}
