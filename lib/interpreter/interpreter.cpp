/*
 * Eshkol Tree-Walking Interpreter
 */
#include "interpreter.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>

// ═══════════════════════════════════════════════════════════════════════════
// Memory helpers — simple malloc for now (could use arena later)
// ═══════════════════════════════════════════════════════════════════════════

static interp_val_t* alloc_val(interp_ctx_t* /*ctx*/) {
    auto* v = (interp_val_t*)calloc(1, sizeof(interp_val_t));
    return v;
}

// ═══════════════════════════════════════════════════════════════════════════
// Value constructors
// ═══════════════════════════════════════════════════════════════════════════

interp_val_t* interp_make_null(interp_ctx_t* ctx) {
    auto* v = alloc_val(ctx);
    v->type = INTERP_VAL_NULL;
    return v;
}

interp_val_t* interp_make_int(interp_ctx_t* ctx, int64_t n) {
    auto* v = alloc_val(ctx);
    v->type = INTERP_VAL_INT;
    v->int_val = n;
    return v;
}

interp_val_t* interp_make_double(interp_ctx_t* ctx, double d) {
    auto* v = alloc_val(ctx);
    v->type = INTERP_VAL_DOUBLE;
    v->double_val = d;
    return v;
}

interp_val_t* interp_make_bool(interp_ctx_t* ctx, bool b) {
    auto* v = alloc_val(ctx);
    v->type = INTERP_VAL_BOOL;
    v->bool_val = b;
    return v;
}

interp_val_t* interp_make_char(interp_ctx_t* ctx, char c) {
    auto* v = alloc_val(ctx);
    v->type = INTERP_VAL_CHAR;
    v->char_val = c;
    return v;
}

interp_val_t* interp_make_string(interp_ctx_t* ctx, const char* s, uint64_t len) {
    auto* v = alloc_val(ctx);
    v->type = INTERP_VAL_STRING;
    v->str_val.ptr = (char*)malloc(len + 1);
    memcpy(v->str_val.ptr, s, len);
    v->str_val.ptr[len] = '\0';
    v->str_val.len = len;
    return v;
}

interp_val_t* interp_make_cons(interp_ctx_t* ctx, interp_val_t* car, interp_val_t* cdr) {
    auto* v = alloc_val(ctx);
    v->type = INTERP_VAL_CONS;
    v->cons.car = car;
    v->cons.cdr = cdr;
    return v;
}

interp_val_t* interp_make_void(interp_ctx_t* ctx) {
    auto* v = alloc_val(ctx);
    v->type = INTERP_VAL_VOID;
    return v;
}

interp_val_t* interp_make_error(interp_ctx_t* ctx, const char* msg) {
    auto* v = alloc_val(ctx);
    v->type = INTERP_VAL_ERROR;
    v->error_msg = strdup(msg);
    ctx->error_msg = v->error_msg;
    return v;
}

interp_val_t* interp_make_symbol(interp_ctx_t* ctx, const char* name) {
    auto* v = alloc_val(ctx);
    v->type = INTERP_VAL_SYMBOL;
    v->symbol = strdup(name);
    return v;
}

// ═══════════════════════════════════════════════════════════════════════════
// Value utilities
// ═══════════════════════════════════════════════════════════════════════════

bool interp_val_is_truthy(const interp_val_t* val) {
    if (!val) return false;
    if (val->type == INTERP_VAL_BOOL) return val->bool_val;
    if (val->type == INTERP_VAL_NULL) return false;
    return true; // everything else is truthy in Scheme
}

const char* interp_val_type_name(const interp_val_t* val) {
    if (!val) return "null";
    switch (val->type) {
        case INTERP_VAL_NULL: return "null";
        case INTERP_VAL_INT: return "integer";
        case INTERP_VAL_DOUBLE: return "real";
        case INTERP_VAL_BOOL: return "boolean";
        case INTERP_VAL_CHAR: return "char";
        case INTERP_VAL_STRING: return "string";
        case INTERP_VAL_CONS: return "pair";
        case INTERP_VAL_CLOSURE: return "procedure";
        case INTERP_VAL_BUILTIN: return "procedure";
        case INTERP_VAL_VOID: return "void";
        case INTERP_VAL_SYMBOL: return "symbol";
        case INTERP_VAL_ERROR: return "error";
    }
    return "unknown";
}

// Check if a value is a proper list (chain of cons ending in null)
static bool is_proper_list(const interp_val_t* val) {
    while (val && val->type == INTERP_VAL_CONS) {
        val = val->cons.cdr;
    }
    return val && val->type == INTERP_VAL_NULL;
}

// Count list length
static uint64_t list_length(const interp_val_t* val) {
    uint64_t n = 0;
    while (val && val->type == INTERP_VAL_CONS) {
        n++;
        val = val->cons.cdr;
    }
    return n;
}

char* interp_val_to_string(const interp_val_t* val) {
    if (!val) return strdup("()");

    char buf[256];
    switch (val->type) {
        case INTERP_VAL_NULL:
            return strdup("()");
        case INTERP_VAL_INT:
            snprintf(buf, sizeof(buf), "%lld", (long long)val->int_val);
            return strdup(buf);
        case INTERP_VAL_DOUBLE: {
            // Print nicely — integer-valued doubles without decimal
            if (val->double_val == (int64_t)val->double_val &&
                val->double_val >= -1e15 && val->double_val <= 1e15) {
                snprintf(buf, sizeof(buf), "%lld.0", (long long)val->double_val);
            } else {
                snprintf(buf, sizeof(buf), "%.15g", val->double_val);
            }
            return strdup(buf);
        }
        case INTERP_VAL_BOOL:
            return strdup(val->bool_val ? "#t" : "#f");
        case INTERP_VAL_CHAR:
            if (val->char_val == ' ') return strdup("#\\space");
            if (val->char_val == '\n') return strdup("#\\newline");
            if (val->char_val == '\t') return strdup("#\\tab");
            snprintf(buf, sizeof(buf), "#\\%c", val->char_val);
            return strdup(buf);
        case INTERP_VAL_STRING:
            snprintf(buf, sizeof(buf), "\"%.*s\"", (int)val->str_val.len, val->str_val.ptr);
            return strdup(buf);
        case INTERP_VAL_SYMBOL:
            return strdup(val->symbol);
        case INTERP_VAL_CONS: {
            // Build list string
            std::string result = "(";
            const interp_val_t* cur = val;
            bool first = true;
            while (cur && cur->type == INTERP_VAL_CONS) {
                if (!first) result += " ";
                first = false;
                char* s = interp_val_to_string(cur->cons.car);
                result += s;
                free(s);
                cur = cur->cons.cdr;
            }
            if (cur && cur->type != INTERP_VAL_NULL) {
                result += " . ";
                char* s = interp_val_to_string(cur);
                result += s;
                free(s);
            }
            result += ")";
            return strdup(result.c_str());
        }
        case INTERP_VAL_CLOSURE:
            return strdup("#<procedure>");
        case INTERP_VAL_BUILTIN:
            snprintf(buf, sizeof(buf), "#<builtin:%s>", val->builtin.name);
            return strdup(buf);
        case INTERP_VAL_VOID:
            return strdup("");
        case INTERP_VAL_ERROR:
            snprintf(buf, sizeof(buf), "error: %s", val->error_msg ? val->error_msg : "unknown");
            return strdup(buf);
    }
    return strdup("#<unknown>");
}

// ═══════════════════════════════════════════════════════════════════════════
// Environment
// ═══════════════════════════════════════════════════════════════════════════

interp_frame_t* interp_frame_create(interp_frame_t* parent) {
    auto* f = (interp_frame_t*)calloc(1, sizeof(interp_frame_t));
    f->parent = parent;
    f->capacity = 16;
    f->bindings = (interp_binding_t*)calloc(f->capacity, sizeof(interp_binding_t));
    f->count = 0;
    return f;
}

void interp_frame_define(interp_frame_t* frame, const char* name, interp_val_t* value) {
    // Check if already defined in this frame — update
    for (uint64_t i = 0; i < frame->count; i++) {
        if (strcmp(frame->bindings[i].name, name) == 0) {
            frame->bindings[i].value = value;
            return;
        }
    }
    // New binding
    if (frame->count >= frame->capacity) {
        frame->capacity *= 2;
        frame->bindings = (interp_binding_t*)realloc(frame->bindings,
            frame->capacity * sizeof(interp_binding_t));
    }
    frame->bindings[frame->count].name = strdup(name);
    frame->bindings[frame->count].value = value;
    frame->count++;
}

interp_val_t* interp_frame_lookup(interp_frame_t* frame, const char* name) {
    while (frame) {
        for (uint64_t i = 0; i < frame->count; i++) {
            if (strcmp(frame->bindings[i].name, name) == 0) {
                return frame->bindings[i].value;
            }
        }
        frame = frame->parent;
    }
    return nullptr;
}

bool interp_frame_set(interp_frame_t* frame, const char* name, interp_val_t* value) {
    while (frame) {
        for (uint64_t i = 0; i < frame->count; i++) {
            if (strcmp(frame->bindings[i].name, name) == 0) {
                frame->bindings[i].value = value;
                return true;
            }
        }
        frame = frame->parent;
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════════════════
// Output capture
// ═══════════════════════════════════════════════════════════════════════════

void interp_output_append(interp_ctx_t* ctx, const char* str, uint64_t len) {
    while (ctx->output_len + len >= ctx->output_cap) {
        ctx->output_cap = ctx->output_cap ? ctx->output_cap * 2 : 256;
        ctx->output_buf = (char*)realloc(ctx->output_buf, ctx->output_cap);
    }
    memcpy(ctx->output_buf + ctx->output_len, str, len);
    ctx->output_len += len;
    ctx->output_buf[ctx->output_len] = '\0';
}

// ═══════════════════════════════════════════════════════════════════════════
// Context
// ═══════════════════════════════════════════════════════════════════════════

interp_ctx_t* interp_ctx_create(void) {
    auto* ctx = (interp_ctx_t*)calloc(1, sizeof(interp_ctx_t));
    ctx->env = interp_frame_create(nullptr);
    ctx->max_recursion_depth = 10000;
    interp_register_builtins(ctx);
    return ctx;
}

void interp_ctx_destroy(interp_ctx_t* ctx) {
    if (!ctx) return;
    free(ctx->output_buf);
    // Note: not freeing all allocated values — simple for now
    free(ctx);
}

void interp_ctx_reset(interp_ctx_t* ctx) {
    if (!ctx) return;
    ctx->env = interp_frame_create(nullptr);
    ctx->error_msg = nullptr;
    ctx->output_len = 0;
    if (ctx->output_buf) ctx->output_buf[0] = '\0';
    interp_register_builtins(ctx);
}

// Forward declarations
static interp_val_t* interp_ast_to_datum(const eshkol_ast_t* ast, interp_ctx_t* ctx);
interp_val_t* interp_eval_tail(const eshkol_ast_t* ast, interp_ctx_t* ctx);

// ═══════════════════════════════════════════════════════════════════════════
// Apply — call a closure or builtin with evaluated arguments
// ═══════════════════════════════════════════════════════════════════════════

// Helper: bind closure params and set up call frame
static interp_frame_t* setup_closure_frame(interp_val_t* func, interp_val_t** args,
                                            uint64_t num_args, interp_ctx_t* ctx) {
    auto& cl = func->closure;
    interp_frame_t* call_frame = interp_frame_create(cl.env);

    uint64_t fixed = cl.num_params;
    // num_params is the count of fixed params (rest param is separate)

    for (uint64_t i = 0; i < fixed && i < num_args; i++) {
        if (cl.params && cl.params[i].type == ESHKOL_VAR) {
            interp_frame_define(call_frame, cl.params[i].variable.id, args[i]);
        }
    }

    if (cl.is_variadic && cl.rest_param) {
        interp_val_t* rest = interp_make_null(ctx);
        for (int64_t i = (int64_t)num_args - 1; i >= (int64_t)fixed; i--) {
            rest = interp_make_cons(ctx, args[i], rest);
        }
        interp_frame_define(call_frame, cl.rest_param, rest);
    }

    return call_frame;
}

// Create a tail-call thunk (avoids C stack growth)
static interp_val_t* make_tail_call(interp_ctx_t* ctx, interp_val_t* func,
                                     interp_val_t** args, uint64_t num_args) {
    auto* v = (interp_val_t*)calloc(1, sizeof(interp_val_t));
    v->type = INTERP_VAL_TAIL_CALL;
    v->tail_call.func = func;
    // Copy args array
    v->tail_call.args = (interp_val_t**)malloc(num_args * sizeof(interp_val_t*));
    memcpy(v->tail_call.args, args, num_args * sizeof(interp_val_t*));
    v->tail_call.num_args = num_args;
    return v;
}

interp_val_t* interp_apply(interp_val_t* func, interp_val_t** args,
                           uint64_t num_args, interp_ctx_t* ctx) {
    // Trampoline loop: keep executing tail calls without growing C stack
    while (true) {
        if (!func) return interp_make_error(ctx, "cannot call null");

        if (func->type == INTERP_VAL_BUILTIN) {
            if (func->builtin.min_arity >= 0 && (int)num_args < func->builtin.min_arity) {
                char buf[128];
                snprintf(buf, sizeof(buf), "%s: expected at least %d arguments, got %llu",
                         func->builtin.name, func->builtin.min_arity, (unsigned long long)num_args);
                return interp_make_error(ctx, buf);
            }
            if (func->builtin.max_arity >= 0 && (int)num_args > func->builtin.max_arity) {
                char buf[128];
                snprintf(buf, sizeof(buf), "%s: expected at most %d arguments, got %llu",
                         func->builtin.name, func->builtin.max_arity, (unsigned long long)num_args);
                return interp_make_error(ctx, buf);
            }
            interp_val_t* result = func->builtin.fn(args, num_args, ctx);
            // Trampoline: if builtin returned a tail call, continue loop
            if (result && result->type == INTERP_VAL_TAIL_CALL) {
                func = result->tail_call.func;
                args = result->tail_call.args;
                num_args = result->tail_call.num_args;
                continue;
            }
            return result;
        }

        if (func->type == INTERP_VAL_CLOSURE) {
            ctx->recursion_depth++;
            if (ctx->recursion_depth > ctx->max_recursion_depth) {
                ctx->recursion_depth--;
                return interp_make_error(ctx, "maximum recursion depth exceeded");
            }

            interp_frame_t* call_frame = setup_closure_frame(func, args, num_args, ctx);
            interp_frame_t* saved_env = ctx->env;
            ctx->env = call_frame;

            auto& cl = func->closure;
            interp_val_t* result;
            if (cl.body_ast) {
                result = interp_eval_tail(cl.body_ast, ctx);
            } else if (cl.body_op) {
                result = interp_eval_op(cl.body_op, ctx);
            } else {
                result = interp_make_void(ctx);
            }

            ctx->env = saved_env;
            ctx->recursion_depth--;

            // Trampoline: if body returned a tail call, loop instead of recursing
            if (result && result->type == INTERP_VAL_TAIL_CALL) {
                func = result->tail_call.func;
                args = result->tail_call.args;
                num_args = result->tail_call.num_args;
                continue;
            }
            return result;
        }

        char buf[64];
        snprintf(buf, sizeof(buf), "cannot call %s", interp_val_type_name(func));
        return interp_make_error(ctx, buf);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Resolve tail calls — use when result must be a value (non-tail position)
// ═══════════════════════════════════════════════════════════════════════════

static interp_val_t* resolve_tail_calls(interp_val_t* val, interp_ctx_t* ctx) {
    while (val && val->type == INTERP_VAL_TAIL_CALL) {
        val = interp_apply(val->tail_call.func, val->tail_call.args,
                           val->tail_call.num_args, ctx);
    }
    return val;
}

// ═══════════════════════════════════════════════════════════════════════════
// Helper to get a numeric value as double
// ═══════════════════════════════════════════════════════════════════════════

static bool val_is_number(const interp_val_t* v) {
    return v && (v->type == INTERP_VAL_INT || v->type == INTERP_VAL_DOUBLE);
}

static double val_as_double(const interp_val_t* v) {
    if (v->type == INTERP_VAL_INT) return (double)v->int_val;
    return v->double_val;
}

// ═══════════════════════════════════════════════════════════════════════════
// Eval operation
// ═══════════════════════════════════════════════════════════════════════════

interp_val_t* interp_eval_op(const eshkol_operations_t* op, interp_ctx_t* ctx) {
    if (!op) return interp_make_null(ctx);

    switch (op->op) {

    // ── define ──────────────────────────────────────────────────────
    case ESHKOL_DEFINE_OP: {
        const auto& d = op->define_op;
        interp_val_t* val;

        if (d.is_function) {
            // (define (name params...) body)
            auto* closure = alloc_val(ctx);
            closure->type = INTERP_VAL_CLOSURE;
            closure->closure.body_ast = d.value;
            closure->closure.body_op = nullptr;
            closure->closure.params = d.parameters;
            closure->closure.num_params = d.num_params;
            closure->closure.is_variadic = d.is_variadic;
            closure->closure.rest_param = d.rest_param;
            closure->closure.env = ctx->env;
            val = closure;
        } else {
            val = interp_eval(d.value, ctx);
            if (ctx->error_msg) return val;
        }

        interp_frame_define(ctx->env, d.name, val);
        return interp_make_void(ctx);
    }

    // ── lambda ─────────────────────────────────────────────────────
    case ESHKOL_LAMBDA_OP: {
        const auto& l = op->lambda_op;
        auto* closure = alloc_val(ctx);
        closure->type = INTERP_VAL_CLOSURE;
        closure->closure.body_ast = l.body;
        closure->closure.body_op = nullptr;
        closure->closure.params = l.parameters;
        closure->closure.num_params = l.num_params;
        closure->closure.is_variadic = l.is_variadic;
        closure->closure.rest_param = l.rest_param;
        closure->closure.env = ctx->env;
        return closure;
    }

    // ── call (also used for if, arithmetic, when, unless, cond) ────
    case ESHKOL_CALL_OP: {
        const auto& c = op->call_op;

        // Special forms stored as CALL_OP with func name
        if (c.func && c.func->type == ESHKOL_VAR && c.func->variable.id) {
            const char* fname = c.func->variable.id;

            // if
            if (strcmp(fname, "if") == 0 && c.num_vars >= 2) {
                interp_val_t* cond = interp_eval(&c.variables[0], ctx);
                if (ctx->error_msg) return cond;
                if (interp_val_is_truthy(cond)) {
                    return interp_eval_tail(&c.variables[1], ctx);
                } else if (c.num_vars >= 3) {
                    return interp_eval_tail(&c.variables[2], ctx);
                }
                return interp_make_void(ctx);
            }

            // begin — evaluate sequentially (defines need this)
            if (strcmp(fname, "begin") == 0) {
                if (c.num_vars == 0) return interp_make_void(ctx);
                for (uint64_t i = 0; i + 1 < c.num_vars; i++) {
                    interp_val_t* r = interp_eval(&c.variables[i], ctx);
                    if (ctx->error_msg) return r;
                }
                return interp_eval_tail(&c.variables[c.num_vars - 1], ctx);
            }
        }

        // Regular function call — return as tail call thunk
        // The trampoline in interp_apply will execute it
        interp_val_t* func = interp_eval(c.func, ctx);
        if (ctx->error_msg) return func;

        interp_val_t** args = nullptr;
        if (c.num_vars > 0) {
            args = (interp_val_t**)malloc(c.num_vars * sizeof(interp_val_t*));
            for (uint64_t i = 0; i < c.num_vars; i++) {
                args[i] = interp_eval(&c.variables[i], ctx);
                if (ctx->error_msg) {
                    free(args);
                    return args[i];
                }
            }
        }

        // Return tail call thunk — the trampoline will handle it
        return make_tail_call(ctx, func, args ? args : nullptr, c.num_vars);
    }

    // ── Arithmetic ops stored as ops (ADD_OP, SUB_OP, etc.) ────────
    case ESHKOL_ADD_OP:
    case ESHKOL_SUB_OP:
    case ESHKOL_MUL_OP:
    case ESHKOL_DIV_OP: {
        // These use call_op structure
        const auto& c = op->call_op;
        interp_val_t** args = nullptr;
        uint64_t nargs = c.num_vars;
        if (nargs > 0) {
            args = (interp_val_t**)malloc(nargs * sizeof(interp_val_t*));
            for (uint64_t i = 0; i < nargs; i++) {
                args[i] = interp_eval(&c.variables[i], ctx);
                if (ctx->error_msg) { free(args); return args[i]; }
            }
        }

        // Perform arithmetic
        bool has_double = false;
        for (uint64_t i = 0; i < nargs; i++) {
            if (!val_is_number(args[i])) {
                free(args);
                return interp_make_error(ctx, "arithmetic on non-number");
            }
            if (args[i]->type == INTERP_VAL_DOUBLE) has_double = true;
        }

        interp_val_t* result;
        if (has_double) {
            double acc;
            switch (op->op) {
                case ESHKOL_ADD_OP: acc = 0; break;
                case ESHKOL_MUL_OP: acc = 1; break;
                default: acc = nargs > 0 ? val_as_double(args[0]) : 0; break;
            }
            uint64_t start = (op->op == ESHKOL_ADD_OP || op->op == ESHKOL_MUL_OP) ? 0 : 1;
            if (op->op == ESHKOL_SUB_OP && nargs == 1) { acc = -val_as_double(args[0]); start = 1; }
            for (uint64_t i = start; i < nargs; i++) {
                double v = val_as_double(args[i]);
                switch (op->op) {
                    case ESHKOL_ADD_OP: acc += v; break;
                    case ESHKOL_SUB_OP: acc -= v; break;
                    case ESHKOL_MUL_OP: acc *= v; break;
                    case ESHKOL_DIV_OP:
                        if (v == 0) { free(args); return interp_make_error(ctx, "division by zero"); }
                        acc /= v; break;
                    default: break;
                }
            }
            result = interp_make_double(ctx, acc);
        } else {
            int64_t acc;
            switch (op->op) {
                case ESHKOL_ADD_OP: acc = 0; break;
                case ESHKOL_MUL_OP: acc = 1; break;
                default: acc = nargs > 0 ? args[0]->int_val : 0; break;
            }
            uint64_t start = (op->op == ESHKOL_ADD_OP || op->op == ESHKOL_MUL_OP) ? 0 : 1;
            if (op->op == ESHKOL_SUB_OP && nargs == 1) { acc = -args[0]->int_val; start = 1; }
            for (uint64_t i = start; i < nargs; i++) {
                int64_t v = args[i]->int_val;
                switch (op->op) {
                    case ESHKOL_ADD_OP: acc += v; break;
                    case ESHKOL_SUB_OP: acc -= v; break;
                    case ESHKOL_MUL_OP: acc *= v; break;
                    case ESHKOL_DIV_OP:
                        if (v == 0) { free(args); return interp_make_error(ctx, "division by zero"); }
                        acc /= v; break;
                    default: break;
                }
            }
            result = interp_make_int(ctx, acc);
        }
        free(args);
        return result;
    }

    // ── sequence (begin) ───────────────────────────────────────────
    case ESHKOL_SEQUENCE_OP: {
        const auto& s = op->sequence_op;
        if (s.num_expressions == 0) return interp_make_void(ctx);
        // Eval all but last normally
        for (uint64_t i = 0; i + 1 < s.num_expressions; i++) {
            interp_val_t* r = interp_eval(&s.expressions[i], ctx);
            if (ctx->error_msg) return r;
        }
        // Last expression in tail position
        return interp_eval_tail(&s.expressions[s.num_expressions - 1], ctx);
    }

    // ── if (direct form with nested operations) ────────────────────
    case ESHKOL_IF_OP: {
        // Note: the parser actually stores if as CALL_OP, but handle this just in case
        if (op->if_op.if_true) {
            interp_val_t* cond = interp_eval_op(op->if_op.if_true, ctx);
            if (ctx->error_msg) return cond;
            if (interp_val_is_truthy(cond)) {
                return interp_eval_op(op->if_op.if_true, ctx);
            } else if (op->if_op.if_false) {
                return interp_eval_op(op->if_op.if_false, ctx);
            }
        }
        return interp_make_void(ctx);
    }

    // ── let ────────────────────────────────────────────────────────
    case ESHKOL_LET_OP: {
        const auto& l = op->let_op;
        interp_frame_t* let_frame = interp_frame_create(ctx->env);

        // Eval all bindings (parallel — in the parent scope)
        for (uint64_t i = 0; i < l.num_bindings; i++) {
            const eshkol_ast_t* binding = &l.bindings[i];
            // Bindings are stored as cons: (variable . value)
            if (binding->type == ESHKOL_CONS && binding->cons_cell.car &&
                binding->cons_cell.car->type == ESHKOL_VAR) {
                interp_val_t* val = interp_eval(binding->cons_cell.cdr, ctx);
                if (ctx->error_msg) return val;
                interp_frame_define(let_frame, binding->cons_cell.car->variable.id, val);
            }
        }

        interp_frame_t* saved = ctx->env;
        ctx->env = let_frame;
        interp_val_t* result = interp_eval_tail(l.body, ctx);
        ctx->env = saved;
        return result;
    }

    // ── let* ───────────────────────────────────────────────────────
    case ESHKOL_LET_STAR_OP: {
        const auto& l = op->let_op;  // same struct as let_op
        interp_frame_t* let_frame = interp_frame_create(ctx->env);
        interp_frame_t* saved = ctx->env;
        ctx->env = let_frame;

        // Sequential bindings — each sees previous
        for (uint64_t i = 0; i < l.num_bindings; i++) {
            const eshkol_ast_t* binding = &l.bindings[i];
            if (binding->type == ESHKOL_CONS && binding->cons_cell.car &&
                binding->cons_cell.car->type == ESHKOL_VAR) {
                interp_val_t* val = interp_eval(binding->cons_cell.cdr, ctx);
                if (ctx->error_msg) { ctx->env = saved; return val; }
                interp_frame_define(let_frame, binding->cons_cell.car->variable.id, val);
            }
        }

        interp_val_t* result = interp_eval(l.body, ctx);
        ctx->env = saved;
        return result;
    }

    // ── letrec ─────────────────────────────────────────────────────
    case ESHKOL_LETREC_OP: {
        const auto& l = op->let_op;
        interp_frame_t* let_frame = interp_frame_create(ctx->env);
        interp_frame_t* saved = ctx->env;
        ctx->env = let_frame;

        // First pass: bind all names to void
        for (uint64_t i = 0; i < l.num_bindings; i++) {
            const eshkol_ast_t* binding = &l.bindings[i];
            if (binding->type == ESHKOL_CONS && binding->cons_cell.car &&
                binding->cons_cell.car->type == ESHKOL_VAR) {
                interp_frame_define(let_frame, binding->cons_cell.car->variable.id,
                                    interp_make_void(ctx));
            }
        }

        // Second pass: eval values (all names visible)
        for (uint64_t i = 0; i < l.num_bindings; i++) {
            const eshkol_ast_t* binding = &l.bindings[i];
            if (binding->type == ESHKOL_CONS && binding->cons_cell.car &&
                binding->cons_cell.car->type == ESHKOL_VAR) {
                interp_val_t* val = interp_eval(binding->cons_cell.cdr, ctx);
                if (ctx->error_msg) { ctx->env = saved; return val; }
                interp_frame_define(let_frame, binding->cons_cell.car->variable.id, val);
            }
        }

        interp_val_t* result = interp_eval(l.body, ctx);
        ctx->env = saved;
        return result;
    }

    // ── and (short-circuit) ────────────────────────────────────────
    case ESHKOL_AND_OP: {
        const auto& s = op->sequence_op;  // uses sequence_op struct
        interp_val_t* result = interp_make_bool(ctx, true);
        for (uint64_t i = 0; i < s.num_expressions; i++) {
            result = interp_eval(&s.expressions[i], ctx);
            if (ctx->error_msg) return result;
            if (!interp_val_is_truthy(result)) return result;
        }
        return result;
    }

    // ── or (short-circuit) ─────────────────────────────────────────
    case ESHKOL_OR_OP: {
        const auto& s = op->sequence_op;
        interp_val_t* result = interp_make_bool(ctx, false);
        for (uint64_t i = 0; i < s.num_expressions; i++) {
            result = interp_eval(&s.expressions[i], ctx);
            if (ctx->error_msg) return result;
            if (interp_val_is_truthy(result)) return result;
        }
        return result;
    }

    // ── when ───────────────────────────────────────────────────────
    case ESHKOL_WHEN_OP: {
        // Uses call_op: variables[0] = test, rest = body
        const auto& c = op->call_op;
        if (c.num_vars < 1) return interp_make_void(ctx);
        interp_val_t* test = interp_eval(&c.variables[0], ctx);
        if (ctx->error_msg) return test;
        if (interp_val_is_truthy(test)) {
            interp_val_t* result = interp_make_void(ctx);
            for (uint64_t i = 1; i < c.num_vars; i++) {
                result = interp_eval(&c.variables[i], ctx);
                if (ctx->error_msg) return result;
            }
            return result;
        }
        return interp_make_void(ctx);
    }

    // ── unless ─────────────────────────────────────────────────────
    case ESHKOL_UNLESS_OP: {
        const auto& c = op->call_op;
        if (c.num_vars < 1) return interp_make_void(ctx);
        interp_val_t* test = interp_eval(&c.variables[0], ctx);
        if (ctx->error_msg) return test;
        if (!interp_val_is_truthy(test)) {
            interp_val_t* result = interp_make_void(ctx);
            for (uint64_t i = 1; i < c.num_vars; i++) {
                result = interp_eval(&c.variables[i], ctx);
                if (ctx->error_msg) return result;
            }
            return result;
        }
        return interp_make_void(ctx);
    }

    // ── cond ───────────────────────────────────────────────────────
    case ESHKOL_COND_OP: {
        // Uses call_op: each variable is a clause (a cons or list)
        const auto& c = op->call_op;
        for (uint64_t i = 0; i < c.num_vars; i++) {
            const eshkol_ast_t* clause = &c.variables[i];
            // Each clause is parsed as a list — stored as cons cells
            // First element is the test, rest are body expressions
            if (clause->type == ESHKOL_CONS) {
                // Check for 'else' clause
                bool is_else = false;
                if (clause->cons_cell.car && clause->cons_cell.car->type == ESHKOL_VAR &&
                    strcmp(clause->cons_cell.car->variable.id, "else") == 0) {
                    is_else = true;
                }

                if (is_else) {
                    // Eval body expressions
                    const eshkol_ast_t* body = clause->cons_cell.cdr;
                    interp_val_t* result = interp_make_void(ctx);
                    while (body && body->type == ESHKOL_CONS) {
                        result = interp_eval(body->cons_cell.car, ctx);
                        if (ctx->error_msg) return result;
                        body = body->cons_cell.cdr;
                    }
                    return result;
                }

                interp_val_t* test = interp_eval(clause->cons_cell.car, ctx);
                if (ctx->error_msg) return test;
                if (interp_val_is_truthy(test)) {
                    const eshkol_ast_t* body = clause->cons_cell.cdr;
                    interp_val_t* result = test; // (cond (test) ...) returns test if no body
                    while (body && body->type == ESHKOL_CONS) {
                        result = interp_eval(body->cons_cell.car, ctx);
                        if (ctx->error_msg) return result;
                        body = body->cons_cell.cdr;
                    }
                    return result;
                }
            }
        }
        return interp_make_void(ctx);
    }

    // ── set! ───────────────────────────────────────────────────────
    case ESHKOL_SET_OP: {
        const auto& s = op->set_op;
        interp_val_t* val = interp_eval(s.value, ctx);
        if (ctx->error_msg) return val;
        if (!interp_frame_set(ctx->env, s.name, val)) {
            char buf[128];
            snprintf(buf, sizeof(buf), "set!: unbound variable '%s'", s.name);
            return interp_make_error(ctx, buf);
        }
        return interp_make_void(ctx);
    }

    // ── quote ──────────────────────────────────────────────────────
    case ESHKOL_QUOTE_OP: {
        // Uses call_op: variables[0] is the quoted datum
        const auto& c = op->call_op;
        if (c.num_vars >= 1) {
            return interp_ast_to_datum(&c.variables[0], ctx);
        }
        return interp_make_null(ctx);
    }

    // ── compose ────────────────────────────────────────────────────
    case ESHKOL_COMPOSE_OP: {
        interp_val_t* f = interp_eval(op->compose_op.func_a, ctx);
        if (ctx->error_msg) return f;
        interp_val_t* g = interp_eval(op->compose_op.func_b, ctx);
        if (ctx->error_msg) return g;
        // Return a closure that applies f(g(x))
        // For simplicity, store as a special cons
        // TODO: create a proper composed closure
        (void)f; (void)g;
        return interp_make_error(ctx, "compose: not yet implemented");
    }

    default: {
        char buf[64];
        snprintf(buf, sizeof(buf), "unsupported operation: %d", op->op);
        return interp_make_error(ctx, buf);
    }
    } // switch
}

// ═══════════════════════════════════════════════════════════════════════════
// Convert AST to datum (for quote)
// ═══════════════════════════════════════════════════════════════════════════

static interp_val_t* interp_ast_to_datum(const eshkol_ast_t* ast, interp_ctx_t* ctx) {
    if (!ast) return interp_make_null(ctx);

    switch (ast->type) {
        case ESHKOL_INT64: return interp_make_int(ctx, ast->int64_val);
        case ESHKOL_INT32: return interp_make_int(ctx, ast->int32_val);
        case ESHKOL_INT16: return interp_make_int(ctx, ast->int16_val);
        case ESHKOL_INT8:  return interp_make_int(ctx, ast->int8_val);
        case ESHKOL_UINT64: return interp_make_int(ctx, (int64_t)ast->uint64_val);
        case ESHKOL_UINT32: return interp_make_int(ctx, ast->uint32_val);
        case ESHKOL_UINT16: return interp_make_int(ctx, ast->uint16_val);
        case ESHKOL_UINT8:  return interp_make_int(ctx, ast->uint8_val);
        case ESHKOL_DOUBLE: return interp_make_double(ctx, ast->double_val);
        case ESHKOL_STRING: {
            // Parser includes null terminator in size — use strlen for actual length
            uint64_t slen = ast->str_val.ptr ? strlen(ast->str_val.ptr) : 0;
            return interp_make_string(ctx, ast->str_val.ptr, slen);
        }
        case ESHKOL_BOOL: return interp_make_bool(ctx, ast->int64_val != 0);
        case ESHKOL_CHAR: return interp_make_char(ctx, (char)ast->int64_val);
        case ESHKOL_NULL: return interp_make_null(ctx);
        case ESHKOL_VAR:  return interp_make_symbol(ctx, ast->variable.id);
        case ESHKOL_CONS:
            return interp_make_cons(ctx,
                interp_ast_to_datum(ast->cons_cell.car, ctx),
                interp_ast_to_datum(ast->cons_cell.cdr, ctx));
        case ESHKOL_OP: {
            const auto& op = ast->operation;
            // Quoted list: (list elem1 elem2 ...) stored as CALL_OP(func="list", args=[...])
            if (op.op == ESHKOL_CALL_OP && op.call_op.func &&
                op.call_op.func->type == ESHKOL_VAR &&
                strcmp(op.call_op.func->variable.id, "list") == 0) {
                interp_val_t* result = interp_make_null(ctx);
                for (int64_t i = (int64_t)op.call_op.num_vars - 1; i >= 0; i--) {
                    result = interp_make_cons(ctx,
                        interp_ast_to_datum(&op.call_op.variables[i], ctx), result);
                }
                return result;
            }
            // Nested quote
            if (op.op == ESHKOL_QUOTE_OP && op.call_op.num_vars >= 1) {
                return interp_make_cons(ctx,
                    interp_make_symbol(ctx, "quote"),
                    interp_make_cons(ctx,
                        interp_ast_to_datum(&op.call_op.variables[0], ctx),
                        interp_make_null(ctx)));
            }
            return interp_make_symbol(ctx, "#<ast>");
        }
        default:
            return interp_make_symbol(ctx, "#<ast>");
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Eval AST node
// ═══════════════════════════════════════════════════════════════════════════

interp_val_t* interp_eval(const eshkol_ast_t* ast, interp_ctx_t* ctx) {
    if (!ast) return interp_make_null(ctx);
    if (ctx->error_msg) return interp_make_null(ctx);

    switch (ast->type) {
        case ESHKOL_INT64: return interp_make_int(ctx, ast->int64_val);
        case ESHKOL_INT32: return interp_make_int(ctx, ast->int32_val);
        case ESHKOL_INT16: return interp_make_int(ctx, ast->int16_val);
        case ESHKOL_INT8:  return interp_make_int(ctx, ast->int8_val);
        case ESHKOL_UINT64: return interp_make_int(ctx, (int64_t)ast->uint64_val);
        case ESHKOL_UINT32: return interp_make_int(ctx, ast->uint32_val);
        case ESHKOL_UINT16: return interp_make_int(ctx, ast->uint16_val);
        case ESHKOL_UINT8:  return interp_make_int(ctx, ast->uint8_val);
        case ESHKOL_DOUBLE: return interp_make_double(ctx, ast->double_val);
        case ESHKOL_STRING: {
            // Parser includes null terminator in size — use strlen for actual length
            uint64_t slen = ast->str_val.ptr ? strlen(ast->str_val.ptr) : 0;
            return interp_make_string(ctx, ast->str_val.ptr, slen);
        }
        case ESHKOL_BOOL:
            return interp_make_bool(ctx, ast->int64_val != 0);
        case ESHKOL_CHAR:
            return interp_make_char(ctx, (char)ast->int64_val);
        case ESHKOL_NULL:
            return interp_make_null(ctx);

        case ESHKOL_VAR: {
            // Clean variable name — parser may include trailing non-printable bytes at EOF
            const char* name = ast->variable.id;
            char clean_name[256];
            size_t len = 0;
            if (name) {
                for (size_t i = 0; name[i] && i < 255; i++) {
                    if ((unsigned char)name[i] >= 32 && (unsigned char)name[i] < 127)
                        clean_name[len++] = name[i];
                }
            }
            clean_name[len] = '\0';

            interp_val_t* val = interp_frame_lookup(ctx->env, clean_name);
            if (!val && name) val = interp_frame_lookup(ctx->env, name);
            if (val) return val;
            char buf[128];
            snprintf(buf, sizeof(buf), "unbound variable: %s", clean_name);
            return interp_make_error(ctx, buf);
        }

        case ESHKOL_CONS: {
            interp_val_t* car = interp_eval(ast->cons_cell.car, ctx);
            if (ctx->error_msg) return car;
            interp_val_t* cdr = interp_eval(ast->cons_cell.cdr, ctx);
            if (ctx->error_msg) return cdr;
            return interp_make_cons(ctx, car, cdr);
        }

        case ESHKOL_FUNC: {
            // Named function definition — create closure
            auto* closure = alloc_val(ctx);
            closure->type = INTERP_VAL_CLOSURE;
            closure->closure.body_op = ast->eshkol_func.func_commands;
            closure->closure.body_ast = nullptr;
            closure->closure.params = ast->eshkol_func.variables;
            closure->closure.num_params = ast->eshkol_func.num_variables;
            closure->closure.is_variadic = ast->eshkol_func.is_variadic;
            closure->closure.rest_param = ast->eshkol_func.rest_param;
            closure->closure.env = ctx->env;

            // If named, define in environment
            if (ast->eshkol_func.id) {
                interp_frame_define(ctx->env, ast->eshkol_func.id, closure);
            }
            return closure;
        }

        case ESHKOL_OP: {
            interp_val_t* r = interp_eval_op(&ast->operation, ctx);
            // Resolve any tail calls — interp_eval is used in non-tail positions
            return resolve_tail_calls(r, ctx);
        }

        default: {
            char buf[64];
            snprintf(buf, sizeof(buf), "unsupported AST type: %d", ast->type);
            return interp_make_error(ctx, buf);
        }
    }
}

// Eval in tail position — may return tail call thunks
interp_val_t* interp_eval_tail(const eshkol_ast_t* ast, interp_ctx_t* ctx) {
    if (!ast) return interp_make_null(ctx);
    if (ctx->error_msg) return interp_make_null(ctx);
    if (ast->type == ESHKOL_OP) {
        // Don't resolve — let tail calls propagate
        return interp_eval_op(&ast->operation, ctx);
    }
    // Non-ops can't produce tail calls
    return interp_eval(ast, ctx);
}
