/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <vector>

#include <entt/entity/entity.hpp>

/// Which glows of the disciples a frame draws. The frame draws the glow of a disciple when it draws the body of the
/// disciple, and it draws a limited number of glows in a frame.
namespace openblack::ecs::disciple_glow_draw
{

/// The most glows that one frame draws. The frame does not draw the glows after these in its order, and not their
/// symbols.
constexpr size_t k_MostGlows = 1024;

/// What a frame knows of a villager that glows
struct Body
{
	/// A script draws it in high detail. The frame then draws the high-detail body, which has no glow.
	bool highDetail {false};
	/// The land's draw list lets the frame draw it, or the creature that holds it
	bool drawn {true};
};

/// Whether the frame draws the glow of a villager that glows, before the limit of the glows
[[nodiscard]] constexpr bool Shown(const Body& body)
{
	return !body.highDetail && body.drawn;
}

/// The parts of a frame that draw the bodies of the villagers, in the order of the frame
enum class Draw : uint8_t
{
	/// The land's draw list, thing by thing. A villager in the hand of a creature comes with the creature.
	LandList,
	/// The things that scripts walk along tracks
	WalkPath,
	/// The things that move in the physics, and other things that are not in the map
	Physics,
	/// The things in the hands. The frame sorts them with the other sorted things at its end, the farthest first.
	Hand,
};

/// A glow that the frame can draw
struct Glow
{
	entt::entity villager {entt::null};
	Draw draw {Draw::LandList};
	/// The place in the land's draw list, for the land's draw list
	uint32_t place {0};
	/// The sort key of the body, its distance from the camera squared, for the hands
	float key {0.0f};
	/// The sphere of the body is on the screen. The frame does not draw the body or the glow of a body off the screen,
	/// and such a glow does not count.
	bool onScreen {true};
};

/// The villagers whose glows the frame draws: the first `most` glows on the screen, in the order of the frame. Of the
/// glows in one part of the frame with the same place and key, the first in `glows` comes first.
[[nodiscard]] inline std::vector<entt::entity> Kept(std::vector<Glow> glows, size_t most = k_MostGlows)
{
	std::erase_if(glows, [](const Glow& glow) { return !glow.onScreen; });
	std::ranges::stable_sort(glows, [](const Glow& a, const Glow& b) {
		if (a.draw != b.draw)
		{
			return a.draw < b.draw;
		}
		if (a.draw == Draw::Hand)
		{
			return a.key > b.key;
		}
		return a.place < b.place;
	});
	std::vector<entt::entity> kept;
	kept.reserve(std::min(glows.size(), most));
	for (const auto& glow : glows)
	{
		if (kept.size() >= most)
		{
			break;
		}
		kept.push_back(glow.villager);
	}
	return kept;
}

} // namespace openblack::ecs::disciple_glow_draw
