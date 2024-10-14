#pragma once

#include <optional>

// CRTP base class for RAII scoped values. This handles the basic RAII pattern,
// including move semantics. The derived class must implement the following
// static methods:
// - bool IsValidImpl(const T & value) - returns true if the value is valid
// - void CleanupValue(T && value) - cleans up the value when the scope is exited.
template <class Derived, class T>
class ScopedValue
{
public:
    ScopedValue() = default;
    explicit ScopedValue(T value)
    {
        if (Derived::IsValidImpl(value))
        {
            value_ = std::move(value);
        }
    }
    ~ScopedValue()
    {
        if (IsValid())
        {
            Derived::CleanupValue(std::move(value_).value());
        }
    }

    ScopedValue(const ScopedValue&) = delete;
    ScopedValue(ScopedValue&& other) noexcept {
        std::swap(value_, other.value_);
    }
    ScopedValue& operator=(const ScopedValue&) = delete;
    ScopedValue& operator=(ScopedValue&& other) noexcept
    {
        std::swap(value_, other.value_);
        return *this;
    }

    const T& GetValue() const { return *value_; }

    explicit operator bool() const { return IsValid(); }

    bool IsValid() const { return value_.has_value(); }

private:
    std::optional<T> value_;
};