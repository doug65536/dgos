#pragma once

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x)   __builtin_expect(!!(x), 0)

// Doesn't appear to actually do anything
// This ought to encourage a branch free alternative
#define unpredictable(x) __builtin_expect_with_probability(!!(x), 0, 0.5)
