include(CTest)

# The tests that use the game's library are built into one program, openblack_tests: linking a program for each of them
# took most of a build. Each test source is still a ctest test of its own, named as before: ctest runs the program with
# --openblack-test-source=<source>, which runs only that source's tests, in a process of its own.
add_executable(openblack_tests test_runner_main.cpp)
target_link_libraries(openblack_tests PRIVATE GTest::gtest openblack_lib)
set_property(TARGET openblack_tests PROPERTY FOLDER "tests")
# Its Debug link only references the objects' debug information, which is quicker (the game keeps full debug information)
if (MSVC)
  target_link_options(
    openblack_tests PRIVATE $<$<CONFIG:Debug>:/DEBUG:FASTLINK>
  )
endif ()

# The libraries each test links (target_link_libraries(test_x PRIVATE lib) after adding it) are linked into the program
# once every test is added
function (openblack_link_test_libraries)
  get_property(targets GLOBAL PROPERTY OPENBLACK_TESTS_IN_PROGRAM)
  foreach (target IN LISTS targets)
    get_target_property(libraries ${target} LINK_LIBRARIES)
    if (libraries)
      target_link_libraries(openblack_tests PRIVATE ${libraries})
    endif ()
  endforeach ()
endfunction ()
cmake_language(DEFER CALL openblack_link_test_libraries)

# Macro for setting up and adding a simple test.
# TEST_NAME is the name of the test: its ctest test and its target, an object library of the test program that
# target_link_libraries, target_include_directories, target_compile_definitions and add_dependencies apply to.
# TEST_SOURCE is the source file of the test, relative to this folder.
macro (OPENBLACK_SETUP_AND_ADD_TEST TEST_NAME TEST_SOURCE)
  add_library(${TEST_NAME} OBJECT ${TEST_SOURCE})
  add_dependencies(${TEST_NAME} generate_mock_game_data)
  target_link_libraries(${TEST_NAME} PRIVATE GTest::gtest openblack_lib)
  target_compile_definitions(
    ${TEST_NAME} PRIVATE TEST_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}"
                         GLM_ENABLE_EXPERIMENTAL
  )
  set_property(TARGET ${TEST_NAME} PROPERTY FOLDER "tests")
  target_sources(openblack_tests PRIVATE $<TARGET_OBJECTS:${TEST_NAME}>)
  set_property(GLOBAL APPEND PROPERTY OPENBLACK_TESTS_IN_PROGRAM ${TEST_NAME})
  add_test(NAME ${TEST_NAME} COMMAND openblack_tests
                                     --openblack-test-source=${TEST_SOURCE}
  )
endmacro ()

# Macro for setting up a test that uses JSON scenario files for validation.
# This macro does everything the standard test setup does, but also copies a
# directory of JSON scenarios into the build directory and adds a dependency on
# these scenarios.
# The TEST_NAME and TEST_SOURCE arguments have the same purpose as in the standard setup.
# The test should be in its own directory
macro (OPENBLACK_SETUP_AND_ADD_JSON_TEST TEST_NAME TEST_SOURCE)
  get_filename_component(TEST_DIR_NAME ${TEST_SOURCE} DIRECTORY)
  add_custom_target(
    ${TEST_NAME}_scenarios
    COMMAND
      ${CMAKE_COMMAND} -E copy_directory
      ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_DIR_NAME}/scenarios
      ${CMAKE_CURRENT_BINARY_DIR}/${TEST_DIR_NAME}/scenarios
  )
  set_property(TARGET ${TEST_NAME}_scenarios PROPERTY FOLDER "tests")
  openblack_setup_and_add_test(${TEST_NAME} ${TEST_SOURCE})
  target_include_directories(
    ${TEST_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party
                         vector_compare.h
  )
  add_dependencies(${TEST_NAME} ${TEST_NAME}_scenarios)
endmacro ()

openblack_setup_and_add_test(test_game_initialize test_game_initialize.cpp)
openblack_setup_and_add_test(test_load_scene test_load_scene.cpp)
openblack_setup_and_add_test(test_fixed test_fixed.cpp)
openblack_setup_and_add_test(test_alignment test_alignment.cpp)
openblack_setup_and_add_test(test_virtual_influence test_virtual_influence.cpp)
openblack_setup_and_add_test(test_mana_path test_mana_path.cpp)
target_link_libraries(test_mana_path PRIVATE psys)
openblack_setup_and_add_test(
  test_influence_virtual_grant test_influence_virtual_grant.cpp
)
openblack_setup_and_add_test(test_player_system test_player_system.cpp)
openblack_setup_and_add_test(test_sound_tags test_sound_tags.cpp)
target_link_libraries(test_sound_tags PRIVATE pack l3d morph)
openblack_setup_and_add_test(test_interpolator test_interpolator.cpp)
openblack_setup_and_add_test(test_set_camera_pos camera/test_set_camera_pos.cpp)
openblack_setup_and_add_test(
  test_keyboard_move_speed camera/test_keyboard_move_speed.cpp
)
openblack_setup_and_add_json_test(
  test_mobile_wall_hug mobile_wall_hug/test_mobile_wall_hug.cpp
)
openblack_setup_and_add_json_test(test_camera camera/test_camera.cpp)
openblack_setup_and_add_json_test(test_atmos audio/test_atmos.cpp)
target_link_libraries(test_atmos PRIVATE pack lnd)
openblack_setup_and_add_test(test_sound_decoding audio/test_sound_decoding.cpp)
openblack_setup_and_add_test(test_music audio/test_music.cpp)
openblack_setup_and_add_test(test_music_decoding audio/test_music_decoding.cpp)
openblack_setup_and_add_test(test_anim_effects audio/test_anim_effects.cpp)
openblack_setup_and_add_test(test_help_speech audio/test_help_speech.cpp)
openblack_setup_and_add_test(test_listener_frame audio/test_listener_frame.cpp)
openblack_setup_and_add_test(test_hearing_range audio/test_hearing_range.cpp)
openblack_setup_and_add_test(
  test_script_sound_effect audio/test_script_sound_effect.cpp
)
target_link_libraries(test_script_sound_effect PRIVATE ScriptLibrary)
openblack_setup_and_add_test(test_interface gui/test_interface.cpp)
openblack_setup_and_add_test(test_text_wrap gui/test_text_wrap.cpp)
openblack_setup_and_add_test(test_skip_box gui/test_skip_box.cpp)
openblack_setup_and_add_test(test_tool_tips gui/test_tool_tips.cpp)
openblack_setup_and_add_test(test_screen_fade gui/test_screen_fade.cpp)
openblack_setup_and_add_test(test_cinematics gui/test_cinematics.cpp)
openblack_setup_and_add_test(
  test_cinematic_director gui/test_cinematic_director.cpp
)
openblack_setup_and_add_test(
  test_dialogue_control gui/test_dialogue_control.cpp
)
target_link_libraries(test_sound_decoding PRIVATE pack)
openblack_setup_and_add_test(test_hand_animation hand/test_hand_animation.cpp)
target_link_libraries(test_hand_animation PRIVATE pack l3d morph)
openblack_setup_and_add_test(test_hand_shadow hand/test_hand_shadow.cpp)
openblack_setup_and_add_test(test_hand_cross_fade hand/test_hand_cross_fade.cpp)
openblack_setup_and_add_test(test_hand_visibility hand/test_hand_visibility.cpp)
openblack_setup_and_add_test(
  test_hand_orientation hand/test_hand_orientation.cpp
)
openblack_setup_and_add_test(
  test_hand_navigation_pose hand/test_hand_navigation_pose.cpp
)
openblack_setup_and_add_test(test_hand_morph hand/test_hand_morph.cpp)
openblack_setup_and_add_test(test_hand_grab hand/test_hand_grab.cpp)
openblack_setup_and_add_test(
  test_hand_grab_system hand/test_hand_grab_system.cpp
)
openblack_setup_and_add_test(test_hand_feel hand/test_hand_feel.cpp)
openblack_setup_and_add_test(test_hand_reach hand/test_hand_reach.cpp)
openblack_setup_and_add_test(test_villager_pickup hand/test_villager_pickup.cpp)
openblack_setup_and_add_test(test_hand_click hand/test_hand_click.cpp)
target_link_libraries(test_hand_click PRIVATE ScriptLibrary)
openblack_setup_and_add_test(test_script_properties test_script_properties.cpp)
target_link_libraries(test_script_properties PRIVATE ScriptLibrary)
openblack_setup_and_add_test(test_script_timers test_script_timers.cpp)
openblack_setup_and_add_test(test_script_switches test_script_switches.cpp)
openblack_setup_and_add_test(test_script_name_lists test_script_name_lists.cpp)
openblack_setup_and_add_test(test_script_highlights test_script_highlights.cpp)
openblack_setup_and_add_test(test_hand_hold_pose test_hand_hold_pose.cpp)
openblack_setup_and_add_test(test_tribal_power_spin test_tribal_power_spin.cpp)
openblack_setup_and_add_test(
  test_object_shadows graphics/test_object_shadows.cpp
)
openblack_setup_and_add_test(test_hand_light graphics/test_hand_light.cpp)
openblack_setup_and_add_test(
  test_hand_water_glow graphics/test_hand_water_glow.cpp
)
openblack_setup_and_add_test(test_model_light graphics/test_model_light.cpp)
openblack_setup_and_add_test(test_sun graphics/test_sun.cpp)
openblack_setup_and_add_test(test_moon graphics/test_moon.cpp)
openblack_setup_and_add_test(test_bone_budget graphics/test_bone_budget.cpp)
openblack_setup_and_add_test(test_bone_palette graphics/test_bone_palette.cpp)
openblack_setup_and_add_test(test_draw_order graphics/test_draw_order.cpp)
openblack_setup_and_add_test(test_view_frustum graphics/test_view_frustum.cpp)
openblack_setup_and_add_test(test_mesh_detail graphics/test_mesh_detail.cpp)
openblack_setup_and_add_test(test_detail_level graphics/test_detail_level.cpp)
openblack_setup_and_add_test(test_land_detail graphics/test_land_detail.cpp)
target_link_libraries(test_land_detail PRIVATE lnd)
openblack_setup_and_add_test(
  test_sea_reflection graphics/test_sea_reflection.cpp
)
openblack_setup_and_add_test(
  test_shader_samplers graphics/test_shader_samplers.cpp
)
openblack_setup_and_add_test(test_pack_bank_info pack/test_pack_bank_info.cpp)
target_link_libraries(test_pack_bank_info PRIVATE pack)
openblack_setup_and_add_test(
  test_pack_audio_index pack/test_pack_audio_index.cpp
)
target_link_libraries(test_pack_audio_index PRIVATE pack)
openblack_setup_and_add_test(test_game_random test_game_random.cpp)
openblack_setup_and_add_test(test_crash_report test_crash_report.cpp)
openblack_setup_and_add_test(test_game_clock test_game_clock.cpp)
openblack_setup_and_add_test(test_run_seed test_run_seed.cpp)
openblack_setup_and_add_test(test_key_bindings input/test_key_bindings.cpp)
openblack_setup_and_add_test(test_cursor_freeze input/test_cursor_freeze.cpp)
openblack_setup_and_add_test(test_camera_drag camera/test_camera_drag.cpp)
openblack_setup_and_add_test(test_camera_pan camera/test_camera_pan.cpp)
openblack_setup_and_add_test(test_camera_help camera/test_camera_help.cpp)
openblack_setup_and_add_test(
  test_camera_help_events camera/test_camera_help_events.cpp
)
openblack_setup_and_add_test(test_magic_tables test_magic_tables.cpp)
openblack_setup_and_add_test(test_spell_chants test_spell_chants.cpp)
openblack_setup_and_add_test(test_worship_battery test_worship_battery.cpp)
openblack_setup_and_add_test(test_particle_file test_particle_file.cpp)
target_link_libraries(test_particle_file PRIVATE psys)
openblack_setup_and_add_test(test_particle_maths test_particle_maths.cpp)
target_link_libraries(test_particle_maths PRIVATE psys)
openblack_setup_and_add_test(test_particle_effect test_particle_effect.cpp)
target_link_libraries(test_particle_effect PRIVATE psys)
openblack_setup_and_add_test(test_particle_draw test_particle_draw.cpp)
target_link_libraries(test_particle_draw PRIVATE psys)
openblack_setup_and_add_test(test_particle_beam test_particle_beam.cpp)
target_link_libraries(test_particle_beam PRIVATE psys)
openblack_setup_and_add_test(test_particle_plasma test_particle_plasma.cpp)
target_link_libraries(test_particle_plasma PRIVATE psys)
openblack_setup_and_add_test(test_map_coords test_map_coords.cpp)
openblack_setup_and_add_test(test_land_normal test_land_normal.cpp)
openblack_setup_and_add_test(test_picking test_picking.cpp)
openblack_setup_and_add_test(test_tree_brightness test_tree_brightness.cpp)
openblack_setup_and_add_test(test_block_texture test_block_texture.cpp)
openblack_setup_and_add_test(test_footprint_paint test_footprint_paint.cpp)
target_link_libraries(test_footprint_paint PRIVATE lnd)
target_link_libraries(test_block_texture PRIVATE lnd)
openblack_setup_and_add_test(test_land_vertex test_land_vertex.cpp)
openblack_setup_and_add_test(test_sea_rows test_sea_rows.cpp)
openblack_setup_and_add_test(test_texel_depth test_texel_depth.cpp)
openblack_setup_and_add_test(test_day_night_clock test_day_night_clock.cpp)
openblack_setup_and_add_test(test_mists test_mists.cpp)
openblack_setup_and_add_test(test_clouds test_clouds.cpp)
openblack_setup_and_add_test(test_village_lights test_village_lights.cpp)
openblack_setup_and_add_test(test_land_light_table test_land_light_table.cpp)
openblack_setup_and_add_test(test_lightning test_lightning.cpp)
openblack_setup_and_add_test(
  test_land_colour_stamps test_land_colour_stamps.cpp
)
openblack_setup_and_add_test(test_sky_dome test_sky_dome.cpp)
openblack_setup_and_add_test(test_sky_frame test_sky_frame.cpp)
openblack_setup_and_add_test(test_camera_track test_camera_track.cpp)
target_link_libraries(test_camera_track PRIVATE edt)
openblack_setup_and_add_test(test_shark_rules test_shark_rules.cpp)
openblack_setup_and_add_test(test_villager_eyes test_villager_eyes.cpp)
openblack_setup_and_add_test(test_script_flocks test_script_flocks.cpp)
openblack_setup_and_add_test(test_dances test_dances.cpp)
target_link_libraries(test_dances PRIVATE dance)
openblack_setup_and_add_test(test_sky_system test_sky_system.cpp)
openblack_setup_and_add_test(test_near_clipping test_near_clipping.cpp)
openblack_setup_and_add_test(test_ground_blobs test_ground_blobs.cpp)
openblack_setup_and_add_test(test_zsort test_zsort.cpp)
openblack_setup_and_add_test(test_rain test_rain.cpp)
openblack_setup_and_add_test(test_chimney_smoke test_chimney_smoke.cpp)
openblack_setup_and_add_test(test_native_stub_calls test_native_stub_calls.cpp)
openblack_setup_and_add_test(test_influence_circle test_influence_circle.cpp)
openblack_setup_and_add_test(test_field_crop test_field_crop.cpp)
openblack_setup_and_add_test(test_fish_farm test_fish_farm.cpp)
openblack_setup_and_add_test(test_snow_cover test_snow_cover.cpp)
openblack_setup_and_add_test(test_snowfall test_snowfall.cpp)
openblack_setup_and_add_test(test_water_rings test_water_rings.cpp)
openblack_setup_and_add_test(test_frame_pace test_frame_pace.cpp)
openblack_setup_and_add_test(test_flat_land test_flat_land.cpp)
openblack_setup_and_add_test(test_creature_body test_creature_body.cpp)
openblack_setup_and_add_test(test_creature_removal test_creature_removal.cpp)
openblack_setup_and_add_test(test_injected_input test_injected_input.cpp)
openblack_setup_and_add_test(test_creature_morph test_creature_morph.cpp)
openblack_setup_and_add_test(
  test_creature_animation test_creature_animation.cpp
)
openblack_setup_and_add_test(test_creature_eyes test_creature_eyes.cpp)
openblack_setup_and_add_test(test_creature_layers test_creature_layers.cpp)
openblack_setup_and_add_test(test_creature_face test_creature_face.cpp)
openblack_setup_and_add_test(test_creature_mind test_creature_mind.cpp)
openblack_setup_and_add_test(test_creature_learning test_creature_learning.cpp)
openblack_setup_and_add_test(
  test_creature_mind_file test_creature_mind_file.cpp
)
openblack_setup_and_add_test(
  test_creature_mind_file_body test_creature_mind_file_body.cpp
)
openblack_setup_and_add_test(test_file_dialog test_file_dialog.cpp)
openblack_setup_and_add_test(
  test_creature_physiology test_creature_physiology.cpp
)
openblack_setup_and_add_test(test_creature_skin test_creature_skin.cpp)
openblack_setup_and_add_test(test_creature_hair test_creature_hair.cpp)
openblack_setup_and_add_test(test_creature_sway test_creature_sway.cpp)
openblack_setup_and_add_test(test_creature_catch test_creature_catch.cpp)
openblack_setup_and_add_test(test_creature_tattoo test_creature_tattoo.cpp)
openblack_setup_and_add_test(test_tattoo_editor test_tattoo_editor.cpp)
openblack_setup_and_add_test(
  test_tattoo_editor_system test_tattoo_editor_system.cpp
)
openblack_setup_and_add_test(test_creature_marks test_creature_marks.cpp)
openblack_setup_and_add_test(
  test_creature_carry_over test_creature_carry_over.cpp
)
openblack_setup_and_add_test(test_vertex_blend test_vertex_blend.cpp)
openblack_setup_and_add_test(
  test_creature_locomotion test_creature_locomotion.cpp
)
openblack_setup_and_add_test(test_miracle_surfaces test_miracle_surfaces.cpp)
target_link_libraries(test_miracle_surfaces PRIVATE psys)
openblack_setup_and_add_test(test_miracle_visuals test_miracle_visuals.cpp)
openblack_setup_and_add_test(test_material_stages test_material_stages.cpp)
openblack_setup_and_add_test(test_resource_piles test_resource_piles.cpp)
openblack_setup_and_add_test(test_resource_cache test_resource_cache.cpp)
openblack_setup_and_add_test(test_upload_pacer test_upload_pacer.cpp)
openblack_setup_and_add_test(test_map_cells test_map_cells.cpp)
openblack_setup_and_add_test(test_script_cast test_script_cast.cpp)
openblack_setup_and_add_test(test_creature_route test_creature_route.cpp)
openblack_setup_and_add_test(
  test_creature_obstacles test_creature_obstacles.cpp
)
openblack_setup_and_add_test(test_creature_audio test_creature_audio.cpp)
openblack_setup_and_add_test(
  test_creature_interaction test_creature_interaction.cpp
)
openblack_setup_and_add_test(
  test_creature_footprints test_creature_footprints.cpp
)
openblack_setup_and_add_test(test_creature_fight test_creature_fight.cpp)
openblack_setup_and_add_test(test_fight_marks test_fight_marks.cpp)
openblack_setup_and_add_test(test_creature_fizz test_creature_fizz.cpp)
openblack_setup_and_add_test(
  test_creature_fizz_look test_creature_fizz_look.cpp
)
openblack_setup_and_add_test(test_creature_arena test_creature_arena.cpp)
openblack_setup_and_add_test(test_land_avoid test_land_avoid.cpp)
openblack_setup_and_add_test(
  test_creature_fight_system test_creature_fight_system.cpp
)
openblack_setup_and_add_test(test_fight_view test_fight_view.cpp)
openblack_setup_and_add_test(test_leash_rope test_leash_rope.cpp)
openblack_setup_and_add_test(test_leash_rules test_leash_rules.cpp)
openblack_setup_and_add_test(test_temple_pen test_temple_pen.cpp)
openblack_setup_and_add_test(test_leash_ownership test_leash_ownership.cpp)
openblack_setup_and_add_test(test_primary_creature test_primary_creature.cpp)
openblack_setup_and_add_test(test_leash_orders test_leash_orders.cpp)
openblack_setup_and_add_test(test_temple_leashes test_temple_leashes.cpp)
openblack_setup_and_add_test(test_testbed_scenarios test_testbed_scenarios.cpp)
openblack_setup_and_add_test(
  test_creature_status_panel test_creature_status_panel.cpp
)
openblack_setup_and_add_test(test_creature_mode test_creature_mode.cpp)
openblack_setup_and_add_test(
  test_creature_attention test_creature_attention.cpp
)
target_link_libraries(test_flat_land PRIVATE lnd)
target_link_libraries(test_testbed_scenarios PRIVATE lnd)
openblack_setup_and_add_test(test_town_desire test_town_desire.cpp)
openblack_setup_and_add_test(test_town_desire_flags test_town_desire_flags.cpp)
openblack_setup_and_add_test(test_gutils_angle test_gutils_angle.cpp)
openblack_setup_and_add_test(test_gutils_distance test_gutils_distance.cpp)
openblack_setup_and_add_test(test_l3d_lightmaps l3d/test_l3d_lightmaps.cpp)
target_link_libraries(test_l3d_lightmaps PRIVATE l3d)
openblack_setup_and_add_test(test_l3d_copy l3d/test_l3d_copy.cpp)
openblack_setup_and_add_test(test_l3d_ray_cast l3d/test_l3d_ray_cast.cpp)
target_link_libraries(test_l3d_ray_cast PRIVATE l3d)
target_link_libraries(test_l3d_copy PRIVATE l3d)
openblack_setup_and_add_test(
  test_l3d_submesh_names l3d/test_l3d_submesh_names.cpp
)
target_link_libraries(test_l3d_submesh_names PRIVATE l3d)
openblack_setup_and_add_test(test_light_beams graphics/test_light_beams.cpp)
target_link_libraries(test_light_beams PRIVATE l3d glw)
openblack_setup_and_add_test(test_temple_camera camera/test_temple_camera.cpp)
openblack_setup_and_add_test(
  test_camera_projection camera/test_camera_projection.cpp
)
openblack_setup_and_add_test(test_camera_lens camera/test_camera_lens.cpp)
openblack_setup_and_add_test(test_script_camera camera/test_script_camera.cpp)
target_link_libraries(test_script_camera PRIVATE edt)
openblack_setup_and_add_test(test_camera_zones camera/test_camera_zones.cpp)
target_link_libraries(test_camera_zones PRIVATE exc)
openblack_setup_and_add_test(test_temple_doors temple/test_temple_doors.cpp)
openblack_setup_and_add_test(
  test_creature_cave_effects temple/test_creature_cave_effects.cpp
)
openblack_setup_and_add_test(
  test_creature_cave_targets temple/test_creature_cave_targets.cpp
)
openblack_setup_and_add_test(test_cave_creature temple/test_cave_creature.cpp)
openblack_setup_and_add_test(
  test_creature_cave_trophies temple/test_creature_cave_trophies.cpp
)
openblack_setup_and_add_test(
  test_temple_exterior_morph temple/test_temple_exterior_morph.cpp
)
openblack_setup_and_add_test(test_temple_light temple/test_temple_light.cpp)
openblack_setup_and_add_test(test_temple_fog temple/test_temple_fog.cpp)
openblack_setup_and_add_test(test_temple_scroll temple/test_temple_scroll.cpp)
openblack_setup_and_add_test(test_temple_toggles temple/test_temple_toggles.cpp)
openblack_setup_and_add_test(test_challenge_room temple/test_challenge_room.cpp)
openblack_setup_and_add_test(test_temple_map temple/test_temple_map.cpp)
target_link_libraries(test_temple_map PRIVATE lnd)
openblack_setup_and_add_test(
  test_temple_map_spells temple/test_temple_map_spells.cpp
)
openblack_setup_and_add_test(test_frame_stats test_frame_stats.cpp)
openblack_setup_and_add_test(test_benchmark test_benchmark.cpp)
target_link_libraries(test_benchmark PRIVATE lnd)
openblack_setup_and_add_test(test_uniform_table graphics/test_uniform_table.cpp)
openblack_setup_and_add_test(test_editor test_editor.cpp)
openblack_setup_and_add_test(test_editor_scripts test_editor_scripts.cpp)
target_link_libraries(test_editor_scripts PRIVATE ScriptLibrary)
openblack_setup_and_add_test(
  test_land_change_scripts lhvm/test_land_change_scripts.cpp
)
target_link_libraries(test_land_change_scripts PRIVATE ScriptLibrary)
openblack_setup_and_add_test(test_chl_language lhvm/test_chl_language.cpp)
target_link_libraries(test_chl_language PRIVATE lhvmlang)
openblack_setup_and_add_test(test_lhvm_decompiler lhvm/test_lhvm_decompiler.cpp)
target_link_libraries(test_lhvm_decompiler PRIVATE lhvmdecompiler)
openblack_setup_and_add_test(test_magic_spells test_magic_spells.cpp)
openblack_setup_and_add_test(test_flock_miracles test_flock_miracles.cpp)
openblack_setup_and_add_test(test_animal_animation test_animal_animation.cpp)
openblack_setup_and_add_test(test_animated_static test_animated_static.cpp)
openblack_setup_and_add_test(test_apple_barrel test_apple_barrel.cpp)
openblack_setup_and_add_test(test_animal_move test_animal_move.cpp)
openblack_setup_and_add_test(test_bird_rules test_bird_rules.cpp)
openblack_setup_and_add_test(test_teleport_rules test_teleport_rules.cpp)
openblack_setup_and_add_test(test_vortex_rules test_vortex_rules.cpp)
openblack_setup_and_add_test(test_land_followers test_land_followers.cpp)
target_link_libraries(test_land_followers PRIVATE lnd)
openblack_setup_and_add_test(test_fire test_fire.cpp)
openblack_setup_and_add_test(test_blast test_blast.cpp)
openblack_setup_and_add_test(test_gesture_events test_gesture_events.cpp)
openblack_setup_and_add_test(test_magic_foundation test_magic_foundation.cpp)
openblack_setup_and_add_test(
  test_magic_shield_forest test_magic_shield_forest.cpp
)
openblack_setup_and_add_test(test_reactions test_reactions.cpp)
openblack_setup_and_add_test(test_map_production test_map_production.cpp)
openblack_setup_and_add_test(test_magic_heal test_magic_heal.cpp)
openblack_setup_and_add_test(test_lightning_maths test_lightning_maths.cpp)
openblack_setup_and_add_test(test_tornado_creature test_tornado_creature.cpp)
openblack_setup_and_add_test(
  test_creature_spell_mind test_creature_spell_mind.cpp
)
openblack_setup_and_add_test(
  test_creature_spell_casting test_creature_spell_casting.cpp
)
openblack_setup_and_add_test(test_town_belief test_town_belief.cpp)
openblack_setup_and_add_test(test_town_owner test_town_owner.cpp)
openblack_setup_and_add_test(test_villager_speed test_villager_speed.cpp)
openblack_setup_and_add_test(test_wall_hug_rules test_wall_hug_rules.cpp)
openblack_setup_and_add_test(test_walker_placement test_walker_placement.cpp)
openblack_setup_and_add_test(test_villager_age test_villager_age.cpp)
openblack_setup_and_add_test(test_villager_needs test_villager_needs.cpp)
openblack_setup_and_add_test(test_villager_routine test_villager_routine.cpp)
openblack_setup_and_add_test(test_walk_arrival test_walk_arrival.cpp)
openblack_setup_and_add_test(test_abode_knock test_abode_knock.cpp)
openblack_setup_and_add_test(
  test_villager_animation test_villager_animation.cpp
)
openblack_setup_and_add_test(test_creature_casting test_creature_casting.cpp)
openblack_setup_and_add_test(test_villager_script test_villager_script.cpp)
openblack_setup_and_add_test(test_creature_fire test_creature_fire.cpp)
openblack_setup_and_add_test(test_particle_glints test_particle_glints.cpp)
target_link_libraries(test_particle_glints PRIVATE psys)
openblack_setup_and_add_test(test_gesture_trail test_gesture_trail.cpp)
openblack_setup_and_add_test(test_gesture_effects test_gesture_effects.cpp)
target_link_libraries(test_gesture_effects PRIVATE psys)
openblack_setup_and_add_test(
  test_creature_miracle_reactions test_creature_miracle_reactions.cpp
)
openblack_setup_and_add_test(
  test_creature_spell_particles test_creature_spell_particles.cpp
)
target_link_libraries(test_creature_spell_particles PRIVATE psys)
openblack_setup_and_add_test(
  test_particle_sound_release test_particle_sound_release.cpp
)
openblack_setup_and_add_test(test_gesture_file test_gesture_file.cpp)
target_link_libraries(test_gesture_file PRIVATE gestures)
openblack_setup_and_add_test(
  test_gesture_recognition test_gesture_recognition.cpp
)
target_link_libraries(test_gesture_recognition PRIVATE gestures)
openblack_setup_and_add_test(test_gesture_system test_gesture_system.cpp)
target_link_libraries(test_gesture_system PRIVATE gestures)
openblack_setup_and_add_test(test_creature_spells test_creature_spells.cpp)
openblack_setup_and_add_test(
  test_creature_script_play test_creature_script_play.cpp
)
openblack_setup_and_add_test(test_particle_miracles test_particle_miracles.cpp)
target_link_libraries(test_particle_miracles PRIVATE psys)
openblack_setup_and_add_test(test_storm_miracle test_storm_miracle.cpp)
target_link_libraries(test_storm_miracle PRIVATE psys)
openblack_setup_and_add_test(test_tornado test_tornado.cpp)
target_link_libraries(test_tornado PRIVATE psys)
openblack_setup_and_add_test(test_chl_compiler lhvm/test_chl_compiler.cpp)
target_link_libraries(test_chl_compiler PRIVATE lhvmcompiler)
openblack_setup_and_add_test(test_chl_roundtrip lhvm/test_chl_roundtrip.cpp)
target_link_libraries(test_chl_roundtrip PRIVATE lhvmcompiler lhvmdecompiler)
openblack_setup_and_add_test(test_chl_forms lhvm/test_chl_forms.cpp)
target_link_libraries(test_chl_forms PRIVATE lhvmcompiler lhvmdecompiler)
openblack_setup_and_add_test(test_chl_constructs lhvm/test_chl_constructs.cpp)
target_link_libraries(test_chl_constructs PRIVATE lhvmcompiler lhvmdecompiler)
openblack_setup_and_add_test(test_chl_random lhvm/test_chl_random.cpp)
target_link_libraries(test_chl_random PRIVATE lhvmcompiler lhvmdecompiler)
openblack_setup_and_add_test(test_physics_engine test_physics_engine.cpp)
target_link_libraries(test_physics_engine PRIVATE physconst)
openblack_setup_and_add_test(test_physics_rest test_physics_rest.cpp)
openblack_setup_and_add_test(test_dynamics_system test_dynamics_system.cpp)
target_link_libraries(test_dynamics_system PRIVATE psys physconst lnd)
openblack_setup_and_add_test(
  test_physics_draw_matrix test_physics_draw_matrix.cpp
)
openblack_setup_and_add_test(
  test_temple_destruction test_temple_destruction.cpp
)
openblack_setup_and_add_test(
  test_temple_destruction_system test_temple_destruction_system.cpp
)
openblack_setup_and_add_test(test_physics_manager test_physics_manager.cpp)
openblack_setup_and_add_test(test_physics_living test_physics_living.cpp)
openblack_setup_and_add_test(test_physics_objects test_physics_objects.cpp)
openblack_setup_and_add_test(test_tree_roots test_tree_roots.cpp)
openblack_setup_and_add_test(test_damage_mesh test_damage_mesh.cpp)
openblack_setup_and_add_test(
  test_building_construction test_building_construction.cpp
)
openblack_setup_and_add_test(
  test_building_damage_system test_building_damage_system.cpp
)
openblack_setup_and_add_test(test_clip_sound_player test_clip_sound_player.cpp)
target_link_libraries(test_clip_sound_player PRIVATE sas)
openblack_setup_and_add_test(
  test_physics_game_hooks test_physics_game_hooks.cpp
)
openblack_setup_and_add_test(test_town_homes test_town_homes.cpp)
openblack_setup_and_add_test(test_reward test_reward.cpp)
openblack_setup_and_add_test(test_scenic_forest test_scenic_forest.cpp)
openblack_setup_and_add_test(test_nearest_search test_nearest_search.cpp)
openblack_setup_and_add_test(test_land_forests test_land_forests.cpp)
openblack_setup_and_add_test(test_script_objects test_script_objects.cpp)
openblack_setup_and_add_test(test_script_find test_script_find.cpp)
openblack_setup_and_add_test(test_partial_build_cap test_partial_build_cap.cpp)
openblack_setup_and_add_test(test_town_playthings test_town_playthings.cpp)
openblack_setup_and_add_test(test_script_populate test_script_populate.cpp)
openblack_setup_and_add_test(test_worship_sites test_worship_sites.cpp)
target_link_libraries(test_worship_sites PRIVATE dance)
openblack_setup_and_add_test(test_village_totem test_village_totem.cpp)
openblack_setup_and_add_test(test_floating_number test_floating_number.cpp)
openblack_setup_and_add_test(test_temple_heart test_temple_heart.cpp)
openblack_setup_and_add_test(test_model_surface test_model_surface.cpp)
target_link_libraries(test_model_surface PRIVATE l3d)
openblack_setup_and_add_test(test_fireflies test_fireflies.cpp)
openblack_setup_and_add_test(test_forest_growth test_forest_growth.cpp)
openblack_setup_and_add_test(test_store_rules test_store_rules.cpp)
openblack_setup_and_add_test(test_big_forest_rules test_big_forest_rules.cpp)
openblack_setup_and_add_test(test_clip_sounds test_clip_sounds.cpp)
target_link_libraries(test_clip_sounds PRIVATE sas)

# The Bink video component's tests need nothing from the game; those that read the game's videos skip without
# OPENBLACK_GAME_PATH
foreach (BINK_TEST file bitstream blocks decoder yuv robustness)
  add_executable(test_bink_${BINK_TEST} bink/test_bink_${BINK_TEST}.cpp)
  target_link_libraries(test_bink_${BINK_TEST} PRIVATE GTest::gtest_main bink)
  set_property(TARGET test_bink_${BINK_TEST} PROPERTY FOLDER "tests")
  add_test(NAME test_bink_${BINK_TEST} COMMAND test_bink_${BINK_TEST})
endforeach ()

# The full-screen videos' rules and system, over synthetic Bink files
openblack_setup_and_add_test(test_video_rules video/test_video_rules.cpp)
openblack_setup_and_add_test(test_video_system video/test_video_system.cpp)
target_include_directories(
  test_video_system PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/bink
)
# The videos' frames decoded ahead on a thread of their own
openblack_setup_and_add_test(test_frame_queue video/test_frame_queue.cpp)
target_include_directories(
  test_frame_queue PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/bink
)
# The loading screen's and the start-up screens' rules
openblack_setup_and_add_test(
  test_loading_screen_rules gui/test_loading_screen_rules.cpp
)
# Full-screen pictures kept to their own shape on any screen
openblack_setup_and_add_test(test_screen_fit gui/test_screen_fit.cpp)
openblack_setup_and_add_test(test_story_land_change test_story_land_change.cpp)
openblack_setup_and_add_test(test_dance_file test_dance_file.cpp)
target_link_libraries(test_dance_file PRIVATE dance)
target_link_libraries(test_story_land_change PRIVATE ScriptLibrary)
openblack_setup_and_add_test(test_spirit_distance test_spirit_distance.cpp)
target_link_libraries(test_spirit_distance PRIVATE helpdude)

# The inspector component's tests build without the game, as the component does
macro (OPENBLACK_SETUP_AND_ADD_INSPECTOR_TEST TEST_NAME TEST_SOURCE)
  add_executable(${TEST_NAME} ${TEST_SOURCE})
  target_link_libraries(${TEST_NAME} PRIVATE GTest::gtest_main inspector)
  set_property(TARGET ${TEST_NAME} PROPERTY FOLDER "tests")
  add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endmacro ()
openblack_setup_and_add_inspector_test(
  test_inspector_protocol inspector/test_inspector_protocol.cpp
)
openblack_setup_and_add_inspector_test(
  test_inspector_query inspector/test_inspector_query.cpp
)
openblack_setup_and_add_inspector_test(
  test_inspector_server inspector/test_inspector_server.cpp
)
openblack_setup_and_add_inspector_test(
  test_inspector_discovery inspector/test_inspector_discovery.cpp
)
# The inspector's MCP adapter: its choice among several running games, against fake games
find_package(Python3 COMPONENTS Interpreter)
if (Python3_Interpreter_FOUND)
  add_test(
    NAME test_inspector_mcp
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tools/inspector/test_openblack_inspector_mcp.py
  )
  # Every component's fields are registered: the generated file is up to date with src/ECS/Components
  add_test(
    NAME test_inspector_component_fields
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_SOURCE_DIR}/tools/inspector/generate_component_fields.py --check
  )
endif ()
# The tests talking over loopback sockets with threads of their own take turns, so that on a machine busy with a
# parallel ctest they don't also crowd each other
set(OPENBLACK_INSPECTOR_SOCKET_TESTS test_inspector_server)
if (Python3_Interpreter_FOUND)
  list(APPEND OPENBLACK_INSPECTOR_SOCKET_TESTS test_inspector_mcp)
endif ()
set_tests_properties(
  ${OPENBLACK_INSPECTOR_SOCKET_TESTS} PROPERTIES RESOURCE_LOCK
                                                 inspector_sockets
)
# The game's side of the inspector, built only with it
if (OPENBLACK_INSPECTOR)
  openblack_setup_and_add_test(
    test_inspector_game inspector/test_inspector_game.cpp
  )
  target_link_libraries(test_inspector_game PRIVATE inspector psys)
  openblack_setup_and_add_test(
    test_inspector_edit inspector/test_inspector_edit.cpp
  )
  target_link_libraries(test_inspector_edit PRIVATE inspector)
  openblack_setup_and_add_test(
    test_inspector_systems inspector/test_inspector_systems.cpp
  )
  target_link_libraries(test_inspector_systems PRIVATE inspector psys)
  openblack_setup_and_add_test(
    test_inspector_input inspector/test_inspector_input.cpp
  )
  target_link_libraries(test_inspector_input PRIVATE inspector)
  openblack_setup_and_add_test(
    test_inspector_control inspector/test_inspector_control.cpp
  )
  target_link_libraries(test_inspector_control PRIVATE inspector)
  # gui.press on the game's own dialogs, as the start-of-game question
  openblack_setup_and_add_test(
    test_inspector_dialogs inspector/test_inspector_dialogs.cpp
  )
  target_link_libraries(test_inspector_dialogs PRIVATE inspector)
  # The inspector's pointer with a real mouse's timing, through the action map and the window's event queue
  openblack_setup_and_add_test(
    test_scripted_mouse input/test_scripted_mouse.cpp
  )
  target_link_libraries(test_scripted_mouse PRIVATE inspector)
  openblack_setup_and_add_test(
    test_camera_edge_turn_timing camera/test_camera_edge_turn_timing.cpp
  )
  target_link_libraries(test_camera_edge_turn_timing PRIVATE inspector)
  # The coverage test reads the locator's header for its services. ctest also passes the folder when the test runs,
  # since a compiler cache shared between worktrees can give the test an object built in another worktree.
  target_compile_definitions(
    test_inspector_systems PRIVATE OPENBLACK_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
  )
  set_tests_properties(
    test_inspector_systems
    PROPERTIES ENVIRONMENT "OPENBLACK_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
  )
endif ()
openblack_setup_and_add_test(test_help_dude_file test_help_dude_file.cpp)
target_link_libraries(test_help_dude_file PRIVATE helpdude)
openblack_setup_and_add_test(test_spirits test_spirits.cpp)
target_link_libraries(test_spirits PRIVATE helpdude)
openblack_setup_and_add_test(test_spirit_pose test_spirit_pose.cpp)
target_link_libraries(test_spirit_pose PRIVATE helpdude)
openblack_setup_and_add_test(test_spirit_cling test_spirit_cling.cpp)
target_link_libraries(test_spirit_cling PRIVATE helpdude)
openblack_setup_and_add_test(test_spirit_screen test_spirit_screen.cpp)
target_link_libraries(test_spirit_screen PRIVATE helpdude)
openblack_setup_and_add_test(test_text_splitter test_text_splitter.cpp)
openblack_setup_and_add_test(test_help_text_display test_help_text_display.cpp)
openblack_setup_and_add_test(test_dialogue_text test_dialogue_text.cpp)
openblack_setup_and_add_test(test_advisor_voices test_advisor_voices.cpp)
openblack_setup_and_add_test(test_script_spirits test_script_spirits.cpp)
# Reads the shaders' sources; ctest also passes the folder, as a compiler cache shared between worktrees can give the
# test an object built in another worktree
openblack_setup_and_add_test(
  test_instance_data_inputs graphics/test_instance_data_inputs.cpp
)
target_compile_definitions(
  test_instance_data_inputs PRIVATE OPENBLACK_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
set_tests_properties(
  test_instance_data_inputs
  PROPERTIES ENVIRONMENT "OPENBLACK_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
)
openblack_setup_and_add_test(test_help_profile test_help_profile.cpp)
openblack_setup_and_add_test(test_tip_bubble test_tip_bubble.cpp)
openblack_setup_and_add_test(test_click_cue test_click_cue.cpp)
openblack_setup_and_add_test(test_hand_demo test_hand_demo.cpp)
target_link_libraries(test_hand_demo PRIVATE hnd)
openblack_setup_and_add_test(test_villager_clips test_villager_clips.cpp)
target_link_libraries(test_villager_clips PRIVATE anm)
openblack_setup_and_add_test(test_lip_sync test_lip_sync.cpp)
openblack_setup_and_add_test(test_wave_cue_labels test_wave_cue_labels.cpp)
openblack_setup_and_add_test(
  test_creature_script_agendas test_creature_script_agendas.cpp
)
openblack_setup_and_add_test(test_lhvm_exceptions lhvm/test_lhvm_exceptions.cpp)
target_link_libraries(test_lhvm_exceptions PRIVATE ScriptLibrary)
openblack_setup_and_add_test(
  test_lhvm_native_texts lhvm/test_lhvm_native_texts.cpp
)
target_link_libraries(test_lhvm_native_texts PRIVATE ScriptLibrary)
openblack_setup_and_add_test(
  test_screenshot_capture graphics/test_screenshot_capture.cpp
)
openblack_setup_and_add_test(test_script_random test_script_random.cpp)
openblack_setup_and_add_test(
  test_camera_handback camera/test_camera_handback.cpp
)
target_link_libraries(test_camera_handback PRIVATE edt)
openblack_setup_and_add_test(test_script_influence test_script_influence.cpp)
openblack_setup_and_add_test(
  test_script_challenge_snapshots test_script_challenge_snapshots.cpp
)
target_link_libraries(test_script_challenge_snapshots PRIVATE ScriptLibrary)
openblack_setup_and_add_test(test_hand_tricons hand/test_hand_tricons.cpp)
openblack_setup_and_add_test(test_double_click input/test_double_click.cpp)
openblack_setup_and_add_test(test_force_field camera/test_force_field.cpp)
openblack_setup_and_add_test(test_intro_rules test_intro_rules.cpp)
openblack_setup_and_add_test(test_boat_launch_rules test_boat_launch_rules.cpp)
openblack_setup_and_add_test(test_grazer_rules test_grazer_rules.cpp)
openblack_setup_and_add_test(test_temple_help test_temple_help.cpp)
openblack_setup_and_add_test(test_script_create test_script_create.cpp)
openblack_setup_and_add_test(test_creature_training test_creature_training.cpp)
openblack_setup_and_add_test(
  test_building_site_rules test_building_site_rules.cpp
)
openblack_setup_and_add_test(
  test_creature_action_agendas test_creature_action_agendas.cpp
)
openblack_setup_and_add_test(test_villager_draw test_villager_draw.cpp)
openblack_setup_and_add_test(
  test_script_hit_natives test_script_hit_natives.cpp
)
target_link_libraries(test_script_hit_natives PRIVATE ScriptLibrary)
openblack_setup_and_add_test(test_abode_foundation test_abode_foundation.cpp)
openblack_setup_and_add_test(test_window_fit windowing/test_window_fit.cpp)
openblack_setup_and_add_test(test_light_column test_light_column.cpp)
openblack_setup_and_add_test(test_heartbeat test_heartbeat.cpp)
openblack_setup_and_add_test(
  test_town_belief_symbols test_town_belief_symbols.cpp
)
openblack_setup_and_add_test(test_camera_place camera/test_camera_place.cpp)
openblack_setup_and_add_test(test_control_icons test_control_icons.cpp)
openblack_setup_and_add_test(test_disciple_rules test_disciple_rules.cpp)
openblack_setup_and_add_test(
  test_mobile_static_rules test_mobile_static_rules.cpp
)
openblack_setup_and_add_test(
  test_camera_edge_turn camera/test_camera_edge_turn.cpp
)
openblack_setup_and_add_test(test_mouse_motion input/test_mouse_motion.cpp)
openblack_setup_and_add_test(test_pointer_speed input/test_pointer_speed.cpp)
openblack_setup_and_add_test(test_graveyard_rules test_graveyard_rules.cpp)
openblack_setup_and_add_test(test_villager_labels test_villager_labels.cpp)
openblack_setup_and_add_test(test_special_villagers test_special_villagers.cpp)
openblack_setup_and_add_test(
  test_temple_people_rules temple/test_temple_people_rules.cpp
)
openblack_setup_and_add_test(test_artefact_rules test_artefact_rules.cpp)
openblack_setup_and_add_test(test_camera_limits camera/test_camera_limits.cpp)
openblack_setup_and_add_test(test_ground_shadow graphics/test_ground_shadow.cpp)
openblack_setup_and_add_test(
  test_size_follower windowing/test_size_follower.cpp
)
openblack_setup_and_add_test(
  test_camera_edge_area camera/test_camera_edge_area.cpp
)
openblack_setup_and_add_test(
  test_villager_food_rules test_villager_food_rules.cpp
)
openblack_setup_and_add_test(test_volume_glow test_volume_glow.cpp)
target_link_libraries(test_volume_glow PRIVATE psys)
openblack_setup_and_add_test(test_relaxation_rules test_relaxation_rules.cpp)
openblack_setup_and_add_test(test_villager_tools test_villager_tools.cpp)
openblack_setup_and_add_test(test_tree_shadows graphics/test_tree_shadows.cpp)
openblack_setup_and_add_test(test_villager_work test_villager_work.cpp)
openblack_setup_and_add_test(
  test_descriptor_set_tally graphics/test_descriptor_set_tally.cpp
)
openblack_setup_and_add_test(
  test_graphics_backend_names test_graphics_backend_names.cpp
)
openblack_setup_and_add_test(test_villager_breeding test_villager_breeding.cpp)
openblack_setup_and_add_test(test_worshipper_rules test_worshipper_rules.cpp)
openblack_setup_and_add_test(test_villager_death test_villager_death.cpp)
openblack_setup_and_add_test(test_creche_rules test_creche_rules.cpp)
openblack_setup_and_add_test(test_disciple_work test_disciple_work.cpp)
openblack_setup_and_add_test(test_script_weather test_script_weather.cpp)
openblack_setup_and_add_test(test_challenge_records test_challenge_records.cpp)
target_link_libraries(test_challenge_records PRIVATE ScriptLibrary)
openblack_setup_and_add_test(
  test_temple_shadow_layout test_temple_shadow_layout.cpp
)
openblack_setup_and_add_test(
  test_worship_site_construction test_worship_site_construction.cpp
)
openblack_setup_and_add_test(test_tree_bend test_tree_bend.cpp)
openblack_setup_and_add_test(
  test_creature_route_search test_creature_route_search.cpp
)
openblack_setup_and_add_test(
  test_double_click_flight camera/test_double_click_flight.cpp
)
openblack_setup_and_add_test(test_spell_icons test_spell_icons.cpp)
openblack_setup_and_add_test(
  test_script_stones_natives test_script_stones_natives.cpp
)
target_link_libraries(test_script_stones_natives PRIVATE ScriptLibrary helpdude)
openblack_setup_and_add_test(test_shape_shadows graphics/test_shape_shadows.cpp)
openblack_setup_and_add_test(
  test_land_shadow_casters test_land_shadow_casters.cpp
)
openblack_setup_and_add_test(
  test_level_script_match test_level_script_match.cpp
)
openblack_setup_and_add_test(
  test_creature_development test_creature_development.cpp
)
openblack_setup_and_add_test(test_turn_cheer help/test_turn_cheer.cpp)
openblack_setup_and_add_test(
  test_script_help_queries help/test_script_help_queries.cpp
)
openblack_setup_and_add_test(
  test_script_vortex_natives test_script_vortex_natives.cpp
)
target_link_libraries(test_script_vortex_natives PRIVATE ScriptLibrary)
openblack_setup_and_add_test(test_vortex_pull_rules test_vortex_pull_rules.cpp)
openblack_setup_and_add_test(
  test_script_misc_natives test_script_misc_natives.cpp
)
target_link_libraries(test_script_misc_natives PRIVATE ScriptLibrary)
openblack_setup_and_add_test(test_spell_icon_system test_spell_icon_system.cpp)
openblack_setup_and_add_test(test_music_cues pack/test_music_cues.cpp)
target_link_libraries(test_music_cues PRIVATE pack)
openblack_setup_and_add_test(test_storm_clouds test_storm_clouds.cpp)
openblack_setup_and_add_test(
  test_planned_building_rules test_planned_building_rules.cpp
)
openblack_setup_and_add_test(test_creature_touch test_creature_touch.cpp)
openblack_setup_and_add_test(test_gesture_hints test_gesture_hints.cpp)
openblack_setup_and_add_test(test_object_detail graphics/test_object_detail.cpp)
openblack_setup_and_add_test(test_player_profiles test_player_profiles.cpp)
openblack_setup_and_add_test(test_text_edit_rules gui/test_text_edit_rules.cpp)
openblack_setup_and_add_test(test_new_player_box gui/test_new_player_box.cpp)
openblack_setup_and_add_test(test_creature_names test_creature_names.cpp)
openblack_setup_and_add_test(test_creature_help test_creature_help.cpp)
openblack_setup_and_add_test(test_help_messages test_help_messages.cpp)
openblack_setup_and_add_test(test_object_draw_list test_object_draw_list.cpp)
openblack_setup_and_add_test(
  test_building_site_list test_building_site_list.cpp
)
openblack_setup_and_add_test(test_hand_trace hand/test_hand_trace.cpp)
openblack_setup_and_add_test(
  test_script_guide_ogre_natives test_script_guide_ogre_natives.cpp
)
target_link_libraries(
  test_script_guide_ogre_natives PRIVATE ScriptLibrary helpdude
)
openblack_setup_and_add_test(test_shepherd_rules test_shepherd_rules.cpp)
openblack_setup_and_add_test(
  test_creature_object_beliefs test_creature_object_beliefs.cpp
)
openblack_setup_and_add_test(test_script_texts gui/test_script_texts.cpp)
openblack_setup_and_add_test(test_temple_game_over test_temple_game_over.cpp)
openblack_setup_and_add_test(
  test_escape_key_rules gui/test_escape_key_rules.cpp
)
openblack_setup_and_add_test(test_reward_opening test_reward_opening.cpp)
openblack_setup_and_add_test(test_lhvm_save_state lhvm/test_lhvm_save_state.cpp)
target_link_libraries(test_lhvm_save_state PRIVATE ScriptLibrary)
openblack_setup_and_add_test(test_footpath_rules test_footpath_rules.cpp)
openblack_setup_and_add_test(test_statistics_rules test_statistics_rules.cpp)
openblack_setup_and_add_test(
  test_challenge_pictures test_challenge_pictures.cpp
)
openblack_setup_and_add_test(
  test_vortex_crossing_rules test_vortex_crossing_rules.cpp
)
openblack_setup_and_add_test(
  test_vortex_crossing_lines test_vortex_crossing_lines.cpp
)
openblack_setup_and_add_test(test_creature_swap test_creature_swap.cpp)
openblack_setup_and_add_test(test_leash_snap test_leash_snap.cpp)
openblack_setup_and_add_test(test_creature_giving test_creature_giving.cpp)
openblack_setup_and_add_test(
  test_temple_villagers_survey temple/test_temple_villagers_survey.cpp
)
openblack_setup_and_add_test(test_hand_pointer input/test_hand_pointer.cpp)
openblack_setup_and_add_test(
  test_shared_index_lists test_shared_index_lists.cpp
)
openblack_setup_and_add_test(test_cinema_hides gui/test_cinema_hides.cpp)
openblack_setup_and_add_test(
  test_creature_cut_scene_look test_creature_cut_scene_look.cpp
)
openblack_setup_and_add_test(test_guidance_rules test_guidance_rules.cpp)
openblack_setup_and_add_test(test_help_query test_help_query.cpp)
openblack_setup_and_add_test(test_land_change test_land_change.cpp)
openblack_setup_and_add_test(
  test_villager_walk_rules test_villager_walk_rules.cpp
)
openblack_setup_and_add_test(test_villager_hiding test_villager_hiding.cpp)
openblack_setup_and_add_test(test_undrawn_picks test_undrawn_picks.cpp)
openblack_setup_and_add_test(
  test_creature_attitude_rules test_creature_attitude_rules.cpp
)
openblack_setup_and_add_test(
  test_reaction_timing_rules test_reaction_timing_rules.cpp
)
openblack_setup_and_add_test(
  test_creature_reaction_rules test_creature_reaction_rules.cpp
)
openblack_setup_and_add_test(
  test_creature_kind_actions test_creature_kind_actions.cpp
)
openblack_setup_and_add_test(test_creature_fart test_creature_fart.cpp)
openblack_setup_and_add_test(
  test_creature_footstep_puffs test_creature_footstep_puffs.cpp
)
openblack_setup_and_add_test(test_pick_shapes test_pick_shapes.cpp)
openblack_setup_and_add_test(test_fight_watch_rules test_fight_watch_rules.cpp)
openblack_setup_and_add_test(test_predator_rules test_predator_rules.cpp)
openblack_setup_and_add_test(test_hand_blend hand/test_hand_blend.cpp)
openblack_setup_and_add_test(test_pipeline_cache test_pipeline_cache.cpp)
openblack_setup_and_add_test(test_being_eaten_rules test_being_eaten_rules.cpp)
openblack_setup_and_add_test(test_menu_pause gui/test_menu_pause.cpp)
openblack_setup_and_add_test(
  test_creature_look_while_going test_creature_look_while_going.cpp
)
openblack_setup_and_add_test(test_snow_levels test_snow_levels.cpp)
openblack_setup_and_add_test(test_camera_rays camera/test_camera_rays.cpp)
openblack_setup_and_add_test(
  test_instance_light_point graphics/test_instance_light_point.cpp
)
openblack_setup_and_add_test(test_warm_up_views graphics/test_warm_up_views.cpp)
openblack_setup_and_add_test(test_land_altitude test_land_altitude.cpp)
target_link_libraries(test_land_altitude PRIVATE lnd)
openblack_setup_and_add_test(test_creature_dance test_creature_dance.cpp)
target_link_libraries(test_creature_dance PRIVATE dance)
openblack_setup_and_add_test(test_creature_activity test_creature_activity.cpp)
openblack_setup_and_add_test(test_hand_stillness hand/test_hand_stillness.cpp)
openblack_setup_and_add_test(test_village_voices test_village_voices.cpp)
openblack_setup_and_add_test(
  test_creature_deed_alignment test_creature_deed_alignment.cpp
)
openblack_setup_and_add_test(test_light_shine graphics/test_light_shine.cpp)
openblack_setup_and_add_test(test_animal_reactions test_animal_reactions.cpp)
openblack_setup_and_add_test(test_world_sounds test_world_sounds.cpp)
openblack_setup_and_add_test(test_hand_tooltips gui/test_hand_tooltips.cpp)
openblack_setup_and_add_test(test_world_tooltips test_world_tooltips.cpp)
openblack_setup_and_add_test(test_fight_transcript test_fight_transcript.cpp)
openblack_setup_and_add_test(
  test_fight_autopilot_rules test_fight_autopilot_rules.cpp
)
openblack_setup_and_add_test(test_learning_marker test_learning_marker.cpp)
openblack_setup_and_add_test(test_town_miracle_tool test_town_miracle_tool.cpp)
openblack_setup_and_add_test(
  test_creature_desire_sources test_creature_desire_sources.cpp
)
openblack_setup_and_add_test(
  test_creature_head_look test_creature_head_look.cpp
)
openblack_setup_and_add_test(
  test_creature_emote_agendas test_creature_emote_agendas.cpp
)
openblack_setup_and_add_test(
  test_creature_player_attention test_creature_player_attention.cpp
)
openblack_setup_and_add_test(
  test_creature_action_history test_creature_action_history.cpp
)
openblack_setup_and_add_test(
  test_creature_eat_from_stores test_creature_eat_from_stores.cpp
)
openblack_setup_and_add_test(
  test_creature_lost_thing test_creature_lost_thing.cpp
)
openblack_setup_and_add_test(
  test_creature_follow_up test_creature_follow_up.cpp
)
openblack_setup_and_add_test(
  test_creature_rest_and_home test_creature_rest_and_home.cpp
)
openblack_setup_and_add_test(test_creature_stomp test_creature_stomp.cpp)
openblack_setup_and_add_test(test_posed_draws graphics/test_posed_draws.cpp)
openblack_setup_and_add_test(
  test_creature_alignment_motes test_creature_alignment_motes.cpp
)
openblack_setup_and_add_test(
  test_creature_town_help test_creature_town_help.cpp
)
openblack_setup_and_add_test(test_creature_deeds test_creature_deeds.cpp)
openblack_setup_and_add_test(
  test_creature_exploration test_creature_exploration.cpp
)
openblack_setup_and_add_test(
  test_creature_odd_agendas test_creature_odd_agendas.cpp
)
openblack_setup_and_add_test(test_statistics_box test_statistics_box.cpp)
openblack_setup_and_add_test(
  test_creature_stats_records test_creature_stats_records.cpp
)
target_link_libraries(test_creature_stats_records PRIVATE helpstats)
openblack_setup_and_add_test(
  test_abode_neglect_rules test_abode_neglect_rules.cpp
)
openblack_setup_and_add_test(test_story_history test_story_history.cpp)
openblack_setup_and_add_test(
  test_creature_turn_order test_creature_turn_order.cpp
)
openblack_setup_and_add_test(test_creature_drink test_creature_drink.cpp)
openblack_setup_and_add_test(
  test_creature_proportions test_creature_proportions.cpp
)
openblack_setup_and_add_test(
  test_creature_bring_home test_creature_bring_home.cpp
)
openblack_setup_and_add_test(
  test_creature_sacrifice test_creature_sacrifice.cpp
)
openblack_setup_and_add_test(
  test_creature_emergencies test_creature_emergencies.cpp
)
openblack_setup_and_add_test(
  test_creature_copy_actions test_creature_copy_actions.cpp
)
openblack_setup_and_add_test(test_altar_sacrifice test_altar_sacrifice.cpp)
openblack_setup_and_add_test(
  test_creature_desire_seen_to test_creature_desire_seen_to.cpp
)
openblack_setup_and_add_test(
  test_creature_clear_way test_creature_clear_way.cpp
)
openblack_setup_and_add_test(
  test_script_creature_natives test_script_creature_natives.cpp
)
openblack_setup_and_add_test(
  test_creature_mane_material test_creature_mane_material.cpp
)
openblack_setup_and_add_test(test_profile_folder test_profile_folder.cpp)
target_link_libraries(test_profile_folder PRIVATE helpstats)
openblack_setup_and_add_test(
  test_fizz_draw_order graphics/test_fizz_draw_order.cpp
)
openblack_setup_and_add_test(
  test_creature_temple_dance test_creature_temple_dance.cpp
)
openblack_setup_and_add_test(
  test_creature_hold_pose test_creature_hold_pose.cpp
)
openblack_setup_and_add_test(test_creature_gesture test_creature_gesture.cpp)
openblack_setup_and_add_test(test_music_mood test_music_mood.cpp)
target_link_libraries(test_music_mood PRIVATE creaturemind)
openblack_setup_and_add_test(
  test_custom_music_playlist test_custom_music_playlist.cpp
)
openblack_setup_and_add_test(
  test_creature_music_reaction test_creature_music_reaction.cpp
)
openblack_setup_and_add_test(
  test_temple_entrance_tap temple/test_temple_entrance_tap.cpp
)
openblack_setup_and_add_test(
  test_custom_music_controls gui/test_custom_music_controls.cpp
)
openblack_setup_and_add_test(test_bitmap test_bitmap.cpp)
target_link_libraries(test_bitmap PRIVATE rawimage)
openblack_setup_and_add_test(test_voice_pool audio/test_voice_pool.cpp)
target_link_libraries(test_voice_pool PRIVATE pack)
openblack_setup_and_add_test(test_creature_steal test_creature_steal.cpp)
openblack_setup_and_add_test(test_tattoo_menu test_tattoo_menu.cpp)
target_link_libraries(test_tattoo_menu PRIVATE creaturemind)
openblack_setup_and_add_test(
  test_creature_town_theft test_creature_town_theft.cpp
)
openblack_setup_and_add_test(
  test_creature_friendship test_creature_friendship.cpp
)
openblack_setup_and_add_test(
  test_creature_temple_parts test_creature_temple_parts.cpp
)
openblack_setup_and_add_test(test_creature_owner test_creature_owner.cpp)
openblack_setup_and_add_test(
  test_creature_action_coverage test_creature_action_coverage.cpp
)
openblack_setup_and_add_test(
  test_script_task_release test_script_task_release.cpp
)
target_link_libraries(test_script_task_release PRIVATE ScriptLibrary)
openblack_setup_and_add_test(
  test_creature_joint_actions test_creature_joint_actions.cpp
)
openblack_setup_and_add_test(test_sim_run test_sim_run.cpp)
openblack_setup_and_add_test(test_sim_checks test_sim_checks.cpp)
openblack_setup_and_add_test(
  test_clocked_audio_player audio/test_clocked_audio_player.cpp
)
add_test(NAME test_sim_report
         COMMAND ${Python3_EXECUTABLE}
                 ${CMAKE_SOURCE_DIR}/tools/sim/test_sim_report.py
)
openblack_setup_and_add_test(test_creature_kiss test_creature_kiss.cpp)
add_test(NAME test_sim_run_training
         COMMAND ${Python3_EXECUTABLE}
                 ${CMAKE_SOURCE_DIR}/tools/sim/test_sim_run.py
)
openblack_setup_and_add_test(
  test_abode_surroundings test_abode_surroundings.cpp
)
target_link_libraries(test_abode_surroundings PRIVATE l3d)
openblack_setup_and_add_test(
  test_l3d_effect_points l3d/test_l3d_effect_points.cpp
)
target_link_libraries(test_l3d_effect_points PRIVATE l3d)
openblack_setup_and_add_test(test_effect_receivers test_effect_receivers.cpp)
openblack_setup_and_add_test(
  test_computer_player_start test_computer_player_start.cpp
)
openblack_setup_and_add_test(
  test_creature_file_load test_creature_file_load.cpp
)
openblack_setup_and_add_test(test_draw_window graphics/test_draw_window.cpp)
openblack_setup_and_add_test(test_creature_web_page test_creature_web_page.cpp)
target_link_libraries(test_creature_web_page PRIVATE creaturehtml lhp)
openblack_setup_and_add_test(test_oracle_catch oracle/test_oracle_catch.cpp)
openblack_setup_and_add_test(test_oracle_sigmoid oracle/test_oracle_sigmoid.cpp)
openblack_setup_and_add_test(
  test_oracle_inverse_square_root oracle/test_oracle_inverse_square_root.cpp
)
openblack_setup_and_add_test(
  test_oracle_data_folder oracle/test_oracle_data_folder.cpp
)
openblack_setup_and_add_test(
  test_oracle_creature_camera_maths
  oracle/test_oracle_creature_camera_maths.cpp
)
openblack_setup_and_add_test(
  test_oracle_float_sweep_3 oracle/test_oracle_float_sweep_3.cpp
)
target_link_libraries(test_oracle_float_sweep_3 PRIVATE pack lnd)
openblack_setup_and_add_test(
  test_oracle_float_sweep_4 oracle/test_oracle_float_sweep_4.cpp
)
openblack_setup_and_add_test(
  test_oracle_formation_land_line oracle/test_oracle_formation_land_line.cpp
)
openblack_setup_and_add_test(
  test_oracle_totem_hand oracle/test_oracle_totem_hand.cpp
)
openblack_setup_and_add_test(
  test_oracle_creature_may_try oracle/test_oracle_creature_may_try.cpp
)
openblack_setup_and_add_test(
  test_oracle_wisp_orbit oracle/test_oracle_wisp_orbit.cpp
)
openblack_setup_and_add_test(
  test_oracle_creature_start_side oracle/test_oracle_creature_start_side.cpp
)
openblack_setup_and_add_test(
  test_oracle_glm_order_3 oracle/test_oracle_glm_order_3.cpp
)
openblack_setup_and_add_test(
  test_oracle_glm_order_4 oracle/test_oracle_glm_order_4.cpp
)
openblack_setup_and_add_test(
  test_oracle_glm_order_5 oracle/test_oracle_glm_order_5.cpp
)
openblack_setup_and_add_test(
  test_oracle_glm_order_6 oracle/test_oracle_glm_order_6.cpp
)
target_link_libraries(test_oracle_glm_order_6 PRIVATE psys)
openblack_setup_and_add_test(
  test_oracle_glm_order_7 oracle/test_oracle_glm_order_7.cpp
)
openblack_setup_and_add_test(
  test_oracle_held_frame_heading oracle/test_oracle_held_frame_heading.cpp
)
openblack_setup_and_add_test(
  test_oracle_lightning_spell_order oracle/test_oracle_lightning_spell_order.cpp
)
openblack_setup_and_add_test(
  test_oracle_sway_ribbon oracle/test_oracle_sway_ribbon.cpp
)
openblack_setup_and_add_test(
  test_oracle_casting_distance oracle/test_oracle_casting_distance.cpp
)
openblack_setup_and_add_test(
  test_oracle_creature_heading oracle/test_oracle_creature_heading.cpp
)
openblack_setup_and_add_test(
  test_oracle_logo_effect oracle/test_oracle_logo_effect.cpp
)
openblack_setup_and_add_test(test_oracle_walk_lead oracle/test_oracle_walk_lead.cpp)
openblack_setup_and_add_test(
  test_oracle_villager_food oracle/test_oracle_villager_food.cpp
)
# The data folder is found at run time (see oracle/OracleData.h), never compiled into an object: ctest sets the
# environment variable for each test, and the file in the build folder serves a program started by hand from there
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/oracle_data_folder.txt
     "${CMAKE_CURRENT_SOURCE_DIR}/oracle/data
"
)
foreach (oracle_test test_oracle_catch test_oracle_sigmoid
                     test_oracle_inverse_square_root test_oracle_data_folder
                     test_oracle_creature_camera_maths
                     test_oracle_creature_may_try
                     test_oracle_totem_hand test_oracle_formation_land_line
                     test_oracle_float_sweep_3 test_oracle_float_sweep_4
                     test_oracle_wisp_orbit test_oracle_creature_start_side
                     test_oracle_glm_order_3
                     test_oracle_glm_order_4
                     test_oracle_glm_order_5
                     test_oracle_glm_order_6
                     test_oracle_glm_order_7
                     test_oracle_held_frame_heading
                     test_oracle_lightning_spell_order
                     test_oracle_sway_ribbon
                     test_oracle_casting_distance
                     test_oracle_creature_heading
                     test_oracle_logo_effect
                     test_oracle_walk_lead
                     test_oracle_villager_food
)
  set_tests_properties(
    ${oracle_test}
    PROPERTIES ENVIRONMENT
               "OPENBLACK_ORACLE_DATA=${CMAKE_CURRENT_SOURCE_DIR}/oracle/data"
  )
endforeach ()
openblack_setup_and_add_test(test_x87 test_x87.cpp)
openblack_setup_and_add_test(test_float_sweep_4 test_float_sweep_4.cpp)
openblack_setup_and_add_test(test_glm_order test_glm_order.cpp)
openblack_setup_and_add_test(test_town_turn_order test_town_turn_order.cpp)
openblack_setup_and_add_test(test_creature_missing_clip test_creature_missing_clip.cpp)
openblack_setup_and_add_test(test_creature_action_mirror test_creature_action_mirror.cpp)
openblack_setup_and_add_test(
  test_save_creature_record test_save_creature_record.cpp
)
openblack_setup_and_add_test(
  test_creature_save_state test_creature_save_state.cpp
)
openblack_setup_and_add_test(test_glm_order_2 test_glm_order_2.cpp)
openblack_setup_and_add_test(test_glm_order_3 test_glm_order_3.cpp)
openblack_setup_and_add_test(test_lhp_file test_lhp_file.cpp)
target_link_libraries(test_lhp_file PRIVATE lhp)
openblack_setup_and_add_test(test_glm_order_4 test_glm_order_4.cpp)
openblack_setup_and_add_test(test_glm_order_5 test_glm_order_5.cpp)
openblack_setup_and_add_test(test_film_creature video/test_film_creature.cpp)
target_link_libraries(test_film_creature PRIVATE cam)
openblack_setup_and_add_test(test_creature_hand_glow test_creature_hand_glow.cpp)
openblack_setup_and_add_test(test_film_effects video/test_film_effects.cpp)
openblack_setup_and_add_test(test_glm_order_6 test_glm_order_6.cpp)
openblack_setup_and_add_test(
  test_creature_mind_file_users test_creature_mind_file_users.cpp
)
target_compile_definitions(
  test_creature_mind_file_users
  PRIVATE OPENBLACK_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
set_tests_properties(
  test_creature_mind_file_users
  PROPERTIES ENVIRONMENT "OPENBLACK_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
)
openblack_setup_and_add_test(test_thing_map_place test_thing_map_place.cpp)
openblack_setup_and_add_test(
  test_creature_reflection graphics/test_creature_reflection.cpp
)
openblack_setup_and_add_test(test_glm_order_7 test_glm_order_7.cpp)
openblack_setup_and_add_test(test_emitter_follow test_emitter_follow.cpp)
openblack_setup_and_add_test(
  test_hand_reflection graphics/test_hand_reflection.cpp
)
openblack_setup_and_add_test(test_axis_locks test_axis_locks.cpp)
openblack_setup_and_add_test(
  test_held_frame_heading test_held_frame_heading.cpp
)
openblack_setup_and_add_test(test_lightning_spell_order test_lightning_spell_order.cpp)
target_link_libraries(test_oracle_sway_ribbon PRIVATE psys)
openblack_setup_and_add_test(test_header_overrides test_header_overrides.cpp)
target_link_libraries(test_header_overrides PRIVATE pack)
openblack_setup_and_add_test(test_game_heading test_game_heading.cpp)
openblack_setup_and_add_test(test_hand_throw_speed test_hand_throw_speed.cpp)
openblack_setup_and_add_test(
  test_object_tint graphics/test_object_tint.cpp
)
openblack_setup_and_add_test(test_creature_files_order test_creature_files_order.cpp)
openblack_setup_and_add_test(
  test_oracle_creature_files_order oracle/test_oracle_creature_files_order.cpp
)
set_tests_properties(
  test_oracle_creature_files_order
  PROPERTIES ENVIRONMENT
             "OPENBLACK_ORACLE_DATA=${CMAKE_CURRENT_SOURCE_DIR}/oracle/data"
)
openblack_setup_and_add_test(test_creature_files_order_3 test_creature_files_order_3.cpp)
openblack_setup_and_add_test(
  test_oracle_creature_files_order_3 oracle/test_oracle_creature_files_order_3.cpp
)
set_tests_properties(
  test_oracle_creature_files_order_3
  PROPERTIES ENVIRONMENT
             "OPENBLACK_ORACLE_DATA=${CMAKE_CURRENT_SOURCE_DIR}/oracle/data"
)
openblack_setup_and_add_test(test_creature_files_order_4 test_creature_files_order_4.cpp)
openblack_setup_and_add_test(
  test_oracle_creature_files_order_4 oracle/test_oracle_creature_files_order_4.cpp
)
set_tests_properties(
  test_oracle_creature_files_order_4
  PROPERTIES ENVIRONMENT
             "OPENBLACK_ORACLE_DATA=${CMAKE_CURRENT_SOURCE_DIR}/oracle/data"
)
openblack_setup_and_add_test(test_creature_walk_planner test_creature_walk_planner.cpp)
openblack_setup_and_add_test(
  test_oracle_creature_walk_planner oracle/test_oracle_creature_walk_planner.cpp
)
set_tests_properties(
  test_oracle_creature_walk_planner
  PROPERTIES ENVIRONMENT
             "OPENBLACK_ORACLE_DATA=${CMAKE_CURRENT_SOURCE_DIR}/oracle/data"
)
openblack_setup_and_add_test(
  test_oracle_creature_walk_planner_2 oracle/test_oracle_creature_walk_planner_2.cpp
)
set_tests_properties(
  test_oracle_creature_walk_planner_2
  PROPERTIES ENVIRONMENT
             "OPENBLACK_ORACLE_DATA=${CMAKE_CURRENT_SOURCE_DIR}/oracle/data"
)
openblack_setup_and_add_test(test_creature_turn_pose test_creature_turn_pose.cpp)
openblack_setup_and_add_test(
  test_oracle_turn_pose oracle/test_oracle_turn_pose.cpp
)
set_tests_properties(
  test_oracle_turn_pose
  PROPERTIES ENVIRONMENT
             "OPENBLACK_ORACLE_DATA=${CMAKE_CURRENT_SOURCE_DIR}/oracle/data"
)
openblack_setup_and_add_test(
  test_oracle_hand_feel_rest_2 oracle/test_oracle_hand_feel_rest_2.cpp
)
openblack_setup_and_add_test(
  test_hand_feel_rest_2 hand/test_hand_feel_rest_2.cpp
)
openblack_setup_and_add_test(
  test_oracle_liquid_particles oracle/test_oracle_liquid_particles.cpp
)
set_tests_properties(
  test_oracle_liquid_particles
  PROPERTIES ENVIRONMENT
             "OPENBLACK_ORACLE_DATA=${CMAKE_CURRENT_SOURCE_DIR}/oracle/data"
)
openblack_setup_and_add_test(test_liquid_particles test_liquid_particles.cpp)
openblack_setup_and_add_test(test_creature_puke test_creature_puke.cpp)
openblack_setup_and_add_test(test_land_point_light test_land_point_light.cpp)
target_link_libraries(test_liquid_particles PRIVATE psys)
openblack_setup_and_add_test(test_oracle_land_shade oracle/test_oracle_land_shade.cpp)
set_tests_properties(
  test_oracle_land_shade
  PROPERTIES ENVIRONMENT
             "OPENBLACK_ORACLE_DATA=${CMAKE_CURRENT_SOURCE_DIR}/oracle/data"
)
openblack_setup_and_add_test(test_land_shade test_land_shade.cpp)
openblack_setup_and_add_test(
  test_oracle_land_shade_particle oracle/test_oracle_land_shade_particle.cpp
)
set_tests_properties(
  test_oracle_land_shade_particle
  PROPERTIES ENVIRONMENT
             "OPENBLACK_ORACLE_DATA=${CMAKE_CURRENT_SOURCE_DIR}/oracle/data"
)
target_link_libraries(test_oracle_land_shade_particle PRIVATE psys)
openblack_setup_and_add_test(test_sing_along help/test_sing_along.cpp)
openblack_setup_and_add_test(
  test_oracle_creature_walk_planner_3 oracle/test_oracle_creature_walk_planner_3.cpp
)
set_tests_properties(
  test_oracle_creature_walk_planner_3
  PROPERTIES ENVIRONMENT
             "OPENBLACK_ORACLE_DATA=${CMAKE_CURRENT_SOURCE_DIR}/oracle/data"
)
openblack_setup_and_add_test(test_save_game_room temple/test_save_game_room.cpp)
openblack_setup_and_add_test(test_instant_saves test_instant_saves.cpp)
openblack_setup_and_add_test(test_alignment_history test_alignment_history.cpp)
openblack_setup_and_add_test(
  test_oracle_creature_walk_planner_4 oracle/test_oracle_creature_walk_planner_4.cpp
)
set_tests_properties(
  test_oracle_creature_walk_planner_4
  PROPERTIES ENVIRONMENT
             "OPENBLACK_ORACLE_DATA=${CMAKE_CURRENT_SOURCE_DIR}/oracle/data"
)
openblack_setup_and_add_test(test_vortex_crossing_file test_vortex_crossing_file.cpp)
openblack_setup_and_add_test(test_world_archive test_world_archive.cpp)
openblack_setup_and_add_test(test_help_without_sound test_help_without_sound.cpp)
target_link_libraries(test_help_without_sound PRIVATE pack lnd)
openblack_setup_and_add_test(test_footpath_planning test_footpath_planning.cpp)
openblack_setup_and_add_test(
  test_oracle_footpath_planning oracle/test_oracle_footpath_planning.cpp
)
set_tests_properties(
  test_oracle_footpath_planning
  PROPERTIES ENVIRONMENT
             "OPENBLACK_ORACLE_DATA=${CMAKE_CURRENT_SOURCE_DIR}/oracle/data"
)
openblack_setup_and_add_test(test_saved_system_state test_saved_system_state.cpp)
openblack_setup_and_add_test(test_save_file_writer test_save_file_writer.cpp)
target_link_libraries(test_saved_system_state PRIVATE edt)
openblack_setup_and_add_test(test_oracle_game_random oracle/test_oracle_game_random.cpp)
set_tests_properties(
  test_oracle_game_random
  PROPERTIES ENVIRONMENT
             "OPENBLACK_ORACLE_DATA=${CMAKE_CURRENT_SOURCE_DIR}/oracle/data"
)
openblack_setup_and_add_test(test_creature_load_after_draws test_creature_load_after_draws.cpp)
openblack_setup_and_add_test(
  test_footpath_obstacles test_footpath_obstacles.cpp
)
openblack_setup_and_add_test(
  test_oracle_footpath_obstacles oracle/test_oracle_footpath_obstacles.cpp
)
set_tests_properties(
  test_oracle_footpath_obstacles
  PROPERTIES ENVIRONMENT
             "OPENBLACK_ORACLE_DATA=${CMAKE_CURRENT_SOURCE_DIR}/oracle/data"
)
openblack_setup_and_add_test(
  test_oracle_creature_area_walk oracle/test_oracle_creature_area_walk.cpp
)
set_tests_properties(
  test_oracle_creature_area_walk
  PROPERTIES ENVIRONMENT
             "OPENBLACK_ORACLE_DATA=${CMAKE_CURRENT_SOURCE_DIR}/oracle/data"
)
openblack_setup_and_add_test(test_world_save_forget test_world_save_forget.cpp)
openblack_setup_and_add_test(test_dead_tree_lean test_dead_tree_lean.cpp)
openblack_setup_and_add_test(test_particle_save test_particle_save.cpp)
target_link_libraries(test_particle_save PRIVATE psys)
target_link_libraries(test_saved_system_state PRIVATE psys)
openblack_setup_and_add_test(test_saving_box test_saving_box.cpp)
openblack_setup_and_add_test(test_placed_turn test_placed_turn.cpp)
openblack_setup_and_add_test(
  test_oracle_placed_turn oracle/test_oracle_placed_turn.cpp
)
set_tests_properties(
  test_oracle_placed_turn
  PROPERTIES ENVIRONMENT
             "OPENBLACK_ORACLE_DATA=${CMAKE_CURRENT_SOURCE_DIR}/oracle/data"
)
openblack_setup_and_add_test(test_fixed_outlines test_fixed_outlines.cpp)
openblack_setup_and_add_test(
  test_oracle_fixed_outlines oracle/test_oracle_fixed_outlines.cpp
)
set_tests_properties(
  test_oracle_fixed_outlines
  PROPERTIES ENVIRONMENT
             "OPENBLACK_ORACLE_DATA=${CMAKE_CURRENT_SOURCE_DIR}/oracle/data"
)
openblack_setup_and_add_test(test_saved_sounds test_saved_sounds.cpp)
target_link_libraries(test_saved_sounds PRIVATE pack lnd)
openblack_setup_and_add_test(test_safe_save_files test_safe_save_files.cpp)
openblack_setup_and_add_test(test_save_problem_box gui/test_save_problem_box.cpp)
openblack_setup_and_add_test(test_creature_script_control test_creature_script_control.cpp)
openblack_setup_and_add_test(
  test_oracle_town_children_desire oracle/test_oracle_town_children_desire.cpp
)
set_tests_properties(
  test_oracle_town_children_desire
  PROPERTIES ENVIRONMENT
             "OPENBLACK_ORACLE_DATA=${CMAKE_CURRENT_SOURCE_DIR}/oracle/data"
)
openblack_setup_and_add_test(test_game_session test_game_session.cpp)
openblack_setup_and_add_test(test_logo_effect gui/test_logo_effect.cpp)
openblack_setup_and_add_test(test_creature_lead test_creature_lead.cpp)
openblack_setup_and_add_test(test_l3d_skin_texels test_l3d_skin_texels.cpp)
openblack_setup_and_add_test(test_town_food_pace test_town_food_pace.cpp)
openblack_setup_and_add_test(test_editor_creature_move test_editor_creature_move.cpp)
openblack_setup_and_add_test(
  test_oracle_physics_angles oracle/test_oracle_physics_angles.cpp
)
set_tests_properties(
  test_oracle_physics_angles
  PROPERTIES ENVIRONMENT
             "OPENBLACK_ORACLE_DATA=${CMAKE_CURRENT_SOURCE_DIR}/oracle/data"
)
openblack_setup_and_add_test(test_pause_text gui/test_pause_text.cpp)
openblack_setup_and_add_test(
  test_oracle_mesh_box oracle/test_oracle_mesh_box.cpp
)
target_link_libraries(test_oracle_mesh_box PRIVATE l3d)
set_tests_properties(
  test_oracle_mesh_box
  PROPERTIES ENVIRONMENT
             "OPENBLACK_ORACLE_DATA=${CMAKE_CURRENT_SOURCE_DIR}/oracle/data"
)
openblack_setup_and_add_test(
  test_oracle_creature_teleport_from_afar
  oracle/test_oracle_creature_teleport_from_afar.cpp
)
set_tests_properties(
  test_oracle_creature_teleport_from_afar
  PROPERTIES ENVIRONMENT
             "OPENBLACK_ORACLE_DATA=${CMAKE_CURRENT_SOURCE_DIR}/oracle/data"
)

openblack_setup_and_add_test(test_creature_agenda_area test_creature_agenda_area.cpp)
openblack_setup_and_add_test(test_save_slot_writes test_save_slot_writes.cpp)
openblack_setup_and_add_test(test_creature_teleport test_creature_teleport.cpp)
openblack_setup_and_add_test(test_town_empty test_town_empty.cpp)
openblack_setup_and_add_test(test_temporary_store test_temporary_store.cpp)
openblack_setup_and_add_test(
  test_oracle_town_desire_voices oracle/test_oracle_town_desire_voices.cpp
)
set_tests_properties(
  test_oracle_town_desire_voices
  PROPERTIES ENVIRONMENT
             "OPENBLACK_ORACLE_DATA=${CMAKE_CURRENT_SOURCE_DIR}/oracle/data"
)
openblack_setup_and_add_test(test_town_area test_town_area.cpp)
openblack_setup_and_add_test(
  test_oracle_town_area oracle/test_oracle_town_area.cpp
)
set_tests_properties(
  test_oracle_town_area
  PROPERTIES ENVIRONMENT
             "OPENBLACK_ORACLE_DATA=${CMAKE_CURRENT_SOURCE_DIR}/oracle/data"
)
openblack_setup_and_add_test(
  test_oracle_town_desire_alignment
  oracle/test_oracle_town_desire_alignment.cpp
)
set_tests_properties(
  test_oracle_town_desire_alignment
  PROPERTIES ENVIRONMENT
             "OPENBLACK_ORACLE_DATA=${CMAKE_CURRENT_SOURCE_DIR}/oracle/data"
)
openblack_setup_and_add_test(test_key_rebinding input/test_key_rebinding.cpp)
openblack_setup_and_add_test(test_dialog_keys gui/test_dialog_keys.cpp)
openblack_setup_and_add_test(test_push_scroll camera/test_push_scroll.cpp)
openblack_setup_and_add_test(
  test_oracle_push_scroll oracle/test_oracle_push_scroll.cpp
)
set_tests_properties(
  test_oracle_push_scroll
  PROPERTIES ENVIRONMENT "OPENBLACK_ORACLE_DATA=${CMAKE_CURRENT_SOURCE_DIR}/oracle/data"
)
openblack_setup_and_add_test(
  test_camera_bookmarks camera/test_camera_bookmarks.cpp
)
openblack_setup_and_add_test(
  test_oracle_marker_pulse oracle/test_oracle_marker_pulse.cpp
)
set_tests_properties(
  test_oracle_marker_pulse
  PROPERTIES ENVIRONMENT
             "OPENBLACK_ORACLE_DATA=${CMAKE_CURRENT_SOURCE_DIR}/oracle/data"
)
openblack_setup_and_add_test(
  test_hand_cycle_choice hand/test_hand_cycle_choice.cpp
)
openblack_setup_and_add_test(
  test_hand_knock hand/test_hand_knock.cpp
)
openblack_setup_and_add_test(test_zoom_to_places camera/test_zoom_to_places.cpp)
openblack_setup_and_add_test(
  test_oracle_zoom_to_places oracle/test_oracle_zoom_to_places.cpp
)
set_tests_properties(
  test_oracle_zoom_to_places
  PROPERTIES ENVIRONMENT
             "OPENBLACK_ORACLE_DATA=${CMAKE_CURRENT_SOURCE_DIR}/oracle/data"
)
openblack_setup_and_add_test(test_click_distance camera/test_click_distance.cpp)
openblack_setup_and_add_test(
  test_oracle_shadow_texture oracle/test_oracle_shadow_texture.cpp
)
set_tests_properties(
  test_oracle_shadow_texture
  PROPERTIES ENVIRONMENT
             "OPENBLACK_ORACLE_DATA=${CMAKE_CURRENT_SOURCE_DIR}/oracle/data"
)
openblack_setup_and_add_test(
  test_shadow_texture graphics/test_shadow_texture.cpp
)
openblack_setup_and_add_test(
  test_moving_shadow_casters test_moving_shadow_casters.cpp
)
openblack_setup_and_add_test(test_game_turn_order test_game_turn_order.cpp)
openblack_setup_and_add_test(test_world_save_clock test_world_save_clock.cpp)
openblack_setup_and_add_test(test_script_interface_levels test_script_interface_levels.cpp)
openblack_setup_and_add_test(test_camera_keys camera/test_camera_keys.cpp)
openblack_setup_and_add_test(test_clear_view camera/test_clear_view.cpp)
openblack_setup_and_add_test(test_temple_heart_look test_temple_heart_look.cpp)
openblack_setup_and_add_test(test_particle_near_villagers test_particle_near_villagers.cpp)
target_link_libraries(test_particle_near_villagers PRIVATE psys)
openblack_setup_and_add_test(
  test_lhvm_instructions lhvm/test_lhvm_instructions.cpp
)
target_link_libraries(test_lhvm_instructions PRIVATE ScriptLibrary)
openblack_setup_and_add_test(test_abode_door test_abode_door.cpp)
openblack_setup_and_add_test(test_game_frame_order test_game_frame_order.cpp)
openblack_setup_and_add_test(test_camera_keys_frame camera/test_camera_keys_frame.cpp)
openblack_setup_and_add_test(test_clear_view_hand camera/test_clear_view_hand.cpp)
openblack_setup_and_add_test(test_mouse_moves help/test_mouse_moves.cpp)
openblack_setup_and_add_test(test_players_symbols test_players_symbols.cpp)
openblack_setup_and_add_test(test_system_libraries test_system_libraries.cpp)
openblack_setup_and_add_test(test_map_script test_map_script.cpp)
openblack_setup_and_add_test(test_world_text_depth test_world_text_depth.cpp)
openblack_setup_and_add_test(
  test_oracle_world_text_depth oracle/test_oracle_world_text_depth.cpp
)
set_tests_properties(
  test_oracle_world_text_depth
  PROPERTIES ENVIRONMENT
             "OPENBLACK_ORACLE_DATA=${CMAKE_CURRENT_SOURCE_DIR}/oracle/data"
)
openblack_setup_and_add_test(test_temple_exit_glow graphics/test_temple_exit_glow.cpp)
openblack_setup_and_add_test(test_temple_room_meshes temple/test_temple_room_meshes.cpp)
openblack_setup_and_add_test(test_future_room temple/test_future_room.cpp)
openblack_setup_and_add_test(test_temple_controls temple/test_temple_controls.cpp)
openblack_setup_and_add_test(test_script_value_json test_script_value_json.cpp)
target_link_libraries(test_script_value_json PRIVATE ScriptLibrary)
openblack_setup_and_add_test(test_path_case test_path_case.cpp)
openblack_setup_and_add_test(test_colour_picker gui/test_colour_picker.cpp)
openblack_setup_and_add_test(test_launch_switches test_launch_switches.cpp)
openblack_setup_and_add_test(test_fight_camera_flight camera/test_fight_camera_flight.cpp)
openblack_setup_and_add_test(test_camera_path_start camera/test_camera_path_start.cpp)
openblack_setup_and_add_test(test_creature_mode_hand_back camera/test_creature_mode_hand_back.cpp)
openblack_setup_and_add_test(test_magic_tree_watch test_magic_tree_watch.cpp)
openblack_setup_and_add_test(test_swimmer_rules test_swimmer_rules.cpp)
openblack_setup_and_add_test(test_player_towns test_player_towns.cpp)
openblack_setup_and_add_test(test_villager_row test_villager_row.cpp)
openblack_setup_and_add_test(test_text_database gui/test_text_database.cpp)
openblack_setup_and_add_test(test_creature_files_draws test_creature_files_draws.cpp)
openblack_setup_and_add_test(test_living_turn_order test_living_turn_order.cpp)
openblack_setup_and_add_test(test_spell_sparing test_spell_sparing.cpp)
openblack_setup_and_add_test(
  test_oracle_creature_land_tilt oracle/test_oracle_creature_land_tilt.cpp
)
set_tests_properties(
  test_oracle_creature_land_tilt
  PROPERTIES ENVIRONMENT
             "OPENBLACK_ORACLE_DATA=${CMAKE_CURRENT_SOURCE_DIR}/oracle/data"
)
openblack_setup_and_add_test(test_land_footprint_rules test_land_footprint_rules.cpp)
openblack_setup_and_add_test(test_script_real_time test_script_real_time.cpp)
target_link_libraries(test_land_vertex PRIVATE lnd)
openblack_setup_and_add_test(test_list_order test_list_order.cpp)
openblack_setup_and_add_test(test_worker_pool test_worker_pool.cpp)
openblack_setup_and_add_test(test_lhvm_program_copy lhvm/test_lhvm_program_copy.cpp)
target_link_libraries(test_lhvm_program_copy PRIVATE ScriptLibrary)
openblack_setup_and_add_test(test_pack_audio_headers pack/test_pack_audio_headers.cpp)
target_link_libraries(test_pack_audio_headers PRIVATE pack)
openblack_setup_and_add_test(test_villager_walk_states test_villager_walk_states.cpp)
openblack_setup_and_add_test(
  test_oracle_creature_looks oracle/test_oracle_creature_looks.cpp
)
set_tests_properties(
  test_oracle_creature_looks
  PROPERTIES ENVIRONMENT
             "OPENBLACK_ORACLE_DATA=${CMAKE_CURRENT_SOURCE_DIR}/oracle/data"
)
openblack_setup_and_add_test(test_creature_looks test_creature_looks.cpp)
openblack_setup_and_add_test(test_creature_hurry test_creature_hurry.cpp)
openblack_setup_and_add_test(test_buffered_log_sink test_buffered_log_sink.cpp)
openblack_setup_and_add_test(test_plain_image test_plain_image.cpp)
openblack_setup_and_add_test(test_player_commands test_player_commands.cpp)
openblack_setup_and_add_test(test_user_folder_index test_user_folder_index.cpp)
openblack_setup_and_add_test(test_texel_1555_bytes test_texel_1555_bytes.cpp)
openblack_setup_and_add_test(test_platform_keys input/test_platform_keys.cpp)
openblack_setup_and_add_test(test_synced_state test_synced_state.cpp)
openblack_setup_and_add_test(test_lobby_api_json test_lobby_api_json.cpp)
openblack_setup_and_add_test(test_lobby_client test_lobby_client.cpp)
openblack_setup_and_add_test(test_http_policy test_http_policy.cpp)
openblack_setup_and_add_test(test_token_store test_token_store.cpp)
openblack_setup_and_add_test(test_real_weather test_real_weather.cpp)
openblack_setup_and_add_test(test_creature_upload test_creature_upload.cpp)
target_link_libraries(test_creature_upload PRIVATE lhp)
openblack_setup_and_add_test(test_turn_codec online/test_turn_codec.cpp)
openblack_setup_and_add_test(
  test_websocket_push_channel online/test_websocket_push_channel.cpp
)
openblack_setup_and_add_test(test_turn_stream online/test_turn_stream.cpp)
openblack_setup_and_add_test(test_curl_websocket online/test_curl_websocket.cpp)
openblack_setup_and_add_test(test_turn_body test_turn_body.cpp)
openblack_setup_and_add_test(test_dedicated_server test_dedicated_server.cpp)
openblack_setup_and_add_test(
  test_multiplayer_front_end test_multiplayer_front_end.cpp
)
openblack_setup_and_add_test(
  test_display_scaling windowing/test_display_scaling.cpp
)
openblack_setup_and_add_test(
  test_instance_upload graphics/test_instance_upload.cpp
)
openblack_setup_and_add_test(test_hand_reports test_hand_reports.cpp)
openblack_setup_and_add_test(test_magic_pile_height test_magic_pile_height.cpp)
openblack_setup_and_add_test(test_build_version test_build_version.cpp)
openblack_setup_and_add_test(test_build_version_file test_build_version_file.cpp)
openblack_setup_and_add_test(test_gods_ring_map online/test_gods_ring_map.cpp)
target_link_libraries(test_gods_ring_map PRIVATE lnd)
openblack_setup_and_add_test(test_gods_ring_land test_gods_ring_land.cpp)
target_link_libraries(test_gods_ring_land PRIVATE lnd)
openblack_setup_and_add_test(test_online_map_file test_online_map_file.cpp)
target_link_libraries(test_online_map_file PRIVATE onlinemap)
openblack_setup_and_add_test(test_computer_settings test_computer_settings.cpp)
target_link_libraries(test_plain_image PRIVATE psys ScriptLibrary edt)
openblack_setup_and_add_test(test_destruction_ghosts test_destruction_ghosts.cpp)
openblack_setup_and_add_test(test_film_surface graphics/test_film_surface.cpp)
openblack_setup_and_add_test(test_system_pointer test_system_pointer.cpp)
  add_test(
    NAME test_system_pointer_calls
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/build/check_system_pointer.py
            ${CMAKE_SOURCE_DIR}
  )
target_link_libraries(test_challenge_pictures PRIVATE ScriptLibrary)
openblack_setup_and_add_test(
  test_challenge_picture_files test_challenge_picture_files.cpp
)
openblack_setup_and_add_test(test_reflection_order test_reflection_order.cpp)
openblack_setup_and_add_test(test_grip_landscape_effect test_grip_landscape_effect.cpp)
openblack_setup_and_add_test(test_game_socket online/test_game_socket.cpp)
openblack_setup_and_add_test(test_join_ticket online/test_join_ticket.cpp)
openblack_setup_and_add_test(test_websocket_frames online/test_websocket_frames.cpp)
openblack_setup_and_add_test(test_turn_playout online/test_turn_playout.cpp)
openblack_setup_and_add_test(test_lockstep online/test_lockstep.cpp)
openblack_setup_and_add_test(test_network_commands test_network_commands.cpp)
openblack_setup_and_add_test(test_lobby_status online/test_lobby_status.cpp)
openblack_setup_and_add_test(test_checksum_log test_checksum_log.cpp)
openblack_setup_and_add_test(test_portable_math test_portable_math.cpp)
openblack_setup_and_add_test(test_build_sized_save test_build_sized_save.cpp)
openblack_setup_and_add_test(test_registry_fresh_ids test_registry_fresh_ids.cpp)
openblack_setup_and_add_test(test_particle_turn_order test_particle_turn_order.cpp)
