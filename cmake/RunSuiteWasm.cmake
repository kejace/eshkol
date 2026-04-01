# RunSuiteWasm.cmake — Run an Eshkol test suite via the wasm interpreter (Node.js)
#
# Required variables:
#   PROJECT_ROOT  — path to eshkol-src
#   BUILD_DIR     — path to wasm build directory
#   SUITE_NAME    — name of the test suite
#   TEST_GLOB     — glob pattern for test files
#   NODE_EXE      — path to node executable
#
# Optional:
#   NEGATIVE_MODE — "none" (default), "compile", or "compile_or_runtime"

if(NOT DEFINED PROJECT_ROOT OR NOT DEFINED BUILD_DIR OR NOT DEFINED SUITE_NAME OR NOT DEFINED TEST_GLOB OR NOT DEFINED NODE_EXE)
    message(FATAL_ERROR "RunSuiteWasm.cmake requires PROJECT_ROOT, BUILD_DIR, SUITE_NAME, TEST_GLOB, NODE_EXE")
endif()

if(NOT DEFINED NEGATIVE_MODE)
    set(NEGATIVE_MODE "none")
endif()

# The Node.js test runner script
set(RUNNER_SCRIPT "${PROJECT_ROOT}/cmake/wasm_test_runner.cjs")
set(WASM_DIR "${BUILD_DIR}/wasm")

if(NOT EXISTS "${WASM_DIR}/eshkol-wasm.js")
    message(FATAL_ERROR "eshkol-wasm.js not found in ${WASM_DIR}")
endif()

file(GLOB TEST_FILES LIST_DIRECTORIES false "${PROJECT_ROOT}/${TEST_GLOB}")
if(NOT TEST_FILES)
    message(FATAL_ERROR "No tests matched ${TEST_GLOB}")
endif()

set(failures "")
set(pass_count 0)
set(fail_count 0)
set(skip_count 0)

message(STATUS "Running Eshkol WASM suite ${SUITE_NAME}")

foreach(test_file IN LISTS TEST_FILES)
    file(RELATIVE_PATH TEST_RELATIVE_PATH "${PROJECT_ROOT}" "${test_file}")

    # Read test to detect markers
    file(READ "${test_file}" TEST_CONTENTS)
    string(FIND "${TEST_CONTENTS}" ";;; Expected: Error" EXPECTED_ERROR_MARKER)
    set(IS_NEGATIVE FALSE)
    if(NOT EXPECTED_ERROR_MARKER EQUAL -1)
        set(IS_NEGATIVE TRUE)
    endif()

    # Run test via Node.js
    execute_process(
        COMMAND "${NODE_EXE}" "${RUNNER_SCRIPT}" "${WASM_DIR}" "${test_file}"
        WORKING_DIRECTORY "${BUILD_DIR}"
        RESULT_VARIABLE RUN_STATUS
        OUTPUT_VARIABLE RUN_STDOUT
        ERROR_VARIABLE RUN_STDERR
        TIMEOUT 30
    )
    set(RUN_OUTPUT "${RUN_STDOUT}\n${RUN_STDERR}")

    # Check for "error:" in output (matches native runner behavior)
    string(FIND "${RUN_OUTPUT}" "error:" RUNTIME_ERROR_MARKER)
    set(HAS_ERROR FALSE)
    if(NOT RUN_STATUS EQUAL 0 OR NOT RUNTIME_ERROR_MARKER EQUAL -1)
        set(HAS_ERROR TRUE)
    endif()

    if(IS_NEGATIVE)
        # Negative test: expected to fail
        if(HAS_ERROR)
            math(EXPR pass_count "${pass_count} + 1")
        else()
            math(EXPR fail_count "${fail_count} + 1")
            string(APPEND failures "\n${TEST_RELATIVE_PATH}: expected error but test succeeded")
        endif()
    else()
        # Positive test: expected to succeed
        if(HAS_ERROR)
            math(EXPR fail_count "${fail_count} + 1")
            # Truncate long output
            string(LENGTH RUN_OUTPUT OUTPUT_LEN)
            if(OUTPUT_LEN GREATER 500)
                string(SUBSTRING "${RUN_OUTPUT}" 0 500 RUN_OUTPUT)
                string(APPEND RUN_OUTPUT "... [truncated]")
            endif()
            string(APPEND failures "\n${TEST_RELATIVE_PATH}: failed\n${RUN_OUTPUT}")
        else()
            math(EXPR pass_count "${pass_count} + 1")
        endif()
    endif()
endforeach()

math(EXPR total "${pass_count} + ${fail_count} + ${skip_count}")

if(fail_count GREATER 0)
    message(FATAL_ERROR "Suite ${SUITE_NAME} (wasm): ${pass_count} passed, ${fail_count} failed of ${total}${failures}")
endif()

message(STATUS "Suite ${SUITE_NAME} (wasm): ${pass_count} passed of ${total}")
