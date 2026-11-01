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

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <CreatureRecord.h>
#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"
#include "Particles/ParticleImport.h"

namespace openblack::gamesave
{
class VanillaWorld;
} // namespace openblack::gamesave

// The particle effects of a saved game of the game: the particle containers, which are the effects that the scripts
// and the temple heart start, and the effect of a vortex that draws things in. Each effect keeps its whole running
// state, which the import gives to a new effect of the same file. The rules here use no service of the game, so that
// tests can give them a world that they make.
namespace openblack::ecs::vanilla_particle_import
{

/// The entity that the import made for an object of the file, none when it made none
using EntityOf = std::function<entt::entity(uint32_t number)>;

/// The game list of the particle containers, the newest first
inline constexpr size_t k_ParticleContainerList = 7;

/// A particle system of the file with its state
struct ImportedEffect
{
	ParticleType type {ParticleType::None};
	/// Its random numbers come from the seed that every computer shares
	bool synced {true};
	particles::RestoredEffect state;
};

/// A particle container of the file
struct ImportedContainer
{
	uint32_t number {0};
	gamesave::MapCoords position {};
	/// The effect draws all its sprites together at its origin
	bool singleSort {false};
	/// The thing that the effect follows and ends with, 0 for none
	uint32_t follows {0};
	/// The turns left, below 0 for ever
	int32_t turns {-1};
	/// The strength that each step gives the effect
	float scale {1.0f};
	ImportedEffect effect;
};

/// The particle system with this number, or the reason why the import cannot take it
struct ReadResult
{
	std::optional<ImportedEffect> effect;
	std::string why;
};
[[nodiscard]] ReadResult ReadEffect(const gamesave::VanillaWorld& world, uint32_t system, const EntityOf& entityOf);

/// The particle containers of the file, the oldest first, as the game steps them. A container that the import cannot
/// take goes to the skipped list with the reason.
struct Containers
{
	std::vector<ImportedContainer> containers;
	std::vector<std::string> skipped;
};
[[nodiscard]] Containers ReadContainers(const gamesave::VanillaWorld& world, const EntityOf& entityOf);

/// The particle container with this number, or the reason why the import cannot take it
struct ContainerResult
{
	std::optional<ImportedContainer> container;
	std::string why;
};
[[nodiscard]] ContainerResult ReadContainer(const gamesave::VanillaWorld& world, uint32_t number, const EntityOf& entityOf);

/// A vortex of the file that draws things in
struct ImportedVortexIn
{
	uint32_t number {0};
	glm::vec3 centre {0.0f};
	VortexStateType state {VortexStateType::Active};
	float altitude {0.0f};
	float groundAverage {0.0f};
	std::vector<uint8_t> groundHeights;
	/// The text that the vortex wrote to the crossing file, none when it writes no crossing file
	std::optional<std::vector<uint8_t>> crossing;
	/// The effect that draws things in, none when the vortex has none
	std::optional<ImportedEffect> mover;
};

/// The vortices of the file that draw things in. A vortex that the import cannot take goes to the skipped list.
struct VorticesIn
{
	std::vector<ImportedVortexIn> vortices;
	std::vector<std::string> skipped;
};
[[nodiscard]] VorticesIn ReadVorticesIn(const gamesave::VanillaWorld& world, const EntityOf& entityOf);

} // namespace openblack::ecs::vanilla_particle_import
