#pragma once

#include <deque>
#include <memory>
#include <cstddef>

#include "dynamic_interface.h"

template<class State>
struct TimedState {
    State state;
    double timestamp = 0.0;
};

template<class State>
class DynamicalSystem {
public:
    explicit DynamicalSystem(std::unique_ptr<DynamicInterface<State>> dynamics_model);

    size_t history_size() const;
    const TimedState& get_history_state(size_t state_id) const;

    void push_back(const TimedState& state);
    void push_back_delta_time(const State& state, double delta_time);
    void push_back_timestamp(const State& state, double timestamp);
    void clear_history();

    State sample_state_by_history(double timestamp = 0.0) const;

    const TimedState& simulate_for(double duration = 0.0, State initial_state = {});
    const TimedState& simulate_from_id(size_t history_id = 0, double duration = 0.0, State initial_state = {});
    const TimedState& simulate_from_timestamp(double start_timestamp = 0.0, double duration = 0.0, State initial_state = {});

private:
    std::unique_ptr<DynamicInterface<State>> m_dynamics_model;
    std::deque<TimedState> m_history;
};
