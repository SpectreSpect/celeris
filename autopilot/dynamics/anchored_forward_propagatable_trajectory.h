#pragma once

#include <functional>
#include <utility>
#include <memory>

#include "../../vulkan_self/logger/logger_header.h"
#include "cached_forward_propagatable_trajectory.h"
#include "cached_trajectory_interface.h"

#include "modeable_interface.h"
#include "clock.h"


namespace celeris {

    // Блин, офигенную штуку придумал. Этой штуке вообще пофиг на дерево наследования!
    template<class Y>
    class ModeableCacheTrajectory {
    public: 
        template<class Trajectory>
        ModeableCacheTrajectory(Trajectory trajectory)
            :   m_trajectory(std::move(trajectory)),
                m_cached_trajectory_ref(m_trajectory),
                m_modeable_ref(m_trajectory) {}
    
        CachedTrajectoryInterface<Y>& cached_trajectory_interface() {
            return m_cached_trajectory_ref.get();
        }

        const CachedTrajectoryInterface<Y>& cached_trajectory_interface() const {
            return m_cached_trajectory_ref.get();
        }

        ModeableCacheTrajectory<Y>& modeable_interface() {
            return m_modeable_ref.get();
        }

        const ModeableCacheTrajectory<Y>& modeable_interface() const {
            return m_modeable_ref.get();
        }

    private:
        std::unique_ptr<CachedTrajectoryInterface<Y>> m_trajectory;

        std::reference_wrapper<CachedTrajectoryInterface<Y>> m_cached_trajectory_ref; // Не обязательно, но для понятности
        std::reference_wrapper<ModeableCacheTrajectory<Y>> m_modeable_ref;
    };

    template<class Y>
    class AnchoredForwardPropagatableTrajectory : public CachedForwardPropagatableTrajectory<Y> {
    public:
        _XCHILD_NAME(AnchoredForwardPropagatableTrajectory);
        
        using Timestamp = typename CachedForwardPropagatableTrajectory<Y>::Timestamp;

        template<class Value>
        using CachePtr = std::unique_ptr<TrajectoryCacheInterface<Value>>;
        using CacheFabric = std::function<std::unique_ptr<TrajectoryCacheInterface<Y>>()>;
        using SplitterPtr = std::unique_ptr<TrajectorySplitterInterface<Y>>;

        struct AnchorContainer {
            Timestamp anchor_timestamp;

            /*
                Указатель, чтобы не рассчитывать на стабильность
                ссылок значений контейнера.
            */
            Y* anchor_value = nullptr;

            CachePtr<Y> contained_values;
        };

        using CacheEntryRefMutableAch = typename TrajectoryCacheInterface<AnchorContainer>::CacheEntryRefMutable;

        AnchoredForwardPropagatableTrajectory(
            Timestamp initial_value_timestamp,
            Y initial_value,
            CachePtr<AnchorContainer> anchor_values_cache, // Неужели его обязательно передавать?
            CacheFabric cache_fabric,
            SplitterPtr splitter
        )   :   CachedForwardPropagatableTrajectory<Y>(
                    initial_value_timestamp,
                    std::move(initial_value),
                    std::move(cache_fabric()),
                    std::move(splitter)
                ),
                m_cache_fabric(std::move(cache_fabric)),
                m_anchor_values_cache(std::move(anchor_values_cache)) {}

        AnchoredForwardPropagatableTrajectory(const AnchoredForwardPropagatableTrajectory&) = delete;
        AnchoredForwardPropagatableTrajectory& operator=(const AnchoredForwardPropagatableTrajectory&) = delete;
    
        AnchoredForwardPropagatableTrajectory(AnchoredForwardPropagatableTrajectory&&) = default;
        AnchoredForwardPropagatableTrajectory& operator=(AnchoredForwardPropagatableTrajectory&&) = default;

    protected:
        /*
            Определяет расположение якорей.

            P. S. Возможно лучше использовать отдельный компонент. Пока так. #TODO
        */
        virtual Timestamp find_anchor_timestamp_at_or_before(Timestamp timestamp) = 0;

        CacheEntryRefMutableAch find_anchor_at_or_before_safe(Timestamp timestamp) {
            LOG_METHOD();

            logger().check(m_anchor_values_cache != nullptr, "`m_anchor_values_cache` is null.");
            logger().check(
                timestamp >= this->initial_value_timestamp(), 
                "`timestamp` cannot preceed the initial value timestamp."
            );

            std::optional<CacheEntryRefMutableAch> anchor_container_entry =
                m_anchor_values_cache->find_cached_value_at_or_before(timestamp);
            
            if (anchor_container_entry.has_value()) {
                return *anchor_container_entry;
            } else {
                // Нет якоря для initial_state
                Timestamp anchor_container_timestamp = this->initial_value_timestamp();
                
                AnchorContainer initial_anchor_container = AnchorContainer{
                    .anchor_timestamp = anchor_container_timestamp,
                    .anchor_value = this->initial_value(),
                    .contained_values = m_cache_fabric()
                };

                AnchorContainer& stored_container = &m_anchor_values_cache->store(
                    anchor_container_timestamp,
                    std::move(initial_anchor_container)
                );

                return CacheEntryRefMutableAch{
                    .timestamp = anchor_container_timestamp,
                    .value = stored_container
                };
            }
        }

        Y& store_uncheked_mutable(Timestamp timestamp, Y value) override {
            LOG_METHOD();

            logger().check(m_anchor_values_cache != nullptr, "`m_anchor_values_cache` is null.");
            logger().check(
                timestamp >= this->initial_value_timestamp(), 
                "`timestamp` cannot preceed the initial value timestamp."
            );

            Timestamp anchor_timestamp = find_anchor_timestamp_at_or_before(timestamp);

            logger().check(
                anchor_timestamp >= this->initial_value_timestamp(), 
                "`anchor_timestamp` cannot preceed the initial value timestamp."
            );

            CacheEntryRefMutableAch anchor_container_entry = find_anchor_at_or_before_safe(anchor_timestamp);

            if (anchor_container_entry.timestamp != anchor_timestamp) {
                // Контейнера для этого якоря ещё нет. Нужно создать новый.

                AnchorContainer new_anchor_container = AnchorContainer{
                    .anchor_timestamp = anchor_timestamp,
                    .anchor_value = anchor_value,
                    .contained_values = m_cache_fabric()
                };

                m_anchor_values_cache->store(
                    timestamp,
                    std::move(new_anchor_container)
                );
            }


            if (anchor_timestamp == timestamp) {
                // Попытка сохранения значения *в точке якоря*

                Y& anchor_value = CachedForwardPropagatableTrajectory<Y>::store_uncheked_mutable(
                    timestamp, 
                    std::move(value) // Вроде в этой ветке нигде больше не используется :/
                );

                CacheEntryRefMutableAch anchor_container_entry = find_anchor_at_or_before_safe(timestamp);
                
                if (anchor_container_entry.timestamp == timestamp) {
                    /*
                        Контейнер для этого якоря уже есть. Проверим его корректность, и,
                        для надёжности, обновим значение указателя.
                    */

                    AnchorContainer& container = anchor_container_entry.value.get();

                    logger().check(
                        container.anchor_timestamp == timestamp, 
                        "Mismatch between the anchor container and the anchor timing."
                    );
                    logger().check(
                        container.anchor_value != nullptr, 
                        "`container.anchor_value` is null."
                    );

                    // На всякий случай - если ссылка у контейнера не стабильна.
                    container.anchor_value = &anchor_value;
                } else {
                    // Контейнера для этого якоря ещё нет. Нужно создать новый.
                    AnchorContainer new_anchor_container = AnchorContainer{
                        .anchor_timestamp = timestamp,
                        .anchor_value = anchor_value,
                        .contained_values = m_cache_fabric()
                    };

                    m_anchor_values_cache->store(
                        timestamp,
                        std::move(new_anchor_container)
                    );
                }
            } else {
                // Попытка сохранения значения *внутри якоря*

                CacheEntryRefMutableAch anchor_container_entry = find_anchor_at_or_before_safe(timestamp);

                
            }

        template<class ValueType>
        struct CacheEntryRef {
            Timestamp timestamp;
            std::reference_wrapper<ValueType> value;
        };



            logger().check(m_cache != nullptr, "`m_cache_component` is null.");
            logger().check(
                timestamp >= this->initial_value_timestamp(), 
                "`timestamp` cannot preceed the initial value timestamp."
            );

            return m_cache->store(timestamp, std::move(value));
        }

    private:
        CacheFabric m_cache_fabric;
        CachePtr<AnchorContainer> m_anchor_values_cache;
    };
}
