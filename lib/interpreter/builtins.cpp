/*
 * Eshkol Interpreter — Built-in Functions
 */
#include "interpreter.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <string>

// Helper macros
#define REQUIRE_ARGS(fname, cnt) \
    if (n != (cnt)) { \
        char buf[128]; \
        snprintf(buf, sizeof(buf), "%s: expected %d arguments, got %llu", \
                 fname, (int)(cnt), (unsigned long long)n); \
        return interp_make_error(ctx, buf); \
    }

#define REQUIRE_NUMBER(fname, val) \
    if (!(val) || ((val)->type != INTERP_VAL_INT && (val)->type != INTERP_VAL_DOUBLE)) { \
        char buf[128]; \
        snprintf(buf, sizeof(buf), "%s: expected number, got %s", \
                 fname, interp_val_type_name(val)); \
        return interp_make_error(ctx, buf); \
    }

static bool is_number(const interp_val_t* v) {
    return v && (v->type == INTERP_VAL_INT || v->type == INTERP_VAL_DOUBLE);
}

static double as_double(const interp_val_t* v) {
    if (v->type == INTERP_VAL_INT) return (double)v->int_val;
    return v->double_val;
}

// ═══════════════════════════════════════════════════════════════════════════
// Arithmetic
// ═══════════════════════════════════════════════════════════════════════════

static interp_val_t* builtin_add(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    bool has_d = false;
    for (uint64_t i = 0; i < n; i++) {
        REQUIRE_NUMBER("+", args[i]);
        if (args[i]->type == INTERP_VAL_DOUBLE) has_d = true;
    }
    if (has_d) {
        double acc = 0;
        for (uint64_t i = 0; i < n; i++) acc += as_double(args[i]);
        return interp_make_double(ctx, acc);
    }
    int64_t acc = 0;
    for (uint64_t i = 0; i < n; i++) acc += args[i]->int_val;
    return interp_make_int(ctx, acc);
}

static interp_val_t* builtin_sub(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    if (n == 0) return interp_make_int(ctx, 0);
    for (uint64_t i = 0; i < n; i++) REQUIRE_NUMBER("-", args[i]);
    bool has_d = false;
    for (uint64_t i = 0; i < n; i++) if (args[i]->type == INTERP_VAL_DOUBLE) has_d = true;
    if (n == 1) return has_d ? interp_make_double(ctx, -as_double(args[0]))
                             : interp_make_int(ctx, -args[0]->int_val);
    if (has_d) {
        double acc = as_double(args[0]);
        for (uint64_t i = 1; i < n; i++) acc -= as_double(args[i]);
        return interp_make_double(ctx, acc);
    }
    int64_t acc = args[0]->int_val;
    for (uint64_t i = 1; i < n; i++) acc -= args[i]->int_val;
    return interp_make_int(ctx, acc);
}

static interp_val_t* builtin_mul(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    bool has_d = false;
    for (uint64_t i = 0; i < n; i++) {
        REQUIRE_NUMBER("*", args[i]);
        if (args[i]->type == INTERP_VAL_DOUBLE) has_d = true;
    }
    if (has_d) {
        double acc = 1;
        for (uint64_t i = 0; i < n; i++) acc *= as_double(args[i]);
        return interp_make_double(ctx, acc);
    }
    int64_t acc = 1;
    for (uint64_t i = 0; i < n; i++) acc *= args[i]->int_val;
    return interp_make_int(ctx, acc);
}

static interp_val_t* builtin_div(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    if (n == 0) return interp_make_error(ctx, "/: need at least 1 argument");
    for (uint64_t i = 0; i < n; i++) REQUIRE_NUMBER("/", args[i]);
    double acc = as_double(args[0]);
    if (n == 1) {
        if (acc == 0) return interp_make_error(ctx, "/: division by zero");
        return interp_make_double(ctx, 1.0 / acc);
    }
    for (uint64_t i = 1; i < n; i++) {
        double d = as_double(args[i]);
        if (d == 0) return interp_make_error(ctx, "/: division by zero");
        acc /= d;
    }
    return interp_make_double(ctx, acc);
}

static interp_val_t* builtin_modulo(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("modulo", 2);
    REQUIRE_NUMBER("modulo", args[0]);
    REQUIRE_NUMBER("modulo", args[1]);
    if (args[1]->int_val == 0) return interp_make_error(ctx, "modulo: division by zero");
    return interp_make_int(ctx, args[0]->int_val % args[1]->int_val);
}

static interp_val_t* builtin_abs(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("abs", 1);
    REQUIRE_NUMBER("abs", args[0]);
    if (args[0]->type == INTERP_VAL_DOUBLE) return interp_make_double(ctx, fabs(args[0]->double_val));
    return interp_make_int(ctx, args[0]->int_val < 0 ? -args[0]->int_val : args[0]->int_val);
}

static interp_val_t* builtin_min(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    if (n == 0) return interp_make_error(ctx, "min: need at least 1 argument");
    for (uint64_t i = 0; i < n; i++) REQUIRE_NUMBER("min", args[i]);
    uint64_t best = 0;
    for (uint64_t i = 1; i < n; i++) {
        if (as_double(args[i]) < as_double(args[best])) best = i;
    }
    return args[best];
}

static interp_val_t* builtin_max(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    if (n == 0) return interp_make_error(ctx, "max: need at least 1 argument");
    for (uint64_t i = 0; i < n; i++) REQUIRE_NUMBER("max", args[i]);
    uint64_t best = 0;
    for (uint64_t i = 1; i < n; i++) {
        if (as_double(args[i]) > as_double(args[best])) best = i;
    }
    return args[best];
}

// ═══════════════════════════════════════════════════════════════════════════
// Comparison
// ═══════════════════════════════════════════════════════════════════════════

static interp_val_t* builtin_eq(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("=", 2);
    REQUIRE_NUMBER("=", args[0]); REQUIRE_NUMBER("=", args[1]);
    return interp_make_bool(ctx, as_double(args[0]) == as_double(args[1]));
}

static interp_val_t* builtin_lt(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("<", 2);
    REQUIRE_NUMBER("<", args[0]); REQUIRE_NUMBER("<", args[1]);
    return interp_make_bool(ctx, as_double(args[0]) < as_double(args[1]));
}

static interp_val_t* builtin_gt(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS(">", 2);
    REQUIRE_NUMBER(">", args[0]); REQUIRE_NUMBER(">", args[1]);
    return interp_make_bool(ctx, as_double(args[0]) > as_double(args[1]));
}

static interp_val_t* builtin_le(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("<=", 2);
    REQUIRE_NUMBER("<=", args[0]); REQUIRE_NUMBER("<=", args[1]);
    return interp_make_bool(ctx, as_double(args[0]) <= as_double(args[1]));
}

static interp_val_t* builtin_ge(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS(">=", 2);
    REQUIRE_NUMBER(">=", args[0]); REQUIRE_NUMBER(">=", args[1]);
    return interp_make_bool(ctx, as_double(args[0]) >= as_double(args[1]));
}

// ═══════════════════════════════════════════════════════════════════════════
// Predicates
// ═══════════════════════════════════════════════════════════════════════════

static interp_val_t* builtin_not(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("not", 1);
    return interp_make_bool(ctx, !interp_val_is_truthy(args[0]));
}

static interp_val_t* builtin_null_p(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("null?", 1);
    return interp_make_bool(ctx, args[0]->type == INTERP_VAL_NULL);
}

static interp_val_t* builtin_pair_p(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("pair?", 1);
    return interp_make_bool(ctx, args[0]->type == INTERP_VAL_CONS);
}

static interp_val_t* builtin_number_p(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("number?", 1);
    return interp_make_bool(ctx, is_number(args[0]));
}

static interp_val_t* builtin_string_p(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("string?", 1);
    return interp_make_bool(ctx, args[0]->type == INTERP_VAL_STRING);
}

static interp_val_t* builtin_boolean_p(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("boolean?", 1);
    return interp_make_bool(ctx, args[0]->type == INTERP_VAL_BOOL);
}

static interp_val_t* builtin_procedure_p(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("procedure?", 1);
    return interp_make_bool(ctx, args[0]->type == INTERP_VAL_CLOSURE || args[0]->type == INTERP_VAL_BUILTIN);
}

static interp_val_t* builtin_zero_p(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("zero?", 1);
    REQUIRE_NUMBER("zero?", args[0]);
    return interp_make_bool(ctx, as_double(args[0]) == 0);
}

static interp_val_t* builtin_eq_p(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("eq?", 2);
    // Pointer equality for non-primitives, value equality for primitives
    if (args[0]->type != args[1]->type) return interp_make_bool(ctx, false);
    switch (args[0]->type) {
        case INTERP_VAL_INT: return interp_make_bool(ctx, args[0]->int_val == args[1]->int_val);
        case INTERP_VAL_DOUBLE: return interp_make_bool(ctx, args[0]->double_val == args[1]->double_val);
        case INTERP_VAL_BOOL: return interp_make_bool(ctx, args[0]->bool_val == args[1]->bool_val);
        case INTERP_VAL_CHAR: return interp_make_bool(ctx, args[0]->char_val == args[1]->char_val);
        case INTERP_VAL_NULL: return interp_make_bool(ctx, true);
        case INTERP_VAL_SYMBOL: return interp_make_bool(ctx, strcmp(args[0]->symbol, args[1]->symbol) == 0);
        default: return interp_make_bool(ctx, args[0] == args[1]);
    }
}

static interp_val_t* builtin_equal_p(interp_val_t** args, uint64_t n, interp_ctx_t* ctx);

static bool vals_equal(const interp_val_t* a, const interp_val_t* b) {
    if (a == b) return true;
    if (!a || !b) return false;
    if (a->type != b->type) {
        // Allow int/double comparison
        if (is_number(a) && is_number(b)) return as_double(a) == as_double(b);
        return false;
    }
    switch (a->type) {
        case INTERP_VAL_INT: return a->int_val == b->int_val;
        case INTERP_VAL_DOUBLE: return a->double_val == b->double_val;
        case INTERP_VAL_BOOL: return a->bool_val == b->bool_val;
        case INTERP_VAL_CHAR: return a->char_val == b->char_val;
        case INTERP_VAL_NULL: return true;
        case INTERP_VAL_STRING:
            return a->str_val.len == b->str_val.len &&
                   memcmp(a->str_val.ptr, b->str_val.ptr, a->str_val.len) == 0;
        case INTERP_VAL_SYMBOL: return strcmp(a->symbol, b->symbol) == 0;
        case INTERP_VAL_CONS:
            return vals_equal(a->cons.car, b->cons.car) && vals_equal(a->cons.cdr, b->cons.cdr);
        default: return a == b;
    }
}

static interp_val_t* builtin_equal_p(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("equal?", 2);
    return interp_make_bool(ctx, vals_equal(args[0], args[1]));
}

// ═══════════════════════════════════════════════════════════════════════════
// List operations
// ═══════════════════════════════════════════════════════════════════════════

static interp_val_t* builtin_cons(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("cons", 2);
    return interp_make_cons(ctx, args[0], args[1]);
}

static interp_val_t* builtin_car(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("car", 1);
    if (args[0]->type != INTERP_VAL_CONS) return interp_make_error(ctx, "car: not a pair");
    return args[0]->cons.car;
}

static interp_val_t* builtin_cdr(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("cdr", 1);
    if (args[0]->type != INTERP_VAL_CONS) return interp_make_error(ctx, "cdr: not a pair");
    return args[0]->cons.cdr;
}

static interp_val_t* builtin_list(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    interp_val_t* result = interp_make_null(ctx);
    for (int64_t i = (int64_t)n - 1; i >= 0; i--) {
        result = interp_make_cons(ctx, args[i], result);
    }
    return result;
}

static interp_val_t* builtin_length(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("length", 1);
    if (args[0]->type == INTERP_VAL_NULL) return interp_make_int(ctx, 0);
    if (args[0]->type != INTERP_VAL_CONS) return interp_make_error(ctx, "length: not a list");
    int64_t len = 0;
    const interp_val_t* cur = args[0];
    while (cur && cur->type == INTERP_VAL_CONS) { len++; cur = cur->cons.cdr; }
    return interp_make_int(ctx, len);
}

static interp_val_t* builtin_append(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    if (n == 0) return interp_make_null(ctx);
    if (n == 1) return args[0];

    // Copy all but last list
    interp_val_t* result = args[n - 1];
    for (int64_t i = (int64_t)n - 2; i >= 0; i--) {
        // Copy list args[i] and append result
        const interp_val_t* cur = args[i];
        // Collect elements
        interp_val_t* items[1024];
        int count = 0;
        while (cur && cur->type == INTERP_VAL_CONS && count < 1024) {
            items[count++] = cur->cons.car;
            cur = cur->cons.cdr;
        }
        for (int j = count - 1; j >= 0; j--) {
            result = interp_make_cons(ctx, items[j], result);
        }
    }
    return result;
}

static interp_val_t* builtin_reverse(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("reverse", 1);
    interp_val_t* result = interp_make_null(ctx);
    const interp_val_t* cur = args[0];
    while (cur && cur->type == INTERP_VAL_CONS) {
        result = interp_make_cons(ctx, cur->cons.car, result);
        cur = cur->cons.cdr;
    }
    return result;
}

static interp_val_t* builtin_map(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    if (n < 2) return interp_make_error(ctx, "map: expected at least 2 arguments");
    interp_val_t* func = args[0];
    interp_val_t* lst = args[1];

    // Collect results
    interp_val_t* items[4096];
    int count = 0;
    while (lst && lst->type == INTERP_VAL_CONS && count < 4096) {
        interp_val_t* arg = lst->cons.car;
        items[count] = interp_apply(func, &arg, 1, ctx);
        if (ctx->error_msg) return items[count];
        count++;
        lst = lst->cons.cdr;
    }

    interp_val_t* result = interp_make_null(ctx);
    for (int i = count - 1; i >= 0; i--) {
        result = interp_make_cons(ctx, items[i], result);
    }
    return result;
}

static interp_val_t* builtin_filter(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    if (n < 2) return interp_make_error(ctx, "filter: expected 2 arguments");
    interp_val_t* func = args[0];
    interp_val_t* lst = args[1];

    interp_val_t* items[4096];
    int count = 0;
    while (lst && lst->type == INTERP_VAL_CONS && count < 4096) {
        interp_val_t* arg = lst->cons.car;
        interp_val_t* test = interp_apply(func, &arg, 1, ctx);
        if (ctx->error_msg) return test;
        if (interp_val_is_truthy(test)) {
            items[count++] = arg;
        }
        lst = lst->cons.cdr;
    }

    interp_val_t* result = interp_make_null(ctx);
    for (int i = count - 1; i >= 0; i--) {
        result = interp_make_cons(ctx, items[i], result);
    }
    return result;
}

static interp_val_t* builtin_apply(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    if (n < 2) return interp_make_error(ctx, "apply: expected at least 2 arguments");
    interp_val_t* func = args[0];
    interp_val_t* lst = args[n - 1];

    // Collect args: middle args + list elements
    interp_val_t* call_args[1024];
    int count = 0;
    for (uint64_t i = 1; i < n - 1 && count < 1024; i++) {
        call_args[count++] = args[i];
    }
    while (lst && lst->type == INTERP_VAL_CONS && count < 1024) {
        call_args[count++] = lst->cons.car;
        lst = lst->cons.cdr;
    }

    return interp_apply(func, call_args, count, ctx);
}

// ═══════════════════════════════════════════════════════════════════════════
// I/O
// ═══════════════════════════════════════════════════════════════════════════

static interp_val_t* builtin_display(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("display", 1);
    if (args[0]->type == INTERP_VAL_STRING) {
        interp_output_append(ctx, args[0]->str_val.ptr, args[0]->str_val.len);
    } else {
        char* s = interp_val_to_string(args[0]);
        interp_output_append(ctx, s, strlen(s));
        free(s);
    }
    return interp_make_void(ctx);
}

static interp_val_t* builtin_newline(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    interp_output_append(ctx, "\n", 1);
    return interp_make_void(ctx);
}

static interp_val_t* builtin_write(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("write", 1);
    char* s = interp_val_to_string(args[0]);
    interp_output_append(ctx, s, strlen(s));
    free(s);
    return interp_make_void(ctx);
}

// ═══════════════════════════════════════════════════════════════════════════
// Math
// ═══════════════════════════════════════════════════════════════════════════

static interp_val_t* builtin_sqrt(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("sqrt", 1); REQUIRE_NUMBER("sqrt", args[0]);
    return interp_make_double(ctx, sqrt(as_double(args[0])));
}

static interp_val_t* builtin_expt(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("expt", 2); REQUIRE_NUMBER("expt", args[0]); REQUIRE_NUMBER("expt", args[1]);
    return interp_make_double(ctx, pow(as_double(args[0]), as_double(args[1])));
}

static interp_val_t* builtin_floor(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("floor", 1); REQUIRE_NUMBER("floor", args[0]);
    return interp_make_int(ctx, (int64_t)floor(as_double(args[0])));
}

static interp_val_t* builtin_ceiling(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("ceiling", 1); REQUIRE_NUMBER("ceiling", args[0]);
    return interp_make_int(ctx, (int64_t)ceil(as_double(args[0])));
}

static interp_val_t* builtin_round(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("round", 1); REQUIRE_NUMBER("round", args[0]);
    return interp_make_int(ctx, (int64_t)round(as_double(args[0])));
}

static interp_val_t* builtin_sin(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("sin", 1); REQUIRE_NUMBER("sin", args[0]);
    return interp_make_double(ctx, sin(as_double(args[0])));
}

static interp_val_t* builtin_cos(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("cos", 1); REQUIRE_NUMBER("cos", args[0]);
    return interp_make_double(ctx, cos(as_double(args[0])));
}

static interp_val_t* builtin_tan(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("tan", 1); REQUIRE_NUMBER("tan", args[0]);
    return interp_make_double(ctx, tan(as_double(args[0])));
}

static interp_val_t* builtin_exp(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("exp", 1); REQUIRE_NUMBER("exp", args[0]);
    return interp_make_double(ctx, exp(as_double(args[0])));
}

static interp_val_t* builtin_log(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("log", 1); REQUIRE_NUMBER("log", args[0]);
    return interp_make_double(ctx, log(as_double(args[0])));
}

// ═══════════════════════════════════════════════════════════════════════════
// String
// ═══════════════════════════════════════════════════════════════════════════

static interp_val_t* builtin_string_length(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("string-length", 1);
    if (args[0]->type != INTERP_VAL_STRING) return interp_make_error(ctx, "string-length: not a string");
    return interp_make_int(ctx, (int64_t)args[0]->str_val.len);
}

static interp_val_t* builtin_string_append(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    std::string result;
    for (uint64_t i = 0; i < n; i++) {
        if (args[i]->type != INTERP_VAL_STRING) return interp_make_error(ctx, "string-append: not a string");
        result.append(args[i]->str_val.ptr, args[i]->str_val.len);
    }
    return interp_make_string(ctx, result.c_str(), result.size());
}

static interp_val_t* builtin_number_to_string(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("number->string", 1); REQUIRE_NUMBER("number->string", args[0]);
    char* s = interp_val_to_string(args[0]);
    auto* v = interp_make_string(ctx, s, strlen(s));
    free(s);
    return v;
}

static interp_val_t* builtin_string_to_number(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("string->number", 1);
    if (args[0]->type != INTERP_VAL_STRING) return interp_make_error(ctx, "string->number: not a string");
    char* end;
    int64_t iv = strtoll(args[0]->str_val.ptr, &end, 10);
    if (*end == '\0') return interp_make_int(ctx, iv);
    double dv = strtod(args[0]->str_val.ptr, &end);
    if (*end == '\0') return interp_make_double(ctx, dv);
    return interp_make_bool(ctx, false); // Scheme returns #f on failure
}

// ═══════════════════════════════════════════════════════════════════════════
// Additional predicates
// ═══════════════════════════════════════════════════════════════════════════

static interp_val_t* builtin_positive_p(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("positive?", 1); REQUIRE_NUMBER("positive?", args[0]);
    return interp_make_bool(ctx, as_double(args[0]) > 0);
}

static interp_val_t* builtin_negative_p(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("negative?", 1); REQUIRE_NUMBER("negative?", args[0]);
    return interp_make_bool(ctx, as_double(args[0]) < 0);
}

static interp_val_t* builtin_odd_p(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("odd?", 1); REQUIRE_NUMBER("odd?", args[0]);
    return interp_make_bool(ctx, ((int64_t)as_double(args[0])) % 2 != 0);
}

static interp_val_t* builtin_even_p(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("even?", 1); REQUIRE_NUMBER("even?", args[0]);
    return interp_make_bool(ctx, ((int64_t)as_double(args[0])) % 2 == 0);
}

static interp_val_t* builtin_integer_p(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("integer?", 1);
    if (args[0]->type == INTERP_VAL_INT) return interp_make_bool(ctx, true);
    if (args[0]->type == INTERP_VAL_DOUBLE)
        return interp_make_bool(ctx, args[0]->double_val == (int64_t)args[0]->double_val);
    return interp_make_bool(ctx, false);
}

static interp_val_t* builtin_real_p(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("real?", 1);
    return interp_make_bool(ctx, is_number(args[0]));
}

static interp_val_t* builtin_char_p(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("char?", 1);
    return interp_make_bool(ctx, args[0]->type == INTERP_VAL_CHAR);
}

static interp_val_t* builtin_symbol_p(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("symbol?", 1);
    return interp_make_bool(ctx, args[0]->type == INTERP_VAL_SYMBOL);
}

static interp_val_t* builtin_list_p(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("list?", 1);
    const interp_val_t* v = args[0];
    while (v && v->type == INTERP_VAL_CONS) v = v->cons.cdr;
    return interp_make_bool(ctx, v && v->type == INTERP_VAL_NULL);
}

// ═══════════════════════════════════════════════════════════════════════════
// Additional list operations
// ═══════════════════════════════════════════════════════════════════════════

static interp_val_t* builtin_cadr(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("cadr", 1);
    if (args[0]->type != INTERP_VAL_CONS) return interp_make_error(ctx, "cadr: not a pair");
    auto* d = args[0]->cons.cdr;
    if (!d || d->type != INTERP_VAL_CONS) return interp_make_error(ctx, "cadr: cdr is not a pair");
    return d->cons.car;
}

static interp_val_t* builtin_cdar(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("cdar", 1);
    if (args[0]->type != INTERP_VAL_CONS) return interp_make_error(ctx, "cdar: not a pair");
    auto* a = args[0]->cons.car;
    if (!a || a->type != INTERP_VAL_CONS) return interp_make_error(ctx, "cdar: car is not a pair");
    return a->cons.cdr;
}

static interp_val_t* builtin_caar(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("caar", 1);
    if (args[0]->type != INTERP_VAL_CONS) return interp_make_error(ctx, "caar: not a pair");
    auto* a = args[0]->cons.car;
    if (!a || a->type != INTERP_VAL_CONS) return interp_make_error(ctx, "caar: car is not a pair");
    return a->cons.car;
}

static interp_val_t* builtin_cddr(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("cddr", 1);
    if (args[0]->type != INTERP_VAL_CONS) return interp_make_error(ctx, "cddr: not a pair");
    auto* d = args[0]->cons.cdr;
    if (!d || d->type != INTERP_VAL_CONS) return interp_make_error(ctx, "cddr: cdr is not a pair");
    return d->cons.cdr;
}

static interp_val_t* builtin_caddr(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("caddr", 1);
    if (args[0]->type != INTERP_VAL_CONS) return interp_make_error(ctx, "caddr: not a pair");
    auto* d = args[0]->cons.cdr;
    if (!d || d->type != INTERP_VAL_CONS) return interp_make_error(ctx, "caddr: not enough elements");
    auto* dd = d->cons.cdr;
    if (!dd || dd->type != INTERP_VAL_CONS) return interp_make_error(ctx, "caddr: not enough elements");
    return dd->cons.car;
}

static interp_val_t* builtin_list_ref(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("list-ref", 2); REQUIRE_NUMBER("list-ref", args[1]);
    int64_t idx = (int64_t)as_double(args[1]);
    const interp_val_t* cur = args[0];
    for (int64_t i = 0; i < idx; i++) {
        if (!cur || cur->type != INTERP_VAL_CONS) return interp_make_error(ctx, "list-ref: index out of range");
        cur = cur->cons.cdr;
    }
    if (!cur || cur->type != INTERP_VAL_CONS) return interp_make_error(ctx, "list-ref: index out of range");
    return cur->cons.car;
}

static interp_val_t* builtin_list_tail(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("list-tail", 2); REQUIRE_NUMBER("list-tail", args[1]);
    int64_t idx = (int64_t)as_double(args[1]);
    interp_val_t* cur = args[0];
    for (int64_t i = 0; i < idx; i++) {
        if (!cur || cur->type != INTERP_VAL_CONS) return interp_make_error(ctx, "list-tail: index out of range");
        cur = cur->cons.cdr;
    }
    return cur;
}

static interp_val_t* builtin_for_each(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    if (n < 2) return interp_make_error(ctx, "for-each: expected at least 2 arguments");
    interp_val_t* func = args[0];
    interp_val_t* lst = args[1];
    while (lst && lst->type == INTERP_VAL_CONS) {
        interp_val_t* arg = lst->cons.car;
        interp_apply(func, &arg, 1, ctx);
        if (ctx->error_msg) return interp_make_error(ctx, ctx->error_msg);
        lst = lst->cons.cdr;
    }
    return interp_make_void(ctx);
}

static interp_val_t* builtin_assoc(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("assoc", 2);
    interp_val_t* key = args[0];
    interp_val_t* alist = args[1];
    while (alist && alist->type == INTERP_VAL_CONS) {
        interp_val_t* pair = alist->cons.car;
        if (pair && pair->type == INTERP_VAL_CONS && vals_equal(pair->cons.car, key)) {
            return pair;
        }
        alist = alist->cons.cdr;
    }
    return interp_make_bool(ctx, false);
}

static interp_val_t* builtin_member(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("member", 2);
    interp_val_t* key = args[0];
    interp_val_t* lst = args[1];
    while (lst && lst->type == INTERP_VAL_CONS) {
        if (vals_equal(lst->cons.car, key)) return lst;
        lst = lst->cons.cdr;
    }
    return interp_make_bool(ctx, false);
}

static interp_val_t* builtin_set_car(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("set-car!", 2);
    if (args[0]->type != INTERP_VAL_CONS) return interp_make_error(ctx, "set-car!: not a pair");
    args[0]->cons.car = args[1];
    return interp_make_void(ctx);
}

static interp_val_t* builtin_set_cdr(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("set-cdr!", 2);
    if (args[0]->type != INTERP_VAL_CONS) return interp_make_error(ctx, "set-cdr!: not a pair");
    args[0]->cons.cdr = args[1];
    return interp_make_void(ctx);
}

static interp_val_t* builtin_range(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    if (n < 1 || n > 3) return interp_make_error(ctx, "range: expected 1-3 arguments");
    for (uint64_t i = 0; i < n; i++) REQUIRE_NUMBER("range", args[i]);
    int64_t start = 0, end_val, step = 1;
    if (n == 1) { end_val = (int64_t)as_double(args[0]); }
    else { start = (int64_t)as_double(args[0]); end_val = (int64_t)as_double(args[1]); }
    if (n == 3) step = (int64_t)as_double(args[2]);
    if (step == 0) return interp_make_error(ctx, "range: step cannot be zero");

    interp_val_t* result = interp_make_null(ctx);
    if (step > 0) {
        for (int64_t i = end_val - step; i >= start; i -= step)
            result = interp_make_cons(ctx, interp_make_int(ctx, i), result);
    } else {
        for (int64_t i = end_val - step; i <= start; i -= step)
            result = interp_make_cons(ctx, interp_make_int(ctx, i), result);
    }
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// Additional string operations
// ═══════════════════════════════════════════════════════════════════════════

static interp_val_t* builtin_string_ref(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("string-ref", 2);
    if (args[0]->type != INTERP_VAL_STRING) return interp_make_error(ctx, "string-ref: not a string");
    REQUIRE_NUMBER("string-ref", args[1]);
    int64_t idx = (int64_t)as_double(args[1]);
    if (idx < 0 || (uint64_t)idx >= args[0]->str_val.len)
        return interp_make_error(ctx, "string-ref: index out of range");
    return interp_make_char(ctx, args[0]->str_val.ptr[idx]);
}

static interp_val_t* builtin_substring(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    if (n < 2 || n > 3) return interp_make_error(ctx, "substring: expected 2-3 arguments");
    if (args[0]->type != INTERP_VAL_STRING) return interp_make_error(ctx, "substring: not a string");
    REQUIRE_NUMBER("substring", args[1]);
    int64_t start = (int64_t)as_double(args[1]);
    int64_t end_val = (int64_t)args[0]->str_val.len;
    if (n == 3) { REQUIRE_NUMBER("substring", args[2]); end_val = (int64_t)as_double(args[2]); }
    if (start < 0 || end_val < start || (uint64_t)end_val > args[0]->str_val.len)
        return interp_make_error(ctx, "substring: index out of range");
    return interp_make_string(ctx, args[0]->str_val.ptr + start, (uint64_t)(end_val - start));
}

static interp_val_t* builtin_string_contains(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("string-contains", 2);
    if (args[0]->type != INTERP_VAL_STRING || args[1]->type != INTERP_VAL_STRING)
        return interp_make_error(ctx, "string-contains: not a string");
    return interp_make_bool(ctx, strstr(args[0]->str_val.ptr, args[1]->str_val.ptr) != nullptr);
}

static interp_val_t* builtin_string_upcase(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("string-upcase", 1);
    if (args[0]->type != INTERP_VAL_STRING) return interp_make_error(ctx, "string-upcase: not a string");
    char* s = (char*)malloc(args[0]->str_val.len + 1);
    for (uint64_t i = 0; i < args[0]->str_val.len; i++)
        s[i] = (char)toupper((unsigned char)args[0]->str_val.ptr[i]);
    s[args[0]->str_val.len] = '\0';
    auto* v = interp_make_string(ctx, s, args[0]->str_val.len);
    free(s);
    return v;
}

static interp_val_t* builtin_string_downcase(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("string-downcase", 1);
    if (args[0]->type != INTERP_VAL_STRING) return interp_make_error(ctx, "string-downcase: not a string");
    char* s = (char*)malloc(args[0]->str_val.len + 1);
    for (uint64_t i = 0; i < args[0]->str_val.len; i++)
        s[i] = (char)tolower((unsigned char)args[0]->str_val.ptr[i]);
    s[args[0]->str_val.len] = '\0';
    auto* v = interp_make_string(ctx, s, args[0]->str_val.len);
    free(s);
    return v;
}

// ═══════════════════════════════════════════════════════════════════════════
// Type conversions
// ═══════════════════════════════════════════════════════════════════════════

static interp_val_t* builtin_exact_to_inexact(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("exact->inexact", 1); REQUIRE_NUMBER("exact->inexact", args[0]);
    return interp_make_double(ctx, as_double(args[0]));
}

static interp_val_t* builtin_inexact_to_exact(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("inexact->exact", 1); REQUIRE_NUMBER("inexact->exact", args[0]);
    return interp_make_int(ctx, (int64_t)as_double(args[0]));
}

static interp_val_t* builtin_char_to_integer(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("char->integer", 1);
    if (args[0]->type != INTERP_VAL_CHAR) return interp_make_error(ctx, "char->integer: not a char");
    return interp_make_int(ctx, (int64_t)(unsigned char)args[0]->char_val);
}

static interp_val_t* builtin_integer_to_char(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("integer->char", 1); REQUIRE_NUMBER("integer->char", args[0]);
    return interp_make_char(ctx, (char)(int64_t)as_double(args[0]));
}

static interp_val_t* builtin_symbol_to_string(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("symbol->string", 1);
    if (args[0]->type != INTERP_VAL_SYMBOL) return interp_make_error(ctx, "symbol->string: not a symbol");
    return interp_make_string(ctx, args[0]->symbol, strlen(args[0]->symbol));
}

static interp_val_t* builtin_string_to_symbol(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("string->symbol", 1);
    if (args[0]->type != INTERP_VAL_STRING) return interp_make_error(ctx, "string->symbol: not a string");
    return interp_make_symbol(ctx, args[0]->str_val.ptr);
}

// ═══════════════════════════════════════════════════════════════════════════
// Math (additional trig/hyperbolic)
// ═══════════════════════════════════════════════════════════════════════════

static interp_val_t* builtin_asin(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("asin", 1); REQUIRE_NUMBER("asin", args[0]);
    return interp_make_double(ctx, asin(as_double(args[0])));
}

static interp_val_t* builtin_acos(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("acos", 1); REQUIRE_NUMBER("acos", args[0]);
    return interp_make_double(ctx, acos(as_double(args[0])));
}

static interp_val_t* builtin_atan(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    if (n == 1) { REQUIRE_NUMBER("atan", args[0]); return interp_make_double(ctx, atan(as_double(args[0]))); }
    if (n == 2) { REQUIRE_NUMBER("atan", args[0]); REQUIRE_NUMBER("atan", args[1]);
                   return interp_make_double(ctx, atan2(as_double(args[0]), as_double(args[1]))); }
    return interp_make_error(ctx, "atan: expected 1-2 arguments");
}

static interp_val_t* builtin_truncate(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("truncate", 1); REQUIRE_NUMBER("truncate", args[0]);
    return interp_make_int(ctx, (int64_t)as_double(args[0]));
}

static interp_val_t* builtin_quotient(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("quotient", 2); REQUIRE_NUMBER("quotient", args[0]); REQUIRE_NUMBER("quotient", args[1]);
    int64_t b = (int64_t)as_double(args[1]);
    if (b == 0) return interp_make_error(ctx, "quotient: division by zero");
    return interp_make_int(ctx, (int64_t)as_double(args[0]) / b);
}

static interp_val_t* builtin_gcd(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("gcd", 2); REQUIRE_NUMBER("gcd", args[0]); REQUIRE_NUMBER("gcd", args[1]);
    int64_t a = llabs((int64_t)as_double(args[0])), b = llabs((int64_t)as_double(args[1]));
    while (b) { int64_t t = b; b = a % b; a = t; }
    return interp_make_int(ctx, a);
}

// ═══════════════════════════════════════════════════════════════════════════
// Misc
// ═══════════════════════════════════════════════════════════════════════════

static interp_val_t* builtin_void(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    return interp_make_void(ctx);
}

static interp_val_t* builtin_error(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    if (n >= 1 && args[0]->type == INTERP_VAL_STRING)
        return interp_make_error(ctx, args[0]->str_val.ptr);
    if (n >= 1) {
        char* s = interp_val_to_string(args[0]);
        auto* v = interp_make_error(ctx, s);
        free(s);
        return v;
    }
    return interp_make_error(ctx, "error");
}

static interp_val_t* builtin_begin(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    if (n == 0) return interp_make_void(ctx);
    return args[n - 1]; // args already evaluated by caller
}

static interp_val_t* builtin_identity(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("identity", 1);
    return args[0];
}

static interp_val_t* builtin_printf(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    if (n < 1 || args[0]->type != INTERP_VAL_STRING)
        return interp_make_error(ctx, "printf: first argument must be a string");
    // Simple printf: just output the format string with ~a substitution
    const char* fmt = args[0]->str_val.ptr;
    uint64_t arg_idx = 1;
    std::string out;
    for (size_t i = 0; fmt[i]; i++) {
        if (fmt[i] == '~' && fmt[i+1] == 'a' && arg_idx < n) {
            char* s = interp_val_to_string(args[arg_idx++]);
            if (args[arg_idx-1]->type == INTERP_VAL_STRING)
                out.append(args[arg_idx-1]->str_val.ptr, args[arg_idx-1]->str_val.len);
            else { out += s; }
            free(s);
            i++; // skip 'a'
        } else if (fmt[i] == '~' && fmt[i+1] == '%') {
            out += '\n'; i++;
        } else {
            out += fmt[i];
        }
    }
    interp_output_append(ctx, out.c_str(), out.size());
    return interp_make_void(ctx);
}

// ═══════════════════════════════════════════════════════════════════════════
// Registration
// ═══════════════════════════════════════════════════════════════════════════

static void reg(interp_ctx_t* ctx, const char* name, interp_builtin_fn fn, int min_a, int max_a) {
    auto* v = (interp_val_t*)calloc(1, sizeof(interp_val_t));
    v->type = INTERP_VAL_BUILTIN;
    v->builtin.fn = fn;
    v->builtin.name = name;
    v->builtin.min_arity = min_a;
    v->builtin.max_arity = max_a;
    interp_frame_define(ctx->env, name, v);
}

void interp_register_builtins(interp_ctx_t* ctx) {
    // Arithmetic
    reg(ctx, "+", builtin_add, 0, -1);
    reg(ctx, "-", builtin_sub, 1, -1);
    reg(ctx, "*", builtin_mul, 0, -1);
    reg(ctx, "/", builtin_div, 1, -1);
    reg(ctx, "modulo", builtin_modulo, 2, 2);
    reg(ctx, "remainder", builtin_modulo, 2, 2);
    reg(ctx, "abs", builtin_abs, 1, 1);
    reg(ctx, "min", builtin_min, 1, -1);
    reg(ctx, "max", builtin_max, 1, -1);

    // Comparison
    reg(ctx, "=", builtin_eq, 2, 2);
    reg(ctx, "<", builtin_lt, 2, 2);
    reg(ctx, ">", builtin_gt, 2, 2);
    reg(ctx, "<=", builtin_le, 2, 2);
    reg(ctx, ">=", builtin_ge, 2, 2);

    // Predicates
    reg(ctx, "not", builtin_not, 1, 1);
    reg(ctx, "null?", builtin_null_p, 1, 1);
    reg(ctx, "pair?", builtin_pair_p, 1, 1);
    reg(ctx, "number?", builtin_number_p, 1, 1);
    reg(ctx, "string?", builtin_string_p, 1, 1);
    reg(ctx, "boolean?", builtin_boolean_p, 1, 1);
    reg(ctx, "procedure?", builtin_procedure_p, 1, 1);
    reg(ctx, "zero?", builtin_zero_p, 1, 1);
    reg(ctx, "positive?", builtin_positive_p, 1, 1);
    reg(ctx, "negative?", builtin_negative_p, 1, 1);
    reg(ctx, "odd?", builtin_odd_p, 1, 1);
    reg(ctx, "even?", builtin_even_p, 1, 1);
    reg(ctx, "integer?", builtin_integer_p, 1, 1);
    reg(ctx, "real?", builtin_real_p, 1, 1);
    reg(ctx, "complex?", builtin_real_p, 1, 1);
    reg(ctx, "rational?", builtin_real_p, 1, 1);
    reg(ctx, "exact?", builtin_integer_p, 1, 1);
    reg(ctx, "inexact?", builtin_real_p, 1, 1);
    reg(ctx, "char?", builtin_char_p, 1, 1);
    reg(ctx, "symbol?", builtin_symbol_p, 1, 1);
    reg(ctx, "list?", builtin_list_p, 1, 1);
    reg(ctx, "eq?", builtin_eq_p, 2, 2);
    reg(ctx, "eqv?", builtin_eq_p, 2, 2);
    reg(ctx, "equal?", builtin_equal_p, 2, 2);

    // List
    reg(ctx, "cons", builtin_cons, 2, 2);
    reg(ctx, "car", builtin_car, 1, 1);
    reg(ctx, "cdr", builtin_cdr, 1, 1);
    reg(ctx, "caar", builtin_caar, 1, 1);
    reg(ctx, "cadr", builtin_cadr, 1, 1);
    reg(ctx, "cdar", builtin_cdar, 1, 1);
    reg(ctx, "cddr", builtin_cddr, 1, 1);
    reg(ctx, "caddr", builtin_caddr, 1, 1);
    reg(ctx, "list", builtin_list, 0, -1);
    reg(ctx, "length", builtin_length, 1, 1);
    reg(ctx, "append", builtin_append, 0, -1);
    reg(ctx, "reverse", builtin_reverse, 1, 1);
    reg(ctx, "list-ref", builtin_list_ref, 2, 2);
    reg(ctx, "list-tail", builtin_list_tail, 2, 2);
    reg(ctx, "map", builtin_map, 2, 2);
    reg(ctx, "filter", builtin_filter, 2, 2);
    reg(ctx, "for-each", builtin_for_each, 2, 2);
    reg(ctx, "apply", builtin_apply, 2, -1);
    reg(ctx, "assoc", builtin_assoc, 2, 2);
    reg(ctx, "assv", builtin_assoc, 2, 2);
    reg(ctx, "assq", builtin_assoc, 2, 2);
    reg(ctx, "member", builtin_member, 2, 2);
    reg(ctx, "memv", builtin_member, 2, 2);
    reg(ctx, "memq", builtin_member, 2, 2);
    reg(ctx, "set-car!", builtin_set_car, 2, 2);
    reg(ctx, "set-cdr!", builtin_set_cdr, 2, 2);
    reg(ctx, "range", builtin_range, 1, 3);
    reg(ctx, "iota", builtin_range, 1, 3);

    // I/O
    reg(ctx, "display", builtin_display, 1, 1);
    reg(ctx, "newline", builtin_newline, 0, 0);
    reg(ctx, "write", builtin_write, 1, 1);
    reg(ctx, "printf", builtin_printf, 1, -1);

    // Math
    reg(ctx, "sqrt", builtin_sqrt, 1, 1);
    reg(ctx, "expt", builtin_expt, 2, 2);
    reg(ctx, "pow", builtin_expt, 2, 2);
    reg(ctx, "floor", builtin_floor, 1, 1);
    reg(ctx, "ceiling", builtin_ceiling, 1, 1);
    reg(ctx, "round", builtin_round, 1, 1);
    reg(ctx, "truncate", builtin_truncate, 1, 1);
    reg(ctx, "quotient", builtin_quotient, 2, 2);
    reg(ctx, "gcd", builtin_gcd, 2, 2);
    reg(ctx, "sin", builtin_sin, 1, 1);
    reg(ctx, "cos", builtin_cos, 1, 1);
    reg(ctx, "tan", builtin_tan, 1, 1);
    reg(ctx, "asin", builtin_asin, 1, 1);
    reg(ctx, "acos", builtin_acos, 1, 1);
    reg(ctx, "atan", builtin_atan, 1, 2);
    reg(ctx, "exp", builtin_exp, 1, 1);
    reg(ctx, "log", builtin_log, 1, 1);

    // String
    reg(ctx, "string-length", builtin_string_length, 1, 1);
    reg(ctx, "string-append", builtin_string_append, 0, -1);
    reg(ctx, "string-ref", builtin_string_ref, 2, 2);
    reg(ctx, "substring", builtin_substring, 2, 3);
    reg(ctx, "string-contains", builtin_string_contains, 2, 2);
    reg(ctx, "string-upcase", builtin_string_upcase, 1, 1);
    reg(ctx, "string-downcase", builtin_string_downcase, 1, 1);
    reg(ctx, "number->string", builtin_number_to_string, 1, 1);
    reg(ctx, "string->number", builtin_string_to_number, 1, 1);
    reg(ctx, "symbol->string", builtin_symbol_to_string, 1, 1);
    reg(ctx, "string->symbol", builtin_string_to_symbol, 1, 1);

    // Type conversions
    reg(ctx, "exact->inexact", builtin_exact_to_inexact, 1, 1);
    reg(ctx, "inexact->exact", builtin_inexact_to_exact, 1, 1);
    reg(ctx, "char->integer", builtin_char_to_integer, 1, 1);
    reg(ctx, "integer->char", builtin_integer_to_char, 1, 1);

    // Misc
    reg(ctx, "void", builtin_void, 0, 0);
    reg(ctx, "error", builtin_error, 0, -1);
    reg(ctx, "begin", builtin_begin, 0, -1);
    reg(ctx, "identity", builtin_identity, 1, 1);

    // Constants
    interp_frame_define(ctx->env, "#t", interp_make_bool(ctx, true));
    interp_frame_define(ctx->env, "#f", interp_make_bool(ctx, false));
}
