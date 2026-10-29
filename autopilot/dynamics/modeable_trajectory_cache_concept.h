#pragma once

#include <unordered_map>
#include <unordered_set>
#include <type_traits>
#include <functional>
#include <optional>
#include <concepts>
#include <iterator>
#include <utility>
#include <ranges>

#include "../../vulkan_self/logger/logger_header.h"
#include "trajectory_cache_concept.h"
#include "clock.h"

namespace celeris {
    // Стоит позже вынести в utils #TODO
    template<class Enum>
    concept countable_enum = 
        std::is_enum_v<Enum> &&
        requires() {
            { Enum::COUNT } -> std::same_as<Enum>;
        };

    template<class ModeType>
    concept enum_with_view_mode = 
        countable_enum<ModeType> &&
        requires() {
            { ModeType::VIEW_ALL } -> std::same_as<ModeType>;
        };
    
    template<template<class> class Cache, class Y, class ModeType>
    requires (
        trajectory_cache<Cache, Y> &&
        enum_with_view_mode<ModeType>
    )
    class ModeableTrajectoryCache {
    public:
        _XPARENT_NAME(ModeableTrajectoryCache);

        struct ViewAllIterator;
        struct Iterator;

        using Timestamp = simulation::Timestamp;
        using NestedCacheIterator = std::ranges::iterator_t<Cache<Y>>;
        using CacheValueType = std::ranges::range_value_t<Cache<Y>>;
        using CacheContainer = std::unordered_map<ModeType, Cache<Y>>;
        using reverse_iterator = std::reverse_iterator<Iterator>;

        ModeableTrajectoryCache() 
            :   m_current_mode(ModeType::VIEW_ALL)
        {
            static_assert(std::bidirectional_iterator<ViewAllIterator>);
            static_assert(std::bidirectional_iterator<Iterator>);

            LOG_METHOD();

            m_caches.try_emplace(m_current_mode);
        }

        const Cache<Y>& current_cache() const {
            LOG_METHOD();

            auto it = m_caches.find(m_current_mode);

            logger().check(
                it != m_caches.end(),
                "Failed to find the current cache."
            );

            return it->second;
        }

        Iterator begin() {
            LOG_METHOD();

            if (m_current_mode == ModeType::VIEW_ALL) {
                ModeType min_mode;
                std::optional<NestedCacheIterator> min_iterator = std::nullopt;
                for (auto& [mode, cache] : m_caches) {
                    NestedCacheIterator it = cache.begin();
                    
                    if (it == cache.end()) {
                        continue;
                    }

                    if (!min_iterator.has_value() || (*it).first < (**min_iterator).first) {
                        min_iterator = std::move(it);
                        min_mode = mode;
                    }
                }

                return min_iterator.has_value() ?
                    Iterator::view_all_iterator(m_caches, min_mode, *min_iterator) :
                    Iterator::view_all_iterator_end(m_caches);
            }

            return Iterator(current_cache_mutable().begin());
        }

        Iterator end() {
            LOG_METHOD();

            if (m_current_mode == ModeType::VIEW_ALL) {
                return Iterator::view_all_iterator_end(m_caches);
            }

            return Iterator(current_cache_mutable().end());
        }

        reverse_iterator rbegin() {
            return std::make_reverse_iterator(end());
        }

        reverse_iterator rend() {
            return std::make_reverse_iterator(begin());
        }

        Iterator lower_bound(Timestamp timestamp) {
            LOG_METHOD();

            static auto nested_lower_bound = [](Cache<Y>& cache, Timestamp timestamp) {
                return cache.lower_bound(timestamp);
            };

            return find_bound(timestamp, nested_lower_bound);
        }

        Iterator upper_bound(Timestamp timestamp) {
            LOG_METHOD();

            static auto nested_upper_bound = [](Cache<Y>& cache, Timestamp timestamp) {
                return cache.upper_bound(timestamp);
            };

            return find_bound(timestamp, nested_upper_bound);
        }

        Iterator erase(Iterator first, Iterator last) {
            LOG_METHOD();
            
            if (m_current_mode == ModeType::VIEW_ALL) {
                ViewAllIterator* first_view_all = first.as_view_all();
                ViewAllIterator* last_view_all = last.as_view_all();

                logger().check(
                    first_view_all != nullptr,
                    "The first iterator must contain a `ViewAllIterator` holder."
                );

                logger().check(
                    last_view_all != nullptr,
                    "The last iterator must contain a `ViewAllIterator` holder."
                );

                logger().check(
                    first_view_all->m_caches == &m_caches &&
                    last_view_all->m_caches == &m_caches,
                    "Both iterators must belong to this cache."
                );

                if (first_view_all->is_end() && last_view_all->is_end()) {
                    return last;
                }

                logger().check(
                    !first_view_all->is_end(),
                    "Reversed range: first == end(), while last != end()."
                );

                Timestamp first_timestamp = (*first).first;
                std::optional<Timestamp> last_timestamp = 
                    last_view_all->is_end() ?
                    std::nullopt :
                    std::optional<Timestamp>{(*last).first};

                logger().check(
                    !last_timestamp.has_value() || first_timestamp <= *last_timestamp,
                    "Reversed range: first > last"
                );

                for (auto& [mode, cache] : m_caches) {
                    auto from = first_view_all->m_iterators.at(mode);
                    auto to = last_view_all->m_iterators.at(mode);

                    last_view_all->m_iterators.at(mode) = cache.erase(from, to);
                }

                return last;
            }

            NestedCacheIterator* first_nested = first.as_nested_iterator();
            NestedCacheIterator* last_nested = last.as_nested_iterator();

            logger().check(
                first_nested != nullptr,
                "The first iterator must contain a `NestedCacheIterator` holder."
            );

            logger().check(
                last_nested != nullptr,
                "The last iterator must contain a `NestedCacheIterator` holder."
            );

            Cache<Y>& current_cache_ = current_cache_mutable();

            std::optional<Timestamp> first_timestamp = 
                *first_nested == current_cache_.end() ? 
                std::nullopt : 
                std::optional<Timestamp>{(**first_nested).first};
            
            std::optional<Timestamp> last_timestamp = 
                *last_nested == current_cache_.end() ? 
                std::nullopt : 
                std::optional<Timestamp>{(**last_nested).first};
            
            if (!first_timestamp.has_value() && !last_timestamp.has_value()) {
                return last;
            }

            logger().check(
                first_timestamp.has_value(),
                "Reversed range: first == end(), while last != end()."
            );

            logger().check(
                !last_timestamp.has_value() || *first_timestamp <= *last_timestamp,
                "Reversed range: first > last"
            );

            return current_cache_mutable().erase(*first_nested, *last_nested);
        }

        std::pair<Iterator, bool> insert(std::pair<Timestamp, Y> value) {
            LOG_METHOD();

            logger().check(
                m_current_mode != ModeType::VIEW_ALL,
                "Values ​​cannot be inserted in `VIEW_ALL` mode."
            );

            bool timestamp_is_free = true;
            for (auto& [mode, cache] : m_caches) {
                if (cache_contains(cache, value.first)) {
                    timestamp_is_free = false;
                    break;
                }
            }

            Cache<Y>& cache = m_caches.at(m_current_mode);

            if (!timestamp_is_free) {
                auto it = cache.lower_bound(value.first);

                if (it != cache.end() && (*it).first != value.first) {
                    it = cache.end();
                }

                return {Iterator(std::move(it)), false};
            }

            return cache.insert(std::move(value));
        }

        std::pair<Iterator, bool> emplace(Timestamp timestamp, Y value) {
            LOG_METHOD();

            logger().check(
                m_current_mode != ModeType::VIEW_ALL,
                "Values ​​cannot be inserted in `VIEW_ALL` mode."
            );

            bool timestamp_is_free = true;
            for (auto& [mode, cache] : m_caches) {
                if (cache_contains(cache, timestamp)) {
                    timestamp_is_free = false;
                    break;
                }
            }

            Cache<Y>& cache = m_caches.at(m_current_mode);

            if (!timestamp_is_free) {
                auto it = cache.lower_bound(timestamp);

                if (it != cache.end() && (*it).first != timestamp) {
                    it = cache.end();
                }

                return {Iterator(std::move(it)), false};
            }

            return cache.emplace(timestamp, std::move(value));
        }

        void set_mode(ModeType mode) {
            LOG_METHOD();

            if (m_current_mode == mode) {
                return;
            }

            m_caches.try_emplace(mode);
            m_current_mode = mode;
        }
    
    private:
        CacheContainer m_caches;
        ModeType m_current_mode;

    private:
        Cache<Y>& current_cache_mutable() {
            LOG_METHOD();

            auto it = m_caches.find(m_current_mode);

            logger().check(
                it != m_caches.end(),
                "Failed to find the current cache."
            );

            return it->second;
        }

        // Хотя для такой микро хелповой функции как будто можно было бы и без requires... Ну да пофиг :p
        template<class Func>
        requires(
            requires(
                Func& bound_finder,
                Cache<Y>& cache,
                Timestamp timestamp)
            {
                { bound_finder(cache, timestamp) } -> std::same_as<NestedCacheIterator>;
            }
        )
        Iterator find_bound(Timestamp timestamp, Func& bound_finder) {
            LOG_METHOD();

            if (m_current_mode == ModeType::VIEW_ALL) {
                ModeType bound_mode;
                std::optional<NestedCacheIterator> bound_iterator = std::nullopt;
                for (auto& [mode, cache] : m_caches) {
                    NestedCacheIterator it = bound_finder(cache, timestamp);

                    if (it == cache.end()) {
                        continue;
                    }

                    if (!bound_iterator.has_value() || (*it).first < (**bound_iterator).first) {
                        bound_iterator = std::move(it);
                        bound_mode = mode;
                    }
                }

                return bound_iterator.has_value() ?
                        Iterator::view_all_iterator(m_caches, bound_mode, *bound_iterator) :
                        Iterator::view_all_iterator_end(m_caches);
            }

            return Iterator(bound_finder(current_cache_mutable(), timestamp));
        }

        
        bool cache_contains(Cache<Y>& cache, Timestamp timestamp) const
            requires(
                    requires(Cache<Y>& cache, Timestamp timestamp) {
                    { cache.contains(timestamp) } -> std::same_as<bool>;
                }
            )
        {

            LOG_METHOD();

            return cache.contains(timestamp);
        }

        bool cache_contains(Cache<Y>& cache, Timestamp timestamp) const {
            LOG_METHOD();

            auto it = cache.lower_bound(timestamp);
            return it != cache.end() && (*it).first == timestamp;

        }
    };
}

#include "modeable_iterator.tpp"
