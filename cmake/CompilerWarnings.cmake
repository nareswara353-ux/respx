function(kave_set_warnings TARGET)
    target_compile_options(${TARGET} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -Wshadow
        -Wformat=2
        -Wconversion
        -Wsign-conversion
        -Wnull-dereference
        -Wdouble-promotion
        -Wstrict-prototypes
        -Wmissing-prototypes
        -Wmissing-declarations
        -Wredundant-decls
        -Wwrite-strings
        -std=c11
    )
    if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
        target_compile_options(${TARGET} PRIVATE -Wno-gnu-statement-expression)
    endif()
endfunction()
