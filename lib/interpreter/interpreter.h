/*
 * Eshkol Tree-Walking Interpreter
 * Evaluates parsed ASTs without LLVM/JIT — suitable for WebAssembly.
 */
#ifndef ESHKOL_INTERPRETER_H
#define ESHKOL_INTERPRETER_H

#include <eshkol/eshkol.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ═══════════════════════════════════════════════════════════════════════════
// Value representation
// ═══════════════════════════════════════════════════════════════════════════

// Interpreter value — simple tagged union for runtime values
typedef enum {
    INTERP_VAL_NULL,
    INTERP_VAL_INT,
    INTERP_VAL_DOUBLE,
    INTERP_VAL_BOOL,
    INTERP_VAL_CHAR,
    INTERP_VAL_STRING,
    INTERP_VAL_CONS,
    INTERP_VAL_CLOSURE,
    INTERP_VAL_BUILTIN,
    INTERP_VAL_VOID,
    INTERP_VAL_SYMBOL,
    INTERP_VAL_ERROR,
    INTERP_VAL_TAIL_CALL,
} interp_val_type_t;

struct interp_val;
struct interp_frame;

typedef struct interp_cons {
    struct interp_val* car;
    struct interp_val* cdr;
} interp_cons_t;

typedef struct interp_closure {
    const eshkol_operations_t* body_op;   // lambda body (operation node)
    const eshkol_ast_t* body_ast;         // lambda body (ast node, for define-style funcs)
    const eshkol_ast_t* params;           // parameter AST array
    uint64_t num_params;
    uint8_t is_variadic;
    char* rest_param;
    struct interp_frame* env;             // captured lexical environment
} interp_closure_t;

struct interp_ctx;

typedef struct interp_val* (*interp_builtin_fn)(
    struct interp_val** args, uint64_t num_args,
    struct interp_ctx* ctx);

typedef struct interp_builtin {
    interp_builtin_fn fn;
    const char* name;
    int min_arity;    // -1 = any
    int max_arity;    // -1 = unlimited
} interp_builtin_t;

typedef struct interp_val {
    interp_val_type_t type;
    union {
        int64_t int_val;
        double double_val;
        bool bool_val;
        char char_val;
        struct { char* ptr; uint64_t len; } str_val;
        interp_cons_t cons;
        interp_closure_t closure;
        interp_builtin_t builtin;
        char* error_msg;
        char* symbol;
        struct { struct interp_val* func; struct interp_val** args; uint64_t num_args; } tail_call;
    };
} interp_val_t;

// ═══════════════════════════════════════════════════════════════════════════
// Environment
// ═══════════════════════════════════════════════════════════════════════════

typedef struct interp_binding {
    const char* name;
    interp_val_t* value;
} interp_binding_t;

typedef struct interp_frame {
    interp_binding_t* bindings;
    uint64_t count;
    uint64_t capacity;
    struct interp_frame* parent;
} interp_frame_t;

// ═══════════════════════════════════════════════════════════════════════════
// Interpreter context
// ═══════════════════════════════════════════════════════════════════════════

typedef struct interp_ctx {
    interp_frame_t* env;            // current scope
    char* output_buf;               // captured display/newline output
    uint64_t output_len;
    uint64_t output_cap;
    const char* error_msg;          // non-null if error occurred
    int recursion_depth;
    int max_recursion_depth;
} interp_ctx_t;

// ═══════════════════════════════════════════════════════════════════════════
// API
// ═══════════════════════════════════════════════════════════════════════════

interp_ctx_t* interp_ctx_create(void);
void interp_ctx_destroy(interp_ctx_t* ctx);
void interp_ctx_reset(interp_ctx_t* ctx);

interp_val_t* interp_eval(const eshkol_ast_t* ast, interp_ctx_t* ctx);
interp_val_t* interp_eval_op(const eshkol_operations_t* op, interp_ctx_t* ctx);
interp_val_t* interp_apply(interp_val_t* func, interp_val_t** args,
                            uint64_t num_args, interp_ctx_t* ctx);

// Value constructors
interp_val_t* interp_make_null(interp_ctx_t* ctx);
interp_val_t* interp_make_int(interp_ctx_t* ctx, int64_t v);
interp_val_t* interp_make_double(interp_ctx_t* ctx, double v);
interp_val_t* interp_make_bool(interp_ctx_t* ctx, bool v);
interp_val_t* interp_make_char(interp_ctx_t* ctx, char v);
interp_val_t* interp_make_string(interp_ctx_t* ctx, const char* s, uint64_t len);
interp_val_t* interp_make_cons(interp_ctx_t* ctx, interp_val_t* car, interp_val_t* cdr);
interp_val_t* interp_make_void(interp_ctx_t* ctx);
interp_val_t* interp_make_error(interp_ctx_t* ctx, const char* msg);
interp_val_t* interp_make_symbol(interp_ctx_t* ctx, const char* name);

// Value display
char* interp_val_to_string(const interp_val_t* val);
const char* interp_val_type_name(const interp_val_t* val);
bool interp_val_is_truthy(const interp_val_t* val);

// Environment
interp_frame_t* interp_frame_create(interp_frame_t* parent);
void interp_frame_define(interp_frame_t* frame, const char* name, interp_val_t* value);
interp_val_t* interp_frame_lookup(interp_frame_t* frame, const char* name);
bool interp_frame_set(interp_frame_t* frame, const char* name, interp_val_t* value);

// Output capture
void interp_output_append(interp_ctx_t* ctx, const char* str, uint64_t len);

// Builtins
void interp_register_builtins(interp_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif // ESHKOL_INTERPRETER_H
