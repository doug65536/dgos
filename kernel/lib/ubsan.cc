#include "ubsan.h"
#include "debug.h"
#include "export.h"
#include "printk.h"

struct source_location {
    const char *filename;
    uint32_t line;
    uint32_t column;
};

struct type_descriptor {
    /// A value from the \c Kind enumeration, specifying what flavor of type we
    /// have.
    uint16_t TypeKind;
    /// A \c Type-specific value providing information which allows us to
    /// interpret the meaning of a ValueHandle of this type.
    uint16_t TypeInfo;
    /// The name of the type follows, in a format suitable for including in
    /// diagnostics.
    char TypeName[1];
};

struct nonnull_arg_data {
    source_location loc;
    source_location attr_loc;
    int arg_index;
};

struct overflow_data;

struct type_mismatch_data_v1 {
    source_location loc;
    type_descriptor const& type;
    unsigned char log_alignment;
    unsigned char type_check_kind;
};

struct invalid_builtin_data {
    source_location loc;
    unsigned char kind;
};

struct out_of_bounds_data;
struct shift_out_of_bounds_data;
struct unreachable_data;
struct invalid_value_data;

#define __ubsan_report(...) printdbg("ubsan: " __VA_ARGS__)

extern "C"
EXPORT void __ubsan_handle_add_overflow(
        overflow_data *data, void *lhs, void *rhs)
{
    cpu_debug_break();
}

extern "C"
EXPORT void __ubsan_handle_sub_overflow(
        overflow_data *data, void *lhs, void *rhs)
{
    cpu_debug_break();
}

extern "C"
EXPORT void __ubsan_handle_mul_overflow(
        overflow_data *data, void *lhs, void *rhs)
{
    cpu_debug_break();
}

extern "C"
EXPORT void __ubsan_handle_negate_overflow(
        overflow_data *data, void *old_val)
{
    cpu_debug_break();
}


extern "C"
EXPORT void __ubsan_handle_pointer_overflow(
        overflow_data *data, void *old_val)
{
    cpu_debug_break();
}

extern "C"
EXPORT void __ubsan_handle_divrem_overflow(
        overflow_data *data, void *lhs, void *rhs)
{
    cpu_debug_break();
}

extern "C"
EXPORT void __ubsan_handle_type_mismatch(
        type_mismatch_data_v1 *data, void *ptr)
{
    cpu_debug_break();
}

static char const *__ubsan_type_check_kinds[] = {
    "load of",
    "store to",
    "reference binding to",
    "member access within",
    "member call on",
    "constructor call on",
    "downcast of",
    "downcast of",
    "upcast of",
    "cast to virtual base of",
};

extern "C"
EXPORT void __ubsan_handle_type_mismatch_v1(
        type_mismatch_data_v1 *data, void *ptr)
{
    if (ptr == nullptr) {
        __ubsan_report("Null pointer access at %s:%d,%d\n",
                 data->loc.filename, data->loc.line, data->loc.column);
    } else {
        __ubsan_report("Type mismatch in %s type \"%s\" at %s:%d,%d\n",
                 data->type_check_kind < countof(__ubsan_type_check_kinds)
                 ? __ubsan_type_check_kinds[data->type_check_kind]
                 : "<unknown kind>", data->type.TypeName,
                 data->loc.filename, data->loc.line, data->loc.column);
    }
    cpu_debug_break();
}

extern "C"
EXPORT void __ubsan_handle_out_of_bounds(
        out_of_bounds_data *data, void *index)
{
    cpu_debug_break();
}

extern "C"
EXPORT void __ubsan_handle_shift_out_of_bounds(
        shift_out_of_bounds_data *data,
					void *lhs, void *rhs)
{
    cpu_debug_break();
}

extern "C"
EXPORT void __ubsan_handle_builtin_unreachable(struct unreachable_data *data)
{
    cpu_debug_break();
}

extern "C"
EXPORT void __ubsan_handle_load_invalid_value(
        invalid_value_data *data, void *val)
{
    cpu_debug_break();
}

extern "C"
EXPORT void __ubsan_handle_nonnull_arg(struct nonnull_arg_data *data)
{
    cpu_debug_break();
}

extern "C"
EXPORT void __ubsan_handle_invalid_builtin(struct invalid_builtin_data *data)
{
    cpu_debug_break();
}
