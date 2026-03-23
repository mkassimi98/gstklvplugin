# Author: Mouhsine Kassimi Farhaoui
# Mail: mouhsine98@gmail.com

cmake_minimum_required(VERSION 3.16)
project(gstklvplugin VERSION 1.0.0 LANGUAGES C CXX)

include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(GstKlvOptions)
include(GstKlvDependencies)
include(GstKlvFormatting)

add_subdirectory(src)

if(GSTKLVPLUGIN_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

install(
  FILES data/stanag4609_tags.ini
  DESTINATION "${CMAKE_INSTALL_DATADIR}/gstklvplugin"
)

install(
  FILES
    LICENSE
    README.md
    CHANGELOG.md
  DESTINATION "${CMAKE_INSTALL_DOCDIR}"
)
