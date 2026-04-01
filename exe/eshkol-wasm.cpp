/*
 * Copyright (C) tsotchke
 *
 * SPDX-License-Identifier: MIT
 *
 * WebAssembly entry point for the Eshkol compiler frontend.
 * Exposes parsing and pretty-printing to JavaScript via Emscripten.
 */
#include <eshkol/eshkol.h>
#include <eshkol/llvm_backend.h>
#include "../lib/interpreter/interpreter.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define WASM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define WASM_EXPORT
#endif

// ===== AST to JSON Serializer =====

static const char* type_name(eshkol_type_t type) {
    switch (type) {
        case ESHKOL_INVALID: return "invalid";
        case ESHKOL_UNTYPED: return "untyped";
        case ESHKOL_UINT8:   return "uint8";
        case ESHKOL_UINT16:  return "uint16";
        case ESHKOL_UINT32:  return "uint32";
        case ESHKOL_UINT64:  return "uint64";
        case ESHKOL_INT8:    return "int8";
        case ESHKOL_INT16:   return "int16";
        case ESHKOL_INT32:   return "int32";
        case ESHKOL_INT64:   return "int64";
        case ESHKOL_DOUBLE:  return "double";
        case ESHKOL_STRING:  return "string";
        case ESHKOL_FUNC:    return "function";
        case ESHKOL_VAR:     return "variable";
        case ESHKOL_OP:      return "operation";
        case ESHKOL_CONS:    return "cons";
        case ESHKOL_NULL:    return "null";
        case ESHKOL_TENSOR:  return "tensor";
        case ESHKOL_CHAR:    return "char";
        case ESHKOL_BOOL:    return "bool";
        default:             return "unknown";
    }
}

static const char* op_name(eshkol_op_t op) {
    switch (op) {
        case ESHKOL_INVALID_OP:    return "invalid";
        case ESHKOL_COMPOSE_OP:    return "compose";
        case ESHKOL_IF_OP:         return "if";
        case ESHKOL_ADD_OP:        return "add";
        case ESHKOL_SUB_OP:        return "sub";
        case ESHKOL_MUL_OP:        return "mul";
        case ESHKOL_DIV_OP:        return "div";
        case ESHKOL_CALL_OP:       return "call";
        case ESHKOL_DEFINE_OP:     return "define";
        case ESHKOL_SEQUENCE_OP:   return "sequence";
        case ESHKOL_EXTERN_OP:     return "extern";
        case ESHKOL_EXTERN_VAR_OP: return "extern-var";
        case ESHKOL_LAMBDA_OP:     return "lambda";
        case ESHKOL_LET_OP:        return "let";
        case ESHKOL_LET_STAR_OP:   return "let*";
        case ESHKOL_LETREC_OP:     return "letrec";
        case ESHKOL_AND_OP:        return "and";
        case ESHKOL_OR_OP:         return "or";
        case ESHKOL_COND_OP:       return "cond";
        case ESHKOL_CASE_OP:       return "case";
        case ESHKOL_MATCH_OP:      return "match";
        case ESHKOL_DO_OP:         return "do";
        case ESHKOL_WHEN_OP:       return "when";
        case ESHKOL_UNLESS_OP:     return "unless";
        case ESHKOL_QUOTE_OP:      return "quote";
        case ESHKOL_QUASIQUOTE_OP: return "quasiquote";
        case ESHKOL_UNQUOTE_OP:    return "unquote";
        case ESHKOL_UNQUOTE_SPLICING_OP: return "unquote-splicing";
        case ESHKOL_SET_OP:        return "set!";
        case ESHKOL_DEFINE_TYPE_OP: return "define-type";
        case ESHKOL_IMPORT_OP:     return "import";
        case ESHKOL_REQUIRE_OP:    return "require";
        case ESHKOL_PROVIDE_OP:    return "provide";
        case ESHKOL_TENSOR_OP:     return "tensor";
        case ESHKOL_DIFF_OP:       return "diff";
        case ESHKOL_DERIVATIVE_OP: return "derivative";
        case ESHKOL_GRADIENT_OP:   return "gradient";
        case ESHKOL_GUARD_OP:      return "guard";
        case ESHKOL_RAISE_OP:      return "raise";
        case ESHKOL_VALUES_OP:     return "values";
        case ESHKOL_DEFINE_SYNTAX_OP: return "define-syntax";
        default:                   return "unknown";
    }
}

// Escape a string for JSON output
static std::string json_escape(const char* s) {
    if (!s) return "null";
    std::string out = "\"";
    for (; *s; ++s) {
        switch (*s) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if ((unsigned char)*s < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)*s);
                    out += buf;
                } else {
                    out += *s;
                }
        }
    }
    out += "\"";
    return out;
}

static void ast_to_json(const eshkol_ast_t* ast, std::string& out);
static void op_to_json(const eshkol_operations_t* op, std::string& out);

static void op_to_json(const eshkol_operations_t* op, std::string& out) {
    if (!op) { out += "null"; return; }

    out += "{\"op\":\"";
    out += op_name(op->op);
    out += "\"";

    switch (op->op) {
        case ESHKOL_COMPOSE_OP:
            out += ",\"func_a\":";
            ast_to_json(op->compose_op.func_a, out);
            out += ",\"func_b\":";
            ast_to_json(op->compose_op.func_b, out);
            break;

        case ESHKOL_IF_OP:
            out += ",\"if_true\":";
            op_to_json(op->if_op.if_true, out);
            out += ",\"if_false\":";
            op_to_json(op->if_op.if_false, out);
            break;

        case ESHKOL_CALL_OP:
            out += ",\"func\":";
            ast_to_json(op->call_op.func, out);
            out += ",\"args\":[";
            for (uint64_t i = 0; i < op->call_op.num_vars; i++) {
                if (i > 0) out += ",";
                ast_to_json(&op->call_op.variables[i], out);
            }
            out += "]";
            break;

        case ESHKOL_DEFINE_OP:
            out += ",\"name\":";
            out += json_escape(op->define_op.name);
            out += ",\"is_function\":";
            out += op->define_op.is_function ? "true" : "false";
            if (op->define_op.is_function && op->define_op.parameters) {
                out += ",\"params\":[";
                for (uint64_t i = 0; i < op->define_op.num_params; i++) {
                    if (i > 0) out += ",";
                    ast_to_json(&op->define_op.parameters[i], out);
                }
                out += "]";
            }
            out += ",\"value\":";
            ast_to_json(op->define_op.value, out);
            break;

        case ESHKOL_SEQUENCE_OP:
            out += ",\"expressions\":[";
            for (uint64_t i = 0; i < op->sequence_op.num_expressions; i++) {
                if (i > 0) out += ",";
                ast_to_json(&op->sequence_op.expressions[i], out);
            }
            out += "]";
            break;

        case ESHKOL_LAMBDA_OP:
            out += ",\"params\":[";
            for (uint64_t i = 0; i < op->lambda_op.num_params; i++) {
                if (i > 0) out += ",";
                ast_to_json(&op->lambda_op.parameters[i], out);
            }
            out += "],\"body\":";
            ast_to_json(op->lambda_op.body, out);
            break;

        default:
            break;
    }

    out += "}";
}

static void ast_to_json(const eshkol_ast_t* ast, std::string& out) {
    if (!ast) { out += "null"; return; }

    out += "{\"type\":\"";
    out += type_name(ast->type);
    out += "\"";

    // Source location
    if (ast->line > 0) {
        out += ",\"line\":" + std::to_string(ast->line);
        out += ",\"column\":" + std::to_string(ast->column);
    }

    switch (ast->type) {
        case ESHKOL_UINT8:
            out += ",\"value\":" + std::to_string(ast->uint8_val);
            break;
        case ESHKOL_UINT16:
            out += ",\"value\":" + std::to_string(ast->uint16_val);
            break;
        case ESHKOL_UINT32:
            out += ",\"value\":" + std::to_string(ast->uint32_val);
            break;
        case ESHKOL_UINT64:
            out += ",\"value\":" + std::to_string(ast->uint64_val);
            break;
        case ESHKOL_INT8:
            out += ",\"value\":" + std::to_string(ast->int8_val);
            break;
        case ESHKOL_INT16:
            out += ",\"value\":" + std::to_string(ast->int16_val);
            break;
        case ESHKOL_INT32:
            out += ",\"value\":" + std::to_string(ast->int32_val);
            break;
        case ESHKOL_INT64:
            out += ",\"value\":" + std::to_string(ast->int64_val);
            break;
        case ESHKOL_DOUBLE:
            out += ",\"value\":" + std::to_string(ast->double_val);
            break;
        case ESHKOL_STRING:
            out += ",\"value\":";
            out += json_escape(ast->str_val.ptr);
            break;
        case ESHKOL_VAR:
            out += ",\"name\":";
            out += json_escape(ast->variable.id);
            if (ast->variable.data) {
                out += ",\"data\":";
                ast_to_json(ast->variable.data, out);
            }
            break;
        case ESHKOL_FUNC:
            out += ",\"name\":";
            out += json_escape(ast->eshkol_func.id);
            out += ",\"is_lambda\":";
            out += ast->eshkol_func.is_lambda ? "true" : "false";
            if (ast->eshkol_func.variables && ast->eshkol_func.num_variables > 0) {
                out += ",\"variables\":[";
                for (uint64_t i = 0; i < ast->eshkol_func.num_variables; i++) {
                    if (i > 0) out += ",";
                    ast_to_json(&ast->eshkol_func.variables[i], out);
                }
                out += "]";
            }
            if (ast->eshkol_func.func_commands) {
                out += ",\"body\":";
                op_to_json(ast->eshkol_func.func_commands, out);
            }
            break;
        case ESHKOL_OP:
            out += ",\"operation\":";
            op_to_json(&ast->operation, out);
            break;
        case ESHKOL_BOOL:
            out += ",\"value\":";
            out += ast->uint8_val ? "true" : "false";
            break;
        case ESHKOL_CHAR:
            out += ",\"value\":" + std::to_string(ast->uint32_val);
            break;
        case ESHKOL_CONS:
            out += ",\"car\":";
            ast_to_json(ast->cons_cell.car, out);
            out += ",\"cdr\":";
            ast_to_json(ast->cons_cell.cdr, out);
            break;
        case ESHKOL_NULL:
            break;
        default:
            break;
    }

    out += "}";
}

// ===== Exported WASM API =====

extern "C" {

// Parse eshkol source code and return JSON AST
WASM_EXPORT
const char* eshkol_wasm_parse(const char* source) {
    if (!source) {
        const char* err = "{\"error\":\"null source\"}";
        char* result = (char*)malloc(strlen(err) + 1);
        strcpy(result, err);
        return result;
    }

    std::istringstream stream(source);
    std::vector<eshkol_ast_t> asts;

    // Parse all top-level expressions
    while (stream.good() && !stream.eof()) {
        // Skip whitespace
        while (stream.good() && std::isspace(stream.peek())) {
            stream.get();
        }
        if (!stream.good() || stream.eof()) break;

        // Check for comments (skip lines starting with ;)
        if (stream.peek() == ';') {
            std::string line;
            std::getline(stream, line);
            continue;
        }

        eshkol_ast_t ast = eshkol_parse_next_ast_from_stream(stream);
        if (ast.type == ESHKOL_INVALID) {
            // Could be EOF or a parse error
            break;
        }
        asts.push_back(ast);
    }

    // Serialize to JSON
    std::string json = "{\"asts\":[";
    for (size_t i = 0; i < asts.size(); i++) {
        if (i > 0) json += ",";
        ast_to_json(&asts[i], json);
    }
    json += "],\"count\":";
    json += std::to_string(asts.size());
    json += "}";

    char* result = (char*)malloc(json.size() + 1);
    memcpy(result, json.c_str(), json.size() + 1);
    return result;
}

// Pretty-print eshkol source code (parse then dump AST text)
WASM_EXPORT
const char* eshkol_wasm_pretty_print(const char* source) {
    if (!source) {
        const char* err = "(null source)";
        char* result = (char*)malloc(strlen(err) + 1);
        strcpy(result, err);
        return result;
    }

    // Redirect stdout to capture pretty-print output
    // Use a temporary file approach since eshkol_ast_pretty_print uses printf
    std::istringstream stream(source);
    std::string output;

    while (stream.good() && !stream.eof()) {
        while (stream.good() && std::isspace(stream.peek())) {
            stream.get();
        }
        if (!stream.good() || stream.eof()) break;

        if (stream.peek() == ';') {
            std::string line;
            std::getline(stream, line);
            continue;
        }

        eshkol_ast_t ast = eshkol_parse_next_ast_from_stream(stream);
        if (ast.type == ESHKOL_INVALID) break;

        // Capture printf output by redirecting stdout via dup2
        // (Emscripten declares stdout as const, so we redirect at fd level)
        fflush(stdout);
        int saved_fd = dup(fileno(stdout));
        char tmpname[] = "/tmp/eshkol_wasm_XXXXXX";
        int tmpfd = mkstemp(tmpname);
        if (tmpfd >= 0 && saved_fd >= 0) {
            dup2(tmpfd, fileno(stdout));
            close(tmpfd);
            eshkol_ast_pretty_print(&ast, 0);
            fflush(stdout);
            dup2(saved_fd, fileno(stdout));
            close(saved_fd);
            // Read back captured output
            FILE* tmpf = fopen(tmpname, "r");
            if (tmpf) {
                fseek(tmpf, 0, SEEK_END);
                long len = ftell(tmpf);
                if (len > 0) {
                    fseek(tmpf, 0, SEEK_SET);
                    char* buf = (char*)malloc(len + 1);
                    size_t nread = fread(buf, 1, len, tmpf);
                    buf[nread] = '\0';
                    output += buf;
                    output += "\n";
                    free(buf);
                }
                fclose(tmpf);
            }
            unlink(tmpname);
        } else {
            if (saved_fd >= 0) close(saved_fd);
            if (tmpfd >= 0) close(tmpfd);
        }
    }

    char* result = (char*)malloc(output.size() + 1);
    memcpy(result, output.c_str(), output.size() + 1);
    return result;
}

// Free a string returned by the parse/pretty-print functions
WASM_EXPORT
void eshkol_wasm_free(const char* ptr) {
    free((void*)ptr);
}

// ═══════════════════════════════════════════════════════════════════════════
// Interpreter API
// ═══════════════════════════════════════════════════════════════════════════

// Create a persistent interpreter context
WASM_EXPORT
void* eshkol_wasm_interp_create(void) {
    return (void*)interp_ctx_create();
}

// Destroy the interpreter context
WASM_EXPORT
void eshkol_wasm_interp_destroy(void* ctx_ptr) {
    interp_ctx_destroy((interp_ctx_t*)ctx_ptr);
}

// Reset interpreter state (clear all user definitions)
WASM_EXPORT
void eshkol_wasm_interp_reset(void* ctx_ptr) {
    interp_ctx_reset((interp_ctx_t*)ctx_ptr);
}

// Set maximum recursion depth (capped at 50000 to avoid stack overflow)
WASM_EXPORT
void eshkol_wasm_interp_set_recursion_limit(void* ctx_ptr, int limit) {
    if (ctx_ptr) {
        if (limit > 50000) limit = 50000;
        if (limit < 10) limit = 10;
        ((interp_ctx_t*)ctx_ptr)->max_recursion_depth = limit;
    }
}

// Evaluate source code and return JSON result
// Returns: {"value":"42","type":"integer","output":"..."}
// Or:      {"error":"message","output":"..."}
WASM_EXPORT
const char* eshkol_wasm_eval(void* ctx_ptr, const char* source) {
    if (!ctx_ptr || !source) {
        char* r = (char*)malloc(32);
        strcpy(r, "{\"error\":\"null argument\"}");
        return r;
    }

    interp_ctx_t* ctx = (interp_ctx_t*)ctx_ptr;
    ctx->error_msg = nullptr;
    ctx->output_len = 0;
    if (ctx->output_buf) ctx->output_buf[0] = '\0';

    std::istringstream stream(source);
    interp_val_t* result = nullptr;

    while (true) {
        eshkol_ast_t ast = eshkol_parse_next_ast_from_stream(stream);
        if (ast.type == ESHKOL_INVALID) break;

        result = interp_eval(&ast, ctx);
        if (ctx->error_msg) break;
    }

    // Build JSON response
    std::string json = "{";

    if (ctx->error_msg) {
        json += "\"error\":\"";
        // Escape the error message
        for (const char* p = ctx->error_msg; *p; p++) {
            if (*p == '"') json += "\\\"";
            else if (*p == '\\') json += "\\\\";
            else if (*p == '\n') json += "\\n";
            else json += *p;
        }
        json += "\"";
    } else if (result) {
        char* val_str = interp_val_to_string(result);
        json += "\"value\":\"";
        for (const char* p = val_str; *p; p++) {
            if (*p == '"') json += "\\\"";
            else if (*p == '\\') json += "\\\\";
            else if (*p == '\n') json += "\\n";
            else json += *p;
        }
        json += "\",\"type\":\"";
        json += interp_val_type_name(result);
        json += "\"";
        free(val_str);
    } else {
        json += "\"value\":\"\",\"type\":\"void\"";
    }

    // Include captured output
    if (ctx->output_len > 0) {
        json += ",\"output\":\"";
        for (uint64_t i = 0; i < ctx->output_len; i++) {
            char c = ctx->output_buf[i];
            if (c == '"') json += "\\\"";
            else if (c == '\\') json += "\\\\";
            else if (c == '\n') json += "\\n";
            else if (c == '\r') json += "\\r";
            else if (c == '\t') json += "\\t";
            else json += c;
        }
        json += "\"";
    }

    json += "}";

    char* out = (char*)malloc(json.size() + 1);
    memcpy(out, json.c_str(), json.size() + 1);
    return out;
}

} // extern "C"
