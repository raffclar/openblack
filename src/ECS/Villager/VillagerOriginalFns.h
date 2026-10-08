/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>

// The original's villager state table, one row a state with an id for its entry, exit and validate functions: each
// column numbers its distinct functions in table order, 0 = no function there, so two rows share a function exactly
// when they share its id (the functions themselves are listed in docs/bw1-notes/villagers.md). openblack warns when a
// row it reaches is not ported (k_TodoEntry). Also the town emergency column (k_TownEmergencyReaction). Generated from
// the original's table initialiser.

namespace openblack::ecs::villager
{
struct OriginalStateFns
{
	uint8_t entry;
	uint8_t exit;
	uint8_t validate;
};

/// The validate function of the reaction states (villager_reactions::ReactionValidate)
inline constexpr uint8_t k_ReactionValidate = 4;
/// The exit function of the building states (ExitBuilding)
inline constexpr uint8_t k_ExitBuilding = 13;

// clang-format off
inline constexpr std::array<OriginalStateFns, 255> k_OriginalStateFns = {{
    {0, 0, 0}, // 0 INVALID_STATE
    {0, 1, 1}, // 1 MOVE_TO_POS
    {0, 1, 2}, // 2 MOVE_TO_OBJECT
    {0, 0, 3}, // 3 MOVE_ON_STRUCTURE
    {1, 2, 0}, // 4 IN_SCRIPT
    {2, 3, 0}, // 5 IN_DANCE
    {0, 4, 4}, // 6 FLEEING_FROM_OBJECT_REACTION
    {0, 4, 4}, // 7 LOOKING_AT_OBJECT_REACTION
    {0, 4, 4}, // 8 FOLLOWING_OBJECT_REACTION
    {0, 4, 4}, // 9 INSPECT_OBJECT_REACTION
    {0, 5, 0}, // 10 FLYING
    {0, 6, 0}, // 11 LANDED
    {0, 4, 4}, // 12 LOOK_AT_FLYING_OBJECT_REACTION
    {0, 0, 0}, // 13 SET_DYING
    {0, 0, 0}, // 14 DYING
    {0, 7, 0}, // 15 DEAD
    {3, 8, 0}, // 16 DROWNING
    {0, 0, 0}, // 17 DOWNED
    {0, 9, 0}, // 18 BEING_EATEN
    {0, 4, 4}, // 19 GOTO_FOOD_REACTION
    {0, 4, 4}, // 20 ARRIVES_AT_FOOD_REACTION
    {0, 4, 4}, // 21 GOTO_WOOD_REACTION
    {0, 4, 4}, // 22 ARRIVES_AT_WOOD_REACTION
    {0, 0, 0}, // 23 WAIT_FOR_ANIMATION
    {4, 10, 0}, // 24 IN_HAND
    {0, 4, 4}, // 25 GOTO_PICKUP_BALL_REACTION
    {0, 4, 4}, // 26 ARRIVES_AT_PICKUP_BALL_REACTION
    {0, 0, 0}, // 27 MOVE_IN_FLOCK
    {1, 2, 0}, // 28 MOVE_ALONG_PATH
    {5, 11, 0}, // 29 MOVE_ON_PATH
    {0, 4, 4}, // 30 FLEEING_AND_LOOKING_AT_OBJECT_REACTION
    {0, 0, 0}, // 31 GOTO_STORAGE_PIT_FOR_DROP_OFF
    {0, 0, 0}, // 32 ARRIVES_AT_STORAGE_PIT_FOR_DROP_OFF
    {0, 0, 0}, // 33 GOTO_STORAGE_PIT_FOR_FOOD
    {0, 0, 0}, // 34 ARRIVES_AT_STORAGE_PIT_FOR_FOOD
    {0, 12, 0}, // 35 ARRIVES_AT_HOME_WITH_FOOD
    {0, 12, 0}, // 36 GO_HOME
    {0, 12, 0}, // 37 ARRIVES_HOME
    {0, 12, 0}, // 38 AT_HOME
    {6, 13, 0}, // 39 ARRIVES_AT_STORAGE_PIT_FOR_BUILDING_MATERIALS
    {6, 13, 0}, // 40 ARRIVES_AT_BUILDING_SITE
    {6, 13, 0}, // 41 BUILDING
    {0, 14, 0}, // 42 GOTO_STORAGE_PIT_FOR_WORSHIP_SUPPLIES
    {0, 14, 0}, // 43 ARRIVES_AT_STORAGE_PIT_FOR_WORSHIP_SUPPLIES
    {0, 14, 0}, // 44 GOTO_WORSHIP_SITE_WITH_SUPPLIES
    {0, 14, 0}, // 45 MOVE_TO_WORSHIP_SITE_WITH_SUPPLIES
    {0, 14, 0}, // 46 ARRIVES_AT_WORSHIP_SITE_WITH_SUPPLIES
    {0, 15, 1}, // 47 FORESTER_MOVE_TO_FOREST
    {0, 15, 0}, // 48 FORESTER_GOTO_FOREST
    {0, 15, 0}, // 49 FORESTER_ARRIVES_AT_FOREST
    {0, 15, 0}, // 50 FORESTER_CHOPS_TREE
    {6, 13, 0}, // 51 FORESTER_CHOPS_TREE_FOR_BUILDING
    {0, 15, 0}, // 52 FORESTER_FINISHED_FORESTERING
    {0, 0, 0}, // 53 ARRIVES_AT_BIG_FOREST
    {6, 13, 0}, // 54 ARRIVES_AT_BIG_FOREST_FOR_BUILDING
    {7, 16, 0}, // 55 FISHERMAN_ARRIVES_AT_FISHING
    {7, 16, 0}, // 56 FISHING
    {0, 0, 0}, // 57 WAIT_FOR_COUNTER
    {0, 17, 0}, // 58 GOTO_WORSHIP_SITE_FOR_WORSHIP
    {0, 17, 0}, // 59 ARRIVES_AT_WORSHIP_SITE_FOR_WORSHIP
    {0, 18, 0}, // 60 WORSHIPPING_AT_WORSHIP_SITE
    {0, 0, 0}, // 61 GOTO_ALTAR_FOR_REST
    {0, 0, 0}, // 62 ARRIVES_AT_ALTAR_FOR_REST
    {0, 0, 0}, // 63 AT_ALTAR_REST
    {0, 0, 0}, // 64 AT_ALTAR_FINISHED_REST
    {0, 19, 0}, // 65 RESTART_WORSHIPPING_AT_WORSHIP_SITE
    {0, 0, 0}, // 66 RESTART_WORSHIPPING_CREATURE
    {8, 20, 0}, // 67 FARMER_ARRIVES_AT_FARM
    {8, 20, 0}, // 68 FARMER_PLANTS_CROP
    {8, 20, 0}, // 69 FARMER_DIGS_UP_CROP
    {0, 0, 0}, // 70 MOVE_TO_FOOTBALL_PITCH_CONSTRUCTION
    {0, 21, 1}, // 71 FOOTBALL_WALK_TO_POSITION
    {0, 21, 0}, // 72 FOOTBALL_WAIT_FOR_KICK_OFF
    {0, 21, 0}, // 73 FOOTBALL_ATTACKER
    {0, 21, 0}, // 74 FOOTBALL_GOALIE
    {0, 21, 0}, // 75 FOOTBALL_DEFENDER
    {0, 21, 0}, // 76 FOOTBALL_WON_GOAL
    {0, 21, 0}, // 77 FOOTBALL_LOST_GOAL
    {0, 21, 0}, // 78 START_MOVE_TO_PICK_UP_BALL_FOR_DEAD_BALL
    {0, 21, 0}, // 79 ARRIVED_AT_PICK_UP_BALL_FOR_DEAD_BALL
    {0, 21, 0}, // 80 ARRIVED_AT_PUT_DOWN_BALL_FOR_DEAD_BALL_START
    {0, 21, 0}, // 81 ARRIVED_AT_PUT_DOWN_BALL_FOR_DEAD_BALL_END
    {0, 21, 0}, // 82 FOOTBALL_MATCH_PAUSED
    {0, 21, 0}, // 83 FOOTBALL_WATCH_MATCH
    {0, 21, 0}, // 84 FOOTBALL_MEXICAN_WAVE
    {0, 0, 0}, // 85 CREATED
    {0, 0, 0}, // 86 ARRIVES_IN_ABODE_TO_TRADE
    {0, 0, 0}, // 87 ARRIVES_IN_ABODE_TO_PICK_UP_EXCESS
    {0, 0, 0}, // 88 MAKE_SCARED_STIFF
    {0, 0, 0}, // 89 SCARED_STIFF
    {0, 22, 0}, // 90 WORSHIPPING_CREATURE
    {0, 23, 0}, // 91 SHEPHERD_LOOK_FOR_FLOCK
    {0, 23, 0}, // 92 SHEPHERD_MOVE_FLOCK_TO_WATER
    {0, 23, 0}, // 93 SHEPHERD_MOVE_FLOCK_TO_FOOD
    {0, 23, 0}, // 94 SHEPHERD_MOVE_FLOCK_BACK
    {0, 23, 0}, // 95 SHEPHERD_DECIDE_WHAT_TO_DO_WITH_FLOCK
    {0, 23, 0}, // 96 SHEPHERD_WAIT_FOR_FLOCK
    {0, 23, 0}, // 97 SHEPHERD_SLAUGHTER_ANIMAL
    {0, 23, 0}, // 98 SHEPHERD_FETCH_STRAY
    {0, 23, 0}, // 99 SHEPHERD_GOTO_FLOCK
    {0, 12, 0}, // 100 HOUSEWIFE_AT_HOME
    {0, 0, 0}, // 101 HOUSEWIFE_GOTO_STORAGE_PIT
    {0, 0, 0}, // 102 HOUSEWIFE_ARRIVES_AT_STORAGE_PIT
    {0, 0, 0}, // 103 HOUSEWIFE_PICKUP_FROM_STORAGE_PIT
    {0, 0, 0}, // 104 HOUSEWIFE_RETURN_HOME_WITH_FOOD
    {0, 0, 0}, // 105 HOUSEWIFE_MAKE_DINNER
    {0, 0, 0}, // 106 HOUSEWIFE_SERVES_DINNER
    {0, 0, 0}, // 107 HOUSEWIFE_CLEARS_AWAY_DINNER
    {0, 0, 0}, // 108 HOUSEWIFE_DOES_HOUSEWORK
    {0, 0, 0}, // 109 HOUSEWIFE_GOSSIPS_AROUND_STORAGE_PIT
    {0, 0, 0}, // 110 HOUSEWIFE_STARTS_GIVING_BIRTH
    {0, 0, 0}, // 111 HOUSEWIFE_GIVING_BIRTH
    {0, 0, 0}, // 112 HOUSEWIFE_GIVEN_BIRTH
    {0, 0, 0}, // 113 CHILD_AT_CRECHE
    {0, 0, 0}, // 114 CHILD_FOLLOWS_MOTHER
    {0, 0, 0}, // 115 CHILD_BECOMES_ADULT
    {0, 0, 0}, // 116 SITS_DOWN_TO_DINNER
    {0, 0, 0}, // 117 EAT_FOOD
    {0, 12, 0}, // 118 EAT_FOOD_AT_HOME
    {0, 12, 0}, // 119 GOTO_BED_AT_HOME
    {0, 12, 0}, // 120 SLEEPING_AT_HOME
    {0, 12, 0}, // 121 WAKE_UP_AT_HOME
    {9, 24, 5}, // 122 START_HAVING_SEX
    {9, 24, 0}, // 123 HAVING_SEX
    {9, 24, 0}, // 124 STOP_HAVING_SEX
    {0, 12, 0}, // 125 START_HAVING_SEX_AT_HOME
    {0, 12, 0}, // 126 HAVING_SEX_AT_HOME
    {0, 12, 0}, // 127 STOP_HAVING_SEX_AT_HOME
    {0, 0, 0}, // 128 WAIT_FOR_DINNER
    {0, 0, 0}, // 129 HOMELESS_START
    {0, 0, 0}, // 130 VAGRANT_START
    {0, 0, 0}, // 131 MORN_DEATH
    {0, 0, 0}, // 132 PERFORM_INSPECTION_REACTION
    {0, 0, 0}, // 133 APPROACH_OBJECT_REACTION
    {0, 0, 0}, // 134 INITIALISE_TELL_OTHERS_ABOUT_OBJECT
    {0, 0, 0}, // 135 TELL_OTHERS_ABOUT_INTERESTING_OBJECT
    {0, 0, 0}, // 136 APPROACH_VILLAGER_TO_TALK_TO
    {0, 0, 0}, // 137 TELL_PARTICULAR_VILLAGER_ABOUT_OBJECT
    {0, 0, 0}, // 138 INITIALISE_LOOK_AROUND_FOR_VILLAGER_TO_TELL
    {0, 0, 0}, // 139 LOOK_AROUND_FOR_VILLAGER_TO_TELL
    {0, 4, 6}, // 140 MOVE_TOWARDS_OBJECT_TO_LOOK_AT
    {0, 4, 4}, // 141 INITIALISE_IMPRESSED_REACTION
    {0, 4, 4}, // 142 PERFORM_IMPRESSED_REACTION
    {0, 4, 4}, // 143 INITIALISE_FIGHT_REACTION
    {0, 4, 4}, // 144 PERFORM_FIGHT_REACTION
    {0, 0, 0}, // 145 HOMELESS_EAT_DINNER
    {0, 4, 4}, // 146 INSPECT_CREATURE_REACTION
    {0, 4, 4}, // 147 PERFORM_INSPECT_CREATURE_REACTION
    {0, 4, 4}, // 148 APPROACH_CREATURE_REACTION
    {0, 4, 4}, // 149 INITIALISE_BEWILDERED_BY_MAGIC_TREE_REACTION
    {0, 4, 4}, // 150 PERFORM_BEWILDERED_BY_MAGIC_TREE_REACTION
    {0, 4, 4}, // 151 TURN_TO_FACE_MAGIC_TREE
    {0, 4, 4}, // 152 LOOK_AT_MAGIC_TREE
    {0, 22, 0}, // 153 DANCE_FOR_EDITING_PURPOSES
    {0, 0, 1}, // 154 MOVE_TO_DANCE_POS
    {0, 4, 4}, // 155 INITIALISE_RESPECT_CREATURE_REACTION
    {0, 4, 4}, // 156 PERFORM_RESPECT_CREATURE_REACTION
    {0, 4, 4}, // 157 FINISH_RESPECT_CREATURE_REACTION
    {0, 4, 4}, // 158 APPROACH_HAND_REACTION
    {0, 4, 4}, // 159 FLEEING_FROM_CREATURE_REACTION
    {0, 4, 4}, // 160 TURN_TO_FACE_CREATURE_REACTION
    {0, 4, 4}, // 161 WATCH_FLYING_OBJECT_REACTION
    {0, 4, 4}, // 162 POINT_AT_FLYING_OBJECT_REACTION
    {0, 0, 0}, // 163 DECIDE_WHAT_TO_DO
    {0, 0, 0}, // 164 INTERACT_DECIDE_WHAT_TO_DO
    {0, 0, 0}, // 165 EAT_OUTSIDE
    {0, 4, 4}, // 166 RUN_AWAY_FROM_OBJECT_REACTION
    {0, 4, 6}, // 167 MOVE_TOWARDS_CREATURE_REACTION
    {0, 4, 4}, // 168 AMAZED_BY_MAGIC_SHIELD_REACTION
    {0, 0, 0}, // 169 VILLAGER_GOSSIPS
    {0, 0, 0}, // 170 CHECK_INTERACT_WITH_ANIMAL
    {0, 0, 0}, // 171 CHECK_INTERACT_WITH_WORSHIP_SITE
    {0, 0, 0}, // 172 CHECK_INTERACT_WITH_ABODE
    {0, 0, 0}, // 173 CHECK_INTERACT_WITH_FIELD
    {0, 0, 0}, // 174 CHECK_INTERACT_WITH_FISH_FARM
    {0, 0, 0}, // 175 CHECK_INTERACT_WITH_TREE
    {0, 0, 0}, // 176 CHECK_INTERACT_WITH_BALL
    {0, 0, 0}, // 177 CHECK_INTERACT_WITH_POT
    {0, 0, 0}, // 178 CHECK_INTERACT_WITH_FOOTBALL
    {0, 0, 0}, // 179 CHECK_INTERACT_WITH_VILLAGER
    {0, 0, 0}, // 180 CHECK_INTERACT_WITH_MAGIC_LIVING
    {0, 0, 0}, // 181 CHECK_INTERACT_WITH_ROCK
    {0, 0, 0}, // 182 ARRIVES_AT_ROCK_FOR_WOOD
    {0, 0, 0}, // 183 GOT_WOOD_FROM_ROCK
    {6, 13, 0}, // 184 REENTER_BUILDING_STATE
    {0, 0, 0}, // 185 ARRIVE_AT_PUSH_OBJECT
    {0, 0, 0}, // 186 TAKE_WOOD_FROM_TREE
    {0, 0, 0}, // 187 TAKE_WOOD_FROM_POT
    {6, 13, 0}, // 188 TAKE_WOOD_FROM_TREE_FOR_BUILDING
    {6, 13, 0}, // 189 TAKE_WOOD_FROM_POT_FOR_BUILDING
    {0, 23, 0}, // 190 SHEPHERD_TAKE_ANIMAL_FOR_SLAUGHTER
    {0, 23, 0}, // 191 SHEPHERD_TAKES_CONTROL_OF_FLOCK
    {0, 23, 0}, // 192 SHEPHERD_RELEASES_CONTROL_OF_FLOCK
    {0, 22, 0}, // 193 DANCE_BUT_NOT_WORSHIP
    {0, 4, 4}, // 194 FAINTING_REACTION
    {0, 4, 4}, // 195 START_CONFUSED_REACTION
    {0, 4, 4}, // 196 CONFUSED_REACTION
    {0, 0, 0}, // 197 AFTER_TAP_ON_ABODE
    {0, 25, 0}, // 198 WEAK_ON_GROUND
    {10, 26, 0}, // 199 SCRIPT_WANDER_AROUND_POSITION
    {11, 27, 0}, // 200 SCRIPT_PLAY_ANIM
    {0, 28, 4}, // 201 GO_TOWARDS_TELEPORT_REACTION
    {0, 28, 4}, // 202 TELEPORT_REACTION
    {0, 4, 4}, // 203 DANCE_WHILE_REACTING
    {0, 29, 0}, // 204 CONTROLLED_BY_CREATURE
    {0, 4, 4}, // 205 POINT_AT_DEAD_PERSON
    {0, 4, 4}, // 206 GO_TOWARDS_DEAD_PERSON
    {0, 4, 4}, // 207 LOOK_AT_DEAD_PERSON
    {0, 4, 4}, // 208 MOURN_DEAD_PERSON
    {0, 0, 0}, // 209 NOTHING_TO_DO
    {0, 0, 0}, // 210 ARRIVES_AT_WORKSHOP_FOR_DROP_OFF
    {0, 0, 0}, // 211 ARRIVES_AT_STORAGE_PIT_FOR_WORKSHOP_MATERIALS
    {0, 0, 0}, // 212 SHOW_POISONED
    {0, 18, 0}, // 213 HIDING_AT_WORSHIP_SITE
    {0, 4, 4}, // 214 CROWD_REACTION
    {0, 4, 4}, // 215 REACT_TO_FIRE
    {12, 30, 4}, // 216 PUT_OUT_FIRE_BY_BEATING
    {12, 30, 4}, // 217 PUT_OUT_FIRE_WITH_WATER
    {12, 30, 4}, // 218 GET_WATER_TO_PUT_OUT_FIRE
    {13, 31, 0}, // 219 ON_FIRE
    {12, 30, 4}, // 220 MOVE_AROUND_FIRE
    {14, 0, 0}, // 221 DISCIPLE_NOTHING_TO_DO
    {0, 1, 0}, // 222 FOOTBALL_MOVE_TO_BALL
    {0, 0, 0}, // 223 ARRIVES_AT_STORAGE_PIT_FOR_TRADER_PICK_UP
    {0, 0, 0}, // 224 ARRIVES_AT_STORAGE_PIT_FOR_TRADER_DROP_OFF
    {15, 32, 5}, // 225 BREEDER_DISCIPLE
    {0, 0, 0}, // 226 MISSIONARY_DISCIPLE
    {0, 4, 4}, // 227 REACT_TO_BREEDER
    {0, 23, 0}, // 228 SHEPHERD_CHECK_ANIMAL_FOR_SLAUGHTER
    {0, 0, 0}, // 229 INTERACT_DECIDE_WHAT_TO_DO_FOR_OTHER_VILLAGER
    {0, 22, 0}, // 230 ARTIFACT_DANCE
    {0, 4, 4}, // 231 FLEEING_FROM_PREDATOR_REACTION
    {0, 0, 0}, // 232 WAIT_FOR_WOOD
    {0, 0, 7}, // 233 INSPECT_OBJECT
    {0, 33, 0}, // 234 GO_HOME_AND_CHANGE
    {0, 4, 4}, // 235 WAIT_FOR_MATE
    {0, 4, 4}, // 236 GO_AND_HIDE_IN_NEARBY_BUILDING
    {0, 4, 4}, // 237 LOOK_TO_SEE_IF_IT_IS_SAFE
    {0, 0, 0}, // 238 SLEEP_IN_TENT
    {0, 0, 0}, // 239 PAUSE_FOR_A_SECOND
    {0, 0, 0}, // 240 PANIC_REACTION
    {0, 19, 0}, // 241 GET_FOOD_AT_WORSHIP_SITE
    {0, 0, 0}, // 242 GOTO_CONGREGATE_IN_TOWN_AFTER_EMERGENCY
    {0, 0, 0}, // 243 CONGREGATE_IN_TOWN_AFTER_EMERGENCY
    {0, 0, 0}, // 244 SCRIPT_IN_CROWD
    {0, 0, 0}, // 245 GO_AND_CHILLOUT_OUTSIDE_HOME
    {16, 0, 0}, // 246 SIT_AND_CHILLOUT
    {1, 2, 0}, // 247 SCRIPT_GO_AND_MOVE_ALONG_PATH
    {0, 12, 0}, // 248 GO_HOME_FROM_WORSHIP
    {0, 12, 0}, // 249 ARRIVES_HOME_FROM_WORSHIP
    {0, 0, 0}, // 250 SLEEP_IN_TENT_FROM_WORSHIP
    {0, 28, 4}, // 251 GO_TOWARDS_TELEPORT_REACTION_QUICKLY
    {0, 0, 0}, // 252 GO_AND_CHILLOUT_IN_TOWN
    {17, 22, 0}, // 253 WAIT_FOR_ARTIFACT_DANCE
    {0, 0, 0}, // 254 BREEDER_JUST_LANDED
}};
// clang-format on

/// Each state's answer to a town emergency, read only when all villagers are called to a town emergency, through the
/// final state. Always = always reacts (206 rows), PreviousState = reacts when it has a PREVIOUS state (only 220
/// MOVE_AROUND_FIRE), None = an empty slot (48 rows: not called). Generated from the original table, so the answer
/// does not depend on which rows openblack has ported (k_VillagerStateTable's field0x50 is not used)
enum class TownEmergencyReaction : uint8_t
{
	None,
	Always,
	PreviousState,
};

// clang-format off
inline constexpr std::array<TownEmergencyReaction, 255> k_TownEmergencyReaction = {{
    TownEmergencyReaction::None, // 0 INVALID_STATE
    TownEmergencyReaction::Always, // 1 MOVE_TO_POS
    TownEmergencyReaction::Always, // 2 MOVE_TO_OBJECT
    TownEmergencyReaction::Always, // 3 MOVE_ON_STRUCTURE
    TownEmergencyReaction::None, // 4 IN_SCRIPT
    TownEmergencyReaction::None, // 5 IN_DANCE
    TownEmergencyReaction::Always, // 6 FLEEING_FROM_OBJECT_REACTION
    TownEmergencyReaction::Always, // 7 LOOKING_AT_OBJECT_REACTION
    TownEmergencyReaction::Always, // 8 FOLLOWING_OBJECT_REACTION
    TownEmergencyReaction::Always, // 9 INSPECT_OBJECT_REACTION
    TownEmergencyReaction::None, // 10 FLYING
    TownEmergencyReaction::None, // 11 LANDED
    TownEmergencyReaction::Always, // 12 LOOK_AT_FLYING_OBJECT_REACTION
    TownEmergencyReaction::None, // 13 SET_DYING
    TownEmergencyReaction::None, // 14 DYING
    TownEmergencyReaction::None, // 15 DEAD
    TownEmergencyReaction::None, // 16 DROWNING
    TownEmergencyReaction::None, // 17 DOWNED
    TownEmergencyReaction::None, // 18 BEING_EATEN
    TownEmergencyReaction::Always, // 19 GOTO_FOOD_REACTION
    TownEmergencyReaction::Always, // 20 ARRIVES_AT_FOOD_REACTION
    TownEmergencyReaction::Always, // 21 GOTO_WOOD_REACTION
    TownEmergencyReaction::Always, // 22 ARRIVES_AT_WOOD_REACTION
    TownEmergencyReaction::Always, // 23 WAIT_FOR_ANIMATION
    TownEmergencyReaction::None, // 24 IN_HAND
    TownEmergencyReaction::Always, // 25 GOTO_PICKUP_BALL_REACTION
    TownEmergencyReaction::Always, // 26 ARRIVES_AT_PICKUP_BALL_REACTION
    TownEmergencyReaction::Always, // 27 MOVE_IN_FLOCK
    TownEmergencyReaction::Always, // 28 MOVE_ALONG_PATH
    TownEmergencyReaction::Always, // 29 MOVE_ON_PATH
    TownEmergencyReaction::Always, // 30 FLEEING_AND_LOOKING_AT_OBJECT_REACTION
    TownEmergencyReaction::Always, // 31 GOTO_STORAGE_PIT_FOR_DROP_OFF
    TownEmergencyReaction::Always, // 32 ARRIVES_AT_STORAGE_PIT_FOR_DROP_OFF
    TownEmergencyReaction::Always, // 33 GOTO_STORAGE_PIT_FOR_FOOD
    TownEmergencyReaction::Always, // 34 ARRIVES_AT_STORAGE_PIT_FOR_FOOD
    TownEmergencyReaction::Always, // 35 ARRIVES_AT_HOME_WITH_FOOD
    TownEmergencyReaction::Always, // 36 GO_HOME
    TownEmergencyReaction::Always, // 37 ARRIVES_HOME
    TownEmergencyReaction::None, // 38 AT_HOME
    TownEmergencyReaction::Always, // 39 ARRIVES_AT_STORAGE_PIT_FOR_BUILDING_MATERIALS
    TownEmergencyReaction::Always, // 40 ARRIVES_AT_BUILDING_SITE
    TownEmergencyReaction::Always, // 41 BUILDING
    TownEmergencyReaction::Always, // 42 GOTO_STORAGE_PIT_FOR_WORSHIP_SUPPLIES
    TownEmergencyReaction::Always, // 43 ARRIVES_AT_STORAGE_PIT_FOR_WORSHIP_SUPPLIES
    TownEmergencyReaction::Always, // 44 GOTO_WORSHIP_SITE_WITH_SUPPLIES
    TownEmergencyReaction::Always, // 45 MOVE_TO_WORSHIP_SITE_WITH_SUPPLIES
    TownEmergencyReaction::Always, // 46 ARRIVES_AT_WORSHIP_SITE_WITH_SUPPLIES
    TownEmergencyReaction::Always, // 47 FORESTER_MOVE_TO_FOREST
    TownEmergencyReaction::Always, // 48 FORESTER_GOTO_FOREST
    TownEmergencyReaction::Always, // 49 FORESTER_ARRIVES_AT_FOREST
    TownEmergencyReaction::Always, // 50 FORESTER_CHOPS_TREE
    TownEmergencyReaction::Always, // 51 FORESTER_CHOPS_TREE_FOR_BUILDING
    TownEmergencyReaction::Always, // 52 FORESTER_FINISHED_FORESTERING
    TownEmergencyReaction::Always, // 53 ARRIVES_AT_BIG_FOREST
    TownEmergencyReaction::Always, // 54 ARRIVES_AT_BIG_FOREST_FOR_BUILDING
    TownEmergencyReaction::Always, // 55 FISHERMAN_ARRIVES_AT_FISHING
    TownEmergencyReaction::Always, // 56 FISHING
    TownEmergencyReaction::Always, // 57 WAIT_FOR_COUNTER
    TownEmergencyReaction::Always, // 58 GOTO_WORSHIP_SITE_FOR_WORSHIP
    TownEmergencyReaction::Always, // 59 ARRIVES_AT_WORSHIP_SITE_FOR_WORSHIP
    TownEmergencyReaction::Always, // 60 WORSHIPPING_AT_WORSHIP_SITE
    TownEmergencyReaction::Always, // 61 GOTO_ALTAR_FOR_REST
    TownEmergencyReaction::Always, // 62 ARRIVES_AT_ALTAR_FOR_REST
    TownEmergencyReaction::Always, // 63 AT_ALTAR_REST
    TownEmergencyReaction::Always, // 64 AT_ALTAR_FINISHED_REST
    TownEmergencyReaction::Always, // 65 RESTART_WORSHIPPING_AT_WORSHIP_SITE
    TownEmergencyReaction::Always, // 66 RESTART_WORSHIPPING_CREATURE
    TownEmergencyReaction::Always, // 67 FARMER_ARRIVES_AT_FARM
    TownEmergencyReaction::Always, // 68 FARMER_PLANTS_CROP
    TownEmergencyReaction::Always, // 69 FARMER_DIGS_UP_CROP
    TownEmergencyReaction::Always, // 70 MOVE_TO_FOOTBALL_PITCH_CONSTRUCTION
    TownEmergencyReaction::Always, // 71 FOOTBALL_WALK_TO_POSITION
    TownEmergencyReaction::Always, // 72 FOOTBALL_WAIT_FOR_KICK_OFF
    TownEmergencyReaction::Always, // 73 FOOTBALL_ATTACKER
    TownEmergencyReaction::Always, // 74 FOOTBALL_GOALIE
    TownEmergencyReaction::Always, // 75 FOOTBALL_DEFENDER
    TownEmergencyReaction::Always, // 76 FOOTBALL_WON_GOAL
    TownEmergencyReaction::Always, // 77 FOOTBALL_LOST_GOAL
    TownEmergencyReaction::Always, // 78 START_MOVE_TO_PICK_UP_BALL_FOR_DEAD_BALL
    TownEmergencyReaction::Always, // 79 ARRIVED_AT_PICK_UP_BALL_FOR_DEAD_BALL
    TownEmergencyReaction::Always, // 80 ARRIVED_AT_PUT_DOWN_BALL_FOR_DEAD_BALL_START
    TownEmergencyReaction::Always, // 81 ARRIVED_AT_PUT_DOWN_BALL_FOR_DEAD_BALL_END
    TownEmergencyReaction::Always, // 82 FOOTBALL_MATCH_PAUSED
    TownEmergencyReaction::Always, // 83 FOOTBALL_WATCH_MATCH
    TownEmergencyReaction::Always, // 84 FOOTBALL_MEXICAN_WAVE
    TownEmergencyReaction::Always, // 85 CREATED
    TownEmergencyReaction::Always, // 86 ARRIVES_IN_ABODE_TO_TRADE
    TownEmergencyReaction::Always, // 87 ARRIVES_IN_ABODE_TO_PICK_UP_EXCESS
    TownEmergencyReaction::None, // 88 MAKE_SCARED_STIFF
    TownEmergencyReaction::None, // 89 SCARED_STIFF
    TownEmergencyReaction::Always, // 90 WORSHIPPING_CREATURE
    TownEmergencyReaction::Always, // 91 SHEPHERD_LOOK_FOR_FLOCK
    TownEmergencyReaction::Always, // 92 SHEPHERD_MOVE_FLOCK_TO_WATER
    TownEmergencyReaction::Always, // 93 SHEPHERD_MOVE_FLOCK_TO_FOOD
    TownEmergencyReaction::Always, // 94 SHEPHERD_MOVE_FLOCK_BACK
    TownEmergencyReaction::Always, // 95 SHEPHERD_DECIDE_WHAT_TO_DO_WITH_FLOCK
    TownEmergencyReaction::Always, // 96 SHEPHERD_WAIT_FOR_FLOCK
    TownEmergencyReaction::Always, // 97 SHEPHERD_SLAUGHTER_ANIMAL
    TownEmergencyReaction::Always, // 98 SHEPHERD_FETCH_STRAY
    TownEmergencyReaction::Always, // 99 SHEPHERD_GOTO_FLOCK
    TownEmergencyReaction::None, // 100 HOUSEWIFE_AT_HOME
    TownEmergencyReaction::Always, // 101 HOUSEWIFE_GOTO_STORAGE_PIT
    TownEmergencyReaction::Always, // 102 HOUSEWIFE_ARRIVES_AT_STORAGE_PIT
    TownEmergencyReaction::Always, // 103 HOUSEWIFE_PICKUP_FROM_STORAGE_PIT
    TownEmergencyReaction::Always, // 104 HOUSEWIFE_RETURN_HOME_WITH_FOOD
    TownEmergencyReaction::Always, // 105 HOUSEWIFE_MAKE_DINNER
    TownEmergencyReaction::Always, // 106 HOUSEWIFE_SERVES_DINNER
    TownEmergencyReaction::Always, // 107 HOUSEWIFE_CLEARS_AWAY_DINNER
    TownEmergencyReaction::Always, // 108 HOUSEWIFE_DOES_HOUSEWORK
    TownEmergencyReaction::Always, // 109 HOUSEWIFE_GOSSIPS_AROUND_STORAGE_PIT
    TownEmergencyReaction::None, // 110 HOUSEWIFE_STARTS_GIVING_BIRTH
    TownEmergencyReaction::None, // 111 HOUSEWIFE_GIVING_BIRTH
    TownEmergencyReaction::None, // 112 HOUSEWIFE_GIVEN_BIRTH
    TownEmergencyReaction::Always, // 113 CHILD_AT_CRECHE
    TownEmergencyReaction::Always, // 114 CHILD_FOLLOWS_MOTHER
    TownEmergencyReaction::Always, // 115 CHILD_BECOMES_ADULT
    TownEmergencyReaction::Always, // 116 SITS_DOWN_TO_DINNER
    TownEmergencyReaction::Always, // 117 EAT_FOOD
    TownEmergencyReaction::None, // 118 EAT_FOOD_AT_HOME
    TownEmergencyReaction::None, // 119 GOTO_BED_AT_HOME
    TownEmergencyReaction::None, // 120 SLEEPING_AT_HOME
    TownEmergencyReaction::Always, // 121 WAKE_UP_AT_HOME
    TownEmergencyReaction::Always, // 122 START_HAVING_SEX
    TownEmergencyReaction::Always, // 123 HAVING_SEX
    TownEmergencyReaction::Always, // 124 STOP_HAVING_SEX
    TownEmergencyReaction::Always, // 125 START_HAVING_SEX_AT_HOME
    TownEmergencyReaction::Always, // 126 HAVING_SEX_AT_HOME
    TownEmergencyReaction::Always, // 127 STOP_HAVING_SEX_AT_HOME
    TownEmergencyReaction::Always, // 128 WAIT_FOR_DINNER
    TownEmergencyReaction::Always, // 129 HOMELESS_START
    TownEmergencyReaction::Always, // 130 VAGRANT_START
    TownEmergencyReaction::None, // 131 MORN_DEATH
    TownEmergencyReaction::Always, // 132 PERFORM_INSPECTION_REACTION
    TownEmergencyReaction::Always, // 133 APPROACH_OBJECT_REACTION
    TownEmergencyReaction::Always, // 134 INITIALISE_TELL_OTHERS_ABOUT_OBJECT
    TownEmergencyReaction::Always, // 135 TELL_OTHERS_ABOUT_INTERESTING_OBJECT
    TownEmergencyReaction::Always, // 136 APPROACH_VILLAGER_TO_TALK_TO
    TownEmergencyReaction::Always, // 137 TELL_PARTICULAR_VILLAGER_ABOUT_OBJECT
    TownEmergencyReaction::Always, // 138 INITIALISE_LOOK_AROUND_FOR_VILLAGER_TO_TELL
    TownEmergencyReaction::Always, // 139 LOOK_AROUND_FOR_VILLAGER_TO_TELL
    TownEmergencyReaction::Always, // 140 MOVE_TOWARDS_OBJECT_TO_LOOK_AT
    TownEmergencyReaction::Always, // 141 INITIALISE_IMPRESSED_REACTION
    TownEmergencyReaction::Always, // 142 PERFORM_IMPRESSED_REACTION
    TownEmergencyReaction::Always, // 143 INITIALISE_FIGHT_REACTION
    TownEmergencyReaction::Always, // 144 PERFORM_FIGHT_REACTION
    TownEmergencyReaction::Always, // 145 HOMELESS_EAT_DINNER
    TownEmergencyReaction::Always, // 146 INSPECT_CREATURE_REACTION
    TownEmergencyReaction::Always, // 147 PERFORM_INSPECT_CREATURE_REACTION
    TownEmergencyReaction::Always, // 148 APPROACH_CREATURE_REACTION
    TownEmergencyReaction::Always, // 149 INITIALISE_BEWILDERED_BY_MAGIC_TREE_REACTION
    TownEmergencyReaction::Always, // 150 PERFORM_BEWILDERED_BY_MAGIC_TREE_REACTION
    TownEmergencyReaction::Always, // 151 TURN_TO_FACE_MAGIC_TREE
    TownEmergencyReaction::Always, // 152 LOOK_AT_MAGIC_TREE
    TownEmergencyReaction::Always, // 153 DANCE_FOR_EDITING_PURPOSES
    TownEmergencyReaction::Always, // 154 MOVE_TO_DANCE_POS
    TownEmergencyReaction::Always, // 155 INITIALISE_RESPECT_CREATURE_REACTION
    TownEmergencyReaction::Always, // 156 PERFORM_RESPECT_CREATURE_REACTION
    TownEmergencyReaction::Always, // 157 FINISH_RESPECT_CREATURE_REACTION
    TownEmergencyReaction::Always, // 158 APPROACH_HAND_REACTION
    TownEmergencyReaction::Always, // 159 FLEEING_FROM_CREATURE_REACTION
    TownEmergencyReaction::Always, // 160 TURN_TO_FACE_CREATURE_REACTION
    TownEmergencyReaction::Always, // 161 WATCH_FLYING_OBJECT_REACTION
    TownEmergencyReaction::Always, // 162 POINT_AT_FLYING_OBJECT_REACTION
    TownEmergencyReaction::Always, // 163 DECIDE_WHAT_TO_DO
    TownEmergencyReaction::Always, // 164 INTERACT_DECIDE_WHAT_TO_DO
    TownEmergencyReaction::Always, // 165 EAT_OUTSIDE
    TownEmergencyReaction::Always, // 166 RUN_AWAY_FROM_OBJECT_REACTION
    TownEmergencyReaction::Always, // 167 MOVE_TOWARDS_CREATURE_REACTION
    TownEmergencyReaction::None, // 168 AMAZED_BY_MAGIC_SHIELD_REACTION
    TownEmergencyReaction::Always, // 169 VILLAGER_GOSSIPS
    TownEmergencyReaction::Always, // 170 CHECK_INTERACT_WITH_ANIMAL
    TownEmergencyReaction::Always, // 171 CHECK_INTERACT_WITH_WORSHIP_SITE
    TownEmergencyReaction::Always, // 172 CHECK_INTERACT_WITH_ABODE
    TownEmergencyReaction::Always, // 173 CHECK_INTERACT_WITH_FIELD
    TownEmergencyReaction::Always, // 174 CHECK_INTERACT_WITH_FISH_FARM
    TownEmergencyReaction::Always, // 175 CHECK_INTERACT_WITH_TREE
    TownEmergencyReaction::Always, // 176 CHECK_INTERACT_WITH_BALL
    TownEmergencyReaction::Always, // 177 CHECK_INTERACT_WITH_POT
    TownEmergencyReaction::Always, // 178 CHECK_INTERACT_WITH_FOOTBALL
    TownEmergencyReaction::Always, // 179 CHECK_INTERACT_WITH_VILLAGER
    TownEmergencyReaction::Always, // 180 CHECK_INTERACT_WITH_MAGIC_LIVING
    TownEmergencyReaction::Always, // 181 CHECK_INTERACT_WITH_ROCK
    TownEmergencyReaction::Always, // 182 ARRIVES_AT_ROCK_FOR_WOOD
    TownEmergencyReaction::Always, // 183 GOT_WOOD_FROM_ROCK
    TownEmergencyReaction::Always, // 184 REENTER_BUILDING_STATE
    TownEmergencyReaction::Always, // 185 ARRIVE_AT_PUSH_OBJECT
    TownEmergencyReaction::Always, // 186 TAKE_WOOD_FROM_TREE
    TownEmergencyReaction::Always, // 187 TAKE_WOOD_FROM_POT
    TownEmergencyReaction::Always, // 188 TAKE_WOOD_FROM_TREE_FOR_BUILDING
    TownEmergencyReaction::Always, // 189 TAKE_WOOD_FROM_POT_FOR_BUILDING
    TownEmergencyReaction::Always, // 190 SHEPHERD_TAKE_ANIMAL_FOR_SLAUGHTER
    TownEmergencyReaction::Always, // 191 SHEPHERD_TAKES_CONTROL_OF_FLOCK
    TownEmergencyReaction::Always, // 192 SHEPHERD_RELEASES_CONTROL_OF_FLOCK
    TownEmergencyReaction::Always, // 193 DANCE_BUT_NOT_WORSHIP
    TownEmergencyReaction::None, // 194 FAINTING_REACTION
    TownEmergencyReaction::None, // 195 START_CONFUSED_REACTION
    TownEmergencyReaction::None, // 196 CONFUSED_REACTION
    TownEmergencyReaction::Always, // 197 AFTER_TAP_ON_ABODE
    TownEmergencyReaction::None, // 198 WEAK_ON_GROUND
    TownEmergencyReaction::None, // 199 SCRIPT_WANDER_AROUND_POSITION
    TownEmergencyReaction::None, // 200 SCRIPT_PLAY_ANIM
    TownEmergencyReaction::Always, // 201 GO_TOWARDS_TELEPORT_REACTION
    TownEmergencyReaction::Always, // 202 TELEPORT_REACTION
    TownEmergencyReaction::Always, // 203 DANCE_WHILE_REACTING
    TownEmergencyReaction::Always, // 204 CONTROLLED_BY_CREATURE
    TownEmergencyReaction::None, // 205 POINT_AT_DEAD_PERSON
    TownEmergencyReaction::None, // 206 GO_TOWARDS_DEAD_PERSON
    TownEmergencyReaction::None, // 207 LOOK_AT_DEAD_PERSON
    TownEmergencyReaction::None, // 208 MOURN_DEAD_PERSON
    TownEmergencyReaction::Always, // 209 NOTHING_TO_DO
    TownEmergencyReaction::Always, // 210 ARRIVES_AT_WORKSHOP_FOR_DROP_OFF
    TownEmergencyReaction::Always, // 211 ARRIVES_AT_STORAGE_PIT_FOR_WORKSHOP_MATERIALS
    TownEmergencyReaction::None, // 212 SHOW_POISONED
    TownEmergencyReaction::None, // 213 HIDING_AT_WORSHIP_SITE
    TownEmergencyReaction::Always, // 214 CROWD_REACTION
    TownEmergencyReaction::None, // 215 REACT_TO_FIRE
    TownEmergencyReaction::None, // 216 PUT_OUT_FIRE_BY_BEATING
    TownEmergencyReaction::None, // 217 PUT_OUT_FIRE_WITH_WATER
    TownEmergencyReaction::None, // 218 GET_WATER_TO_PUT_OUT_FIRE
    TownEmergencyReaction::None, // 219 ON_FIRE
    TownEmergencyReaction::PreviousState, // 220 MOVE_AROUND_FIRE
    TownEmergencyReaction::Always, // 221 DISCIPLE_NOTHING_TO_DO
    TownEmergencyReaction::Always, // 222 FOOTBALL_MOVE_TO_BALL
    TownEmergencyReaction::Always, // 223 ARRIVES_AT_STORAGE_PIT_FOR_TRADER_PICK_UP
    TownEmergencyReaction::Always, // 224 ARRIVES_AT_STORAGE_PIT_FOR_TRADER_DROP_OFF
    TownEmergencyReaction::Always, // 225 BREEDER_DISCIPLE
    TownEmergencyReaction::Always, // 226 MISSIONARY_DISCIPLE
    TownEmergencyReaction::Always, // 227 REACT_TO_BREEDER
    TownEmergencyReaction::Always, // 228 SHEPHERD_CHECK_ANIMAL_FOR_SLAUGHTER
    TownEmergencyReaction::Always, // 229 INTERACT_DECIDE_WHAT_TO_DO_FOR_OTHER_VILLAGER
    TownEmergencyReaction::Always, // 230 ARTIFACT_DANCE
    TownEmergencyReaction::Always, // 231 FLEEING_FROM_PREDATOR_REACTION
    TownEmergencyReaction::Always, // 232 WAIT_FOR_WOOD
    TownEmergencyReaction::Always, // 233 INSPECT_OBJECT
    TownEmergencyReaction::Always, // 234 GO_HOME_AND_CHANGE
    TownEmergencyReaction::Always, // 235 WAIT_FOR_MATE
    TownEmergencyReaction::Always, // 236 GO_AND_HIDE_IN_NEARBY_BUILDING
    TownEmergencyReaction::Always, // 237 LOOK_TO_SEE_IF_IT_IS_SAFE
    TownEmergencyReaction::Always, // 238 SLEEP_IN_TENT
    TownEmergencyReaction::Always, // 239 PAUSE_FOR_A_SECOND
    TownEmergencyReaction::Always, // 240 PANIC_REACTION
    TownEmergencyReaction::Always, // 241 GET_FOOD_AT_WORSHIP_SITE
    TownEmergencyReaction::None, // 242 GOTO_CONGREGATE_IN_TOWN_AFTER_EMERGENCY
    TownEmergencyReaction::None, // 243 CONGREGATE_IN_TOWN_AFTER_EMERGENCY
    TownEmergencyReaction::None, // 244 SCRIPT_IN_CROWD
    TownEmergencyReaction::None, // 245 GO_AND_CHILLOUT_OUTSIDE_HOME
    TownEmergencyReaction::None, // 246 SIT_AND_CHILLOUT
    TownEmergencyReaction::None, // 247 SCRIPT_GO_AND_MOVE_ALONG_PATH
    TownEmergencyReaction::Always, // 248 GO_HOME_FROM_WORSHIP
    TownEmergencyReaction::Always, // 249 ARRIVES_HOME_FROM_WORSHIP
    TownEmergencyReaction::Always, // 250 SLEEP_IN_TENT_FROM_WORSHIP
    TownEmergencyReaction::Always, // 251 GO_TOWARDS_TELEPORT_REACTION_QUICKLY
    TownEmergencyReaction::None, // 252 GO_AND_CHILLOUT_IN_TOWN
    TownEmergencyReaction::Always, // 253 WAIT_FOR_ARTIFACT_DANCE
    TownEmergencyReaction::Always, // 254 BREEDER_JUST_LANDED
}};
// clang-format on
} // namespace openblack::ecs::villager
