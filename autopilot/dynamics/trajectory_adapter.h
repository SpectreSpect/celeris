#pragma once

#include <algorithm>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "detail/prospective_prescribed_values_view.h"
#include "invalidation_region.h"
#include "prescribed_values_concept.h"
#include "prescribed_values_view.h"
#include "trajectory_cache_concept.h"
#include "trajectory_evaluator_interface.h"
#include "trajectory_interface.h"

namespace celeris {
    /*
        Адаптер получает исключительное владение компонентами и проверяет
        совместимость исходных E и D. Начальное содержимое кэша инвалидируется.

        Заданные значения имеют приоритет. Вложенные resolve обслуживаются
        с теми же E и D. Успешно вычисленные значения можно сохранять в кэше,
        включая дочерние результаты при исключении во внешнем вычислении.

        change и replace_evaluator выполняются между запросами. При исключении
        текущие E и D сохраняются. Кэш может потерять часть записей, но все
        оставшиеся записи соответствуют текущей функции. Компоненты используют
        последовательно, а внешние ссылки на них служат только для чтения.
    */
    template<class Y, trajectory_cache<Y> Cache, prescribed_values_source<Y> Source>
    class TrajectoryAdapter final
        :   public TrajectoryInterface<Y>,
            private TrajectoryEvaluationContext<Y> {
    public:
        using Timestamp = simulation::Timestamp;
        using Evaluator = TrajectoryEvaluatorInterface<Y>;

        TrajectoryAdapter(
            std::unique_ptr<Cache> cache,
            std::unique_ptr<Source> source,
            std::unique_ptr<Evaluator> evaluator)
            :   m_cache(std::move(cache)),
                m_source(std::move(source)),
                m_view(checked_source(m_source)),
                m_evaluator(std::move(evaluator))
        {
            if (!m_cache) {
                throw std::invalid_argument("Null trajectory cache");
            }

            if (!m_evaluator || !m_evaluator->compatible(m_view)) {
                throw std::invalid_argument("Incompatible initial evaluator/source");
            }

            invalidate_all();
        }

        TrajectoryAdapter(const TrajectoryAdapter&) = delete;
        TrajectoryAdapter& operator=(const TrajectoryAdapter&) = delete;
        TrajectoryAdapter(TrajectoryAdapter&&) = delete;
        TrajectoryAdapter& operator=(TrajectoryAdapter&&) = delete;

        void change(const PrescribedValuesChange<Y>& change) {
            OperationGuard operation(m_busy);
            const detail::ProspectivePrescribedValuesView<Y, Source> prospective(*m_source, change);

            if (!m_evaluator->compatible(prospective)) {
                throw std::invalid_argument("Incompatible proposed source data");
            }

            {
                // Предикат может заимствовать данные из прежнего D.
                const auto region = m_evaluator->invalidation_region(m_view, change);
                m_cache->invalidate(std::as_const(*region));
            }

            apply_prescribed_values_change<Y>(*m_source, change);
        }

        void replace_evaluator(std::unique_ptr<Evaluator> evaluator) {
            OperationGuard operation(m_busy);

            if (!evaluator || !evaluator->compatible(m_view)) {
                throw std::invalid_argument("Incompatible replacement evaluator");
            }

            invalidate_all();
            m_evaluator = std::move(evaluator);
        }

        // Представление заимствуется у адаптера и отражает завершённые изменения D.
        [[nodiscard]]
        const PrescribedValuesViewInterface<Y>& source_view() const noexcept {
            return m_view;
        }

    private:
        using Ranked = RankedTrajectoryEvaluatorInterface<Y>;
        using Rank = typename Ranked::Rank;

        struct Frame {
            Timestamp timestamp;
            std::optional<Rank> rank;
        };

        class OperationGuard {
        public:
            explicit OperationGuard(bool& busy) : m_busy(busy) {
                if (m_busy) {
                    throw std::logic_error("A trajectory operation is already in progress");
                }

                m_busy = true;
            }

            ~OperationGuard() {
                m_busy = false;
            }

            OperationGuard(const OperationGuard&) = delete;
            OperationGuard& operator=(const OperationGuard&) = delete;

        private:
            bool& m_busy;
        };

        static const Source& checked_source(const std::unique_ptr<Source>& source) {
            if (!source) {
                throw std::invalid_argument("Null prescribed values source");
            }

            return *source;
        }

        void invalidate_all() {
            const IntervalInvalidationRegion all({TimeInterval{}});
            const InvalidationRegionInterface& region = all;
            m_cache->invalidate(region);
        }

        const PrescribedValuesViewInterface<Y>& prescribed_values() const noexcept override {
            return m_view;
        }

        Y resolve(Timestamp timestamp) override {
            return query(timestamp);
        }

        Y evaluate_impl(Timestamp timestamp) override {
            OperationGuard operation(m_busy);
            return query(timestamp);
        }

        Y query(Timestamp timestamp) {
            if (auto prescribed = m_view.find(timestamp)) {
                return std::move(*prescribed);
            }

            std::optional<Rank> rank;

            if (const auto* ranked = dynamic_cast<const Ranked*>(m_evaluator.get())) {
                rank = ranked->rank(timestamp, m_view);

                if (!m_active.empty() && *rank >= *m_active.back().rank) {
                    throw std::logic_error("Recursive trajectory rank did not decrease");
                }
            }

            if (auto value = std::as_const(*m_cache).find(timestamp)) {
                return std::move(*value);
            }

            if (!rank &&
                std::ranges::any_of(
                    m_active,
                    [timestamp](const Frame& frame) { return frame.timestamp == timestamp; }
                )) {
                throw std::logic_error("Recursive trajectory dependency cycle");
            }

            m_active.push_back({timestamp, rank});

            struct PopFrame {
                std::vector<Frame>& frames;

                ~PopFrame() {
                    frames.pop_back();
                }
            } pop{m_active};

            Y value = m_evaluator->evaluate(timestamp, *this);
            m_cache->store(timestamp, Y(value));
            return value;
        }

        std::unique_ptr<Cache> m_cache;
        std::unique_ptr<Source> m_source;
        PrescribedValuesView<Y, Source> m_view;
        std::unique_ptr<Evaluator> m_evaluator;
        std::vector<Frame> m_active;
        bool m_busy = false;
    };
}
