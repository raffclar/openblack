# EXPERIMENT: a reader of binary glTF files for the imported test creature. It uses the tinygltf and json headers
# that the l3dtool app already carries.
file(GLOB SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp")
file(GLOB HEADERS "${CMAKE_CURRENT_LIST_DIR}/include/*.h")

add_library(glb STATIC ${SOURCES} ${HEADERS})

target_include_directories(
  glb
  PUBLIC $<INSTALL_INTERFACE:include>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  PRIVATE ${CMAKE_SOURCE_DIR}/apps/l3dtool
)

if (MSVC)
  target_compile_definitions(
    glb PRIVATE _HAS_EXCEPTIONS=0 _CRT_SECURE_NO_WARNINGS
  )
  target_compile_options(glb PRIVATE /W4 /EHs-c-)
else ()
  target_compile_options(
    glb PRIVATE -Wall -Wextra -pedantic -fno-exceptions -Wno-switch
  )
endif ()

set_property(TARGET glb PROPERTY FOLDER "components")
