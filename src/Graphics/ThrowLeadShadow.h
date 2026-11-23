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

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "Graphics/GroundShadow.h"

/// The shadow of a thing that the player threw, while the thing goes along the lead of the throw (from the let-go to
/// the turn of the throw). The thing casts this shadow in addition to the shadow of the hand.
///
/// Each frame of the lead makes the shadow again from the drawn thing. When the lead ends, the shadow goes away. The
/// game keeps the shadow of the last frame of the lead on the land until the next throw. openblack does not keep it
/// (deviations, signed off by the user).
namespace openblack::graphics::throw_lead_shadow
{

/// The cast on a frame of the lead: from a light straight above the drawn origin of the thing, flattened onto the
/// height of that origin, and laid on the land at the height under the origin
[[nodiscard]] GroundShadow::Cast LeadCast(const glm::vec3& origin, float ground);

/// The model of the thrown thing in the shadow of the hand. The hand makes its shadow before the frame puts the thing
/// on the lead. Thus on the lead, the hand's shadow takes the thing where the frame before drew it. On the first frame of
/// the lead, that is where the hand held it. Off the lead, the hand puts the thing first and takes it where it is now.
[[nodiscard]] glm::mat4 HandShadowModel(const glm::mat4& drawnNow, bool onLead, const std::optional<glm::mat4>& drawnBefore);

/// What the shadow does on a frame
enum class Step
{
	/// The thing is not on the lead, or it is too far from the camera: nothing shows
	None,
	/// The thing is on the lead: the frame makes the shadow again
	Lead,
};

/// The step of a frame. On a frame of the lead, `alpha` is the fade of the shadow; a fade of 0 shows nothing.
[[nodiscard]] Step StepOf(bool onLead, uint8_t alpha);

} // namespace openblack::graphics::throw_lead_shadow
