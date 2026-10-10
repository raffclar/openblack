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
openblack_setup_and_add_test(test_view_frustum graphics/test_view_frustum.cpp)
openblack_setup_and_add_test(test_mesh_detail graphics/test_mesh_detail.cpp)
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
target_link_libraries(test_block_texture PRIVATE lnd)
openblack_setup_and_add_test(test_sea_rows test_sea_rows.cpp)
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
  test_creature_shadow graphics/test_creature_shadow.cpp
)
openblack_setup_and_add_test(
  test_creature_locomotion test_creature_locomotion.cpp
)
openblack_setup_and_add_test(test_miracle_surfaces test_miracle_surfaces.cpp)
target_link_libraries(test_miracle_surfaces PRIVATE psys)
openblack_setup_and_add_test(test_miracle_visuals test_miracle_visuals.cpp)
openblack_setup_and_add_test(test_resource_piles test_resource_piles.cpp)
openblack_setup_and_add_test(test_resource_cache test_resource_cache.cpp)
openblack_setup_and_add_test(test_upload_pacer test_upload_pacer.cpp)
openblack_setup_and_add_test(test_map_cells test_map_cells.cpp)
openblack_setup_and_add_test(test_script_cast test_script_cast.cpp)
openblack_setup_and_add_test(test_creature_route test_creature_route.cpp)
openblack_setup_and_add_test(test_creature_audio test_creature_audio.cpp)
openblack_setup_and_add_test(
  test_creature_interaction test_creature_interaction.cpp
)
openblack_setup_and_add_test(
  test_creature_footprints test_creature_footprints.cpp
)
openblack_setup_and_add_test(test_creature_fight test_creature_fight.cpp)
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
target_link_libraries(test_flat_land PRIVATE lnd)
target_link_libraries(test_testbed_scenarios PRIVATE lnd)
openblack_setup_and_add_test(test_town_desire test_town_desire.cpp)
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
openblack_setup_and_add_test(
  test_creature_cave_trophies temple/test_creature_cave_trophies.cpp
)
openblack_setup_and_add_test(
  test_temple_exterior_morph temple/test_temple_exterior_morph.cpp
)
openblack_setup_and_add_test(test_temple_light temple/test_temple_light.cpp)
openblack_setup_and_add_test(test_temple_scroll temple/test_temple_scroll.cpp)
openblack_setup_and_add_test(test_temple_toggles temple/test_temple_toggles.cpp)
openblack_setup_and_add_test(test_temple_map temple/test_temple_map.cpp)
target_link_libraries(test_temple_map PRIVATE lnd)
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
openblack_setup_and_add_test(test_animal_move test_animal_move.cpp)
openblack_setup_and_add_test(test_bird_rules test_bird_rules.cpp)
openblack_setup_and_add_test(test_teleport_rules test_teleport_rules.cpp)
openblack_setup_and_add_test(test_vortex_rules test_vortex_rules.cpp)
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
openblack_setup_and_add_test(
  test_creature_script_agendas test_creature_script_agendas.cpp
)
openblack_setup_and_add_test(test_lhvm_exceptions lhvm/test_lhvm_exceptions.cpp)
target_link_libraries(test_lhvm_exceptions PRIVATE ScriptLibrary)
openblack_setup_and_add_test(
  test_screenshot_capture graphics/test_screenshot_capture.cpp
)
openblack_setup_and_add_test(test_script_random test_script_random.cpp)
openblack_setup_and_add_test(
  test_camera_handback camera/test_camera_handback.cpp
)
target_link_libraries(test_camera_handback PRIVATE edt)
openblack_setup_and_add_test(test_intro_rules test_intro_rules.cpp)
