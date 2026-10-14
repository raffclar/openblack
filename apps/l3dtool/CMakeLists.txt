set(L3DTOOL l3dtool.cpp GltfExport.cpp GltfExport.h json.hpp tiny_gltf.h)
# The exporter poses the bones and blends the seams with the game's own rules
set(L3DTOOL_GAME_SOURCES ${CMAKE_SOURCE_DIR}/src/3D/SkeletalAnimation.cpp ${CMAKE_SOURCE_DIR}/src/3D/VertexBlend.cpp)

source_group(apps\\l3dtool FILES ${L3DTOOL})

add_executable(l3dtool ${L3DTOOL} ${L3DTOOL_GAME_SOURCES})

target_compile_definitions(
  l3dtool PRIVATE CXXOPTS_NO_EXCEPTIONS GLM_FORCE_LEFT_HANDED GLM_FORCE_RADIANS
)
target_link_libraries(l3dtool PRIVATE l3d morph pack glm::glm)
target_include_directories(
  l3dtool PRIVATE ${CXXOPTS_INCLUDE_DIRS} ${Stb_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/src
)

if (OPENBLACK_CLANG_TIDY_CHECKS)
  if (CLANG_TIDY)
    set_target_properties(l3dtool PROPERTIES CXX_CLANG_TIDY ${CLANG_TIDY})
  else ()
    message("Clang-tidy checks requested but unavailable")
  endif ()
endif ()

if (MSVC)
  target_compile_definitions(l3dtool PRIVATE _HAS_EXCEPTIONS=0 _CRT_SECURE_NO_WARNINGS)
  # glm uses nameless structs
  target_compile_options(l3dtool PRIVATE /W4 /WX /EHs-c- /wd4201)
else ()
  target_compile_options(
    l3dtool
    PRIVATE -Wall -Wextra -pedantic -Werror -fno-exceptions
            # Disabled due to tiny-gltf
            -Wno-switch
  )
endif ()

set_property(TARGET l3dtool PROPERTY FOLDER "tools")
