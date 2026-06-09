#pragma once
#include <concepts>
#include <string_view>

// template<class T>
// concept has_class_name = requires {
//     { T::k_class_name } -> std::convertible_to<std::string_view>;
// };

template <typename T>
concept has_class_name = requires(const T& obj) {
    { obj.k_class_name() } -> std::convertible_to<std::string_view>;
};

#define _PARENT_NAME(NAME) inline virtual std::string_view k_class_name() const {return std::string_view(#NAME);}
#define _XPARENT_NAME(NAME) _PARENT_NAME(NAME)

#define _XCLASS_NAME(NAME) _PARENT_NAME(NAME)

#define _CHILD_NAME(NAME) inline virtual std::string_view k_class_name() const override {return std::string_view(#NAME);}
#define _XCHILD_NAME(NAME) _CHILD_NAME(NAME)

// В теории такой код можно было бы вынести в отельный utils для этого, но пока так...
