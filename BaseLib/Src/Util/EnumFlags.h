#pragma once

#define DEFINE_ENUM_FLAGS(_enumName_ , _enumType_) \
inline _enumName_ operator | (_enumName_ lhs, _enumName_ rhs) \
        { \
    return static_cast<_enumName_>((static_cast<_enumType_>(lhs) | static_cast<_enumType_>(rhs))); \
        } \
inline _enumName_& operator |= (_enumName_& lhs, _enumName_ rhs) \
        { \
    lhs = static_cast<_enumName_>((static_cast<_enumType_>(lhs) | static_cast<_enumType_>(rhs))); \
    return lhs; \
        } \
inline _enumName_ operator & (_enumName_ lhs, _enumName_ rhs) \
        { \
return static_cast<_enumName_>((static_cast<_enumType_>(lhs) & static_cast<_enumType_>(rhs))); \
        } \
inline _enumName_& operator &= (_enumName_& lhs, _enumName_ rhs) \
        { \
lhs = static_cast<_enumName_>((static_cast<_enumType_>(lhs) & static_cast<_enumType_>(rhs))); \
return lhs; \
        }  \
inline _enumName_ operator ~(_enumName_ hint) \
    { \
    return static_cast<_enumName_>(~static_cast<_enumType_>(hint)); \
    } \
inline bool IsFlagSet(_enumName_ hint, _enumName_ test) \
    { \
    return (static_cast<_enumType_>(hint)& static_cast<_enumType_>(test)) != 0; \
    } \
inline bool AreAllFlagsSet(_enumName_ hint, _enumName_ test) \
    { \
    return (static_cast<_enumType_>(hint)& static_cast<_enumType_>(test)) == static_cast<_enumType_>(test); \
    } \
inline void ClearFlag(_enumName_ &hint, _enumName_ clear) \
    { \
    hint &= ~clear; \
    }