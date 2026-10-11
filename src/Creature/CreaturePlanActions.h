/******************************************************************************
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

#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureIdleMind.h"

/// The actions of the game's table that a creature can carry out here, and the agenda of steps each becomes. The planner
/// only weighs these; the rest (miracles, building, dancing and so on) wait for the systems they need.
namespace openblack::creature_plan_actions
{
/// The kind of thing an action is done to
enum class Target : uint8_t
{
	/// Nothing: done on the spot
	None,
	/// Anything it can eat, or only living things it can eat
	Food,
	LiveFood,
	/// Anything it can pick up
	Pickable,
	/// Anything it can knock down, or only trees
	Destroyable,
	Tree,
	/// Villagers, villagers hurt enough to be healed, other creatures, or either
	Villager,
	HurtVillager,
	Creature,
	Living,
	/// What frightens creatures: other creatures, bats, vultures and lions, and miracles
	Frightening,
	/// Anything at all it can see
	Anything,
	/// Anything on fire, or anything not on fire
	Burning,
	Unburnt,
	/// A storage pit
	StoragePit,
};
/// How many kinds of target there are
constexpr size_t k_TargetCount = static_cast<size_t>(Target::StoragePit) + 1;

/// How the agenda is made
enum class Build : uint8_t
{
	Eat,
	Sleep,
	Poo,
	Puke,
	Drink,
	ExamineByPickingUp,
	ExamineByLooking,
	ExamineByFollowing,
	ThrowAbout,
	Hurl,
	Destroy,
	SitDown,
	BeIdle,
	HangAround,
	ShowDesire,
	/// An action on the spot, or facing the player, or at what it goes up to
	Emote,
	FaceCameraEmote,
	ApproachEmote,
	RunFromObject,
	RunFromPlayer,
	LookAbout,
	/// Casting a miracle at what it is done to: a lightning bolt, a helpful miracle, a spell on another creature
	CastLightning,
	CastHelpful,
	CastPlayful,
	/// Going to a fish farm's shoal, bringing food out of the sea there and eating it
	FishAndEat,
	/// Bringing food out of the sea, unless it has some in its hand, and throwing it into a storage pit, or putting it
	/// down at its home
	GiveFishToStore,
	TakeFishHome,
	/// Casting the water miracle at a burning thing
	CastWater,
	/// Setting a thing alight by tossing a burning thing at it
	SetFire,
	/// The actions scripts force to stage scenes: looking at a thing for good, or a little without going up to it;
	/// looking at the camera; pointing at a thing or at the camera
	LookForever,
	LookButDontApproach,
	LookAtCamera,
	PointAtThing,
	PointAtCamera,
	/// Hurling, sleeping at home, smiling or waving at someone, being frightened on the spot, putting down what it
	/// holds and lying dead for good, as the game builds them whoever chose them
	SleepAtHome,
	SmileAt,
	WaveAt,
	BeFrightened,
	PutDown,
	DeadForever,
	/// Pointing out a lesson's highlight to the player
	PointOutHighlight,
	/// An action whose agenda can never be made
	Never,
};

struct Executor
{
	/// The action's name in the game's table
	std::string_view action;
	Target target {Target::None};
	Build build {Build::Emote};
	/// The animation played, for the builds that play one
	size_t animation {0};
	creature_mind::Activity activity {creature_mind::Activity::Planned};
};

/// Every action that can be carried out
[[nodiscard]] std::span<const Executor> All();
[[nodiscard]] const Executor* For(std::string_view action);

/// What the agenda needs to know of where the creature is
struct Situation
{
	/// Where the player looks from, the nearest water's edge and the water, somewhere to hurl things at, and the action
	/// that shows its strongest desire
	std::optional<glm::vec2> camera;
	std::optional<creature_mind::Wants::WaterSpot> water;
	std::optional<glm::vec2> hurlTarget;
	std::optional<size_t> showDesireAnimation;
	/// Fishing at the nearest fish farm: its shoal (the farm itself without one), where the creature would stand to fish
	/// for itself (the shoal, or the nearest place it can stand near enough to it; none for none), how near it goes,
	/// whether its hand must be emptied first, whether it already has food in its hand, and its height. None with no
	/// farm near.
	struct Fishing
	{
		glm::vec2 shoal {0.0f};
		std::optional<glm::vec2> standAt;
		float arriveWithin {0.0f};
		bool putDownFirst {false};
		bool holdingFood {false};
		float height {0.0f};
	};
	std::optional<Fishing> fishing;
	/// Where its home is, none for a creature without one
	std::optional<glm::vec2> home;
	/// The thing the action uses, by its entity's number, as a burning thing to set something alight with; and whether
	/// its hand holds something already
	std::optional<uint32_t> instrument;
	bool handFull {false};
	/// Where the camera is, whether the creature has a player (whose camera it can look to), whether a script controls
	/// it, its radius on the ground, and a chance from 0 to 1 for what varies
	std::optional<glm::vec3> eye;
	bool hasPlayer {false};
	bool controlledByScript {false};
	float radius {0.0f};
	/// Its height, and the height of the thing the action is done to
	float height {0.0f};
	float thingHeight {0.0f};
	/// How far the camera is from it
	float eyeDistance {0.0f};
	float chance {0.0f};
};
/// What a casting action casts, from its row of the game's table and the creature
struct CastInfo
{
	uint32_t magicType {0};
	/// The gesture drawn before casting, 0 for none
	uint32_t gesture {0};
	/// The creature's height
	float height {0.0f};
};
/// Whether an action casts a miracle
[[nodiscard]] bool IsCast(const Executor& executor);

/// Whether an action can be carried out now, in this situation, before any thing is chosen
[[nodiscard]] bool Possible(const Executor& executor, const Situation& situation);
/// The agenda for an action on a thing (by its entity number, and where it is), if it can be done
[[nodiscard]] std::optional<std::vector<creature_mind::Step>> Agenda(const Executor& executor, std::optional<uint32_t> object,
                                                                     glm::vec2 objectPoint, const Situation& situation,
                                                                     const creature_mind::Random& random,
                                                                     const std::optional<CastInfo>& cast = std::nullopt);

} // namespace openblack::creature_plan_actions
