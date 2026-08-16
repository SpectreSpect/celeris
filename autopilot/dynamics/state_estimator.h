#pragma once

#include <memory>
#include <deque>
#include <type_traits>
#include <concepts>
#include <optional>
#include <cstddef>

#include "dynamic_interface.h"
#include "../../vulkan_self/logger/logger_header.h"

template<class State>
struct StateEstimate {
    State state;
    double timestamp;
    // В теории сюда можно добавить информацию о "достоверности" состояния...
};

template<class Control>
struct TimedControl {
    Control control;
    double timestamp;
};

struct MeasurementBase {
    double timestamp;

    MeasurementBase(double timestamp_) : timestamp(timestamp_) {}
};

template<class Measurement, class State>
class CorrectionModel {
public:
    virtual ~CorrectionModel() noexcept = default;

    virtual void correct_state(const Measurement& measurement, StateEstimate<State>& state) const = 0;
};

template<class State, class Control>
class StateEstimator {
public:
    _XPARENT_NAME(StateEstimator);

    explicit StateEstimator(std::unique_ptr<DynamicInterface<State>> propagator);

    const StateEstimate<State>& current_state() const;
    /*
        Возможно для внутренней поддержки таких операций стоит
        действительно сделать отдельные контейнеры для истории вместо std::deque...
    */
    std::optional<size_t> find_state_at_or_before(double timestamp) const;
    std::optional<size_t> find_control_at_or_before(double timestamp) const;

    void rewind_to_state(size_t state_index);
    void rewind_to_state_at_or_before(double timestamp);

    void predict_for(const Control& control, double duration);
    void predict_until(double timestamp); // Предсказывает по командам управления из истории

    /*
        Не занимается предсказанием, а чисто записывает управляющую команду
        в момент control.timestamp. Изначально я ошибочно предполагал, что
        control.timestamp мы будем задавать время, до которого будем предсказывать,
        но ведь control.timestamp - это время, когда пришла команда, и до этого
        момента она ещё не действовала.
    */
    void record_control(const TimedControl<Control>& control);

    template<class Measurement>
    requires (std::derived_from<Measurement, MeasurementBase>)
    void correct(const Measurement& measurement, const CorrectionModel<Measurement, State>& correction_model) {
        LOG_METHOD();

        std::optional<size_t> prev_state_idx = find_state_at_or_before(measurement.timestamp);
        logger.check(prev_state_idx.has_value())
            << "There are no states prior to the timestamp."
            << "The system must ensure `initial_state(timestamp = 0.0)`."
            << "Here, this invariant is violated.";

        double previous_current_time = m_state_history.back().timestamp;

        // Стираем все состояния, у которых idx > *prev_state_idx
        rewind_to_state(*prev_state_idx);

        // Узнаём предсказанное состояние в момент измерения
        predict_until(measurement.timestamp);

        // Корректируем состояние в момент измерения
        correction_model.correct_state(measurement, m_state_history.back());

        /*
            Восстанавлием стёртые состояния с учётом корректировки.
            
            #TODO. Необходимо добавить историю не только команд управления, но и историю измерений,
            чтобы была возможность корректно восстанавливать историю без потерь.
        */
        if (previous_current_time > measurement.timestamp) {
            predict_until(previous_current_time);
        }
    }

private:
    std::unique_ptr<DynamicInterface<State>> m_propagator;
    std::deque<StateEstimate<State>> m_state_history;
    std::deque<TimedControl<Control>> m_control_history;
};
