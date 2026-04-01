/**
 * Eshkol Compiler - WebAssembly JavaScript Wrapper
 *
 * Usage:
 *   import { init, parse, prettyPrint } from './eshkol.js';
 *
 *   const eshkol = await init();
 *   const ast = eshkol.parse('(define x 42)');
 *   const text = eshkol.prettyPrint('(+ 1 2)');
 */

let moduleInstance = null;

/**
 * Initialize the Eshkol WebAssembly module.
 * Must be called before using parse() or prettyPrint().
 * @returns {Promise<object>} The eshkol API object
 */
export async function init() {
    // Load the Emscripten-generated module
    // Emscripten with MODULARIZE=1 exposes a factory function.
    // In browser context it's a global (var EshkolCompiler), in Node it's module.exports.
    const imported = await import('./eshkol-wasm.js');
    const createModule = imported.default || imported.EshkolCompiler || (typeof EshkolCompiler !== 'undefined' ? EshkolCompiler : undefined);
    if (typeof createModule !== 'function') {
        throw new Error('Failed to load Emscripten module — EshkolCompiler factory not found');
    }
    moduleInstance = await createModule();

    return {
        parse,
        prettyPrint,
    };
}

/**
 * Parse eshkol source code and return a JSON AST.
 * @param {string} source - Eshkol source code
 * @returns {object} Parsed AST as a JavaScript object
 */
export function parse(source) {
    if (!moduleInstance) throw new Error('Call init() first');

    const ptr = moduleInstance.ccall(
        'eshkol_wasm_parse', 'number', ['string'], [source]
    );
    const json = moduleInstance.UTF8ToString(ptr);
    moduleInstance.ccall('eshkol_wasm_free', null, ['number'], [ptr]);
    return JSON.parse(json);
}

/**
 * Pretty-print eshkol source code (parse then dump AST).
 * @param {string} source - Eshkol source code
 * @returns {string} Pretty-printed AST text
 */
export function prettyPrint(source) {
    if (!moduleInstance) throw new Error('Call init() first');

    const ptr = moduleInstance.ccall(
        'eshkol_wasm_pretty_print', 'number', ['string'], [source]
    );
    const text = moduleInstance.UTF8ToString(ptr);
    moduleInstance.ccall('eshkol_wasm_free', null, ['number'], [ptr]);
    return text;
}
