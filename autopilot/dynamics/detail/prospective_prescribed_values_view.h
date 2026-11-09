#pragma once

#include <iterator>
#include <optional>
#include <ranges>
#include <variant>

#include "../prescribed_values_view.h"

namespace celeris::detail {
    /*
        Заимствующее представление D' на время проверки совместимости. Оно
        показывает одну правку поверх прежнего D. Поиск границ сравнивает
        результат поиска в источнике с новой точкой или пропускает удалённую.
    */
    template<class Y, prescribed_values_source<Y> Source>
    class ProspectivePrescribedValuesView final : public PrescribedValuesViewInterface<Y> {
    public:
        using Timestamp = simulation::Timestamp;
        using Entry = typename PrescribedValuesViewInterface<Y>::Entry;

        ProspectivePrescribedValuesView(
            const Source& source,
            const PrescribedValuesChange<Y>& change)
            :   m_source(source),
                m_set(std::get_if<SetPrescribedValue<Y>>(&change)),
                m_timestamp(
                    m_set ? m_set->timestamp : std::get<ErasePrescribedValue>(change).timestamp
                ) {}

        std::optional<Y> find(Timestamp timestamp) const override {
            if (timestamp != m_timestamp) {
                return m_source.find(timestamp);
            }

            if (m_set) {
                return m_set->value;
            }

            return std::nullopt;
        }

        std::optional<Entry> lower_bound(Timestamp timestamp) const override {
            return next_entry(m_source.lower_bound(timestamp), timestamp <= m_timestamp);
        }

        std::optional<Entry> upper_bound(Timestamp timestamp) const override {
            return next_entry(m_source.upper_bound(timestamp), timestamp < m_timestamp);
        }

        std::optional<Entry> at_or_before(Timestamp timestamp) const override {
            const auto first = std::ranges::begin(m_source);
            const auto end = std::ranges::end(m_source);
            const auto after = m_source.upper_bound(timestamp);
            auto it = after == first ? end : std::ranges::prev(after);

            if (!m_set && it != end && (*it).first == m_timestamp) {
                it = it == first ? end : std::ranges::prev(it);
            }

            if (m_set && m_timestamp <= timestamp && (it == end || (*it).first <= m_timestamp)) {
                return Entry{m_timestamp, m_set->value};
            }

            return entry(it);
        }

    private:
        using Iterator = std::ranges::iterator_t<const Source>;

        std::optional<Entry> next_entry(Iterator it, bool includes_change) const {
            const auto end = std::ranges::end(m_source);

            if (m_set && includes_change && (it == end || m_timestamp <= (*it).first)) {
                return Entry{m_timestamp, m_set->value};
            }

            if (!m_set && it != end && (*it).first == m_timestamp) {
                ++it;
            }

            return entry(it);
        }

        std::optional<Entry> entry(Iterator it) const {
            if (it == std::ranges::end(m_source)) {
                return std::nullopt;
            }

            return Entry{(*it).first, (*it).second};
        }

        const Source& m_source;
        const SetPrescribedValue<Y>* m_set;
        Timestamp m_timestamp;
    };
}
