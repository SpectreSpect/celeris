#pragma once

#include <type_traits>
#include <concepts>
#include <iterator>
#include <utility>
#include <memory>
#include <ranges>

#include "../../vulkan_self/logger/logger_header.h"

template<class Value>
struct IteratorInterface {
    _XPARENT_NAME(IteratorInterface);

    virtual ~IteratorInterface() = default;

    virtual IteratorInterface& operator_pp() = 0;
    virtual IteratorInterface& operator_mm() = 0;
    virtual Value& operator_deref() const = 0;
    virtual bool operator_comp(const IteratorInterface<Value>& iter) const = 0;
    virtual std::unique_ptr<IteratorInterface<Value>> clone() const = 0;
};

template<class Iterator, class Value>
requires(
    std::bidirectional_iterator<Iterator> &&
    std::same_as<std::iter_value_t<Iterator>, Value> &&
    std::same_as<std::iter_reference_t<Iterator>, Value&>
)
struct IteratorHolder : public IteratorInterface<Value> {
    _XCHILD_NAME(IteratorHolder);

    IteratorHolder(Iterator iterator)
        :   m_iterator(std::move(iterator)) {}
    
    IteratorInterface<Value>& operator_pp() override {
        LOG_METHOD();
        
        ++m_iterator;
        
        return *this;
    }

    IteratorInterface<Value>& operator_mm() override {
        LOG_METHOD();

        --m_iterator;

        return *this;
    }

    Value& operator_deref() const override {
        LOG_METHOD();

        return *m_iterator;
    }

    bool operator_comp(const IteratorInterface<Value>& iter) const override {
        LOG_METHOD();

        const auto* other = dynamic_cast<const IteratorHolder<Iterator, Value>*>(&iter);
        
        return other != nullptr && m_iterator == other->m_iterator;
    }

    std::unique_ptr<IteratorInterface<Value>> clone() const override {
        LOG_METHOD();

        return std::make_unique<IteratorHolder<Iterator, Value>>(m_iterator);
    }

    const Iterator& get() const noexcept {
        return m_iterator;
    }

private:
    Iterator m_iterator;
};
