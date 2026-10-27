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

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::computer_hand
{

/// What the hand reads and changes in the game when the god charges a miracle, draws its gesture and casts it
class CastsWorld
{
public:
	virtual ~CastsWorld() = default;
	/// The icon belongs to a worship site
	[[nodiscard]] virtual bool IconHasSite(entt::entity icon) const = 0;
	/// The worshippers chant at the icon now
	[[nodiscard]] virtual bool IconCharging(entt::entity icon) const = 0;
	/// The worshippers start to chant for the miracle at the icon. False when the icon cannot start.
	virtual bool StartChant(entt::entity icon, MagicType type) = 0;
	/// The prayer power that the miracle of the icon still needs
	[[nodiscard]] virtual float ChantNeeded(entt::entity icon) const = 0;
	/// The miracle is felt as an angry one
	[[nodiscard]] virtual bool Angry(MagicType type) const = 0;
	/// The god casts the miracle of the icon at the thing, from the hand. False when the miracle is not made.
	virtual bool Cast(MagicType type, entt::entity target, entt::entity icon, glm::vec3 hand) = 0;
	/// The seconds that the god keeps its hand over a miracle that it cast
	[[nodiscard]] virtual float CastSeconds(MagicType type) const = 0;
	/// The gesture of the seed of a miracle: for stage 0 the gesture that opens the choice of miracles, for stage 1 the
	/// gesture of the miracle. 0 when the miracle has no seed.
	[[nodiscard]] virtual uint32_t SeedGesture(MagicType type, int32_t stage) const = 0;
	/// The length of the shape of a gesture, in a square of side 1. 0 when the gesture has no shape.
	[[nodiscard]] virtual float GestureLength(uint32_t gesture) const = 0;
	/// The point of the shape of a gesture at a share of its length, from 0 to 1, in a square of side 1
	[[nodiscard]] virtual glm::vec3 GesturePoint(uint32_t gesture, float share) const = 0;
	/// The shape of the gesture shows on the land, as a recognised gesture shows. The square is so many metres across,
	/// from a corner.
	virtual void ShowGesture(uint32_t gesture, glm::vec3 corner, float size) = 0;
};

} // namespace openblack::computer_hand
