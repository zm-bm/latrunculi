set(LATRUNCULI_SLIDER_BACKEND "AUTO" CACHE STRING "Sliding attack backend: AUTO, MAGIC, or PEXT")
set_property(CACHE LATRUNCULI_SLIDER_BACKEND PROPERTY STRINGS AUTO MAGIC PEXT)

set(LATRUNCULI_VALID_SLIDER_BACKENDS AUTO MAGIC PEXT)
if(NOT LATRUNCULI_SLIDER_BACKEND IN_LIST LATRUNCULI_VALID_SLIDER_BACKENDS)
    message(FATAL_ERROR
        "Unsupported LATRUNCULI_SLIDER_BACKEND=${LATRUNCULI_SLIDER_BACKEND}; "
        "expected AUTO, MAGIC, or PEXT")
endif()

set(LATRUNCULI_USE_PEXT OFF)
if(NOT LATRUNCULI_SLIDER_BACKEND STREQUAL "MAGIC")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$"
       AND CMAKE_SIZEOF_VOID_P EQUAL 8
       AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        include(CheckCXXCompilerFlag)
        check_cxx_compiler_flag("-mbmi2" LATRUNCULI_COMPILER_HAS_BMI2)

        if(LATRUNCULI_COMPILER_HAS_BMI2)
            if(LATRUNCULI_SLIDER_BACKEND STREQUAL "PEXT")
                set(LATRUNCULI_USE_PEXT ON)
            elseif(NOT CMAKE_CROSSCOMPILING)
                include(CheckCXXSourceRuns)
                # This result depends on the current CPU, not just the compiler.
                unset(LATRUNCULI_CPU_HAS_FAST_PEXT CACHE)
                check_cxx_source_runs([[
                    int main() {
                        __builtin_cpu_init();
                        // Excavator and Zen/Zen+/Zen 2 have slow, microcoded PEXT.
                        const bool fast = __builtin_cpu_supports("bmi2")
                            && (__builtin_cpu_is("intel")
                                || (__builtin_cpu_is("amd")
                                    && !__builtin_cpu_is("bdver4")
                                    && !__builtin_cpu_is("znver1")
                                    && !__builtin_cpu_is("znver2")));
                        return fast ? 0 : 1;
                    }
                ]] LATRUNCULI_CPU_HAS_FAST_PEXT)
                if(LATRUNCULI_CPU_HAS_FAST_PEXT)
                    set(LATRUNCULI_USE_PEXT ON)
                endif()
            endif()
        endif()
    endif()

    if(LATRUNCULI_SLIDER_BACKEND STREQUAL "PEXT" AND NOT LATRUNCULI_USE_PEXT)
        message(FATAL_ERROR "The PEXT backend requires x86-64 and a compiler supporting -mbmi2")
    endif()
endif()

if(LATRUNCULI_USE_PEXT)
    message(STATUS "Sliding attacks: PEXT (${LATRUNCULI_SLIDER_BACKEND}, requires BMI2)")
else()
    message(STATUS "Sliding attacks: MAGIC (${LATRUNCULI_SLIDER_BACKEND})")
endif()
