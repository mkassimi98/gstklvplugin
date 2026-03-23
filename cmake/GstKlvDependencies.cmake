# Author: Mouhsine Kassimi Farhaoui
# Mail: mouhsine98@gmail.com

include_guard(GLOBAL)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GST REQUIRED IMPORTED_TARGET "gstreamer-1.0>=1.20")
pkg_check_modules(GST_BASE REQUIRED IMPORTED_TARGET "gstreamer-base-1.0>=1.20")
pkg_check_modules(GST_APP REQUIRED IMPORTED_TARGET "gstreamer-app-1.0>=1.20")

find_library(GSTKLVPLUGIN_MATH_LIBRARY m)
if(NOT GSTKLVPLUGIN_MATH_LIBRARY)
  message(FATAL_ERROR "Required math library 'm' was not found")
endif()

add_library(gstklv_project_defaults INTERFACE)
add_library(gstklv::project_defaults ALIAS gstklv_project_defaults)
target_include_directories(gstklv_project_defaults
  INTERFACE
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)
target_compile_definitions(gstklv_project_defaults
  INTERFACE
    PACKAGE=\"gstklvplugin\"
)

add_library(gstklv_gstreamer INTERFACE)
add_library(gstklv::gstreamer ALIAS gstklv_gstreamer)
target_link_libraries(gstklv_gstreamer
  INTERFACE
    PkgConfig::GST
    PkgConfig::GST_BASE
)

add_library(gstklv_gstreamer_app INTERFACE)
add_library(gstklv::gstreamer_app ALIAS gstklv_gstreamer_app)
target_link_libraries(gstklv_gstreamer_app
  INTERFACE
    gstklv::gstreamer
    PkgConfig::GST_APP
)
