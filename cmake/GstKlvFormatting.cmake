# Author: Mouhsine Kassimi Farhaoui
# Mail: mouhsine98@gmail.com

include_guard(GLOBAL)

find_program(GSTKLVPLUGIN_BASH_PROGRAM bash)
find_program(GSTKLVPLUGIN_CLANG_FORMAT_PROGRAM clang-format)

if(GSTKLVPLUGIN_BASH_PROGRAM AND GSTKLVPLUGIN_CLANG_FORMAT_PROGRAM)
  add_custom_target(format
    COMMAND "${GSTKLVPLUGIN_BASH_PROGRAM}" "${PROJECT_SOURCE_DIR}/scripts/run_clang_tools.sh" --format
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Format repository C/C++ sources with clang-format"
    VERBATIM
  )

  add_custom_target(format-check
    COMMAND "${GSTKLVPLUGIN_BASH_PROGRAM}" "${PROJECT_SOURCE_DIR}/scripts/run_clang_tools.sh" --format-check
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Validate repository C/C++ formatting with clang-format"
    VERBATIM
  )

  add_custom_target(lint DEPENDS format-check)
elseif(NOT GSTKLVPLUGIN_BASH_PROGRAM)
  message(STATUS "bash not found; clang-format targets disabled")
else()
  message(STATUS "clang-format not found; format targets disabled")
endif()
