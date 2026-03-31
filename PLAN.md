# Plan: Compile Eshkol Compiler to WebAssembly

## Context

[Eshkol](https://github.com/tsotchke/eshkol) is a LISP-like language for scientific computing and AI. The compiler is written in C17/C++20 with an LLVM backend, using CMake as the build system.

**Goal:** Compile the eshkol compiler itself to WebAssembly so it can run in a browser or wasm runtime (e.g., an online playground).

## Approach: Emscripten, Frontend-Only (No LLVM)

All 21 backend `.cpp` files are wrapped in `#ifdef ESHKOL_LLVM_BACKEND_ENABLED`, and `inc/eshkol/llvm_backend.h` provides complete stub macros when the define is absent. This means the frontend (parser, type checker, macro expander, AST, arena memory — ~13k LOC) compiles cleanly without LLVM, producing a small ~2-3MB wasm binary.

**What this gives:** A wasm module that can parse eshkol source, type-check it, and pretty-print ASTs — suitable for a browser playground or editor integration.

**Alternative approaches considered but deferred:**
- **Full LLVM in wasm:** Cross-compile LLVM itself to wasm. Produces 50-100MB+ binary, extremely complex build. Not worth it as a first step.
- **Tree-walking interpreter:** Write a new interpreter backend for wasm. Significant new code required. Could be a follow-up.

## Source Repo

Cloned to `/Users/kejace/devel/eshkol-src/` from `https://github.com/tsotchke/eshkol.git`.

## Architecture Summary

```
eshkol-src/
├── exe/eshkol-run.cpp       # Main compiler executable (~2200 LOC)
├── exe/eshkol-repl.cpp      # REPL executable
├── exe/eshkol-wasm.cpp      # [NEW] Wasm entry point
├── inc/eshkol/
│   ├── eshkol.h             # Core types: eshkol_ast_t, eshkol_op_t, etc.
│   ├── llvm_backend.h       # LLVM API with stubs when disabled
│   └── backend/             # LLVM backend headers (all need LLVM)
├── lib/
│   ├── frontend/
│   │   ├── parser.cpp       # 5636 LOC — recursive descent parser
│   │   └── macro_expander.cpp # 579 LOC
│   ├── types/
│   │   ├── type_checker.cpp # 1563 LOC — HoTT type checker
│   │   ├── hott_types.cpp   # 682 LOC
│   │   └── dependent.cpp    # 440 LOC
│   ├── core/
│   │   ├── ast.cpp          # 564 LOC
│   │   ├── arena_memory.cpp # 3210 LOC
│   │   ├── printer.cpp      # 319 LOC — AST pretty-printer (printf-based)
│   │   ├── logger.cpp       # 171 LOC — needs Emscripten fix
│   │   └── platform_runtime.cpp # 551 LOC — POSIX APIs (mostly Emscripten-safe)
│   ├── backend/             # ~44k LOC — ALL guarded by #ifdef ESHKOL_LLVM_BACKEND_ENABLED
│   └── repl/                # JIT REPL (excluded from main lib)
├── cmake/
│   └── build_config.h.in    # Generates build_config.h with LLC_EXECUTABLE etc.
└── wasm/
    └── eshkol.js            # [NEW] JS convenience wrapper
```

### Key API for wasm

- `eshkol_parse_next_ast_from_stream(std::istream&)` — parses one top-level expression from any stream (including `std::istringstream`)
- `eshkol_ast_pretty_print(const eshkol_ast_t*, int indent)` — prints AST to stdout via printf
- `llvm_backend.h` stubs — all codegen functions become no-ops/return NULL when `ESHKOL_LLVM_BACKEND_ENABLED` is not defined

## Changes Made So Far

### 1. CMakeLists.txt — Make LLVM Optional + Emscripten Target

**Status: DONE** (file modified in eshkol-src)

Changes:
- `set(ESHKOL_BUILD_BACKEND ON)` → `option(ESHKOL_BUILD_BACKEND "Build LLVM backend" ON)`
- Wrapped all LLVM detection (find_program, llvm-config calls, eshkol_apply_llvm_target_settings with LLVM flags) inside `if(ESHKOL_BUILD_BACKEND)`
- When backend is OFF: provides minimal `eshkol_apply_llvm_target_settings()` that only sets include dirs (no LLVM defines), sets `LLC_EXECUTABLE=""` for build_config.h
- Backend sources excluded from `LIB_SRC` when backend is OFF: `list(FILTER LIB_SRC EXCLUDE REGEX "lib/backend/.*")`
- Native executables, stdlib pre-compilation, REPL, Packing — all wrapped in `if(ESHKOL_BUILD_BACKEND)`
- Added Emscripten wasm target at bottom:
  ```cmake
  if(EMSCRIPTEN)
    add_executable(eshkol-wasm exe/eshkol-wasm.cpp)
    # ... EXPORTED_FUNCTIONS, MODULARIZE, ALLOW_MEMORY_GROWTH, etc.
  endif()
  ```

### 2. logger.cpp — Emscripten Guard for execinfo.h

**Status: DONE** (file modified in eshkol-src)

- Changed `#ifndef _WIN32` → `#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)` around `<execinfo.h>` and `<cxxabi.h>` includes
- Changed `#ifdef _WIN32` → `#if defined(_WIN32) || defined(__EMSCRIPTEN__)` in `eshkol_stacktrace()` to skip backtrace on wasm

### 3. exe/eshkol-wasm.cpp — Wasm Entry Point

**Status: DONE** (new file created in eshkol-src)

Exports three C functions:
- `eshkol_wasm_parse(const char* source) -> const char*` — parses source, returns JSON with AST array
- `eshkol_wasm_pretty_print(const char* source) -> const char*` — parses and returns pretty-printed AST text (captures printf output via `open_memstream`)
- `eshkol_wasm_free(const char* ptr)` — frees malloc'd return strings

Includes inline AST-to-JSON serializer that handles all `eshkol_type_t` variants and `eshkol_op_t` operations, with JSON string escaping.

### 4. wasm/eshkol.js — JavaScript Wrapper

**Status: DONE** (new file created in eshkol-src)

ES module wrapper with:
- `init()` — loads the Emscripten module via dynamic import
- `parse(source)` — calls wasm parse, returns parsed JSON object
- `prettyPrint(source)` — calls wasm pretty-print, returns text string
- Automatic memory cleanup (calls `eshkol_wasm_free` after reading strings)

## Remaining Steps

### 5. Install Emscripten SDK

```bash
cd ~
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
```

### 6. Build

```bash
cd /Users/kejace/devel/eshkol-wasm
emcmake cmake ../eshkol-src \
  -DCMAKE_BUILD_TYPE=Release \
  -DESHKOL_BUILD_BACKEND=OFF \
  -DESHKOL_BUILD_TESTS=OFF \
  -DBUILD_REPL=OFF
emmake make -j$(sysctl -n hw.ncpu) eshkol-wasm
```

Output: `wasm/eshkol-wasm.js` + `wasm/eshkol-wasm.wasm`

### 7. Test with Node.js

```bash
cd /Users/kejace/devel/eshkol-wasm/wasm
node -e "
  import('./eshkol-wasm.js').then(async ({default: create}) => {
    const mod = await create();
    const ptr = mod.ccall('eshkol_wasm_parse', 'number', ['string'], ['(+ 1 2)']);
    console.log(mod.UTF8ToString(ptr));
    mod.ccall('eshkol_wasm_free', null, ['number'], [ptr]);
  });
"
```

### 8. Potential Build Issues to Watch For

1. **`open_memstream` on Emscripten** — Used in `eshkol_wasm_pretty_print()` to capture printf output. Emscripten supports it, but if it fails, can fall back to redirecting stdout to a temp file.
2. **`std::filesystem`** — Used in `platform_runtime.cpp`. Emscripten supports it but may need `-s FORCE_FILESYSTEM=1` if file operations are called.
3. **`std::thread`/`std::mutex`** — Used in `platform_runtime.cpp` and `logger.cpp`. Emscripten supports pthreads but they require SharedArrayBuffer. If this causes issues, add `-sUSE_PTHREADS=0` or `-sSINGLE_FILE=1`. The mutex usage is just for thread-safety in logger/drand48, so single-threaded mode is fine.
4. **Binary size** — Target <3MB for the .wasm file. If larger, apply `-Oz` optimization and/or `wasm-opt -Os`.

### 9. Verify Native Build Still Works

After all changes, confirm the original build path is unbroken:
```bash
cd /Users/kejace/devel/eshkol-src
mkdir -p build-native && cd build-native
cmake .. -DCMAKE_BUILD_TYPE=Release  # ESHKOL_BUILD_BACKEND defaults to ON
make -j$(sysctl -n hw.ncpu)
```

## File Inventory

| File | Status | Location |
|------|--------|----------|
| `CMakeLists.txt` | Modified | eshkol-src/CMakeLists.txt |
| `lib/core/logger.cpp` | Modified | eshkol-src/lib/core/logger.cpp |
| `exe/eshkol-wasm.cpp` | New | eshkol-src/exe/eshkol-wasm.cpp |
| `wasm/eshkol.js` | New | eshkol-src/wasm/eshkol.js |
