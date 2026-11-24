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
#include <span>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/WaterRings.h"
#include "Creature/LeashRope.h"
#include "ECS/Systems/CloudSystemInterface.h"
#include "ECS/Systems/MagicShieldSystemInterface.h"
#include "Enums.h"
#include "Particles/ParticleDrawFrame.h"
#include "Particles/ParticleSymbolLayout.h"

namespace openblack::graphics
{

/// What the effects of the frame draw, as the steps of the frame made it before the draw. The renderer fills it on the
/// game's thread. The passes read only this for these effects, not the world.
struct DrawnEffects
{
	/// A corner of a flat effect on the land: where it is, its place on the texture and its colour, a byte each of red,
	/// green, blue and alpha from the lowest
	struct Vertex
	{
		glm::vec3 position;
		glm::vec2 uv;
		uint32_t colour;
	};

	/// What the symbols of belief of one village centre draw in this frame
	struct TownBelief
	{
		entt::entity centre {entt::null};
		particles::draw::Frame frame;
		/// The run of the symbols in `townBeliefSymbols`, when the effect draws only symbols. Then the frame is clear.
		std::optional<uint32_t> symbolRun;
		/// The lowest and the highest corners of a box that holds every corner of the frame's sprites. It is empty when
		/// the frame has more than sprites.
		std::optional<std::pair<glm::vec3, glm::vec3>> spriteBox;
	};

	/// The glow shell over the heart of a temple that the game destroys
	struct CreedShell
	{
		particles::draw::ShellDraw shell;
		/// The centre of the heart
		glm::vec3 position;
		glm::mat4 model;
	};

	/// The collar of a leash post
	struct LeashCollar
	{
		glm::vec3 position;
		glm::mat4 model;
		float scroll;
		LeashType type;
	};

	/// What the particle effects draw in this frame
	particles::draw::Frame particles;
	/// The frames of the village centres' symbols of belief. Only the first `townBeliefCount` are of this frame. The
	/// others keep their memory for the next frames.
	std::vector<TownBelief> townBeliefs;
	size_t townBeliefCount {0};
	/// The symbols of belief of the centres whose effects draw only symbols, all in one layout, with all their sprites
	particles::draw::SymbolLayout townBeliefSymbols;
	/// The blobs of shadow under the feet of the villagers and of the animals, for the main view
	std::vector<Vertex> groundBlobs;
	/// The creatures' footprints on the land, for the main view
	std::vector<Vertex> footprints;
	/// The ropes of the creatures' leashes that the frame shows, in the order of the draw
	std::vector<leash_rope::Rope> leashes;
	std::vector<CreedShell> creedShells;
	std::vector<LeashCollar> leashCollars;
	/// The rings on the water
	std::vector<water_rings::Ring> waterRings;
	/// The clouds of the storms
	std::vector<ecs::systems::StormCloud> stormClouds;
	/// The magic shields' domes, between their last two turns, when the game has magic shields
	std::vector<ecs::systems::MagicShieldSystemInterface::DomeDraw> shieldDomes;
	bool shieldsShown {false};
	/// The ambient wind, which moves the sea's texture
	glm::vec2 ambientWind {0.0f};

	[[nodiscard]] std::span<const TownBelief> TownBeliefs() const
	{
		return std::span<const TownBelief>(townBeliefs).first(townBeliefCount);
	}
};

} // namespace openblack::graphics
