#pragma once

#include <unordered_map>
#include <optional>
#include <iterator>
#include <utility>
#include <variant>
#include <memory>

#include "../../vulkan_self/logger/logger_header.h"
#include "modeable_trajectory_cache.h"
#include "iterator_interface.h"

namespace celeris {
    template<template<class> class Cache, class Y, class ModeType>
    requires (
        trajectory_cache<Cache, Y> &&
        enum_with_view_mode<ModeType>
    )
    struct ModeableTrajectoryCache<Cache, Y, ModeType>::ViewAllIterator {
        _XPARENT_NAME(ViewAllIterator);
        
        std::unordered_map<ModeType, NestedCacheIterator> m_iterators;
        CacheContainer* m_caches = nullptr;
        std::optional<ModeType> m_current_mode = std::nullopt;

        using reference = std::iter_reference_t<NestedCacheIterator>;
        using value_type = std::iter_value_t<NestedCacheIterator>;
        using difference_type = std::iter_difference_t<NestedCacheIterator>;

        using iterator_concept = std::bidirectional_iterator_tag;
        using iterator_category = std::bidirectional_iterator_tag;

        ViewAllIterator() = default;

        ViewAllIterator(
            CacheContainer& caches,
            ModeType iterator_mode,
            NestedCacheIterator iterator)
            :   m_caches(&caches)
        {
            LOG_METHOD();

            auto cache_it = caches.find(iterator_mode);

            logger().check(
                cache_it != caches.end(),
                "Failed to find a cache with the mode `iterator_mode`."
            );

            Cache<Y>& cache = cache_it->second;
            m_current_mode = 
                iterator != cache.end() ?
                std::optional<ModeType>{iterator_mode} :
                std::nullopt;

            auto [it, inserted] = m_iterators.emplace(iterator_mode, std::move(iterator));
            
            logger().check(inserted, "Failed to insert `iterator`");

            ensure_all_iterators();
        }

        ViewAllIterator(CacheContainer& caches)
            :   m_caches(&caches),
                m_current_mode(std::nullopt)
        {
            LOG_METHOD();

            ensure_all_iterators();
        }

        static ViewAllIterator end(CacheContainer& caches) {
            LOG_METHOD();

            return ViewAllIterator(caches);
        }

        bool is_begin() const {
            return !find_closest_prev_mode().has_value();
        }

        bool is_end() const noexcept {
            return !m_current_mode.has_value();
        }

        NestedCacheIterator* get_current_iterator() {
            LOG_METHOD();

            if (!m_current_mode.has_value()) {
                return nullptr;
            }

            auto it = m_iterators.find(*m_current_mode);

            logger().check(
                it != m_iterators.end(),
                "Can't find iterator with `m_current_mode` mode."
            );

            return &it->second;
        }
        
        void ensure_all_iterators() {
            LOG_METHOD();

            logger().check(m_caches != nullptr, "`m_caches` is null.");

            NestedCacheIterator* current_iterator = get_current_iterator();

            for (auto& [mode, cache] : *m_caches) {
                if (!m_iterators.contains(mode)) {
                    std::optional<NestedCacheIterator> iter;

                    if (current_iterator != nullptr) {
                        Timestamp current_timestamp = (**current_iterator).first;

                        // end() возможен - это норм
                        iter = cache.upper_bound(current_timestamp);
                    } else {
                        iter = cache.end();
                    }

                    auto [inserted_it, inserted] = m_iterators.emplace(mode, std::move(*iter));
                    logger().check(inserted, "Failed to insert `upper_bound_it`.");
                }
            }
        }

        std::optional<ModeType> min_iterator_mode() {
            LOG_METHOD();
            
            logger().check(m_caches != nullptr, "`m_caches` is null.");

            ModeType min_mode;
            NestedCacheIterator* min_it = nullptr;
            for (auto& [mode, nested_it] : m_iterators) {
                Cache<Y>& cache = m_caches->at(mode);

                if (nested_it == cache.end()) {
                    continue;
                }

                if (min_it == nullptr || (*nested_it).first < (**min_it).first) {
                    min_it = &nested_it;
                    min_mode = mode;
                }
            }

            if (min_it == nullptr) {
                return std::nullopt;
            }

            return min_mode;
        }

        std::optional<ModeType> find_closest_prev_mode() const {
            LOG_METHOD();

            logger().check(m_caches != nullptr, "`m_caches` is null.");

            ModeType closest_prev_mode;
            std::optional<NestedCacheIterator> closest_prev_iterator;
            for (auto& [mode, iterator] : m_iterators) {
                Cache<Y>& cache = m_caches->at(mode);

                if (iterator == cache.begin()) {
                    continue;
                }

                // Гарантированно перепрыгнет левее current_iterator
                NestedCacheIterator prev = std::prev(iterator); 

                // Поэтому среди всех перепрыгнувших current_iterator, находим тот, что всех правее
                if (!closest_prev_iterator.has_value() || (*prev).first > (**closest_prev_iterator).first) {
                    closest_prev_iterator = std::move(prev);
                    closest_prev_mode = mode;
                }
            }

            if (closest_prev_iterator.has_value()) {
                return closest_prev_mode;
            } else {
                return std::nullopt;
            }
        }

        ViewAllIterator& operator++() {
            LOG_METHOD();

            logger().check(
                m_current_mode.has_value(),
                "The iterator was pointing to end(). You cannot increment "
                "an iterator that points to end() :(."
            );
            
            // Проверка выше гарантирует, что итератор current_iterator здесь есть
            ++(*get_current_iterator());
            
            m_current_mode = min_iterator_mode();

            return *this;
        }

        ViewAllIterator& operator--() {
            LOG_METHOD();

            std::optional<ModeType> closest_prev_mode = find_closest_prev_mode();

            logger().check(
                closest_prev_mode.has_value(),
                "The iterator was pointing to begin(). You cannot decrement "
                "an iterator that points to begin() :(."
            );
            
            m_current_mode = closest_prev_mode;
            --(*get_current_iterator());

            return *this;
        }

        ViewAllIterator operator++(int) {
            LOG_METHOD();

            ViewAllIterator copy = *this;
            ++(*this);

            return copy;
        }

        ViewAllIterator operator--(int) {
            LOG_METHOD();

            ViewAllIterator copy = *this;
            --(*this);

            return copy;
        }

        reference operator*() const {
            LOG_METHOD();

            logger().check(
                !is_end(),
                "You cannot dereference an iterator that points to end()."
            );

            return *m_iterators.at(*m_current_mode);
        }

        bool operator==(const ViewAllIterator& iter) const {
            LOG_METHOD();

            if (m_caches != iter.m_caches) {
                return false;
            }

            if (is_end() || iter.is_end()) {
                return is_end() && iter.is_end();
            }

            if (m_current_mode != iter.m_current_mode) {
                return false;
            }

            return m_iterators.at(*m_current_mode)
                == iter.m_iterators.at(*iter.m_current_mode);
        }
    };

    template<template<class> class Cache, class Y, class ModeType>
    requires (
        trajectory_cache<Cache, Y> &&
        enum_with_view_mode<ModeType>
    )
    struct ModeableTrajectoryCache<Cache, Y, ModeType>::Iterator {
        _XPARENT_NAME(Iterator);

        using reference = std::iter_reference_t<NestedCacheIterator>;
        using value_type = std::iter_value_t<NestedCacheIterator>;
        using difference_type = std::iter_difference_t<NestedCacheIterator>;

        using iterator_concept = std::bidirectional_iterator_tag;
        using iterator_category = std::bidirectional_iterator_tag;

        using IteratorStorage = std::variant<NestedCacheIterator, ViewAllIterator>;

        IteratorStorage m_iterator;

        Iterator() = default;
        
        Iterator(ViewAllIterator iterator)
            :   m_iterator(
                    std::make_unique<IteratorHolder<ViewAllIterator, value_type>>(
                        std::move(iterator)
                    )
                ) {}
        
        Iterator(NestedCacheIterator iterator)
            :   m_iterator(
                    std::make_unique<IteratorHolder<NestedCacheIterator, value_type>>(
                        std::move(iterator)
                    )
                ) {}
        
        Iterator(const Iterator& other)
            :   m_iterator(
                    other.m_iterator != nullptr ? other.m_iterator->clone() : nullptr
                ) {}
        
        static Iterator view_all_iterator(
            CacheContainer& caches,
            ModeType iterator_mode,
            NestedCacheIterator iterator)
        {
            LOG_METHOD();
            return Iterator(
                ViewAllIterator(
                    caches, 
                    iterator_mode, 
                    std::move(iterator)
                )
            );
        }

        static Iterator view_all_iterator_end(CacheContainer& caches) {
            LOG_METHOD();
            return Iterator(
                ViewAllIterator::end(caches)
            );
        }

        Iterator& operator=(const Iterator& other) {
            LOG_METHOD();

            if (this != &other) {
                m_iterator = 
                    other.m_iterator != nullptr ? other.m_iterator->clone() : nullptr;
            }

            return *this;
        }

        Iterator(Iterator&&) noexcept = default;
        Iterator& operator=(Iterator&&) noexcept = default;

        Iterator& operator++() {
            LOG_METHOD();

            logger().check(m_iterator != nullptr, "Iterator is not initialized.");

            m_iterator->operator_pp();

            return *this;
        }

        Iterator& operator--() {
            LOG_METHOD();

            logger().check(m_iterator != nullptr, "Iterator is not initialized.");

            m_iterator->operator_mm();

            return *this;
        }

        Iterator operator++(int) {
            LOG_METHOD();

            Iterator copy = *this;
            ++(*this);

            return copy;
        }

        Iterator operator--(int) {
            LOG_METHOD();

            Iterator copy = *this;
            --(*this);

            return copy;
        }

        reference operator*() const {
            LOG_METHOD();

            logger().check(m_iterator != nullptr, "Iterator is not initialized.");

            return m_iterator->operator_deref();
        }

        bool operator==(const Iterator& iter) const {
            LOG_METHOD();

            if (!m_iterator || !iter.m_iterator) {
                return !m_iterator && !iter.m_iterator;
            }

            return m_iterator->operator_comp(*iter.m_iterator);
        }

    private:
        std::unique_ptr<IteratorInterface<value_type>> m_iterator;

        friend Iterator ModeableTrajectoryCache<Cache, Y, ModeType>::erase(Iterator first, Iterator last);

        const ViewAllIterator* as_view_all() const noexcept {
            LOG_METHOD();

            const auto* it = 
                dynamic_cast<
                    const IteratorHolder<ViewAllIterator, value_type>*
                >(m_iterator.get());

            return it != nullptr
                ? std::addressof(it->get())
                : nullptr;
        }

        ViewAllIterator* as_view_all() noexcept {
            return const_cast<ViewAllIterator*>(
                std::as_const(*this).as_view_all()
            );
        }

        const NestedCacheIterator* as_nested_iterator() const noexcept {
            LOG_METHOD();

            const auto* it = 
                dynamic_cast<
                    const IteratorHolder<NestedCacheIterator, value_type>*
                >(m_iterator.get());

            return it != nullptr
                ? std::addressof(it->get())
                : nullptr;
        }

        NestedCacheIterator* as_nested_iterator() noexcept {
            return const_cast<NestedCacheIterator*>(
                std::as_const(*this).as_nested_iterator()
            );
        }
    };
}
