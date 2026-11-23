/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ThrowLeadShadow.h"

#include "Graphics/ShadowTexture.h"

namespace openblack::graphics
{

GroundShadow::Cast throw_lead_shadow::LeadCast(const glm::vec3& origin, float ground)
{
	return {.light = shadow_texture::LightAbove(origin), .level = origin.y, .ground = ground};
}

glm::mat4 throw_lead_shadow::HandShadowModel(const glm::mat4& drawnNow, bool onLead,
                                             const std::optional<glm::mat4>& drawnBefore)
{
	return onLead ? drawnBefore.value_or(drawnNow) : drawnNow;
}

throw_lead_shadow::Step throw_lead_shadow::StepOf(bool onLead, uint8_t alpha)
{
	return onLead && alpha != 0 ? Step::Lead : Step::None;
}

} // namespace openblack::graphics
