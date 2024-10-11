// An implementation of abstract containers and iterators.

#pragma once

#include <optional>
#include <memory>

// An iterator that produces values of T.
//
// Note that this is the _exact_ value type, not a reference or pointer to it.
// If a reference or pointer is desired, then that should be part of the value type.
template <class T>
class IteratorImpl
{
public:
    virtual ~IteratorImpl() = default;

    [[nodiscard]] virtual bool AtEnd() const = 0;
    virtual void Next() = 0;
    [[nodiscard]] virtual T GetCurrent() const = 0;
};

template <class T>
class ContainerImpl
{
public:
    virtual ~ContainerImpl() = default;

    [[nodiscard]] virtual std::optional<std::size_t> GetSizeHint() const { return std::nullopt; }
    [[nodiscard]] virtual std::unique_ptr<IteratorImpl<T>> CreateIterator() = 0;
};

class IteratorSentinelT
{
public:
    constexpr IteratorSentinelT() = default;
};

constexpr auto IteratorSentinel = IteratorSentinelT();

template <class T>
class Iterator
{
public:
    Iterator() = default;
    explicit Iterator(std::shared_ptr<IteratorImpl<T>> impl) : impl_(std::move(impl)) {}

    Iterator(const Iterator&) = delete;
    Iterator(Iterator&&) = default;
    Iterator& operator=(const Iterator&) = delete;
    Iterator& operator=(Iterator&&) = default;

    T operator*() const { return impl_->GetCurrent(); }
    bool operator==(const IteratorSentinelT&) const { return impl_->AtEnd(); }
    bool operator!=(const IteratorSentinelT&) const { return !impl_->AtEnd(); }
    Iterator& operator++() { impl_->Next(); return *this; }

private:
    std::shared_ptr<IteratorImpl<T>> impl_;
};

template <class T>
class Container
{
public:
    Container() = default;

    explicit Container(std::shared_ptr<ContainerImpl<T>> impl) : impl_(std::move(impl)) {}

    [[nodiscard]] Iterator<T> begin() const { return Iterator<T>(impl_->CreateIterator()); }
    // ReSharper disable once CppMemberFunctionMayBeStatic
    [[nodiscard]] IteratorSentinelT end() const { return IteratorSentinel; }
private:
    std::shared_ptr<ContainerImpl<T>> impl_;
};

template <class T>
Container(std::shared_ptr<ContainerImpl<T>>) -> Container<T>;