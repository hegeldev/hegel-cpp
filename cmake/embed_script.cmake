if(NOT DEFINED INPUT OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "embed_script.cmake: INPUT and OUTPUT must be set")
endif()

file(READ "${INPUT}" CONTENT)
file(WRITE "${OUTPUT}"
"// Generated from ${INPUT}. DO NOT EDIT.
#include <cstddef>

extern const char UV_INSTALL_SCRIPT[];
extern const std::size_t UV_INSTALL_SCRIPT_LEN;

const char UV_INSTALL_SCRIPT[] = R\"HEGELUVINSTALL(${CONTENT})HEGELUVINSTALL\";
const std::size_t UV_INSTALL_SCRIPT_LEN = sizeof(UV_INSTALL_SCRIPT) - 1;
")
