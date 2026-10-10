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
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

/// A scroll or sign a script puts up at a place to mark a challenge or give a tip: it spins where it stands, glints in its
/// colour, and is tapped by the hand. A script holds it as one of its things.
struct ScriptHighlight
{
	/// Its row of the highlights' info table: the scroll's kind
	HighlightInfo kind {HighlightInfo::Scroll};
	/// The challenge it belongs to, or the tip's text once a script gives it one
	uint32_t scriptId {0};
	/// The tip's category, as the temple keeps the tips read
	uint32_t category {0};
	/// Started: it shows its active model and effect
	bool active {false};
	/// The height above the land a script set for it; otherwise it stands on what is under it
	std::optional<float> drawHeight;
	/// How high above the land it stands now
	float heightAbove {0.0f};
	/// Which way it turns, in radians, as it was last drawn
	float yAngle {0.0f};
	/// Its glints and its active effect in the particle system, 0 for none
	uint32_t glints {0};
	uint32_t activeEffect {0};
	/// The thing its glow is drawn as, none until it first shows
	entt::entity glow {entt::null};
	/// Where its model's middle was last drawn, and how far its model reaches from there
	glm::vec3 centre {0.0f};
	float radius {0.0f};
};

/// The glow drawn before a highlight, towards the camera; it goes when its highlight has gone
struct ScriptHighlightGlow
{
	entt::entity highlight {entt::null};
};

} // namespace openblack::ecs::components
