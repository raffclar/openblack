# The mod loader: a shared library of its own, never linked into the game, which loads it at start-up when it is there.
# It depends on nothing but the standard library
add_library(ModLoader SHARED ModLoader.cpp ModLoader.h)
target_compile_definitions(ModLoader PRIVATE MODLOADER_BUILD)
target_include_directories(ModLoader PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# The same file name on every platform (ModLoader.dll, ModLoader.so, ModLoader.dylib), next to the game
set_target_properties(
  ModLoader
  PROPERTIES PREFIX ""
             CXX_VISIBILITY_PRESET hidden
             RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin
             LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin
)
set_property(TARGET ModLoader PROPERTY FOLDER "ModLoader")
