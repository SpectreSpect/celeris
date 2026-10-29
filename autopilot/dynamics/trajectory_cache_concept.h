#pragma once

#include <type_traits>
#include <concepts>
#include <iterator>
#include <utility>
#include <ranges>

#include "clock.h"

namespace celeris {
    template<class T>
    struct decay_pair;

    template<class T1, class T2>
    struct decay_pair<std::pair<T1, T2>> {
        using type = std::pair<std::remove_cvref_t<T1>, std::remove_cvref_t<T2>>;
    };

    template<class T>
    using decay_pair_t = typename decay_pair<std::remove_cvref_t<T>>::type;

    template<class Range, class Key, class Value>
    concept pair_value_range =
        std::same_as<
            decay_pair_t<std::ranges::range_value_t<Range>>,
            decay_pair_t<std::pair<Key, Value>>
        >;
    
    template<class Iter, class Key, class Value>
    concept pair_value_iter =
        std::same_as<
            decay_pair_t<std::iter_reference_t<Iter>>,
            decay_pair_t<std::pair<Key, Value>>
        >;

    // Упорядоченный ассоциотивный контейнер
    template<class Range, class Key, class Value>
    concept ordered_associative_range = 
        std::ranges::common_range<Range> && // Можно и строже, но нижнее условие и так задаёт нужное ограничение
        pair_value_range<Range, Key, Value> &&
        requires(Range& container, const Key& key) {
            { container.lower_bound(key) } -> std::same_as<std::ranges::iterator_t<Range>>;
            { container.upper_bound(key) } -> std::same_as<std::ranges::iterator_t<Range>>;
        };
    
    // Двунапаравленный итератор, который разыминовывается в Value&
    template<class Iterator, class Value>
    concept bidirectional_dereferenceable_iterator =
        std::bidirectional_iterator<Iterator> && 
        requires(Iterator& iterator) {
            { *iterator } -> std::same_as<Value&>;
        };

    // Стираемый контейнер
    template<class Range>
    concept erasable_range =
        std::ranges::forward_range<Range> &&
        requires(
            Range& container,
            std::ranges::iterator_t<Range> first,
            std::ranges::iterator_t<Range> last)
        {
            { container.erase(first, last) } -> std::same_as<std::ranges::iterator_t<Range>>;
        };
    
    // Вставка уникальных значений
    template<class Range, class Value>
    concept unique_value_insert = 
        requires(Range& container, Value value) {
            { container.insert(std::move(value)) } 
                -> std::same_as<
                    std::pair<std::ranges::iterator_t<Range>, bool>
                >;
        };
    
    // Сборка на месте уникальных значений
    template<class Range, class Value, class... Args>
    concept unique_value_emplace = 
        std::constructible_from<Value, Args...> &&
        requires(Range& container, Args&&... args) {
            { container.emplace(std::forward<Args>(args)...) } 
                -> std::same_as<
                    std::pair<std::ranges::iterator_t<Range>, bool>
                >;
        };
    
    template<class Range, class Key, class Value>
    concept ordered_unique_key_range =
        ordered_associative_range<Range, Key, Value> &&
        std::default_initializable<Range> &&
        bidirectional_dereferenceable_iterator<
            std::ranges::iterator_t<Range>, 
            std::ranges::range_value_t<Range>
        > &&
        erasable_range<Range> &&
        unique_value_insert<Range, std::pair<Key, Value>> &&
        unique_value_emplace<Range, std::pair<Key, Value>, Key, Value>;

    template<template <class> class Cache, class Y>
    concept trajectory_cache = ordered_unique_key_range<Cache<Y>, simulation::Timestamp, Y>;
}
