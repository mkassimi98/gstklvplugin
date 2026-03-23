# Author: Mouhsine Kassimi Farhaoui
# Mail: mouhsine98@gmail.com

include_guard(GLOBAL)

function(gstklv_add_example target_name)
  set(options USE_GST_APP)
  set(oneValueArgs SOURCE)
  set(multiValueArgs LIBRARIES)
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(NOT ARG_SOURCE)
    message(FATAL_ERROR "gstklv_add_example(${target_name}) requires SOURCE")
  endif()

  add_executable("${target_name}" "${ARG_SOURCE}")
  target_compile_features("${target_name}" PRIVATE cxx_std_17)
  target_compile_definitions("${target_name}"
    PRIVATE
      GSTKLVPLUGIN_SOURCE_DIR=\"${PROJECT_SOURCE_DIR}\"
  )
  set_target_properties("${target_name}"
    PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
  )

  if(ARG_USE_GST_APP)
    set(gstklv_example_gstreamer_target gstklv::gstreamer_app)
  else()
    set(gstklv_example_gstreamer_target gstklv::gstreamer)
  endif()

  target_link_libraries("${target_name}"
    PRIVATE
      gstklv::project_defaults
      "${gstklv_example_gstreamer_target}"
      ${ARG_LIBRARIES}
  )
endfunction()
