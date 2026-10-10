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
#include <optional>
#include <span>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace openblack
{
class Camera;
}

namespace openblack::help::spirits
{
class AdvisorSpiritController;
struct AdvisorModel;
} // namespace openblack::help::spirits

namespace openblack::ecs::systems
{

/// The player's two advisors, the good one and the evil one: where they are and what they do each frame, and what the
/// renderer draws of them
class AdvisorSystemInterface
{
public:
	/// A sprite of an advisor's, lying in the plane of the screen: its halo and the smoke of its puff
	struct Sprite
	{
		glm::vec3 position {0.0f};
		float halfWidth {1.0f};
		float halfHeight {1.0f};
		/// Its turn on the screen
		float angle {0.0f};
		/// Its cell of the smoke texture, eight to a row
		uint8_t cell {0};
		uint32_t argb {0xFFFFFFFF};
	};

	/// An eye whose pupil's texture moves over its vertices: the vertices of the bone, given the scales across and
	/// down
	struct Pupil
	{
		uint32_t bone {0};
		glm::vec2 scale {0.0f};
	};

	/// What is drawn of an advisor this frame
	struct Draw
	{
		int advisor {0};
		/// Near the screen, in a view of its own over the scene, rather than in the world with the scene
		bool nearScreen {true};
		/// Its alpha, of 255
		uint8_t alpha {255};
		/// Each bone in the world
		std::vector<glm::mat4> bones;
		/// How far it is out in the world, of 255, which the land's light where it is colours it by; 0 for white
		uint8_t worldShade {0};
		/// Where the land's light is taken for it in the world
		glm::vec3 landLightPoint {0.0f};
		/// Near the screen its light is moved here, then towards the world's light by twice how far out in the world it
		/// is
		std::optional<glm::vec3> nearLight;
		float inWorld {0.0f};
		/// The pupils, once its face has been updated
		std::optional<std::array<Pupil, 2>> pupils;
		/// The pupils' texture centre and the axis their down runs along, y for the good advisor and z for the evil one
		glm::vec2 pupilCentre {0.0f};
		bool pupilDownAlongZ {false};
		/// Its halo, then its puff's smoke, in the order drawn
		std::vector<Sprite> sprites;
	};

	/// One vertex of an advisor's rainbow trail, three to a triangle
	struct TrailVertex
	{
		glm::vec3 position {0.0f};
		glm::vec2 uv {0.0f};
		uint32_t argb {0};
	};

	/// What a frame gives the advisors
	struct Frame
	{
		const Camera* camera {nullptr};
		glm::u16vec2 screen {640, 480};
		glm::ivec2 mouse {0};
		/// The frame's time, at least 1 ms
		uint32_t frameMs {1};
		/// The frame's time as the fades and the puff take it: the frame's time inside the citadel, else the game's
		/// step, at most 500 ms
		int32_t stepMs {0};
		/// The time since the computer started, for the flicker
		uint32_t tickMs {0};
		/// A script holds the cinema bars
		bool wideScreen {false};
	};

	virtual ~AdvisorSystemInterface() = default;

	/// Both advisors from their .hd files through the resource caches, both at home
	virtual void Load() = 0;
	[[nodiscard]] virtual bool IsLoaded() const = 0;
	/// Both back home, out of sight, as a new land opens
	virtual void Reset() = 0;

	/// Once a frame: the advisors move, act and are posed, and their draws are made
	virtual void Update(const Frame& frame) = 0;
	/// Once a game turn: what they point at and look at is followed
	virtual void ProcessTurn() = 0;

	/// The advisors' logic, which the scripts drive
	[[nodiscard]] virtual help::spirits::AdvisorSpiritController& GetController() = 0;
	[[nodiscard]] virtual const help::spirits::AdvisorSpiritController& GetController() const = 0;
	/// An advisor's model, nothing before it is loaded
	[[nodiscard]] virtual const help::spirits::AdvisorModel* GetModel(int advisor) const = 0;

	/// This frame's draws, the good advisor's first
	[[nodiscard]] virtual std::span<const Draw> GetDraws() const = 0;
	/// This frame's trails, as triangles
	[[nodiscard]] virtual std::span<const TrailVertex> GetTrails() const = 0;
};

} // namespace openblack::ecs::systems
