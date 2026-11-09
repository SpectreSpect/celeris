#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "../invalidation_region.h"

namespace celeris::tests::models {
    using Timestamp = simulation::Timestamp;

    // Только тестовые модели, не production-реализации компонентов.
    template<class Y>
    class MapCache {
    public:
        std::optional<Y> find(Timestamp timestamp) const {
            auto it = m_values.find(timestamp);
            return it == m_values.end() ? std::nullopt : std::optional<Y>{*it->second};
        }

        void store(Timestamp timestamp, Y value) {
            auto complete = std::make_shared<const Y>(std::move(value));
            m_values.insert_or_assign(timestamp, std::move(complete));
        }

        template<invalidation_region Region>
        void invalidate(const Region& region) {
            if (auto intervals = invalidation_intervals(region)) {
                ++interval_operations;

                for (const auto& interval : *intervals) {
                    if (interval.lower && interval.upper && *interval.lower >= *interval.upper) {
                        continue;
                    }

                    auto first =
                        interval.lower ?
                        m_values.lower_bound(*interval.lower) :
                        m_values.begin();
                    auto last =
                        interval.upper ?
                        m_values.lower_bound(*interval.upper) :
                        m_values.end();

                    m_values.erase(first, last);
                }
            } else {
                for (auto it = m_values.begin(); it != m_values.end();) {
                    ++predicate_checks;

                    if (region(it->first)) {
                        it = m_values.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }

        std::size_t interval_operations = 0;
        std::size_t predicate_checks = 0;

    protected:
        std::map<Timestamp, std::shared_ptr<const Y>> m_values;
    };

    template<class Y>
    class NoCache {
    public:
        std::optional<Y> find(Timestamp) const {
            return std::nullopt;
        }

        void store(Timestamp, Y) {}

        void invalidate(const InvalidationRegionInterface&) {}
    };

    template<class Y>
    class SingleEntryCache {
    public:
        std::optional<Y> find(Timestamp time) const {
            if (m_entry && m_entry->first == time) {
                return m_entry->second;
            }

            return std::nullopt;
        }

        void store(Timestamp time, Y value) {
            m_entry = std::make_shared<const std::pair<Timestamp, Y>>(time, std::move(value));
        }

        void invalidate(const InvalidationRegionInterface& region) {
            if (m_entry && region(m_entry->first)) {
                m_entry.reset();
            }
        }

    private:
        std::shared_ptr<const std::pair<Timestamp, Y>> m_entry;
    };

    template<class Y>
    class StaleCache : public MapCache<Y> {
    public:
        void store(Timestamp time, Y value) {
            this->m_values.try_emplace(time, std::make_shared<const Y>(std::move(value)));
        }
    };

    template<class Y>
    class UninvalidatedCache : public MapCache<Y> {
    public:
        void invalidate(const InvalidationRegionInterface&) {}
    };

    template<class Y>
    class MapSource {
    public:
        using Entry = std::pair<const Timestamp, Y>;

        auto begin() const {
            return m_values.begin();
        }

        auto end() const {
            return m_values.end();
        }

        auto lower_bound(Timestamp time) const {
            return m_values.lower_bound(time);
        }

        auto upper_bound(Timestamp time) const {
            return m_values.upper_bound(time);
        }

        std::optional<Y> find(Timestamp time) const {
            auto it = m_values.find(time);
            return it == m_values.end() ? std::nullopt : std::optional<Y>{it->second};
        }

        void set(Timestamp time, Y value) {
            auto candidate = m_values;
            candidate.erase(time);
            candidate.emplace(time, std::move(value));
            m_values.swap(candidate);
        }

        void erase(Timestamp time) {
            auto candidate = m_values;
            candidate.erase(time);
            m_values.swap(candidate);
        }

    protected:
        std::map<Timestamp, Y> m_values;
    };

    template<class Y>
    class VectorSource {
    public:
        using Entry = std::pair<Timestamp, Y>;

        auto begin() const {
            return m_values.begin();
        }

        auto end() const {
            return m_values.end();
        }

        auto lower_bound(Timestamp time) const {
            return std::lower_bound(
                begin(),
                end(),
                time,
                [](const Entry& entry, Timestamp key) { return entry.first < key; });
        }

        auto upper_bound(Timestamp time) const {
            return std::upper_bound(
                begin(),
                end(),
                time,
                [](Timestamp key, const Entry& entry) { return key < entry.first; });
        }

        std::optional<Y> find(Timestamp time) const {
            auto it = lower_bound(time);
            return it == end() || it->first != time ?
                std::nullopt :
                std::optional<Y>{it->second};
        }

        void set(Timestamp time, Y value) {
            std::vector<Entry> candidate;
            candidate.reserve(m_values.size() + 1);
            bool written = false;

            for (const auto& entry : m_values) {
                if (!written && time <= entry.first) {
                    candidate.emplace_back(time, std::move(value));
                    written = true;
                }

                if (entry.first != time) {
                    candidate.push_back(entry);
                }
            }

            if (!written) {
                candidate.emplace_back(time, std::move(value));
            }

            m_values.swap(candidate);
        }

        void erase(Timestamp time) {
            std::vector<Entry> candidate;
            candidate.reserve(m_values.size());

            for (const auto& entry : m_values) {
                if (entry.first != time) {
                    candidate.push_back(entry);
                }
            }

            m_values.swap(candidate);
        }

    protected:
        std::vector<Entry> m_values;
    };

    /*
        Проверяет обход по контракту C++20. Категория для старых алгоритмов
        слабее iterator_concept, доступ к записи идёт через operator*.
    */
    template<class Iterator>
    class NoArrowIterator {
    public:
        using value_type = std::iter_value_t<Iterator>;
        using difference_type = std::iter_difference_t<Iterator>;
        using iterator_category = std::forward_iterator_tag;
        using iterator_concept = std::bidirectional_iterator_tag;

        NoArrowIterator() = default;

        explicit NoArrowIterator(Iterator iterator)
            :   m_iterator(iterator) {}

        decltype(auto) operator*() const {
            return *m_iterator;
        }

        NoArrowIterator& operator++() {
            ++m_iterator;
            return *this;
        }

        NoArrowIterator operator++(int) {
            auto old = *this;
            ++*this;
            return old;
        }

        NoArrowIterator& operator--() {
            --m_iterator;
            return *this;
        }

        NoArrowIterator operator--(int) {
            auto old = *this;
            --*this;
            return old;
        }

        bool operator==(const NoArrowIterator&) const = default;

    private:
        Iterator m_iterator;
    };

    template<class Y>
    class NoArrowSource : public VectorSource<Y> {
    public:
        auto begin() const {
            return NoArrowIterator(VectorSource<Y>::begin());
        }

        auto end() const {
            return NoArrowIterator(VectorSource<Y>::end());
        }

        auto lower_bound(Timestamp time) const {
            return NoArrowIterator(VectorSource<Y>::lower_bound(time));
        }

        auto upper_bound(Timestamp time) const {
            return NoArrowIterator(VectorSource<Y>::upper_bound(time));
        }
    };

    template<class Y>
    class WrongUpperBoundSource : public MapSource<Y> {
    public:
        auto upper_bound(Timestamp time) const {
            return this->lower_bound(time);
        }
    };

    class InjectedFailure : public std::runtime_error {
    public:
        InjectedFailure()
            :   std::runtime_error("Injected value copy failure") {}
    };

    struct ThrowingValue {
        explicit ThrowingValue(int input)
            :   value(input) {}

        ThrowingValue(const ThrowingValue& other)
            :   value(other.value)
        {
            copy_point();
        }

        ThrowingValue(ThrowingValue&&) noexcept = default;
        ThrowingValue& operator=(ThrowingValue&&) noexcept = default;

        ThrowingValue& operator=(const ThrowingValue& other) {
            value = other.value; // Намеренно частичная запись перед возможной ошибкой.
            copy_point();
            return *this;
        }

        static void copy_point() {
            if (copies_before_failure == 0) {
                throw InjectedFailure();
            }

            if (copies_before_failure > 0) {
                --copies_before_failure;
            }
        }

        inline static int copies_before_failure = -1;
        int value;
    };

    template<class Y>
    class WeakExceptionSource : public MapSource<Y> {
    public:
        void set(Timestamp time, Y value) {
            this->m_values.insert_or_assign(time, value);
        }
    };
}
