/***************************************************************************
    Copyright (c) 2024 Brian Chin

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
***************************************************************************/

template <class C>
class FunctionRef
{
};

template <class RetVal, class... Args>
class FunctionRef<RetVal(Args...)>
{
private:
    class Caller
    {
    public:
        virtual ~Caller() = default;
        virtual RetVal Call(void* callee, Args... args) = 0;
    };
    template <class T>
    class CallerImpl : public Caller {
    public:
        static CallerImpl* GetInstance() {
            static CallerImpl instance;
            return &instance;
        }
        constexpr CallerImpl() = default;
        RetVal Call(void* callee, Args... args) override {
            return (*reinterpret_cast<T*>(callee))(std::move(args)...);
        }
    };

public:
    FunctionRef() : callee_(nullptr), caller_(nullptr) {}

    FunctionRef(const FunctionRef& other) = default;

    template <class T>
    FunctionRef(T& callee) : callee_(&callee), caller_(CallerImpl<T>::GetInstance()) {}

    RetVal operator()(Args... args) {
        return caller_->Call(callee_, std::move(args)...);
    }

private:
    void* callee_;
    Caller* caller_;
};