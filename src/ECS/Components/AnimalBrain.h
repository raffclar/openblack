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

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "Common/Zoomer.h"

namespace openblack::ecs::components
{

/// The fields of an animal that its per-turn AI uses (ECS/AnimalAI.h; docs/bw1-notes/animals.md). Positions and
/// steps in metres, speed and angles in the original's units.
struct AnimalBrain
{
	/// The top state and the final (destination) state, AnimalStates (0..52)
	uint8_t topState {43};
	uint8_t finalState {43};
	/// Turns since the state changed (reset when the top state is set)
	uint16_t turnsSinceStateChange {0};
	/// The speed in MapCoords per turn (6553.6 per metre; the speed state / 10)
	uint16_t speed {0};
	/// The heading, 2048 per circle; the step goes along (COS[a], SIN[a]) in x, z
	uint16_t angle {0};
	/// The per-turn step in MapCoords (WANDER's straight line)
	glm::ivec2 step {0};
	/// MOVE_TO_POS's goal (metres)
	glm::vec2 goal {0.0f};
	/// The wall-hug move state (1 ARRIVED, 4 FINAL_STEP, 5 WANDER, 0xB STEP_THROUGH, 0xC..0x12 the circle hug of
	/// ECS/AnimalWallHug.h)
	uint8_t moveState {0};
	/// The collide circle it walks round (a copy of the collide object: centre and radius, the fixed object it belongs
	/// to, none for a water cell's; set = it has one), the turns until it reaches it (0xFF none) and the distance to
	/// the goal when the orbit began (x 128)
	struct HugCircle
	{
		glm::vec2 centre {0.0f};
		float radius {0.0f};
		entt::entity owner {entt::null};
		bool set {false};
	};
	HugCircle hugCircle;
	uint8_t turnsToObj {0xFF};
	uint32_t hugGoalDistance {0};
	/// The eat counter; also the corpse counter (600 turns to die over) in DEAD
	int16_t counter {0};
	/// The hunger, sleep and breed counters (the needs processing)
	int16_t hunger {0};
	int16_t sleep {0};
	int16_t breed {0};
	/// The sleep place, the flock's domain centre cell at creation; (0, 0) = none
	glm::u16vec2 sleepCell {0};
	/// Bit 0 dying / dead, bits 4-5 the landType of the last landing (end of physics), 0x80 downed by a predator
	uint16_t status {0};
	/// The hunting target (a predator's prey)
	entt::entity target {entt::null};
	/// The cell (centre) of a prey seen, (0, 0) = none
	glm::vec2 preyCell {0.0f};
	/// The game turn the chase started (chaseTime)
	uint32_t chaseStart {0};
	/// What it eats (a predator's downed prey)
	entt::entity foodTarget {entt::null};
	/// The object of its reaction (the predator, the food, the thrown object); the reaction (an id of
	/// ECS/Effects/Reactions' list, 0 none); the state to go back to (the stored previous state: its final state)
	entt::entity predator {entt::null};
	uint32_t reaction {0};
	uint8_t previousState {0};
	/// The reaction records are the Living's components::ReactionRecords (ECS/Effects/Reactions)
	/// The MapCoords altitude, metres above the land (the birds; 0 on the ground) and the goal's
	float altitude {0.0f};
	float goalAltitude {0.0f};
	/// The bank zoomer (roll in radians; a dove's drawn matrix is rolled by it)
	Zoomer bank;
	/// The SpellWolf's final destination; set by the spell
	glm::vec2 finalDestination {0.0f};
	/// The turn it was born (age = (turn - it) / 1500; there is no stored age). Negative for the
	/// animals a map creates already grown (SetAge at turn 0).
	int32_t birthTurn {0};
	/// the ground covered in the last turn (while it moves, the clip advances by distance)
	float movedLastTurn {0.0f};
};

/// A villager downed by a predator (status 0x80, life 0.05): DOWNED, then BEING_EATEN for 300 turns, then dead; the
/// animal AI drives it (ECS/AnimalPredators.cpp)
struct DownedVillager
{
	int16_t counter {0};
};

} // namespace openblack::ecs::components
