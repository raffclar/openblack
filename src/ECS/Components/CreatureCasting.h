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

#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

/// A creature going about casting a miracle: how it goes near, gets away from or turns to face what it casts at, and a
/// try at a miracle that fizzled, which it shows its embarrassment at once it chooses what to do next
struct CreatureCasting
{
	enum class Move : uint8_t
	{
		None,
		GoNear,
		GetAway,
		TurnToFace,
		/// Turning so that its back is to the target
		TurnAway,
		/// Going to where it can throw into the target from
		ToThrowPosition,
		/// Going to where it can throw at a place from
		ToThrowAtPoint,
		/// Turning to face the camera of its player, where the camera is on each turn
		TurnToCamera,
		/// Turning to face the camera of the player that last gave it a thing, where the camera is on each turn
		TurnToGiverCamera,
	};
	enum class Outcome : uint8_t
	{
		Running,
		Done,
		Failed,
	};
	Move move {Move::None};
	Outcome outcome {Outcome::Running};
	entt::entity target {entt::null};
	/// The place that a move to throw at a place goes for
	glm::vec2 point {0.0f};
	/// The distance it keeps, and the turns it holds still once facing
	float keep {0.0f};
	float settleSeconds {0.0f};
	/// The move asks for the run speed. Else it keeps the speed that the creature got last.
	bool run {false};
	/// The stage of the move, the turns it has gone on, and the turns it has yet to hold still
	uint8_t phase {0};
	uint32_t turns {0};
	uint32_t holdTurns {0};
	/// Where it stood when it last looked whether it is stuck, and where it last set off for
	glm::vec2 stuckCheck {0.0f};
	std::optional<glm::vec2> destination;
	/// The agenda and its step that started the move. The move is part of that step only: when the creature goes on to a
	/// different step or agenda, the move stops.
	uint32_t agendaSerial {0};
	uint32_t agendaStep {0};

	/// A try at a miracle that fizzled: what at, and which miracle
	struct Fizzle
	{
		entt::entity target;
		MagicType magicType;
	};
	std::optional<Fizzle> fizzle;
	/// The thing of the last try at a thing that fizzled. When a try at a place fizzles, the creature shows its feelings
	/// about this thing.
	entt::entity lastFizzleTarget {entt::null};
};

} // namespace openblack::ecs::components
