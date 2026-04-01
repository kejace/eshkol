#!/usr/bin/env node
// wasm_test_runner.mjs — Run a single .esk test file via the wasm interpreter
//
// Usage: node wasm_test_runner.mjs <wasm_dir> <test_file>
//
// Exit code 0 = success, non-zero = failure
// Outputs any display/error output to stdout

const fs = require('fs');
const path = require('path');

const wasmDir = process.argv[2];
const testFile = process.argv[3];
if (!wasmDir || !testFile) {
    console.error('Usage: node wasm_test_runner.mjs <wasm_dir> <test_file>');
    process.exit(2);
}

const absWasmDir = path.resolve(wasmDir);

// Read test source
let source;
try {
    source = fs.readFileSync(testFile, 'utf8');
} catch (e) {
    console.error('error: cannot read test file: ' + e.message);
    process.exit(1);
}

// Strip (require ...) directives — the interpreter has builtins registered directly
source = source.replace(/^\s*\(require\s+[^)]*\)\s*$/gm, '');

// Load wasm module
const wasmBinary = fs.readFileSync(path.join(absWasmDir, 'eshkol-wasm.wasm'));
const createModule = require(path.join(absWasmDir, 'eshkol-wasm.js'));

createModule({ wasmBinary: wasmBinary }).then(function(mod) {
    // Create interpreter context
    var ctx = mod.ccall('eshkol_wasm_interp_create', 'number', [], []);
    mod.ccall('eshkol_wasm_interp_set_recursion_limit', null, ['number', 'number'], [ctx, 50000]);

    // Evaluate
    var ptr = mod.ccall('eshkol_wasm_eval', 'number', ['number', 'string'], [ctx, source]);
    var raw = mod.UTF8ToString(ptr, 100 * 1024 * 1024);
    mod.ccall('eshkol_wasm_free', null, ['number'], [ptr]);
    mod.ccall('eshkol_wasm_interp_destroy', null, ['number'], [ctx]);

    var result;
    try {
        result = JSON.parse(raw);
    } catch(e) {
        console.error('error: invalid JSON response from interpreter');
        process.exit(1);
    }

    // Print captured output
    if (result.output) {
        process.stdout.write(result.output);
    }

    // Check for errors
    if (result.error) {
        console.error('error: ' + result.error);
        process.exit(1);
    }

    // Print result value if non-void
    if (result.value && result.type !== 'void') {
        console.log(result.value);
    }

    process.exit(0);
}).catch(function(e) {
    console.error('error: wasm module failed: ' + e.message);
    process.exit(1);
});
