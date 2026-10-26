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
#include <string_view>
#include <utility>
#include <vector>

#include <InspectorProvider.h>
#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureDecisionTree.h"
#include "ECS/Systems/FixedOutlineSystemInterface.h"
#include "ECS/Systems/LandShadeSystemInterface.h"
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
/// The sources of the desire of that name, as the desires query writes them out
[[nodiscard]] Json DesireSources(const creature_desires::Desires& desires, std::string_view name);
/// What a creature is doing: its plan (desire, action, object, priority), its activity and its agenda's steps
[[nodiscard]] Json PlanJson(const ecs::components::CreatureMindState& mind, const creature_mind_tables::Tables* tables);

struct CreatureSources
{
	WorldSources world;
	std::function<const creature_mind_tables::Tables*()> tables;
	/// What a creature believes about a thing now, with the attributes that its decision trees test
	std::function<std::optional<creature_tree::Belief>(entt::entity creature, entt::entity thing)> beliefAbout;
	/// A debug tool: the creature starts or stops to take notice of reactions; none when the inspector may not set it
	std::function<void(entt::entity creature, bool notice)> setReactionNotice;
	/// Whether a cell is clear for a creature to go to, and the nearest clear area so wide about a point
	std::function<bool(entt::entity creature, int32_t x, int32_t z, bool livingBlocks)> cellClear;
	std::function<std::optional<glm::vec2>(entt::entity creature, glm::vec2 point, float width, bool livingBlocks)> clearArea;
};

///   creature.list                       the creatures, what each does and its strongest desire
///   creature.desires {id?, top?}        a creature's strongest desires
///   creature.beliefs {id?, kind?}       a creature's beliefs about things and its attitudes to creatures
///   creature.plan    {id?}              a creature's plan and agenda
///   creature.reactions {id?}            whether a creature takes notice of reactions
///   creature.set_reactions {id?, notice} a debug tool: the creature starts or stops to take notice of reactions
///   creature.clear_area {id?, position, width, living?}  the cells about a point that are not clear for the creature,
///                                       and the nearest clear area so wide
[[nodiscard]] std::unique_ptr<ProviderInterface> MakeCreatureProvider(CreatureSources sources);

// The map's cells

struct MapSources
{
	WorldSources world;
	std::function<const ecs::MapInterface*()> map;
	std::function<const ecs::systems::LandShadeSystemInterface*()> shade;
	std::function<const ecs::systems::FixedOutlineSystemInterface*()> outlines;
};

///   map.cell  {position | cell}         what stands in a cell, as a search meets it
///   map.free  {position}              whether a place is free of the fixed things, and the outlines in its cell
///   map.shade [position]                the land's shading of the last frame: shadows, lights, the blocks they
///                                       changed, and the shaded luminosity of a cell's corners
[[nodiscard]] std::unique_ptr<ProviderInterface> MakeMapProvider(MapSources sources);

// The towns

///   town.list                           the towns: owner, buildings, people without a home
///   town.homes    {id}                  a town's buildings and who lives in each
///   town.homeless {id}                  a town's people without a home
///   town.belief   {id}                  a town's owner and its belief in each player
///   town.spells   {id}                  a town's miracles, its centre's stone hands and its site's icons
[[nodiscard]] std::unique_ptr<ProviderInterface> MakeTownProvider(WorldSources sources);

// The worship sites

///   worship.sites                       the worship sites: temple, player, tribe, place, towns, built, prayer power
[[nodiscard]] std::unique_ptr<ProviderInterface> MakeWorshipProvider(WorldSources sources);

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

// The sounds

struct AudioState
{
	float globalVolume {0.0f};
	float sfxVolume {0.0f};
	float musicVolume {0.0f};
	bool musicActive {false};
	/// The game's music: what is heard, the land's, the one a script started, by their names, and whether the land's
	/// music is on; empty names for none
	std::string musicPlaying;
	std::string landMusic;
	std::string scriptMusic;
	/// The world's clock of the script's music: the last line cue, the word of that line, and the time since its
	/// channel started in milliseconds (none when no channel plays)
	int32_t scriptMusicLine {0};
	int32_t scriptMusicWord {0};
	std::optional<int64_t> scriptMusicMs;
	bool alignmentMusic {false};
	/// Music scripts have attached to objects, the most recently attached first: the music's name and the object
	std::vector<std::pair<std::string, uint32_t>> attachedMusic;
	/// The music channels playing: their bank, volume out of 127, and where a positional one is heard from
	struct MusicChannel
	{
		std::string bank;
		int32_t volume {0};
		std::optional<glm::vec3> position;
	};
	std::vector<MusicChannel> musicChannels;
	/// Where the placed sounds are heard from: the camera, or the inspector's view shown over it while it is shown
	glm::vec3 listener {0.0f};
};

struct AudioSources
{
	WorldSources world;
	std::function<std::optional<AudioState>()> state;
	/// For tools: the game acts as if the machine lost the sound device (true), or has it again (false)
	std::function<void(bool lost)> simulateDeviceLoss;
	/// The machine lost the sound device, or a tool acts as if it did
	std::function<bool()> deviceLost;
	/// The entities of the sounds that play, which are the audio's and not the world's; none with no sound
	std::function<const ecs::Registry*()> emitters;
};

///   audio.state                          the volumes, the music, how many sounds play and where they're heard from
///   audio.sounds near, radius            the sounds playing about a point, nearest first
///   audio.lose_device lost               (writes) act as if the machine lost the sound device, or has it again
[[nodiscard]] std::unique_ptr<ProviderInterface> MakeAudioProvider(AudioSources sources);

} // namespace openblack::inspector
