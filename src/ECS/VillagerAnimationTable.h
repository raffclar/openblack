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

// Only for ECS/VillagerAnimations.cpp: which villager states override their info.dat clip and which play a clip on
// entering or leaving.
namespace openblack::ecs::villager_animation
{

// Generated from the original's villager state table
enum class AnimFn : uint8_t
{
	None,
	AmazedByShield,
	Building,
	ControlledByCreature,
	Dance,
	Dead,
	Dying,
	FootballAttacker,
	FootballDefender,
	FootballGoalKeeper,
	FootballMatchPaused,
	FootballWaitForKickOff,
	FootballWatchMatch,
	Forestering,
	InspectCreature,
	Kissing,
	Landed,
	LookAtFlyingObject,
	LookAtLargeObject,
	MoveToPos,
	PauseForASecond,
	PointAtFlyingObject,
	RandomCrowd,
	RespectCreature,
	Script,
	SitDown,
	Thrown,
	TownEmergency,
	WatchFight,
	Yawn,
};

enum class TransitionFn : uint8_t
{
	None,
	ArrivesAtResource,
	Building,
	Mourn,
	MoveToPos,
	Pray,
	SitDown,
	SleepInTent,
};

struct StateAnimFns
{
	AnimFn anim;
	TransitionFn transition;
};

constexpr std::array<StateAnimFns, 255> k_StateAnimFns = {{
    {AnimFn::None, TransitionFn::None},                   // 0 INVALID_STATE
    {AnimFn::MoveToPos, TransitionFn::MoveToPos},         // 1 MOVE_TO_POS
    {AnimFn::MoveToPos, TransitionFn::MoveToPos},         // 2 MOVE_TO_OBJECT
    {AnimFn::MoveToPos, TransitionFn::MoveToPos},         // 3 MOVE_ON_STRUCTURE
    {AnimFn::None, TransitionFn::None},                   // 4 IN_SCRIPT
    {AnimFn::Dance, TransitionFn::None},                  // 5 IN_DANCE
    {AnimFn::MoveToPos, TransitionFn::MoveToPos},         // 6 FLEEING_FROM_OBJECT_REACTION
    {AnimFn::None, TransitionFn::None},                   // 7 LOOKING_AT_OBJECT_REACTION
    {AnimFn::None, TransitionFn::None},                   // 8 FOLLOWING_OBJECT_REACTION
    {AnimFn::None, TransitionFn::None},                   // 9 INSPECT_OBJECT_REACTION
    {AnimFn::Thrown, TransitionFn::None},                 // 10 FLYING
    {AnimFn::Landed, TransitionFn::None},                 // 11 LANDED
    {AnimFn::LookAtFlyingObject, TransitionFn::None},     // 12 LOOK_AT_FLYING_OBJECT_REACTION
    {AnimFn::None, TransitionFn::None},                   // 13 SET_DYING
    {AnimFn::Dying, TransitionFn::None},                  // 14 DYING
    {AnimFn::Dead, TransitionFn::None},                   // 15 DEAD
    {AnimFn::None, TransitionFn::None},                   // 16 DROWNING
    {AnimFn::None, TransitionFn::None},                   // 17 DOWNED
    {AnimFn::None, TransitionFn::None},                   // 18 BEING_EATEN
    {AnimFn::None, TransitionFn::None},                   // 19 GOTO_FOOD_REACTION
    {AnimFn::None, TransitionFn::ArrivesAtResource},      // 20 ARRIVES_AT_FOOD_REACTION
    {AnimFn::None, TransitionFn::None},                   // 21 GOTO_WOOD_REACTION
    {AnimFn::None, TransitionFn::ArrivesAtResource},      // 22 ARRIVES_AT_WOOD_REACTION
    {AnimFn::None, TransitionFn::None},                   // 23 WAIT_FOR_ANIMATION
    {AnimFn::None, TransitionFn::None},                   // 24 IN_HAND
    {AnimFn::None, TransitionFn::None},                   // 25 GOTO_PICKUP_BALL_REACTION
    {AnimFn::None, TransitionFn::None},                   // 26 ARRIVES_AT_PICKUP_BALL_REACTION
    {AnimFn::None, TransitionFn::None},                   // 27 MOVE_IN_FLOCK
    {AnimFn::MoveToPos, TransitionFn::MoveToPos},         // 28 MOVE_ALONG_PATH
    {AnimFn::MoveToPos, TransitionFn::MoveToPos},         // 29 MOVE_ON_PATH
    {AnimFn::None, TransitionFn::None},                   // 30 FLEEING_AND_LOOKING_AT_OBJECT_REACTION
    {AnimFn::None, TransitionFn::None},                   // 31 GOTO_STORAGE_PIT_FOR_DROP_OFF
    {AnimFn::None, TransitionFn::None},                   // 32 ARRIVES_AT_STORAGE_PIT_FOR_DROP_OFF
    {AnimFn::None, TransitionFn::None},                   // 33 GOTO_STORAGE_PIT_FOR_FOOD
    {AnimFn::None, TransitionFn::None},                   // 34 ARRIVES_AT_STORAGE_PIT_FOR_FOOD
    {AnimFn::None, TransitionFn::None},                   // 35 ARRIVES_AT_HOME_WITH_FOOD
    {AnimFn::None, TransitionFn::None},                   // 36 GO_HOME
    {AnimFn::None, TransitionFn::None},                   // 37 ARRIVES_HOME
    {AnimFn::None, TransitionFn::None},                   // 38 AT_HOME
    {AnimFn::None, TransitionFn::None},                   // 39 ARRIVES_AT_STORAGE_PIT_FOR_BUILDING_MATERIALS
    {AnimFn::None, TransitionFn::None},                   // 40 ARRIVES_AT_BUILDING_SITE
    {AnimFn::Building, TransitionFn::Building},           // 41 BUILDING
    {AnimFn::None, TransitionFn::None},                   // 42 GOTO_STORAGE_PIT_FOR_WORSHIP_SUPPLIES
    {AnimFn::None, TransitionFn::None},                   // 43 ARRIVES_AT_STORAGE_PIT_FOR_WORSHIP_SUPPLIES
    {AnimFn::None, TransitionFn::None},                   // 44 GOTO_WORSHIP_SITE_WITH_SUPPLIES
    {AnimFn::MoveToPos, TransitionFn::MoveToPos},         // 45 MOVE_TO_WORSHIP_SITE_WITH_SUPPLIES
    {AnimFn::None, TransitionFn::None},                   // 46 ARRIVES_AT_WORSHIP_SITE_WITH_SUPPLIES
    {AnimFn::MoveToPos, TransitionFn::MoveToPos},         // 47 FORESTER_MOVE_TO_FOREST
    {AnimFn::None, TransitionFn::None},                   // 48 FORESTER_GOTO_FOREST
    {AnimFn::None, TransitionFn::None},                   // 49 FORESTER_ARRIVES_AT_FOREST
    {AnimFn::None, TransitionFn::None},                   // 50 FORESTER_CHOPS_TREE
    {AnimFn::Forestering, TransitionFn::None},            // 51 FORESTER_CHOPS_TREE_FOR_BUILDING
    {AnimFn::None, TransitionFn::None},                   // 52 FORESTER_FINISHED_FORESTERING
    {AnimFn::None, TransitionFn::None},                   // 53 ARRIVES_AT_BIG_FOREST
    {AnimFn::None, TransitionFn::None},                   // 54 ARRIVES_AT_BIG_FOREST_FOR_BUILDING
    {AnimFn::None, TransitionFn::None},                   // 55 FISHERMAN_ARRIVES_AT_FISHING
    {AnimFn::None, TransitionFn::None},                   // 56 FISHING
    {AnimFn::None, TransitionFn::None},                   // 57 WAIT_FOR_COUNTER
    {AnimFn::None, TransitionFn::None},                   // 58 GOTO_WORSHIP_SITE_FOR_WORSHIP
    {AnimFn::None, TransitionFn::None},                   // 59 ARRIVES_AT_WORSHIP_SITE_FOR_WORSHIP
    {AnimFn::Dance, TransitionFn::None},                  // 60 WORSHIPPING_AT_WORSHIP_SITE
    {AnimFn::None, TransitionFn::None},                   // 61 GOTO_ALTAR_FOR_REST
    {AnimFn::None, TransitionFn::None},                   // 62 ARRIVES_AT_ALTAR_FOR_REST
    {AnimFn::None, TransitionFn::Pray},                   // 63 AT_ALTAR_REST
    {AnimFn::None, TransitionFn::None},                   // 64 AT_ALTAR_FINISHED_REST
    {AnimFn::Dance, TransitionFn::None},                  // 65 RESTART_WORSHIPPING_AT_WORSHIP_SITE
    {AnimFn::Dance, TransitionFn::None},                  // 66 RESTART_WORSHIPPING_CREATURE
    {AnimFn::None, TransitionFn::None},                   // 67 FARMER_ARRIVES_AT_FARM
    {AnimFn::None, TransitionFn::None},                   // 68 FARMER_PLANTS_CROP
    {AnimFn::None, TransitionFn::None},                   // 69 FARMER_DIGS_UP_CROP
    {AnimFn::None, TransitionFn::None},                   // 70 MOVE_TO_FOOTBALL_PITCH_CONSTRUCTION
    {AnimFn::MoveToPos, TransitionFn::MoveToPos},         // 71 FOOTBALL_WALK_TO_POSITION
    {AnimFn::FootballWaitForKickOff, TransitionFn::None}, // 72 FOOTBALL_WAIT_FOR_KICK_OFF
    {AnimFn::FootballAttacker, TransitionFn::None},       // 73 FOOTBALL_ATTACKER
    {AnimFn::FootballGoalKeeper, TransitionFn::None},     // 74 FOOTBALL_GOALIE
    {AnimFn::FootballDefender, TransitionFn::None},       // 75 FOOTBALL_DEFENDER
    {AnimFn::None, TransitionFn::None},                   // 76 FOOTBALL_WON_GOAL
    {AnimFn::None, TransitionFn::None},                   // 77 FOOTBALL_LOST_GOAL
    {AnimFn::MoveToPos, TransitionFn::MoveToPos},         // 78 START_MOVE_TO_PICK_UP_BALL_FOR_DEAD_BALL
    {AnimFn::None, TransitionFn::None},                   // 79 ARRIVED_AT_PICK_UP_BALL_FOR_DEAD_BALL
    {AnimFn::None, TransitionFn::None},                   // 80 ARRIVED_AT_PUT_DOWN_BALL_FOR_DEAD_BALL_START
    {AnimFn::None, TransitionFn::None},                   // 81 ARRIVED_AT_PUT_DOWN_BALL_FOR_DEAD_BALL_END
    {AnimFn::FootballMatchPaused, TransitionFn::None},    // 82 FOOTBALL_MATCH_PAUSED
    {AnimFn::FootballWatchMatch, TransitionFn::None},     // 83 FOOTBALL_WATCH_MATCH
    {AnimFn::None, TransitionFn::None},                   // 84 FOOTBALL_MEXICAN_WAVE
    {AnimFn::None, TransitionFn::None},                   // 85 CREATED
    {AnimFn::None, TransitionFn::None},                   // 86 ARRIVES_IN_ABODE_TO_TRADE
    {AnimFn::None, TransitionFn::None},                   // 87 ARRIVES_IN_ABODE_TO_PICK_UP_EXCESS
    {AnimFn::None, TransitionFn::None},                   // 88 MAKE_SCARED_STIFF
    {AnimFn::None, TransitionFn::None},                   // 89 SCARED_STIFF
    {AnimFn::Dance, TransitionFn::None},                  // 90 WORSHIPPING_CREATURE
    {AnimFn::None, TransitionFn::None},                   // 91 SHEPHERD_LOOK_FOR_FLOCK
    {AnimFn::MoveToPos, TransitionFn::MoveToPos},         // 92 SHEPHERD_MOVE_FLOCK_TO_WATER
    {AnimFn::MoveToPos, TransitionFn::MoveToPos},         // 93 SHEPHERD_MOVE_FLOCK_TO_FOOD
    {AnimFn::MoveToPos, TransitionFn::MoveToPos},         // 94 SHEPHERD_MOVE_FLOCK_BACK
    {AnimFn::None, TransitionFn::None},                   // 95 SHEPHERD_DECIDE_WHAT_TO_DO_WITH_FLOCK
    {AnimFn::None, TransitionFn::None},                   // 96 SHEPHERD_WAIT_FOR_FLOCK
    {AnimFn::None, TransitionFn::None},                   // 97 SHEPHERD_SLAUGHTER_ANIMAL
    {AnimFn::None, TransitionFn::None},                   // 98 SHEPHERD_FETCH_STRAY
    {AnimFn::None, TransitionFn::None},                   // 99 SHEPHERD_GOTO_FLOCK
    {AnimFn::None, TransitionFn::None},                   // 100 HOUSEWIFE_AT_HOME
    {AnimFn::None, TransitionFn::None},                   // 101 HOUSEWIFE_GOTO_STORAGE_PIT
    {AnimFn::None, TransitionFn::None},                   // 102 HOUSEWIFE_ARRIVES_AT_STORAGE_PIT
    {AnimFn::None, TransitionFn::None},                   // 103 HOUSEWIFE_PICKUP_FROM_STORAGE_PIT
    {AnimFn::None, TransitionFn::None},                   // 104 HOUSEWIFE_RETURN_HOME_WITH_FOOD
    {AnimFn::None, TransitionFn::None},                   // 105 HOUSEWIFE_MAKE_DINNER
    {AnimFn::None, TransitionFn::None},                   // 106 HOUSEWIFE_SERVES_DINNER
    {AnimFn::None, TransitionFn::None},                   // 107 HOUSEWIFE_CLEARS_AWAY_DINNER
    {AnimFn::None, TransitionFn::None},                   // 108 HOUSEWIFE_DOES_HOUSEWORK
    {AnimFn::None, TransitionFn::None},                   // 109 HOUSEWIFE_GOSSIPS_AROUND_STORAGE_PIT
    {AnimFn::None, TransitionFn::None},                   // 110 HOUSEWIFE_STARTS_GIVING_BIRTH
    {AnimFn::None, TransitionFn::None},                   // 111 HOUSEWIFE_GIVING_BIRTH
    {AnimFn::None, TransitionFn::None},                   // 112 HOUSEWIFE_GIVEN_BIRTH
    {AnimFn::None, TransitionFn::None},                   // 113 CHILD_AT_CRECHE
    {AnimFn::None, TransitionFn::None},                   // 114 CHILD_FOLLOWS_MOTHER
    {AnimFn::None, TransitionFn::None},                   // 115 CHILD_BECOMES_ADULT
    {AnimFn::None, TransitionFn::None},                   // 116 SITS_DOWN_TO_DINNER
    {AnimFn::None, TransitionFn::None},                   // 117 EAT_FOOD
    {AnimFn::None, TransitionFn::None},                   // 118 EAT_FOOD_AT_HOME
    {AnimFn::None, TransitionFn::None},                   // 119 GOTO_BED_AT_HOME
    {AnimFn::None, TransitionFn::None},                   // 120 SLEEPING_AT_HOME
    {AnimFn::None, TransitionFn::None},                   // 121 WAKE_UP_AT_HOME
    {AnimFn::None, TransitionFn::None},                   // 122 START_HAVING_SEX
    {AnimFn::Kissing, TransitionFn::None},                // 123 HAVING_SEX
    {AnimFn::None, TransitionFn::None},                   // 124 STOP_HAVING_SEX
    {AnimFn::None, TransitionFn::None},                   // 125 START_HAVING_SEX_AT_HOME
    {AnimFn::None, TransitionFn::None},                   // 126 HAVING_SEX_AT_HOME
    {AnimFn::None, TransitionFn::None},                   // 127 STOP_HAVING_SEX_AT_HOME
    {AnimFn::None, TransitionFn::None},                   // 128 WAIT_FOR_DINNER
    {AnimFn::None, TransitionFn::None},                   // 129 HOMELESS_START
    {AnimFn::None, TransitionFn::None},                   // 130 VAGRANT_START
    {AnimFn::None, TransitionFn::Mourn},                  // 131 MORN_DEATH
    {AnimFn::None, TransitionFn::None},                   // 132 PERFORM_INSPECTION_REACTION
    {AnimFn::MoveToPos, TransitionFn::MoveToPos},         // 133 APPROACH_OBJECT_REACTION
    {AnimFn::None, TransitionFn::None},                   // 134 INITIALISE_TELL_OTHERS_ABOUT_OBJECT
    {AnimFn::None, TransitionFn::None},                   // 135 TELL_OTHERS_ABOUT_INTERESTING_OBJECT
    {AnimFn::MoveToPos, TransitionFn::MoveToPos},         // 136 APPROACH_VILLAGER_TO_TALK_TO
    {AnimFn::None, TransitionFn::None},                   // 137 TELL_PARTICULAR_VILLAGER_ABOUT_OBJECT
    {AnimFn::None, TransitionFn::None},                   // 138 INITIALISE_LOOK_AROUND_FOR_VILLAGER_TO_TELL
    {AnimFn::None, TransitionFn::None},                   // 139 LOOK_AROUND_FOR_VILLAGER_TO_TELL
    {AnimFn::MoveToPos, TransitionFn::MoveToPos},         // 140 MOVE_TOWARDS_OBJECT_TO_LOOK_AT
    {AnimFn::None, TransitionFn::None},                   // 141 INITIALISE_IMPRESSED_REACTION
    {AnimFn::None, TransitionFn::Pray},                   // 142 PERFORM_IMPRESSED_REACTION
    {AnimFn::None, TransitionFn::None},                   // 143 INITIALISE_FIGHT_REACTION
    {AnimFn::WatchFight, TransitionFn::None},             // 144 PERFORM_FIGHT_REACTION
    {AnimFn::None, TransitionFn::None},                   // 145 HOMELESS_EAT_DINNER
    {AnimFn::InspectCreature, TransitionFn::None},        // 146 INSPECT_CREATURE_REACTION
    {AnimFn::InspectCreature, TransitionFn::None},        // 147 PERFORM_INSPECT_CREATURE_REACTION
    {AnimFn::InspectCreature, TransitionFn::None},        // 148 APPROACH_CREATURE_REACTION
    {AnimFn::None, TransitionFn::None},                   // 149 INITIALISE_BEWILDERED_BY_MAGIC_TREE_REACTION
    {AnimFn::None, TransitionFn::None},                   // 150 PERFORM_BEWILDERED_BY_MAGIC_TREE_REACTION
    {AnimFn::None, TransitionFn::None},                   // 151 TURN_TO_FACE_MAGIC_TREE
    {AnimFn::LookAtLargeObject, TransitionFn::None},      // 152 LOOK_AT_MAGIC_TREE
    {AnimFn::Dance, TransitionFn::None},                  // 153 DANCE_FOR_EDITING_PURPOSES
    {AnimFn::MoveToPos, TransitionFn::MoveToPos},         // 154 MOVE_TO_DANCE_POS
    {AnimFn::RespectCreature, TransitionFn::None},        // 155 INITIALISE_RESPECT_CREATURE_REACTION
    {AnimFn::RespectCreature, TransitionFn::None},        // 156 PERFORM_RESPECT_CREATURE_REACTION
    {AnimFn::RespectCreature, TransitionFn::None},        // 157 FINISH_RESPECT_CREATURE_REACTION
    {AnimFn::None, TransitionFn::None},                   // 158 APPROACH_HAND_REACTION
    {AnimFn::None, TransitionFn::None},                   // 159 FLEEING_FROM_CREATURE_REACTION
    {AnimFn::InspectCreature, TransitionFn::None},        // 160 TURN_TO_FACE_CREATURE_REACTION
    {AnimFn::LookAtFlyingObject, TransitionFn::None},     // 161 WATCH_FLYING_OBJECT_REACTION
    {AnimFn::PointAtFlyingObject, TransitionFn::None},    // 162 POINT_AT_FLYING_OBJECT_REACTION
    {AnimFn::None, TransitionFn::None},                   // 163 DECIDE_WHAT_TO_DO
    {AnimFn::None, TransitionFn::None},                   // 164 INTERACT_DECIDE_WHAT_TO_DO
    {AnimFn::None, TransitionFn::None},                   // 165 EAT_OUTSIDE
    {AnimFn::None, TransitionFn::None},                   // 166 RUN_AWAY_FROM_OBJECT_REACTION
    {AnimFn::MoveToPos, TransitionFn::MoveToPos},         // 167 MOVE_TOWARDS_CREATURE_REACTION
    {AnimFn::AmazedByShield, TransitionFn::None},         // 168 AMAZED_BY_MAGIC_SHIELD_REACTION
    {AnimFn::None, TransitionFn::None},                   // 169 VILLAGER_GOSSIPS
    {AnimFn::None, TransitionFn::None},                   // 170 CHECK_INTERACT_WITH_ANIMAL
    {AnimFn::None, TransitionFn::None},                   // 171 CHECK_INTERACT_WITH_WORSHIP_SITE
    {AnimFn::None, TransitionFn::None},                   // 172 CHECK_INTERACT_WITH_ABODE
    {AnimFn::None, TransitionFn::None},                   // 173 CHECK_INTERACT_WITH_FIELD
    {AnimFn::None, TransitionFn::None},                   // 174 CHECK_INTERACT_WITH_FISH_FARM
    {AnimFn::None, TransitionFn::None},                   // 175 CHECK_INTERACT_WITH_TREE
    {AnimFn::None, TransitionFn::None},                   // 176 CHECK_INTERACT_WITH_BALL
    {AnimFn::None, TransitionFn::None},                   // 177 CHECK_INTERACT_WITH_POT
    {AnimFn::None, TransitionFn::None},                   // 178 CHECK_INTERACT_WITH_FOOTBALL
    {AnimFn::None, TransitionFn::None},                   // 179 CHECK_INTERACT_WITH_VILLAGER
    {AnimFn::None, TransitionFn::None},                   // 180 CHECK_INTERACT_WITH_MAGIC_LIVING
    {AnimFn::None, TransitionFn::None},                   // 181 CHECK_INTERACT_WITH_ROCK
    {AnimFn::Forestering, TransitionFn::None},            // 182 ARRIVES_AT_ROCK_FOR_WOOD
    {AnimFn::Forestering, TransitionFn::None},            // 183 GOT_WOOD_FROM_ROCK
    {AnimFn::None, TransitionFn::None},                   // 184 REENTER_BUILDING_STATE
    {AnimFn::None, TransitionFn::None},                   // 185 ARRIVE_AT_PUSH_OBJECT
    {AnimFn::Forestering, TransitionFn::None},            // 186 TAKE_WOOD_FROM_TREE
    {AnimFn::Forestering, TransitionFn::None},            // 187 TAKE_WOOD_FROM_POT
    {AnimFn::Forestering, TransitionFn::None},            // 188 TAKE_WOOD_FROM_TREE_FOR_BUILDING
    {AnimFn::Forestering, TransitionFn::None},            // 189 TAKE_WOOD_FROM_POT_FOR_BUILDING
    {AnimFn::None, TransitionFn::None},                   // 190 SHEPHERD_TAKE_ANIMAL_FOR_SLAUGHTER
    {AnimFn::None, TransitionFn::None},                   // 191 SHEPHERD_TAKES_CONTROL_OF_FLOCK
    {AnimFn::None, TransitionFn::None},                   // 192 SHEPHERD_RELEASES_CONTROL_OF_FLOCK
    {AnimFn::Dance, TransitionFn::None},                  // 193 DANCE_BUT_NOT_WORSHIP
    {AnimFn::None, TransitionFn::None},                   // 194 FAINTING_REACTION
    {AnimFn::None, TransitionFn::None},                   // 195 START_CONFUSED_REACTION
    {AnimFn::None, TransitionFn::None},                   // 196 CONFUSED_REACTION
    {AnimFn::Yawn, TransitionFn::None},                   // 197 AFTER_TAP_ON_ABODE
    {AnimFn::None, TransitionFn::None},                   // 198 WEAK_ON_GROUND
    {AnimFn::None, TransitionFn::None},                   // 199 SCRIPT_WANDER_AROUND_POSITION
    {AnimFn::Script, TransitionFn::None},                 // 200 SCRIPT_PLAY_ANIM
    {AnimFn::None, TransitionFn::None},                   // 201 GO_TOWARDS_TELEPORT_REACTION
    {AnimFn::None, TransitionFn::None},                   // 202 TELEPORT_REACTION
    {AnimFn::Dance, TransitionFn::None},                  // 203 DANCE_WHILE_REACTING
    {AnimFn::ControlledByCreature, TransitionFn::None},   // 204 CONTROLLED_BY_CREATURE
    {AnimFn::None, TransitionFn::None},                   // 205 POINT_AT_DEAD_PERSON
    {AnimFn::None, TransitionFn::None},                   // 206 GO_TOWARDS_DEAD_PERSON
    {AnimFn::None, TransitionFn::None},                   // 207 LOOK_AT_DEAD_PERSON
    {AnimFn::None, TransitionFn::Mourn},                  // 208 MOURN_DEAD_PERSON
    {AnimFn::None, TransitionFn::None},                   // 209 NOTHING_TO_DO
    {AnimFn::None, TransitionFn::None},                   // 210 ARRIVES_AT_WORKSHOP_FOR_DROP_OFF
    {AnimFn::None, TransitionFn::None},                   // 211 ARRIVES_AT_STORAGE_PIT_FOR_WORKSHOP_MATERIALS
    {AnimFn::None, TransitionFn::None},                   // 212 SHOW_POISONED
    {AnimFn::None, TransitionFn::None},                   // 213 HIDING_AT_WORSHIP_SITE
    {AnimFn::None, TransitionFn::None},                   // 214 CROWD_REACTION
    {AnimFn::None, TransitionFn::None},                   // 215 REACT_TO_FIRE
    {AnimFn::None, TransitionFn::None},                   // 216 PUT_OUT_FIRE_BY_BEATING
    {AnimFn::None, TransitionFn::None},                   // 217 PUT_OUT_FIRE_WITH_WATER
    {AnimFn::None, TransitionFn::None},                   // 218 GET_WATER_TO_PUT_OUT_FIRE
    {AnimFn::None, TransitionFn::None},                   // 219 ON_FIRE
    {AnimFn::None, TransitionFn::None},                   // 220 MOVE_AROUND_FIRE
    {AnimFn::None, TransitionFn::Pray},                   // 221 DISCIPLE_NOTHING_TO_DO
    {AnimFn::MoveToPos, TransitionFn::MoveToPos},         // 222 FOOTBALL_MOVE_TO_BALL
    {AnimFn::None, TransitionFn::None},                   // 223 ARRIVES_AT_STORAGE_PIT_FOR_TRADER_PICK_UP
    {AnimFn::None, TransitionFn::None},                   // 224 ARRIVES_AT_STORAGE_PIT_FOR_TRADER_DROP_OFF
    {AnimFn::None, TransitionFn::None},                   // 225 BREEDER_DISCIPLE
    {AnimFn::None, TransitionFn::None},                   // 226 MISSIONARY_DISCIPLE
    {AnimFn::None, TransitionFn::None},                   // 227 REACT_TO_BREEDER
    {AnimFn::None, TransitionFn::None},                   // 228 SHEPHERD_CHECK_ANIMAL_FOR_SLAUGHTER
    {AnimFn::None, TransitionFn::None},                   // 229 INTERACT_DECIDE_WHAT_TO_DO_FOR_OTHER_VILLAGER
    {AnimFn::Dance, TransitionFn::None},                  // 230 ARTIFACT_DANCE
    {AnimFn::MoveToPos, TransitionFn::MoveToPos},         // 231 FLEEING_FROM_PREDATOR_REACTION
    {AnimFn::None, TransitionFn::None},                   // 232 WAIT_FOR_WOOD
    {AnimFn::None, TransitionFn::None},                   // 233 INSPECT_OBJECT
    {AnimFn::None, TransitionFn::None},                   // 234 GO_HOME_AND_CHANGE
    {AnimFn::None, TransitionFn::None},                   // 235 WAIT_FOR_MATE
    {AnimFn::None, TransitionFn::None},                   // 236 GO_AND_HIDE_IN_NEARBY_BUILDING
    {AnimFn::None, TransitionFn::None},                   // 237 LOOK_TO_SEE_IF_IT_IS_SAFE
    {AnimFn::None, TransitionFn::SleepInTent},            // 238 SLEEP_IN_TENT
    {AnimFn::PauseForASecond, TransitionFn::None},        // 239 PAUSE_FOR_A_SECOND
    {AnimFn::None, TransitionFn::None},                   // 240 PANIC_REACTION
    {AnimFn::None, TransitionFn::None},                   // 241 GET_FOOD_AT_WORSHIP_SITE
    {AnimFn::None, TransitionFn::None},                   // 242 GOTO_CONGREGATE_IN_TOWN_AFTER_EMERGENCY
    {AnimFn::TownEmergency, TransitionFn::None},          // 243 CONGREGATE_IN_TOWN_AFTER_EMERGENCY
    {AnimFn::RandomCrowd, TransitionFn::None},            // 244 SCRIPT_IN_CROWD
    {AnimFn::None, TransitionFn::None},                   // 245 GO_AND_CHILLOUT_OUTSIDE_HOME
    {AnimFn::SitDown, TransitionFn::SitDown},             // 246 SIT_AND_CHILLOUT
    {AnimFn::None, TransitionFn::None},                   // 247 SCRIPT_GO_AND_MOVE_ALONG_PATH
    {AnimFn::None, TransitionFn::None},                   // 248 GO_HOME_FROM_WORSHIP
    {AnimFn::None, TransitionFn::None},                   // 249 ARRIVES_HOME_FROM_WORSHIP
    {AnimFn::None, TransitionFn::SleepInTent},            // 250 SLEEP_IN_TENT_FROM_WORSHIP
    {AnimFn::None, TransitionFn::None},                   // 251 GO_TOWARDS_TELEPORT_REACTION_QUICKLY
    {AnimFn::None, TransitionFn::None},                   // 252 GO_AND_CHILLOUT_IN_TOWN
    {AnimFn::Dance, TransitionFn::None},                  // 253 WAIT_FOR_ARTIFACT_DANCE
    {AnimFn::None, TransitionFn::None},                   // 254 BREEDER_JUST_LANDED
}};

} // namespace openblack::ecs::villager_animation
