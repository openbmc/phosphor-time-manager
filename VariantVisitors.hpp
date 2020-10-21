#pragma once
#include <boost/asio.hpp>
#include <stdexcept>
#include <string>
#include <variant>

struct VariantToFloatVisitor
{

    template <typename T>
    float operator()(const T& t) const
    {
        if constexpr (std::is_arithmetic_v<T>)
        {
            return static_cast<float>(t);
        }
        throw std::invalid_argument("Cannot translate type to float");
    }
};

struct VariantToIntVisitor
{
    template <typename T>
    int operator()(const T& t) const
    {
        if constexpr (std::is_arithmetic_v<T>)
        {
            return static_cast<int>(t);
        }
        throw std::invalid_argument("Cannot translate type to int");
    }
};

struct VariantToUnsignedIntVisitor
{
    template <typename T>
    unsigned int operator()(const T& t) const
    {
        if constexpr (std::is_arithmetic_v<T>)
        {
            return static_cast<unsigned int>(t);
        }
        throw std::invalid_argument("Cannot translate type to unsigned int");
    }
};

struct VariantToStringVisitor
{
    template <typename T>
    std::string operator()(const T& t) const
    {
        if constexpr (std::is_same_v<T, std::string>)
        {
            return t;
        }
        else if constexpr (std::is_arithmetic_v<T>)
        {
            return std::to_string(t);
        }
        throw std::invalid_argument("Cannot translate type to string");
    }
};

struct VariantToDoubleVisitor
{
    template <typename T>
    double operator()(const T& t) const
    {
        if constexpr (std::is_arithmetic_v<T>)
        {
            return static_cast<double>(t);
        }
        throw std::invalid_argument("Cannot translate type to double");
    }
};
