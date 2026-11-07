#pragma once

#include <ranges>
#include <chrono>
#include <utility>
#include <stdexcept>

#include "../../vulkan_self/logger/logger_header.h"
#include "../../autopilot/dynamics/trajectory_cache.h"

namespace celeris::tests {
    template<template<class> class Cache, class Y>
    consteval bool diagnose_trajectory_cache() {
        using C = Cache<Y>;
        using Key = simulation::Timestamp;
        using Entry = std::pair<Key, Y>;

        static_assert(
            std::default_initializable<C>,
            "Cache must be default-initializable"
        );

        static_assert(
            ordered_associative_range<C, Key, Y>,
            "Cache does not satisfy ordered_associative_range"
        );

        static_assert(
            erasable_range<C>,
            "Cache does not satisfy erasable_range"
        );

        static_assert(
            unique_value_insert<C, Entry>,
            "Cache does not satisfy unique_value_insert"
        );

        static_assert(
            unique_value_emplace<C, Entry, Key, Y>,
            "Cache does not satisfy unique_value_emplace"
        );

        if constexpr (std::ranges::input_range<C>) {
            using I = std::ranges::iterator_t<C>;
            using V = std::ranges::range_value_t<C>;

            static_assert(
                bidirectional_dereferenceable_iterator<I, V>,
                "Cache iterator must be bidirectional and yield V&"
            );
        }

        static_assert(
            trajectory_cache<Cache, Y>,
            "Cache does not satisfy trajectory_cache"
        );

        return true;
    }

    template<template<class> class Cache, class Y>
    requires(trajectory_cache<Cache, Y>)
    void test_empty_cache(Cache<Y>& cache) {
        LOG_METHOD();

        logger().check(
            std::ranges::begin(cache) == std::ranges::begin(cache),
            "A subsequent call to begin() must return the equal value"
        );

        logger().check(
            std::ranges::end(cache) == std::ranges::end(cache),
            "A subsequent call to end() must return the equal value"
        );

        logger().check(
            std::ranges::begin(cache) == std::ranges::end(cache),
            "In an empty cache, begin() should be equal to end()"
        );
    }

    void test_insert() {
        
    }

    void test_emplace() {

    }

    template<template<class> class Cache>
    requires(trajectory_cache<Cache, int>)
    void test_insert_duplicate(Cache<int>& cache, simulation::Timestamp timestamp) {
        LOG_METHOD();

        logger().check(
            std::ranges::begin(cache) == std::ranges::end(cache),
            "Test requires an empty cache"
        );

        {
            auto [it, inserted] = cache.insert(std::pair{timestamp, 10});

            logger().check(inserted, "First insertion must succeed");
            logger().check(it != std::ranges::end(cache), "Expected an entry");
            logger().check((*it).first == timestamp, "Wrong timestamp");
            logger().check((*it).second == 10, "Wrong inserted value");
        }

        auto [it, inserted] = cache.insert(std::pair{timestamp, 20});

        logger().check(!inserted, "Duplicate insertion must return false");
        logger().check(it != std::ranges::end(cache), "Expected the existing entry");
        logger().check((*it).first == timestamp, "Wrong existing timestamp");
        logger().check((*it).second == 10, "Duplicate insertion changed the value");

        logger().check(it == std::ranges::begin(cache), "Wrong returned iterator");
        ++it;
        logger().check(it == std::ranges::end(cache), "Expected exactly one entry");
    }

    template<template<class> class Cache>
    requires(trajectory_cache<Cache, int>)
    void test_boundary_search(Cache<int>& cache) {
        LOG_METHOD();

        logger().check(
            std::ranges::begin(cache) == std::ranges::end(cache),
            "Test requires an empty cache"
        );

        simulation::Timestamp first_timestamp{std::chrono::seconds{5}};
        simulation::Timestamp second_timestamp{std::chrono::seconds{15}};

        auto [first_it, first_inserted] = cache.insert(std::pair{first_timestamp, 10});
        logger().check(first_inserted, "First insertion must succeed");
        logger().check(first_it != std::ranges::end(cache), "Expected an entry");
        logger().check((*first_it).first == first_timestamp, "Wrong timestamp");
        logger().check((*first_it).second == 10, "Wrong inserted value");

        auto [second_it, second_inserted] = cache.insert(std::pair{second_timestamp, 20});
        logger().check(second_inserted, "Second insertion must succeed");
        logger().check(second_it != std::ranges::end(cache), "Expected an entry");
        logger().check((*second_it).first == second_timestamp, "Wrong timestamp");
        logger().check((*second_it).second == 10, "Wrong inserted value");

        simulation::Timestamp before_first{std::chrono::seconds{2}};
        simulation::Timestamp at_first = first_timestamp;
        simulation::Timestamp between_first_and_second{std::chrono::seconds{8}};
        simulation::Timestamp after_last{std::chrono::seconds{20}};

        // Поиск до первого элемента
        {
            auto lower_bound_it = cache.lower_bound(before_first);
            logger().check(lower_bound_it == std::ranges::begin(cache), "begin() was expected");
            logger().check(
                lower_bound_it == first_it,
                "The iterator must match begin(), and the latter must match first_it"
            );

            auto upper_bound_it = cache.upper_bound(before_first);
            logger().check(upper_bound_it == std::ranges::begin(cache), "begin() was expected");
            logger().check(
                upper_bound_it == first_it, 
                "The iterator must match begin(), and the latter must match first_it"
            );

            logger().check(lower_bound_it == upper_bound_it, "Both should point to begin()");
        }
        
        // Поиск в точке первого элемента
        {
            auto lower_bound_it = cache.lower_bound(at_first);
            logger().check(lower_bound_it == first_it, "Must be equal to `first_it`");

            auto upper_bound_it = cache.upper_bound(at_first);
            logger().check(upper_bound_it == second_it, "Must be equal to `second_it`");

            logger().check(lower_bound_it != upper_bound_it, "Must not be equal");
        }

        // Поиск между первым и последним элементами
        {
            auto lower_bound_it = cache.lower_bound(between_first_and_second);
            logger().check(lower_bound_it == second_it, "Must be equal to `second_it`");

            auto upper_bound_it = cache.lower_bound(between_first_and_second);
            logger().check(upper_bound_it == second_it, "Must be equal to `second_it`");

            logger().check(lower_bound_it == upper_bound_it, "Must be equal");
        }

        // Поиск после последнего элемента
        {
            auto lower_bound_it = cache.lower_bound(after_last);
            logger().check(lower_bound_it == std::ranges::end(cache), "end() was expected");

            auto upper_bound_it = cache.lower_bound(after_last);
            logger().check(upper_bound_it == std::ranges::end(cache), "end() was expected");

            logger().check(lower_bound_it == upper_bound_it, "Must be equal");
        }
    }

    void test_bidirectional_iterator() {

    }

    void test_erase() {

    }

    void test_case_1() {

    }
}
