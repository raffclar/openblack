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

#include <glm/vec3.hpp>

#include "Graphics/GroundShadow.h"

/// The shadow of a thing that the player threw, while the thing goes along the lead of the throw (from the let-go to
/// the turn of the throw). The thing casts this shadow in addition to the shadow of the hand.
///
/// The game has one shadow of this kind. Each frame of the lead makes it again from the drawn thing. After the lead,
/// the game does not make it again and does not remove it. Thus the land keeps the shadow of the last frame of the lead
/// until the next throw (vanilla-bugs, kept).
namespace openblack::graphics::throw_lead_shadow
{

/// The cast on a frame of the lead: from a light straight above the drawn origin of the thing, flattened onto the
/// height of that origin, and laid on the land at the height under the origin
[[nodiscard]] GroundShadow::Cast LeadCast(const glm::vec3& origin, float ground);

/// The cast of the shadow that stays after the lead. The light and the level stay as on the last frame of the lead.
/// The land height comes from the place of the thing now. When the thing is gone, the land height stays as it was.
[[nodiscard]] GroundShadow::Cast KeptCast(const GroundShadow::Cast& last, std::optional<float> groundUnderThing);

/// What the shadow does on a frame
enum class Step
{
	/// No throw made the shadow yet, or the last frame of the lead was too far from the camera: nothing shows
	None,
	/// The thing is on the lead: the frame makes the shadow again and keeps it
	Lead,
	/// The thing is not on the lead: the kept shadow shows
	Kept,
};

/// The step of a frame. On a frame of the lead, `alpha` is the fade of the shadow; a fade of 0 removes the kept shadow.
[[nodiscard]] Step StepOf(bool onLead, uint8_t alpha, bool hasKept);

} // namespace openblack::graphics::throw_lead_shadow
