#pragma once

#ifndef AR_NO_LITERAL_PREFIX
#define AR_LITERAL_NAME(name) _##name
#else
#define AR_LITERAL_NAME(name) ##name
#endif

#define DEFINED_LITERAL(name) operator"" AR_LITERAL_NAME(name)

#include "types.h"

namespace ar::literal
{
    consteval u8 DEFINED_LITERAL(u8)(unsigned long long val_)
    {
        return static_cast<u8>(val_);
    }

    consteval u16 DEFINED_LITERAL(u16)(unsigned long long val_)
    {
        return static_cast<u16>(val_);
    }

    consteval u32 DEFINED_LITERAL(u32)(unsigned long long val_)
    {
        return static_cast<u32>(val_);
    }

    consteval u64 DEFINED_LITERAL(u64)(unsigned long long val_)
    {
        return val_;
    }

    consteval i8 DEFINED_LITERAL(i8)(unsigned long long val_)
    {
        return static_cast<i8>(val_);
    }

    consteval i16 DEFINED_LITERAL(i16)(unsigned long long val_)
    {
        return static_cast<i16>(val_);
    }

    consteval i32 DEFINED_LITERAL(i32)(unsigned long long val_)
    {
        return static_cast<i32>(val_);
    }

    consteval i64 DEFINED_LITERAL(i64)(unsigned long long val_)
    {
        return static_cast<i64>(val_);
    }

    consteval f32 DEFINED_LITERAL(f32)(long double val_)
    {
        return static_cast<f32>(val_);
    }

    consteval f64 DEFINED_LITERAL(f64)(long double val_)
    {
        return static_cast<f64>(val_);
    }

    consteval usize DEFINED_LITERAL(us)(unsigned long long val_)
    {
        return val_;
    }

    consteval isize DEFINED_LITERAL(is)(unsigned long long val_)
    {
        return static_cast<isize>(val_);
    }
}

using namespace ar::literal;
using namespace std::literals;
using namespace std::chrono_literals;