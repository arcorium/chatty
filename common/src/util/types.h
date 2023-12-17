#pragma once

#include <cstdint>

using u8 = uint8_t;
using byte = u8;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using usize = uintmax_t;

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using isize = intmax_t;

using f32 = float;
using f64 = double;

static_assert((sizeof(u8) & sizeof(byte) & sizeof(i8)) == 1);
static_assert((sizeof(u16) & sizeof(i16)) == 2);
static_assert((sizeof(u32) & sizeof(i32)) == 4);
static_assert((sizeof(u64) & sizeof(i64)) == 8);
static_assert((sizeof(usize) & sizeof(isize)) == sizeof(u64));

static_assert(sizeof(f32) == 4);
static_assert(sizeof(f64) == 8);
