# Compiles bgfx shaders for every renderer the platform supports, tracking the files each shader includes.
#
# bgfx's own bgfx_compile_shaders() only rebuilds a shader when its .sc file or varying.def.sc changes, so editing an
# included file (model_light.sh, or vs_object.sc that the object variants include) left stale compiled variants in an
# existing build folder. Here every shader and renderer is its own build step, and shaderc writes the files it actually
# read (includes followed transitively, bgfx_shader.sh and varying.def.sc too) to a depfile next to the output, so the
# build system rebuilds exactly the variants that read a changed file.
#
# openblack_compile_shaders(
#   TYPE VERTEX|FRAGMENT|COMPUTE
#   SHADERS files...
#   VARYING_DEF file
#   OUTPUT_DIR directory
#   OUT_FILES_VAR variable   # receives the generated headers; add them to a target's sources so they get built
#   [INCLUDE_DIRS directories...]
# )
#
# Output layout and header symbols match bgfx_compile_shaders(... AS_HEADERS):
# <OUTPUT_DIR>/<renderer>/<shader>.bin.h defining <shader name>_<renderer>.

# Generators that read DEPFILE. Others fall back to depending on every shader source next to the shaders, which
# rebuilds too much but never leaves a variant stale.
if (CMAKE_GENERATOR MATCHES "Ninja" OR NOT CMAKE_VERSION VERSION_LESS 3.21)
  set(OPENBLACK_SHADER_DEPFILES ON)
else ()
  set(OPENBLACK_SHADER_DEPFILES OFF)
endif ()

function (openblack_compile_shaders)
  set(options "")
  set(oneValueArgs TYPE VARYING_DEF OUTPUT_DIR OUT_FILES_VAR)
  set(multiValueArgs SHADERS INCLUDE_DIRS)
  cmake_parse_arguments(
    ARGS "${options}" "${oneValueArgs}" "${multiValueArgs}" "${ARGN}"
  )

  # Renderers per platform, as bgfx_compile_shaders() picks them
  set(profiles 120 300_es spirv)
  if (IOS)
    set(platform IOS)
    list(APPEND profiles metal)
  elseif (ANDROID)
    set(platform ANDROID)
  elseif (UNIX AND NOT APPLE)
    set(platform LINUX)
  elseif (EMSCRIPTEN)
    set(platform ASM_JS)
  elseif (APPLE)
    set(platform OSX)
    list(APPEND profiles metal)
  elseif (
    WIN32
    OR MINGW
    OR MSYS
    OR CYGWIN
  )
    set(platform WINDOWS)
    list(APPEND profiles s_4_0 s_5_0)
  else ()
    message(FATAL_ERROR "shaderc: unsupported platform")
  endif ()

  set(fallback_depends "")
  if (NOT OPENBLACK_SHADER_DEPFILES)
    set(shader_dirs "")
    foreach (shader IN LISTS ARGS_SHADERS)
      get_filename_component(shader_dir "${shader}" DIRECTORY)
      list(APPEND shader_dirs "${shader_dir}")
    endforeach ()
    list(REMOVE_DUPLICATES shader_dirs)
    foreach (shader_dir IN LISTS shader_dirs ARGS_INCLUDE_DIRS)
      file(GLOB dir_sources CONFIGURE_DEPENDS "${shader_dir}/*.sh"
           "${shader_dir}/*.sc"
      )
      list(APPEND fallback_depends ${dir_sources})
    endforeach ()
  endif ()

  set(all_outputs "")
  foreach (shader IN LISTS ARGS_SHADERS)
    get_filename_component(shader_file_name "${shader}" NAME)
    get_filename_component(shader_name "${shader}" NAME_WE)
    get_filename_component(shader_absolute "${shader}" ABSOLUTE)

    foreach (profile IN LISTS profiles)
      _bgfx_get_profile_path_ext(${profile} profile_dir)
      _bgfx_get_profile_ext(${profile} profile_ext)
      set(output_dir "${ARGS_OUTPUT_DIR}/${profile_dir}")
      set(output "${output_dir}/${shader_file_name}.bin.h")
      set(profile_platform ${platform})
      if (profile STREQUAL "spirv")
        set(profile_platform LINUX)
      endif ()

      set(depends_flag "")
      set(depfile_args "")
      if (OPENBLACK_SHADER_DEPFILES)
        # shaderc writes <output>.d listing every file it read
        set(depends_flag DEPENDS)
        set(depfile_args DEPFILE "${output}.d")
      endif ()

      # Same flags as bgfx_compile_shaders() produces (its debug flag never reaches shaderc, so none here either)
      _bgfx_shaderc_parse(
        cli ${ARGS_TYPE} ${profile_platform} WERROR ${depends_flag}
        BIN2C ${shader_name}_${profile_ext}
        FILE ${shader_absolute}
        OUTPUT ${output}
        PROFILE ${profile}
        O "$<$<CONFIG:debug>:0>$<$<CONFIG:release>:3>$<$<CONFIG:relwithdebinfo>:3>$<$<CONFIG:minsizerel>:3>"
        VARYINGDEF ${ARGS_VARYING_DEF}
        INCLUDES ${BGFX_SHADER_INCLUDE_PATH} ${ARGS_INCLUDE_DIRS}
      )

      # Depfile paths are absolute (shaderc gets absolute paths); NEW keeps them right for every generator.
      cmake_policy(PUSH)
      if (POLICY CMP0116)
        cmake_policy(SET CMP0116 NEW)
      endif ()
      add_custom_command(
        OUTPUT "${output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${output_dir}"
        COMMAND bgfx::shaderc ${cli}
        DEPENDS "${shader_absolute}" "${ARGS_VARYING_DEF}"
                "$<TARGET_FILE:bgfx::shaderc>" ${fallback_depends}
                ${depfile_args}
        COMMENT "Compiling shader ${shader_file_name} for ${profile_dir}"
        VERBATIM
      )
      cmake_policy(POP)

      list(APPEND all_outputs "${output}")
    endforeach ()
  endforeach ()

  set(${ARGS_OUT_FILES_VAR}
      ${all_outputs}
      PARENT_SCOPE
  )
endfunction ()
