#pragma once

#include <concepts>
#include <iterator>
#include <optional>
#include <ranges>
#include <utility>

#include "prescribed_values_concept.h"

namespace celeris {
    /*
        Динамическая граница чтения источника при фиксированном Y. Все результаты
        — независимые логические копии. lower/upper имеют ту же семантику, что
        у source, но возвращают optional пары вместо конкретного итератора.
        at_or_before(t) — последняя заданная пара с key<=t, либо nullopt.

        Граф D можно обойти через lower_bound(Timestamp::min()) и повторные
        upper_bound(key). Пустота и экстремальные времена поддерживаются.
        Вызовы завершаются, не меняют D, исключения допускаются. Данные неизменны
        на время одного вычисления/проверки совместимости/применения предиката.
        Это не снимок, не владелец и не новый компонент хранения.
    */
    template<std::copy_constructible Y>
    class PrescribedValuesViewInterface {
    public:
        using Timestamp = simulation::Timestamp;
        using Entry = std::pair<Timestamp, Y>;

        virtual ~PrescribedValuesViewInterface() = default;

        [[nodiscard]]
        virtual std::optional<Y> find(Timestamp timestamp) const = 0;

        [[nodiscard]]
        virtual std::optional<Entry> lower_bound(Timestamp timestamp) const = 0;

        [[nodiscard]]
        virtual std::optional<Entry> upper_bound(Timestamp timestamp) const = 0;

        [[nodiscard]]
        virtual std::optional<Entry> at_or_before(Timestamp timestamp) const = 0;
    };

    /*
        Заимствующий live-view: Source остаётся статическим компонентом. Между
        завершёнными операциями источник можно менять; следующие вызовы этого
        view отражают новое D. Исходный объект должен оставаться тем же и жить
        дольше использования view; удерживать старую версию данных view не умеет.
    */
    template<class Y, prescribed_values_source<Y> Source>
    class PrescribedValuesView final : public PrescribedValuesViewInterface<Y> {
    public:
        using Timestamp = simulation::Timestamp;
        using Entry = typename PrescribedValuesViewInterface<Y>::Entry;

        explicit PrescribedValuesView(const Source& source)
            :   m_source(source) {}

        std::optional<Y> find(Timestamp timestamp) const override {
            return m_source.find(timestamp);
        }

        std::optional<Entry> lower_bound(Timestamp timestamp) const override {
            return entry(m_source.lower_bound(timestamp));
        }

        std::optional<Entry> upper_bound(Timestamp timestamp) const override {
            return entry(m_source.upper_bound(timestamp));
        }

        std::optional<Entry> at_or_before(Timestamp timestamp) const override {
            auto it = m_source.upper_bound(timestamp);

            if (it == std::ranges::begin(m_source)) {
                return std::nullopt;
            }

            return entry(std::ranges::prev(it));
        }

    private:
        std::optional<Entry> entry(std::ranges::iterator_t<const Source> it) const {
            if (it == std::ranges::end(m_source)) {
                return std::nullopt;
            }

            return Entry{(*it).first, (*it).second};
        }

        const Source& m_source;
    };
}
