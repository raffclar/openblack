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
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <InspectorProvider.h>
#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "EntityDescription.h"

namespace openblack::creature_desires
{
struct Desires;
}
namespace openblack::creature_mind_tables
{
struct Tables;
}
namespace openblack::ecs
{
class MapInterface;
}
namespace openblack::ecs::components
{
struct CreatureMindState;
}

/// Named, small queries of the game's main systems. Each provider reads its system through sources, functions that give
/// what it needs (none between lands), so that tests give it fakes and the game gives it the locator's systems.
namespace openblack::inspector
{

/// The registry and the info tables, as most providers read them
struct WorldSources
{
	std::function<const ecs::Registry*()> registry;
	std::function<const InfoConstants*()> info;
};

// The physics

/// A body of the physics as the inspector reports it
struct BodyInfo
{
	entt::entity entity {entt::null};
	glm::vec3 centre {0.0f};
	glm::vec3 velocity {0.0f};
	float speed {0.0f};
	float mass {0.0f};
	float radius {0.0f};
	bool resting {false};
	bool inWater {false};
	/// What it is to the physics' bookkeeping: "other", "villager", "felled_tree", "felled_tree_toppled"
	std::string kind;
	uint8_t flags {0};
	std::optional<int> player;
	entt::entity thrower {entt::null};
	float impact {0.0f};
	int contacts {0};
};

[[nodiscard]] Json BodyItem(const BodyInfo& body);

struct PhysicsSources
{
	/// Every body; none without the physics
	std::function<std::optional<std::vector<BodyInfo>>()> bodies;
};

///   physics.state                      how many bodies, flying, resting and in water
///   physics.body   {id}                one object's body
///   physics.bodies near, radius        the bodies about a point, nearest first
[[nodiscard]] std::unique_ptr<ProviderInterface> MakePhysicsProvider(PhysicsSources sources);

// The creatures' minds

/// A creature's strongest activated desires, strongest first: name, value, its most and whether it is held back
[[nodiscard]] Json DesireItems(const creature_desires::Desires& desires, size_t top);
/// What a creature is doing: its plan (desire, action, object, priority), its activity and its agenda's steps
[[nodiscard]] Json PlanJson(const ecs::components::CreatureMindState& mind, const creature_mind_tables::Tables* tables);

struct CreatureSources
{
	WorldSources world;
	std::function<const creature_mind_tables::Tables*()> tables;
};

///   creature.list                       the creatures, what each does and its strongest desire
///   creature.desires {id?, top?}        a creature's strongest desires
///   creature.plan    {id?}              a creature's plan and agenda
[[nodiscard]] std::unique_ptr<ProviderInterface> MakeCreatureProvider(CreatureSources sources);

// The map's cells

struct MapSources
{
	WorldSources world;
	std::function<const ecs::MapInterface*()> map;
};

///   map.cell  {position | cell}         what stands in a cell, as a search meets it
[[nodiscard]] std::unique_ptr<ProviderInterface> MakeMapProvider(MapSources sources);

// The towns

///   town.list                           the towns: owner, buildings, people without a home
///   town.homes    {id}                  a town's buildings and who lives in each
///   town.homeless {id}                  a town's people without a home
[[nodiscard]] std::unique_ptr<ProviderInterface> MakeTownProvider(WorldSources sources);

// The players' influence

/// A player's influence at a point: with what the hand keeps past the border, at the hand's own place, and its own
struct InfluenceAt
{
	float influence {0.0f};
	float handPoint {0.0f};
	float raw {0.0f};
};

struct InfluenceSources
{
	/// Where the player's hand is, none without one
	std::function<std::optional<glm::vec3>(int player)> hand;
	/// The player's influence at a point, none without the influence system
	std::function<std::optional<InfluenceAt>(int player, glm::vec3 point)> at;
	/// Whether the player's border is shown
	std::function<bool(int player)> borderShown;
	/// How many circles of influence there are
	std::function<size_t()> circles;
};

///   influence.hand {player?}             the hand's share: the player's influence where the hand is
///   influence.at   {position, player?}   the player's influence at a point
[[nodiscard]] std::unique_ptr<ProviderInterface> MakeInfluenceProvider(InfluenceSources sources);

// The camera, read only

struct CameraState
{
	glm::vec3 origin {0.0f};
	glm::vec3 focus {0.0f};
	/// Its angles in degrees
	glm::vec3 rotation {0.0f};
	glm::vec3 forward {0.0f};
	float horizontalFieldOfView {0.0f};
	float nearClip {0.0f};
};

///   camera.state                         where the camera is, what it looks at and its angles
[[nodiscard]] std::unique_ptr<ProviderInterface> MakeCameraProvider(std::function<std::optional<CameraState>()> camera);

// The sounds

struct AudioState
{
	float globalVolume {0.0f};
	float sfxVolume {0.0f};
	float musicVolume {0.0f};
	bool musicActive {false};
};

struct AudioSources
{
	WorldSources world;
	std::function<std::optional<AudioState>()> state;
};

///   audio.state                          the volumes, the music and how many sounds play
///   audio.sounds near, radius            the sounds playing about a point, nearest first
[[nodiscard]] std::unique_ptr<ProviderInterface> MakeAudioProvider(AudioSources sources);

} // namespace openblack::inspector
