#pragma once

#include <concepts>
#include <functional>
#include <map>
#include <span>
#include <stdexcept>
#include <utility>

#include "../clock.h"

namespace celeris::tests {
    class ContractViolation : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

    inline void require(bool condition, const char* message) {
        if (!condition) {
            throw ContractViolation(message);
        }
    }

    template<class Exception, class Function>
    void expect_exception(Function function, const char* message) {
        try {
            function();
        } catch (const Exception&) {
            return;
        }

        throw ContractViolation(message);
    }

    /*
        Общие входные данные набора: >=3 различных ключа, первые два значения
        различаются по equivalent (иначе тест замены ничего не различает).
        Factory в наборах создаёт свежий логически пустой компонент. Это
        требование fixture-а, а не требование default-конструктора компонента.
    */
    template<class Y, class Equivalent>
    void validate_samples(
        std::span<const std::pair<simulation::Timestamp, Y>> samples,
        Equivalent equivalent)
    {
        if (samples.size() < 3) {
            throw std::invalid_argument("Expected at least three sample entries");
        }

        std::map<simulation::Timestamp, Y> unique(samples.begin(), samples.end());

        if (unique.size() != samples.size() ||
            std::invoke(equivalent, samples[0].second, samples[1].second)) {
            throw std::invalid_argument("Expected distinct keys and different replacement values");
        }
    }
}
