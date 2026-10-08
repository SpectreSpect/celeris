#pragma once

#include <functional>
#include <type_traits>
#include <concepts>
#include <utility>
#include <memory>
#include <map>

#include "trajectory_cache_interface.h"
#include "modeable_interface.h"

namespace celeris {
    template<class ModeType>
    concept enum_with_total_mode = 
        std::is_enum_v<ModeType> &&
        requires() {
            { ModeType::VIEW_ALL } -> std::same_as<ModeType>;
        };

    template<
        class Y,
        template<class> class CacheType, 
        class ModeType, 
        int count_modes
    >
    requires (
        enum_with_total_mode<ModeType> &&
        std::derived_from<CacheType<Y>, TrajectoryCacheInterface<Y>>,
        std::derived_from<CacheType<Y*>, TrajectoryCacheInterface<Y*>>
    )
    class ModeableTrajectoryCache
        :   public virtual TrajectoryCacheInterface<Y>,
            public virtual ModeableInterface<ModeType>
    {
    public:
        static_assert(
            count_modes >= 1,
            "The number of modes must be >= 1: there must be at least the VIEW_ALL mode."
        );

        using CacheEntryRefMutable = typename TrajectoryCacheInterface<Y>::CacheEntryRefMutable;
        using CacheEntryRefConst = typename TrajectoryCacheInterface<Y>::CacheEntryRefConst;

        using CacheFabric = std::function<CacheType<Y>()>;

        ModeableTrajectoryCache(const ModeableTrajectoryCache&) = delete;
        ModeableTrajectoryCache& operator=(const ModeableTrajectoryCache&) = delete;

        ModeableTrajectoryCache(ModeableTrajectoryCache&&) = default;
        ModeableTrajectoryCache& operator=(ModeableTrajectoryCache&&) = default;

        ModeableTrajectoryCache(
            ModeType mode_type = ModeType::VIEW_ALL,
            CacheFabric cache_fabric = [](){return CacheType<Y>()}
        )   :   m_mode(mode_type),
                m_cache_fabric(std::move(cache_fabric)),
                m_current_cache(
                    &m_mode_caches.insert(m_mode, m_cache_fabric()).first->second
                ) {}

        void set_mode(ModeType mode) override {
            if (m_mode == mode) {
                return;
            }

            m_mode = mode;

            // Инициализация режимов (странное место, но да :p)
            auto it = m_mode_caches.find(m_mode);
            if (it == m_mode_caches.end()) {
                auto [inserted_it, inserted] = m_mode_caches.insert(m_mode, m_cache_fabric());

                logger().check(
                    inserted,
                    "Failed to initialize cache."
                );

                m_current_cache = &inserted_it->second;
            } else {
                m_current_cache = &it->second;
            }
        }

        const ModeType& get_mode() const override {
            return m_mode;
        }
        
        Y& store(Timestamp timestamp, Y value) override {
            LOG_METHOD();

            logger().check(m_current_cache != nullptr, "`m_current_cache` is null.");
            logger().check(
                get_mode() != ModeType::VIEW_ALL,
                "Values ​​cannot be saved in view mode."
            );

            Y& stored_value = m_current_cache->store(timestamp, std::move(value));

            return stored_value;
        }

        void invalidate(TimestampBound left_bound, TimestampBound right_bound) override {
            LOG_METHOD();

            logger().check(m_current_cache != nullptr, "`m_current_cache` is null.");

            if (get_mode() != ModeType::VIEW_ALL) {
                m_current_cache->invalidate(left_bound, right_bound);
            } else {
                for (auto& [mode, cache] : m_mode_caches) {
                    cache.invalidate(left_bound, right_bound);
                }
            }
        }

        std::optional<CacheEntryRefConst> find_cached_value_at_or_before(Timestamp timestamp) const {
            LOG_METHOD();

            logger().check(m_current_cache != nullptr, "`m_current_cache` is null.");

            if (get_mode() != ModeType::VIEW_ALL) {
                return m_current_cache->find_cached_value_at_or_before(timestamp);
            } else {
                std::optional<CacheEntryRefConst> closest = std::nullopt;

                for (auto& [mode, cache] : m_mode_caches) {
                    std::optional<CacheEntryRefConst> result = cache.find_cached_value_at_or_before(timestamp);

                    logger().check(
                        result->timestamp <= timestamp, 
                        "`result->timestamp` must be less or equal to `timestamp`"
                    )

                    if (!closest.has_value() || (result.has_value() && result->timestamp > closest->timestamp)) {
                        closest = std::move(result);
                    }
                }

                return closest;
            }
        }

        std::optional<CacheEntryRefMutable> find_cached_value_at_or_before(Timestamp timestamp) {
            LOG_METHOD();

            std::optional<CacheEntryRefConst> entry =
                std::as_const(*this).find_cached_value_at_or_before(timestamp);
            
            if (!entry.has_value()) {
                return std::nullopt;
            }
            
            return CacheEntryRefMutable{
                .timestamp = entry->timestamp,
                .value = const_cast<Y&>(entry->value.get())
            }
        }

    private:
        ModeType m_mode;
        CacheFabric<Y> m_cache_fabric;
        std::map<ModeType, CacheType<Y>> m_mode_caches;
        typename CacheType<Y>* m_current_cache = nullptr;
    };
}
