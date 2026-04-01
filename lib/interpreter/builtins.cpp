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
    uint64_t num_lists = n - 1;

    // Multi-list map: (map f list1 list2 ...)
    interp_val_t* lists[64];
    for (uint64_t i = 0; i < num_lists && i < 64; i++) lists[i] = args[i + 1];

    interp_val_t* items[4096];
    int count = 0;

    while (count < 4096) {
        // Check if any list is exhausted
        bool done = false;
        for (uint64_t i = 0; i < num_lists; i++) {
            if (!lists[i] || lists[i]->type != INTERP_VAL_CONS) { done = true; break; }
        }
        if (done) break;

        // Collect car of each list as arguments
        interp_val_t* call_args[64];
        for (uint64_t i = 0; i < num_lists; i++) {
            call_args[i] = lists[i]->cons.car;
        }

        items[count] = interp_apply(func, call_args, num_lists, ctx);
        if (ctx->error_msg) return items[count];
        count++;

        // Advance all lists
        for (uint64_t i = 0; i < num_lists; i++) lists[i] = lists[i]->cons.cdr;
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
    uint64_t num_lists = n - 1;
    interp_val_t* lists[64];
    for (uint64_t i = 0; i < num_lists && i < 64; i++) lists[i] = args[i + 1];

    while (true) {
        bool done = false;
        for (uint64_t i = 0; i < num_lists; i++) {
            if (!lists[i] || lists[i]->type != INTERP_VAL_CONS) { done = true; break; }
        }
        if (done) break;
        interp_val_t* call_args[64];
        for (uint64_t i = 0; i < num_lists; i++) call_args[i] = lists[i]->cons.car;
        interp_apply(func, call_args, num_lists, ctx);
        if (ctx->error_msg) return interp_make_error(ctx, ctx->error_msg);
        for (uint64_t i = 0; i < num_lists; i++) lists[i] = lists[i]->cons.cdr;
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

// ═══════════════════════════════════════════════════════════════════════════
// Additional list operations (missing from backend)
// ═══════════════════════════════════════════════════════════════════════════

static interp_val_t* builtin_caaar(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("caaar", 1);
    auto* v = args[0];
    for (const char* s = "aaa"; *s; s++) {
        if (!v || v->type != INTERP_VAL_CONS) return interp_make_error(ctx, "caaar: not a pair");
        v = (*s == 'a') ? v->cons.car : v->cons.cdr;
    }
    return v;
}

static interp_val_t* builtin_caadr(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("caadr", 1);
    auto* v = args[0];
    if (!v || v->type != INTERP_VAL_CONS) return interp_make_error(ctx, "caadr: not a pair");
    v = v->cons.cdr;
    if (!v || v->type != INTERP_VAL_CONS) return interp_make_error(ctx, "caadr: not a pair");
    v = v->cons.car;
    if (!v || v->type != INTERP_VAL_CONS) return interp_make_error(ctx, "caadr: not a pair");
    return v->cons.car;
}

static interp_val_t* builtin_cadar(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("cadar", 1);
    auto* v = args[0];
    if (!v || v->type != INTERP_VAL_CONS) return interp_make_error(ctx, "cadar: not a pair");
    v = v->cons.car;
    if (!v || v->type != INTERP_VAL_CONS) return interp_make_error(ctx, "cadar: not a pair");
    v = v->cons.cdr;
    if (!v || v->type != INTERP_VAL_CONS) return interp_make_error(ctx, "cadar: not a pair");
    return v->cons.car;
}

static interp_val_t* builtin_cdaar(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("cdaar", 1);
    auto* v = args[0];
    if (!v || v->type != INTERP_VAL_CONS) return interp_make_error(ctx, "cdaar: not a pair");
    v = v->cons.car;
    if (!v || v->type != INTERP_VAL_CONS) return interp_make_error(ctx, "cdaar: not a pair");
    v = v->cons.car;
    if (!v || v->type != INTERP_VAL_CONS) return interp_make_error(ctx, "cdaar: not a pair");
    return v->cons.cdr;
}

static interp_val_t* builtin_cdadr(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("cdadr", 1);
    auto* v = args[0];
    if (!v || v->type != INTERP_VAL_CONS) return interp_make_error(ctx, "cdadr: not a pair");
    v = v->cons.cdr;
    if (!v || v->type != INTERP_VAL_CONS) return interp_make_error(ctx, "cdadr: not a pair");
    v = v->cons.car;
    if (!v || v->type != INTERP_VAL_CONS) return interp_make_error(ctx, "cdadr: not a pair");
    return v->cons.cdr;
}

static interp_val_t* builtin_cddar(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("cddar", 1);
    auto* v = args[0];
    if (!v || v->type != INTERP_VAL_CONS) return interp_make_error(ctx, "cddar: not a pair");
    v = v->cons.car;
    if (!v || v->type != INTERP_VAL_CONS) return interp_make_error(ctx, "cddar: not a pair");
    v = v->cons.cdr;
    if (!v || v->type != INTERP_VAL_CONS) return interp_make_error(ctx, "cddar: not a pair");
    return v->cons.cdr;
}

static interp_val_t* builtin_cdddr(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("cdddr", 1);
    auto* v = args[0];
    for (int i = 0; i < 3; i++) {
        if (!v || v->type != INTERP_VAL_CONS) return interp_make_error(ctx, "cdddr: not a pair");
        v = v->cons.cdr;
    }
    return v;
}

static interp_val_t* builtin_last(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("last", 1);
    const interp_val_t* cur = args[0];
    if (!cur || cur->type != INTERP_VAL_CONS) return interp_make_error(ctx, "last: not a pair");
    while (cur->cons.cdr && cur->cons.cdr->type == INTERP_VAL_CONS) cur = cur->cons.cdr;
    return cur->cons.car;
}

static interp_val_t* builtin_make_list(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    if (n < 1 || n > 2) return interp_make_error(ctx, "make-list: expected 1-2 arguments");
    REQUIRE_NUMBER("make-list", args[0]);
    int64_t len = (int64_t)as_double(args[0]);
    interp_val_t* fill = (n >= 2) ? args[1] : interp_make_int(ctx, 0);
    interp_val_t* result = interp_make_null(ctx);
    for (int64_t i = 0; i < len; i++) result = interp_make_cons(ctx, fill, result);
    return result;
}

static interp_val_t* builtin_take(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("take", 2); REQUIRE_NUMBER("take", args[1]);
    int64_t count = (int64_t)as_double(args[1]);
    interp_val_t* items[4096];
    int got = 0;
    const interp_val_t* cur = args[0];
    while (cur && cur->type == INTERP_VAL_CONS && got < count && got < 4096) {
        items[got++] = cur->cons.car;
        cur = cur->cons.cdr;
    }
    interp_val_t* result = interp_make_null(ctx);
    for (int i = got - 1; i >= 0; i--) result = interp_make_cons(ctx, items[i], result);
    return result;
}

static interp_val_t* builtin_drop(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("drop", 2); REQUIRE_NUMBER("drop", args[1]);
    int64_t count = (int64_t)as_double(args[1]);
    interp_val_t* cur = args[0];
    for (int64_t i = 0; i < count && cur && cur->type == INTERP_VAL_CONS; i++) cur = cur->cons.cdr;
    return cur ? cur : interp_make_null(ctx);
}

static interp_val_t* builtin_zip(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("zip", 2);
    interp_val_t* items[4096];
    int count = 0;
    interp_val_t* a = args[0];
    interp_val_t* b = args[1];
    while (a && a->type == INTERP_VAL_CONS && b && b->type == INTERP_VAL_CONS && count < 4096) {
        items[count++] = interp_make_cons(ctx, a->cons.car, interp_make_cons(ctx, b->cons.car, interp_make_null(ctx)));
        a = a->cons.cdr;
        b = b->cons.cdr;
    }
    interp_val_t* result = interp_make_null(ctx);
    for (int i = count - 1; i >= 0; i--) result = interp_make_cons(ctx, items[i], result);
    return result;
}

static interp_val_t* builtin_flatten(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("flatten", 1);
    // Collect all non-list atoms from nested list structure
    interp_val_t* items[4096];
    int count = 0;
    struct { interp_val_t* v; } stack[1024];
    int sp = 0;
    stack[sp++] = {args[0]};
    while (sp > 0 && count < 4096) {
        interp_val_t* cur = stack[--sp].v;
        if (!cur || cur->type == INTERP_VAL_NULL) continue;
        if (cur->type != INTERP_VAL_CONS) { items[count++] = cur; continue; }
        // Push cdr first (processed later), then car
        if (cur->cons.cdr) stack[sp++] = {cur->cons.cdr};
        if (cur->cons.car) stack[sp++] = {cur->cons.car};
    }
    interp_val_t* result = interp_make_null(ctx);
    for (int i = count - 1; i >= 0; i--) result = interp_make_cons(ctx, items[i], result);
    return result;
}

static interp_val_t* builtin_reduce(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    if (n != 3) return interp_make_error(ctx, "reduce: expected 3 arguments (func init list)");
    interp_val_t* func = args[0];
    interp_val_t* acc = args[1];
    interp_val_t* lst = args[2];
    while (lst && lst->type == INTERP_VAL_CONS) {
        interp_val_t* call_args[2] = {acc, lst->cons.car};
        acc = interp_apply(func, call_args, 2, ctx);
        if (ctx->error_msg) return acc;
        lst = lst->cons.cdr;
    }
    return acc;
}

static interp_val_t* builtin_remove(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("remove", 2);
    interp_val_t* func = args[0];
    interp_val_t* lst = args[1];
    interp_val_t* items[4096];
    int count = 0;
    while (lst && lst->type == INTERP_VAL_CONS && count < 4096) {
        interp_val_t* arg = lst->cons.car;
        interp_val_t* test = interp_apply(func, &arg, 1, ctx);
        if (ctx->error_msg) return test;
        if (!interp_val_is_truthy(test)) items[count++] = arg;
        lst = lst->cons.cdr;
    }
    interp_val_t* result = interp_make_null(ctx);
    for (int i = count - 1; i >= 0; i--) result = interp_make_cons(ctx, items[i], result);
    return result;
}

static interp_val_t* builtin_find(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("find", 2);
    interp_val_t* func = args[0];
    interp_val_t* lst = args[1];
    while (lst && lst->type == INTERP_VAL_CONS) {
        interp_val_t* test = interp_apply(func, &lst->cons.car, 1, ctx);
        if (ctx->error_msg) return test;
        if (interp_val_is_truthy(test)) return lst->cons.car;
        lst = lst->cons.cdr;
    }
    return interp_make_bool(ctx, false);
}

static interp_val_t* builtin_split_at(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("split-at", 2); REQUIRE_NUMBER("split-at", args[1]);
    int64_t idx = (int64_t)as_double(args[1]);
    interp_val_t* items[4096];
    int count = 0;
    interp_val_t* cur = args[0];
    while (cur && cur->type == INTERP_VAL_CONS && count < idx && count < 4096) {
        items[count++] = cur->cons.car;
        cur = cur->cons.cdr;
    }
    interp_val_t* left = interp_make_null(ctx);
    for (int i = count - 1; i >= 0; i--) left = interp_make_cons(ctx, items[i], left);
    // Return (left . right) pair
    return interp_make_cons(ctx, left, cur ? cur : interp_make_null(ctx));
}

static interp_val_t* builtin_fold_right(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    if (n != 3) return interp_make_error(ctx, "fold-right: expected 3 arguments (func init list)");
    interp_val_t* func = args[0];
    interp_val_t* init = args[1];
    interp_val_t* lst = args[2];
    // Collect list elements, then fold from right
    interp_val_t* elems[4096];
    int count = 0;
    while (lst && lst->type == INTERP_VAL_CONS && count < 4096) {
        elems[count++] = lst->cons.car;
        lst = lst->cons.cdr;
    }
    interp_val_t* acc = init;
    for (int i = count - 1; i >= 0; i--) {
        interp_val_t* call_args[2] = {elems[i], acc};
        acc = interp_apply(func, call_args, 2, ctx);
        if (ctx->error_msg) return acc;
    }
    return acc;
}

static interp_val_t* builtin_last_pair(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("last-pair", 1);
    interp_val_t* cur = args[0];
    if (!cur || cur->type != INTERP_VAL_CONS) return interp_make_error(ctx, "last-pair: not a pair");
    while (cur->cons.cdr && cur->cons.cdr->type == INTERP_VAL_CONS) cur = cur->cons.cdr;
    return cur;
}

static interp_val_t* builtin_partition(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("partition", 2);
    interp_val_t* func = args[0];
    interp_val_t* lst = args[1];
    interp_val_t* yes_items[4096], *no_items[4096];
    int yc = 0, nc = 0;
    while (lst && lst->type == INTERP_VAL_CONS) {
        interp_val_t* test = interp_apply(func, &lst->cons.car, 1, ctx);
        if (ctx->error_msg) return test;
        if (interp_val_is_truthy(test)) yes_items[yc++] = lst->cons.car;
        else no_items[nc++] = lst->cons.car;
        lst = lst->cons.cdr;
    }
    interp_val_t* yes_list = interp_make_null(ctx);
    for (int i = yc - 1; i >= 0; i--) yes_list = interp_make_cons(ctx, yes_items[i], yes_list);
    interp_val_t* no_list = interp_make_null(ctx);
    for (int i = nc - 1; i >= 0; i--) no_list = interp_make_cons(ctx, no_items[i], no_list);
    return interp_make_cons(ctx, yes_list, no_list);
}

static interp_val_t* builtin_unique(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("unique", 1);
    interp_val_t* items[4096];
    int count = 0;
    interp_val_t* lst = args[0];
    while (lst && lst->type == INTERP_VAL_CONS && count < 4096) {
        bool dup = false;
        for (int i = 0; i < count; i++) {
            if (vals_equal(items[i], lst->cons.car)) { dup = true; break; }
        }
        if (!dup) items[count++] = lst->cons.car;
        lst = lst->cons.cdr;
    }
    interp_val_t* result = interp_make_null(ctx);
    for (int i = count - 1; i >= 0; i--) result = interp_make_cons(ctx, items[i], result);
    return result;
}

static interp_val_t* builtin_sort(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    if (n < 1 || n > 2) return interp_make_error(ctx, "sort: expected 1-2 arguments");
    // Collect into array
    interp_val_t* items[4096];
    int count = 0;
    const interp_val_t* cur = args[0];
    while (cur && cur->type == INTERP_VAL_CONS && count < 4096) {
        items[count++] = cur->cons.car;
        cur = cur->cons.cdr;
    }
    // Simple insertion sort
    interp_val_t* cmp_func = (n >= 2) ? args[1] : nullptr;
    for (int i = 1; i < count; i++) {
        interp_val_t* key = items[i];
        int j = i - 1;
        while (j >= 0) {
            bool less;
            if (cmp_func) {
                interp_val_t* ca[2] = {key, items[j]};
                interp_val_t* r = interp_apply(cmp_func, ca, 2, ctx);
                if (ctx->error_msg) return r;
                less = interp_val_is_truthy(r);
            } else {
                less = as_double(key) < as_double(items[j]);
            }
            if (!less) break;
            items[j + 1] = items[j];
            j--;
        }
        items[j + 1] = key;
    }
    interp_val_t* result = interp_make_null(ctx);
    for (int i = count - 1; i >= 0; i--) result = interp_make_cons(ctx, items[i], result);
    return result;
}

static interp_val_t* builtin_list_star(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    if (n == 0) return interp_make_null(ctx);
    interp_val_t* result = args[n - 1];
    for (int64_t i = (int64_t)n - 2; i >= 0; i--)
        result = interp_make_cons(ctx, args[i], result);
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// Additional string operations
// ═══════════════════════════════════════════════════════════════════════════

static interp_val_t* builtin_make_string(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    if (n < 1 || n > 2) return interp_make_error(ctx, "make-string: expected 1-2 arguments");
    REQUIRE_NUMBER("make-string", args[0]);
    int64_t len = (int64_t)as_double(args[0]);
    char fill = (n >= 2 && args[1]->type == INTERP_VAL_CHAR) ? args[1]->char_val : ' ';
    char* s = (char*)malloc(len + 1);
    memset(s, fill, len);
    s[len] = '\0';
    auto* v = interp_make_string(ctx, s, len);
    free(s);
    return v;
}

static interp_val_t* builtin_string_set(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    if (n != 3) return interp_make_error(ctx, "string-set!: expected 3 arguments");
    if (args[0]->type != INTERP_VAL_STRING) return interp_make_error(ctx, "string-set!: not a string");
    REQUIRE_NUMBER("string-set!", args[1]);
    if (args[2]->type != INTERP_VAL_CHAR) return interp_make_error(ctx, "string-set!: third arg not a char");
    int64_t idx = (int64_t)as_double(args[1]);
    if (idx < 0 || (uint64_t)idx >= args[0]->str_val.len)
        return interp_make_error(ctx, "string-set!: index out of range");
    args[0]->str_val.ptr[idx] = args[2]->char_val;
    return interp_make_void(ctx);
}

static interp_val_t* builtin_string_to_list(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("string->list", 1);
    if (args[0]->type != INTERP_VAL_STRING) return interp_make_error(ctx, "string->list: not a string");
    interp_val_t* result = interp_make_null(ctx);
    for (int64_t i = (int64_t)args[0]->str_val.len - 1; i >= 0; i--)
        result = interp_make_cons(ctx, interp_make_char(ctx, args[0]->str_val.ptr[i]), result);
    return result;
}

static interp_val_t* builtin_list_to_string(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("list->string", 1);
    std::string s;
    const interp_val_t* cur = args[0];
    while (cur && cur->type == INTERP_VAL_CONS) {
        if (cur->cons.car && cur->cons.car->type == INTERP_VAL_CHAR) s += cur->cons.car->char_val;
        cur = cur->cons.cdr;
    }
    return interp_make_string(ctx, s.c_str(), s.size());
}

static interp_val_t* builtin_string_eq(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("string=?", 2);
    if (args[0]->type != INTERP_VAL_STRING || args[1]->type != INTERP_VAL_STRING)
        return interp_make_error(ctx, "string=?: not strings");
    return interp_make_bool(ctx, args[0]->str_val.len == args[1]->str_val.len &&
        memcmp(args[0]->str_val.ptr, args[1]->str_val.ptr, args[0]->str_val.len) == 0);
}

static interp_val_t* builtin_string_lt(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("string<?", 2);
    if (args[0]->type != INTERP_VAL_STRING || args[1]->type != INTERP_VAL_STRING)
        return interp_make_error(ctx, "string<?: not strings");
    return interp_make_bool(ctx, strcmp(args[0]->str_val.ptr, args[1]->str_val.ptr) < 0);
}

static interp_val_t* builtin_string_gt(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("string>?", 2);
    if (args[0]->type != INTERP_VAL_STRING || args[1]->type != INTERP_VAL_STRING)
        return interp_make_error(ctx, "string>?: not strings");
    return interp_make_bool(ctx, strcmp(args[0]->str_val.ptr, args[1]->str_val.ptr) > 0);
}

static interp_val_t* builtin_char_eq(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("char=?", 2);
    if (args[0]->type != INTERP_VAL_CHAR || args[1]->type != INTERP_VAL_CHAR)
        return interp_make_error(ctx, "char=?: not chars");
    return interp_make_bool(ctx, args[0]->char_val == args[1]->char_val);
}

static interp_val_t* builtin_char_lt(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("char<?", 2);
    if (args[0]->type != INTERP_VAL_CHAR || args[1]->type != INTERP_VAL_CHAR)
        return interp_make_error(ctx, "char<?: not chars");
    return interp_make_bool(ctx, args[0]->char_val < args[1]->char_val);
}

// ═══════════════════════════════════════════════════════════════════════════
// Misc additional
// ═══════════════════════════════════════════════════════════════════════════

static interp_val_t* builtin_type_of(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("type-of", 1);
    return interp_make_symbol(ctx, interp_val_type_name(args[0]));
}

static interp_val_t* builtin_random(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    if (n == 0) return interp_make_double(ctx, (double)rand() / RAND_MAX);
    REQUIRE_NUMBER("random", args[0]);
    return interp_make_int(ctx, rand() % (int64_t)as_double(args[0]));
}

static interp_val_t* builtin_current_seconds(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    return interp_make_double(ctx, (double)time(nullptr));
}

static interp_val_t* builtin_square(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("square", 1); REQUIRE_NUMBER("square", args[0]);
    if (args[0]->type == INTERP_VAL_INT) return interp_make_int(ctx, args[0]->int_val * args[0]->int_val);
    return interp_make_double(ctx, args[0]->double_val * args[0]->double_val);
}

static interp_val_t* builtin_negate(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("negate", 1); REQUIRE_NUMBER("negate", args[0]);
    if (args[0]->type == INTERP_VAL_INT) return interp_make_int(ctx, -args[0]->int_val);
    return interp_make_double(ctx, -args[0]->double_val);
}

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

static interp_val_t* builtin_sinh(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("sinh", 1); REQUIRE_NUMBER("sinh", args[0]);
    return interp_make_double(ctx, sinh(as_double(args[0])));
}

static interp_val_t* builtin_cosh(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("cosh", 1); REQUIRE_NUMBER("cosh", args[0]);
    return interp_make_double(ctx, cosh(as_double(args[0])));
}

static interp_val_t* builtin_tanh(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("tanh", 1); REQUIRE_NUMBER("tanh", args[0]);
    return interp_make_double(ctx, tanh(as_double(args[0])));
}

static interp_val_t* builtin_asinh(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("asinh", 1); REQUIRE_NUMBER("asinh", args[0]);
    return interp_make_double(ctx, asinh(as_double(args[0])));
}

static interp_val_t* builtin_acosh(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("acosh", 1); REQUIRE_NUMBER("acosh", args[0]);
    return interp_make_double(ctx, acosh(as_double(args[0])));
}

static interp_val_t* builtin_atanh(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("atanh", 1); REQUIRE_NUMBER("atanh", args[0]);
    return interp_make_double(ctx, atanh(as_double(args[0])));
}

static interp_val_t* builtin_cbrt(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("cbrt", 1); REQUIRE_NUMBER("cbrt", args[0]);
    return interp_make_double(ctx, cbrt(as_double(args[0])));
}

static interp_val_t* builtin_exp2(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("exp2", 1); REQUIRE_NUMBER("exp2", args[0]);
    return interp_make_double(ctx, exp2(as_double(args[0])));
}

static interp_val_t* builtin_log2(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("log2", 1); REQUIRE_NUMBER("log2", args[0]);
    return interp_make_double(ctx, log2(as_double(args[0])));
}

static interp_val_t* builtin_log10(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("log10", 1); REQUIRE_NUMBER("log10", args[0]);
    return interp_make_double(ctx, log10(as_double(args[0])));
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

static interp_val_t* builtin_lcm(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("lcm", 2); REQUIRE_NUMBER("lcm", args[0]); REQUIRE_NUMBER("lcm", args[1]);
    int64_t a = llabs((int64_t)as_double(args[0])), b = llabs((int64_t)as_double(args[1]));
    if (a == 0 || b == 0) return interp_make_int(ctx, 0);
    int64_t ga = a, gb = b;
    while (gb) { int64_t t = gb; gb = ga % gb; ga = t; }
    return interp_make_int(ctx, (a / ga) * b);
}

static interp_val_t* builtin_nan_p(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("nan?", 1); REQUIRE_NUMBER("nan?", args[0]);
    return interp_make_bool(ctx, std::isnan(as_double(args[0])));
}

static interp_val_t* builtin_infinite_p(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("infinite?", 1); REQUIRE_NUMBER("infinite?", args[0]);
    return interp_make_bool(ctx, std::isinf(as_double(args[0])));
}

static interp_val_t* builtin_finite_p(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("finite?", 1); REQUIRE_NUMBER("finite?", args[0]);
    return interp_make_bool(ctx, std::isfinite(as_double(args[0])));
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
// Vector operations (vectors implemented as tagged cons-lists)
// ═══════════════════════════════════════════════════════════════════════════

// Vectors reuse list representation with a symbol tag for simplicity.
// A vector is stored as: (cons '#vector (cons elem0 (cons elem1 ... null)))

static const char* VECTOR_TAG = "#vector";

static bool is_vector(const interp_val_t* v) {
    return v && v->type == INTERP_VAL_CONS && v->cons.car &&
           v->cons.car->type == INTERP_VAL_SYMBOL &&
           strcmp(v->cons.car->symbol, VECTOR_TAG) == 0;
}

static interp_val_t* vector_data(interp_val_t* v) { return v->cons.cdr; }

static interp_val_t* builtin_vector(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    interp_val_t* lst = interp_make_null(ctx);
    for (int64_t i = (int64_t)n - 1; i >= 0; i--)
        lst = interp_make_cons(ctx, args[i], lst);
    return interp_make_cons(ctx, interp_make_symbol(ctx, VECTOR_TAG), lst);
}

static interp_val_t* builtin_make_vector(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    if (n < 1 || n > 2) return interp_make_error(ctx, "make-vector: expected 1-2 arguments");
    REQUIRE_NUMBER("make-vector", args[0]);
    int64_t len = (int64_t)as_double(args[0]);
    interp_val_t* fill = (n >= 2) ? args[1] : interp_make_int(ctx, 0);
    interp_val_t* lst = interp_make_null(ctx);
    for (int64_t i = len - 1; i >= 0; i--)
        lst = interp_make_cons(ctx, fill, lst);
    return interp_make_cons(ctx, interp_make_symbol(ctx, VECTOR_TAG), lst);
}

static interp_val_t* builtin_vector_length(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("vector-length", 1);
    if (!is_vector(args[0])) return interp_make_error(ctx, "vector-length: not a vector");
    int64_t len = 0;
    const interp_val_t* cur = vector_data(args[0]);
    while (cur && cur->type == INTERP_VAL_CONS) { len++; cur = cur->cons.cdr; }
    return interp_make_int(ctx, len);
}

static interp_val_t* builtin_vector_ref(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("vector-ref", 2);
    if (!is_vector(args[0])) return interp_make_error(ctx, "vector-ref: not a vector");
    REQUIRE_NUMBER("vector-ref", args[1]);
    int64_t idx = (int64_t)as_double(args[1]);
    const interp_val_t* cur = vector_data(args[0]);
    for (int64_t i = 0; i < idx; i++) {
        if (!cur || cur->type != INTERP_VAL_CONS) return interp_make_error(ctx, "vector-ref: index out of range");
        cur = cur->cons.cdr;
    }
    if (!cur || cur->type != INTERP_VAL_CONS) return interp_make_error(ctx, "vector-ref: index out of range");
    return cur->cons.car;
}

static interp_val_t* builtin_vector_set(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    if (n != 3) return interp_make_error(ctx, "vector-set!: expected 3 arguments");
    if (!is_vector(args[0])) return interp_make_error(ctx, "vector-set!: not a vector");
    REQUIRE_NUMBER("vector-set!", args[1]);
    int64_t idx = (int64_t)as_double(args[1]);
    interp_val_t* cur = vector_data(args[0]);
    for (int64_t i = 0; i < idx; i++) {
        if (!cur || cur->type != INTERP_VAL_CONS) return interp_make_error(ctx, "vector-set!: index out of range");
        cur = cur->cons.cdr;
    }
    if (!cur || cur->type != INTERP_VAL_CONS) return interp_make_error(ctx, "vector-set!: index out of range");
    cur->cons.car = args[2];
    return interp_make_void(ctx);
}

static interp_val_t* builtin_vector_to_list(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("vector->list", 1);
    if (!is_vector(args[0])) return interp_make_error(ctx, "vector->list: not a vector");
    return vector_data(args[0]);
}

static interp_val_t* builtin_list_to_vector(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("list->vector", 1);
    return interp_make_cons(ctx, interp_make_symbol(ctx, VECTOR_TAG), args[0]);
}

static interp_val_t* builtin_vector_p(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("vector?", 1);
    return interp_make_bool(ctx, is_vector(args[0]));
}

// ═══════════════════════════════════════════════════════════════════════════
// Tensor operations (element-wise on vectors)
// ═══════════════════════════════════════════════════════════════════════════

// Helper: apply element-wise binary op on two vectors
static interp_val_t* tensor_binop(const char* name, interp_val_t* a, interp_val_t* b,
                                   double (*op)(double, double), interp_ctx_t* ctx) {
    if (!is_vector(a) || !is_vector(b)) {
        char buf[64]; snprintf(buf, sizeof(buf), "%s: expected vectors", name);
        return interp_make_error(ctx, buf);
    }
    interp_val_t* da = vector_data(a);
    interp_val_t* db = vector_data(b);
    interp_val_t* items[4096];
    int count = 0;
    while (da && da->type == INTERP_VAL_CONS && db && db->type == INTERP_VAL_CONS && count < 4096) {
        if (!is_number(da->cons.car) || !is_number(db->cons.car))
            return interp_make_error(ctx, name);
        items[count++] = interp_make_double(ctx, op(as_double(da->cons.car), as_double(db->cons.car)));
        da = da->cons.cdr; db = db->cons.cdr;
    }
    interp_val_t* lst = interp_make_null(ctx);
    for (int i = count - 1; i >= 0; i--) lst = interp_make_cons(ctx, items[i], lst);
    return interp_make_cons(ctx, interp_make_symbol(ctx, VECTOR_TAG), lst);
}

static double op_add(double a, double b) { return a + b; }
static double op_sub(double a, double b) { return a - b; }
static double op_mul(double a, double b) { return a * b; }
static double op_div(double a, double b) { return a / b; }

static interp_val_t* builtin_tensor_add(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("tensor-add", 2); return tensor_binop("tensor-add", args[0], args[1], op_add, ctx);
}
static interp_val_t* builtin_tensor_sub(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("tensor-sub", 2); return tensor_binop("tensor-sub", args[0], args[1], op_sub, ctx);
}
static interp_val_t* builtin_tensor_mul(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("tensor-mul", 2); return tensor_binop("tensor-mul", args[0], args[1], op_mul, ctx);
}
static interp_val_t* builtin_tensor_div(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("tensor-div", 2); return tensor_binop("tensor-div", args[0], args[1], op_div, ctx);
}

static interp_val_t* builtin_tensor_dot(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("tensor-dot", 2);
    if (!is_vector(args[0]) || !is_vector(args[1]))
        return interp_make_error(ctx, "tensor-dot: expected vectors");
    interp_val_t* da = vector_data(args[0]);
    interp_val_t* db = vector_data(args[1]);
    double sum = 0;
    while (da && da->type == INTERP_VAL_CONS && db && db->type == INTERP_VAL_CONS) {
        if (!is_number(da->cons.car) || !is_number(db->cons.car))
            return interp_make_error(ctx, "tensor-dot: non-numeric element");
        sum += as_double(da->cons.car) * as_double(db->cons.car);
        da = da->cons.cdr; db = db->cons.cdr;
    }
    return interp_make_double(ctx, sum);
}

static interp_val_t* builtin_tensor_sum(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("tensor-sum", 1);
    if (!is_vector(args[0])) return interp_make_error(ctx, "tensor-sum: expected vector");
    double sum = 0;
    interp_val_t* d = vector_data(args[0]);
    while (d && d->type == INTERP_VAL_CONS) {
        if (!is_number(d->cons.car)) return interp_make_error(ctx, "tensor-sum: non-numeric");
        sum += as_double(d->cons.car);
        d = d->cons.cdr;
    }
    return interp_make_double(ctx, sum);
}

static interp_val_t* builtin_tensor_mean(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("tensor-mean", 1);
    if (!is_vector(args[0])) return interp_make_error(ctx, "tensor-mean: expected vector");
    double sum = 0; int64_t count = 0;
    interp_val_t* d = vector_data(args[0]);
    while (d && d->type == INTERP_VAL_CONS) {
        if (!is_number(d->cons.car)) return interp_make_error(ctx, "tensor-mean: non-numeric");
        sum += as_double(d->cons.car); count++;
        d = d->cons.cdr;
    }
    return count > 0 ? interp_make_double(ctx, sum / count) : interp_make_double(ctx, 0);
}

static interp_val_t* builtin_tensor_shape(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("tensor-shape", 1);
    if (!is_vector(args[0])) return interp_make_error(ctx, "tensor-shape: expected vector");
    int64_t len = 0;
    interp_val_t* d = vector_data(args[0]);
    while (d && d->type == INTERP_VAL_CONS) { len++; d = d->cons.cdr; }
    return interp_make_cons(ctx, interp_make_symbol(ctx, VECTOR_TAG),
        interp_make_cons(ctx, interp_make_int(ctx, len), interp_make_null(ctx)));
}

static interp_val_t* builtin_tensor_apply(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("tensor-apply", 2);
    if (!is_vector(args[1])) return interp_make_error(ctx, "tensor-apply: expected vector");
    interp_val_t* func = args[0];
    interp_val_t* d = vector_data(args[1]);
    interp_val_t* items[4096]; int count = 0;
    while (d && d->type == INTERP_VAL_CONS && count < 4096) {
        items[count] = interp_apply(func, &d->cons.car, 1, ctx);
        if (ctx->error_msg) return items[count];
        count++;
        d = d->cons.cdr;
    }
    interp_val_t* lst = interp_make_null(ctx);
    for (int i = count - 1; i >= 0; i--) lst = interp_make_cons(ctx, items[i], lst);
    return interp_make_cons(ctx, interp_make_symbol(ctx, VECTOR_TAG), lst);
}

static interp_val_t* builtin_norm(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("norm", 1);
    if (!is_vector(args[0])) return interp_make_error(ctx, "norm: expected vector");
    double sum = 0;
    interp_val_t* d = vector_data(args[0]);
    while (d && d->type == INTERP_VAL_CONS) {
        if (!is_number(d->cons.car)) return interp_make_error(ctx, "norm: non-numeric");
        double v = as_double(d->cons.car); sum += v * v;
        d = d->cons.cdr;
    }
    return interp_make_double(ctx, sqrt(sum));
}

static interp_val_t* builtin_zeros(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("zeros", 1); REQUIRE_NUMBER("zeros", args[0]);
    int64_t len = (int64_t)as_double(args[0]);
    interp_val_t* lst = interp_make_null(ctx);
    for (int64_t i = 0; i < len; i++) lst = interp_make_cons(ctx, interp_make_double(ctx, 0), lst);
    return interp_make_cons(ctx, interp_make_symbol(ctx, VECTOR_TAG), lst);
}

static interp_val_t* builtin_ones(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    REQUIRE_ARGS("ones", 1); REQUIRE_NUMBER("ones", args[0]);
    int64_t len = (int64_t)as_double(args[0]);
    interp_val_t* lst = interp_make_null(ctx);
    for (int64_t i = 0; i < len; i++) lst = interp_make_cons(ctx, interp_make_double(ctx, 1), lst);
    return interp_make_cons(ctx, interp_make_symbol(ctx, VECTOR_TAG), lst);
}

static interp_val_t* builtin_arange(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    if (n < 1 || n > 3) return interp_make_error(ctx, "arange: expected 1-3 arguments");
    for (uint64_t i = 0; i < n; i++) REQUIRE_NUMBER("arange", args[i]);
    double start = 0, end_val, step = 1;
    if (n == 1) { end_val = as_double(args[0]); }
    else { start = as_double(args[0]); end_val = as_double(args[1]); }
    if (n == 3) step = as_double(args[2]);
    if (step == 0) return interp_make_error(ctx, "arange: step cannot be zero");

    interp_val_t* items[4096]; int count = 0;
    for (double v = start; step > 0 ? v < end_val : v > end_val; v += step) {
        if (count >= 4096) break;
        items[count++] = interp_make_double(ctx, v);
    }
    interp_val_t* lst = interp_make_null(ctx);
    for (int i = count - 1; i >= 0; i--) lst = interp_make_cons(ctx, items[i], lst);
    return interp_make_cons(ctx, interp_make_symbol(ctx, VECTOR_TAG), lst);
}

static interp_val_t* builtin_linspace(interp_val_t** args, uint64_t n, interp_ctx_t* ctx) {
    if (n != 3) return interp_make_error(ctx, "linspace: expected 3 arguments (start end count)");
    for (uint64_t i = 0; i < 3; i++) REQUIRE_NUMBER("linspace", args[i]);
    double start = as_double(args[0]), end_val = as_double(args[1]);
    int64_t count = (int64_t)as_double(args[2]);
    if (count < 1) return interp_make_cons(ctx, interp_make_symbol(ctx, VECTOR_TAG), interp_make_null(ctx));

    interp_val_t* items[4096];
    int num = count > 4096 ? 4096 : (int)count;
    for (int i = 0; i < num; i++) {
        double t = (count == 1) ? 0 : (double)i / (count - 1);
        items[i] = interp_make_double(ctx, start + t * (end_val - start));
    }
    interp_val_t* lst = interp_make_null(ctx);
    for (int i = num - 1; i >= 0; i--) lst = interp_make_cons(ctx, items[i], lst);
    return interp_make_cons(ctx, interp_make_symbol(ctx, VECTOR_TAG), lst);
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
    reg(ctx, "%", builtin_modulo, 2, 2);
    reg(ctx, "abs", builtin_abs, 1, 1);
    reg(ctx, "fabs", builtin_abs, 1, 1);
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
    reg(ctx, "map", builtin_map, 2, -1);
    reg(ctx, "filter", builtin_filter, 2, 2);
    reg(ctx, "for-each", builtin_for_each, 2, -1);
    reg(ctx, "apply", builtin_apply, 2, -1);
    reg(ctx, "assoc", builtin_assoc, 2, 2);
    reg(ctx, "assv", builtin_assoc, 2, 2);
    reg(ctx, "assq", builtin_assoc, 2, 2);
    reg(ctx, "member", builtin_member, 2, 2);
    reg(ctx, "memv", builtin_member, 2, 2);
    reg(ctx, "memq", builtin_member, 2, 2);
    reg(ctx, "caaar", builtin_caaar, 1, 1);
    reg(ctx, "caadr", builtin_caadr, 1, 1);
    reg(ctx, "cadar", builtin_cadar, 1, 1);
    reg(ctx, "cdaar", builtin_cdaar, 1, 1);
    reg(ctx, "cdadr", builtin_cdadr, 1, 1);
    reg(ctx, "cddar", builtin_cddar, 1, 1);
    reg(ctx, "cdddr", builtin_cdddr, 1, 1);
    reg(ctx, "set-car!", builtin_set_car, 2, 2);
    reg(ctx, "set-cdr!", builtin_set_cdr, 2, 2);
    reg(ctx, "range", builtin_range, 1, 3);
    reg(ctx, "iota", builtin_range, 1, 3);
    reg(ctx, "last", builtin_last, 1, 1);
    reg(ctx, "make-list", builtin_make_list, 1, 2);
    reg(ctx, "list*", builtin_list_star, 1, -1);
    reg(ctx, "take", builtin_take, 2, 2);
    reg(ctx, "drop", builtin_drop, 2, 2);
    reg(ctx, "zip", builtin_zip, 2, 2);
    reg(ctx, "flatten", builtin_flatten, 1, 1);
    reg(ctx, "reduce", builtin_reduce, 3, 3);
    reg(ctx, "fold", builtin_reduce, 3, 3);
    reg(ctx, "foldl", builtin_reduce, 3, 3);
    reg(ctx, "remove", builtin_remove, 2, 2);
    reg(ctx, "sort", builtin_sort, 1, 2);
    reg(ctx, "find", builtin_find, 2, 2);
    reg(ctx, "split-at", builtin_split_at, 2, 2);
    reg(ctx, "fold-right", builtin_fold_right, 3, 3);
    reg(ctx, "foldr", builtin_fold_right, 3, 3);
    reg(ctx, "last-pair", builtin_last_pair, 1, 1);
    reg(ctx, "partition", builtin_partition, 2, 2);
    reg(ctx, "unique", builtin_unique, 1, 1);

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
    reg(ctx, "exp2", builtin_exp2, 1, 1);
    reg(ctx, "log", builtin_log, 1, 1);
    reg(ctx, "log2", builtin_log2, 1, 1);
    reg(ctx, "log10", builtin_log10, 1, 1);
    reg(ctx, "sinh", builtin_sinh, 1, 1);
    reg(ctx, "cosh", builtin_cosh, 1, 1);
    reg(ctx, "tanh", builtin_tanh, 1, 1);
    reg(ctx, "asinh", builtin_asinh, 1, 1);
    reg(ctx, "acosh", builtin_acosh, 1, 1);
    reg(ctx, "atanh", builtin_atanh, 1, 1);
    reg(ctx, "cbrt", builtin_cbrt, 1, 1);
    reg(ctx, "lcm", builtin_lcm, 2, 2);
    reg(ctx, "nan?", builtin_nan_p, 1, 1);
    reg(ctx, "infinite?", builtin_infinite_p, 1, 1);
    reg(ctx, "finite?", builtin_finite_p, 1, 1);

    // String
    reg(ctx, "string-length", builtin_string_length, 1, 1);
    reg(ctx, "string-append", builtin_string_append, 0, -1);
    reg(ctx, "string-ref", builtin_string_ref, 2, 2);
    reg(ctx, "substring", builtin_substring, 2, 3);
    reg(ctx, "string-contains", builtin_string_contains, 2, 2);
    reg(ctx, "string-contains?", builtin_string_contains, 2, 2);
    reg(ctx, "string-upcase", builtin_string_upcase, 1, 1);
    reg(ctx, "string-downcase", builtin_string_downcase, 1, 1);
    reg(ctx, "make-string", builtin_make_string, 1, 2);
    reg(ctx, "string-set!", builtin_string_set, 3, 3);
    reg(ctx, "string->list", builtin_string_to_list, 1, 1);
    reg(ctx, "list->string", builtin_list_to_string, 1, 1);
    reg(ctx, "string=?", builtin_string_eq, 2, 2);
    reg(ctx, "string<?", builtin_string_lt, 2, 2);
    reg(ctx, "string>?", builtin_string_gt, 2, 2);
    reg(ctx, "char=?", builtin_char_eq, 2, 2);
    reg(ctx, "char<?", builtin_char_lt, 2, 2);
    reg(ctx, "number->string", builtin_number_to_string, 1, 1);
    reg(ctx, "string->number", builtin_string_to_number, 1, 1);
    reg(ctx, "symbol->string", builtin_symbol_to_string, 1, 1);
    reg(ctx, "string->symbol", builtin_string_to_symbol, 1, 1);

    // Vector
    reg(ctx, "vector", builtin_vector, 0, -1);
    reg(ctx, "make-vector", builtin_make_vector, 1, 2);
    reg(ctx, "vector-length", builtin_vector_length, 1, 1);
    reg(ctx, "vector-ref", builtin_vector_ref, 2, 2);
    reg(ctx, "vector-set!", builtin_vector_set, 3, 3);
    reg(ctx, "vector->list", builtin_vector_to_list, 1, 1);
    reg(ctx, "list->vector", builtin_list_to_vector, 1, 1);
    reg(ctx, "vector?", builtin_vector_p, 1, 1);
    reg(ctx, "vref", builtin_vector_ref, 2, 2);
    reg(ctx, "tensor-add", builtin_tensor_add, 2, 2);
    reg(ctx, "tensor-sub", builtin_tensor_sub, 2, 2);
    reg(ctx, "tensor-mul", builtin_tensor_mul, 2, 2);
    reg(ctx, "tensor-div", builtin_tensor_div, 2, 2);
    reg(ctx, "tensor-dot", builtin_tensor_dot, 2, 2);
    reg(ctx, "tensor-sum", builtin_tensor_sum, 1, 1);
    reg(ctx, "tensor-mean", builtin_tensor_mean, 1, 1);
    reg(ctx, "tensor-shape", builtin_tensor_shape, 1, 1);
    reg(ctx, "tensor-apply", builtin_tensor_apply, 2, 2);
    reg(ctx, "norm", builtin_norm, 1, 1);
    reg(ctx, "zeros", builtin_zeros, 1, 1);
    reg(ctx, "ones", builtin_ones, 1, 1);
    reg(ctx, "arange", builtin_arange, 1, 3);
    reg(ctx, "linspace", builtin_linspace, 3, 3);

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
    reg(ctx, "print", builtin_display, 1, 1);  // alias
    reg(ctx, "type-of", builtin_type_of, 1, 1);
    reg(ctx, "random", builtin_random, 0, 1);
    reg(ctx, "current-seconds", builtin_current_seconds, 0, 0);
    reg(ctx, "square", builtin_square, 1, 1);
    reg(ctx, "negate", builtin_negate, 1, 1);

    // Constants
    interp_frame_define(ctx->env, "#t", interp_make_bool(ctx, true));
    interp_frame_define(ctx->env, "#f", interp_make_bool(ctx, false));
}
