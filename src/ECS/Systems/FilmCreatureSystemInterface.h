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
#include <chrono>
#include <optional>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "Video/FilmEffects.h"

namespace openblack::ecs::systems
{

/// The player's creature in the falling spell's film (see film_creature): a copy that the film makes as it starts and
/// removes as it ends. The copy moves on with the film's time, the film's camera path moves round it, and the renderer
/// draws it over the film's picture.
class FilmCreatureSystemInterface
{
public:
	virtual ~FilmCreatureSystemInterface() = default;

	/// The film starts: makes the copy of the player's first creature, which starts its action. Nothing when the player
	/// has no creature.
	virtual void Start() = 0;
	/// Each frame of the film: the film's time in milliseconds, and the real clock's time now
	virtual void Update(int32_t filmMs, std::chrono::steady_clock::time_point now) = 0;
	/// The film ends: the copy goes
	virtual void End() = 0;
	[[nodiscard]] virtual bool IsRunning() const = 0;

	/// What the renderer draws over the film this frame: the copy, the camera's view and projection, the light, the
	/// colour that the light shades (0 to 255) and the colour added after (0 to 1). Then the stars, and the fans of the
	/// burst of light at a depth in front of the camera.
	struct Shown
	{
		entt::entity copy;
		/// Whether the film shows the copy now
		bool copyShown {false};
		glm::mat4 view {1.0f};
		glm::mat4 projection {1.0f};
		glm::vec3 light {0.0f};
		glm::vec3 colour {0.0f};
		glm::vec3 added {0.0f};
		std::vector<film_effects::StarSprite> stars;
		std::vector<std::array<film_effects::FanCorner, film_effects::k_FanCorners>> fans;
		float fanDepth {0.0f};
	};
	/// None when there is no copy
	[[nodiscard]] virtual std::optional<Shown> GetShown() const = 0;
};

} // namespace openblack::ecs::systems
