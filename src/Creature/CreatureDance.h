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

#include <functional>
#include <optional>
#include <span>
#include <vector>

#include <glm/vec2.hpp>

#include "Creature/CreatureIdleMind.h"
#include "Enums.h"

/// A creature dances with the villagers of a town to impress them. It goes to a villager and summons the people of
/// the villager's town. Then it makes a dance in a clear place, sends the people to their places and dances with them.
/// While they dance, the people worship it, which gives the town belief in the creature's player.
namespace openblack::creature_dance
{

/// The row of the dance table that a creature dances to impress villagers
constexpr uint32_t k_ImpressDanceType = 12;
/// A creature dances to impress again only this many turns after the last such dance started
constexpr uint32_t k_TurnsBetweenImpressDances = 200;
/// The dance needs a clear place this many times as wide as the creature, and never less wide than k_SmallestArea
constexpr float k_AreaRadiusShare = 1.5f;
constexpr float k_SmallestArea = 30.0f;
/// The creature goes near the villager to this many times its height, then faces it for this many seconds
constexpr float k_GoNearHeights = 5.0f;
constexpr float k_FaceVillagerSeconds = 0.1f;
/// The animation it plays to summon the people, and the seconds it points at the place of the dance
constexpr size_t k_SummonAnimation = 52;
constexpr float k_PointSeconds = 1.0f;
/// The creature dances five turns for each beat of the dance's length
constexpr uint32_t k_TurnsPerBeat = 5;
/// A creature dances only with a villager nearer to it than this, in metres
constexpr float k_FurthestVillager = 200.0f;
/// The most villagers that the creature calls, before it counts an even number
constexpr uint32_t k_MostDancers = 12;
/// The creature dances in the groups of its dance that take this kind of dancer, of either sex
constexpr uint32_t k_CreatureDanceType = 1;
/// It walks back to its place in the dance when it is further than this from it, and stops this near
constexpr float k_FarFromPlace = 0.2f;
constexpr float k_NearPlace = 0.1f;

/// The rows of the dance table for the other dances of a creature with villagers: a playful dance with them, a story,
/// a game, a playful dance that they watch, and a dance of love
constexpr uint32_t k_PlayfulDanceType = 1;
constexpr uint32_t k_StoryDanceType = 9;
constexpr uint32_t k_GameDanceType = 10;
constexpr uint32_t k_WatchedDanceType = 11;
constexpr uint32_t k_AmorousDanceType = 13;
/// A creature does the playful dance with villagers again only this many turns after it last planned one, and a
/// story only this many turns after the last story
constexpr uint32_t k_TurnsBetweenPlayfulDances = 300;
constexpr uint32_t k_TurnsBetweenStories = 450;

/// The turns that must go by before a creature does a dance of a row again, for the rows that it notes as it plans
/// them, and none for the other rows
[[nodiscard]] std::optional<uint32_t> TurnsBetween(uint32_t type);

/// Whether a creature may do a dance of a row again: its last noted dance was of another row, and more than the turns
/// of the row ago. A row that the creature does not note is always possible.
[[nodiscard]] bool MayDanceAgain(uint32_t type, uint32_t lastType, uint32_t lastTurn, uint32_t turn);

/// Whether a creature's dance that dances continues: it continues while the agenda that started it continues, through
/// all of its steps. It stops when that agenda ends, when the creature gives it up, or when a new agenda replaces it.
[[nodiscard]] bool DanceGoesOn(std::optional<uint32_t> startedBy, uint32_t agenda, bool agendaUnderway);

/// When the creature stops what it does, the villagers that it called to dance leave its list. A villager that is in
/// the hand, that dances for the creature or that the creature controls stops its dance and decides what to do.
[[nodiscard]] bool DecidesWhenReleased(VillagerStates top);

/// Whether a creature may dance to impress villagers: its last such dance was of another kind, and more than 200
/// turns ago
[[nodiscard]] bool MayDanceToImpress(uint32_t lastType, uint32_t lastTurn, uint32_t turn);

/// A dance alone lasts this many seconds, and each move of it lasts this many milliseconds. The game has ten turns in
/// a second.
constexpr float k_PracticeSeconds = 20.0f;
constexpr float k_PracticeMoveMs = 1000.0f;
constexpr uint32_t k_TurnsPerSecond = 10;
/// The agenda of a dance alone: the creature dances where it stands, with a smile
[[nodiscard]] std::vector<creature_mind::Step> PracticeDance();

/// How wide a clear place the dance needs for a creature of a radius on the ground
[[nodiscard]] float AreaWidth(float radius);

/// What the agenda of a dance with villagers needs
struct AgendaInput
{
	/// The villager the dance is for, by its entity's number
	uint32_t villager {0};
	/// The clear place of the dance
	glm::vec2 place {0.0f};
	/// The row of the dance table, the fewest dancers the row asks for, and the beats of its length
	uint32_t type {0};
	uint32_t fewestDancers {0};
	uint32_t beats {0};
	/// The creature's height
	float height {0.0f};
};
/// The agenda of a dance with villagers. The creature stops and goes near the villager, faces it and summons the
/// people. It makes the dance and calls the people. Then, with its head still, it walks to the place of the dance and
/// points at it, smiling. It sends the people to their places, waits until they stand still and starts the dance. It
/// tells them to dance, and dances with them.
[[nodiscard]] std::vector<creature_mind::Step> Agenda(const AgendaInput& input);

/// Which of the villagers that are free a creature calls to its dance. `start` tells a villager to come, and is true
/// when the villager comes. It calls them in the given order. It stops after an even number of villagers, when it has
/// more than 11, when all but one of an odd number of free villagers come, or when more than two thirds of the
/// town's adults come. None when fewer villagers are free than the fewest the dance asks for.
[[nodiscard]] std::optional<std::vector<uint32_t>> Summon(std::span<const uint32_t> free, uint32_t fewest, uint32_t adults,
                                                          const std::function<bool(uint32_t)>& start);

/// The belief that one villager who worships in a creature's dance gives its town in the creature's player, each turn:
/// the town table's population for unmodified belief over the town's people, times the creature's impressive size and
/// the dance's impressiveness
[[nodiscard]] float WorshipBelief(float unmodifiedPopulation, uint32_t people, float impressiveSize, float danceImpressiveness);

/// What the creature does in one turn of its dance, by the move of its group
struct DanceTurn
{
	/// A point to turn to face, when the creature does not face it within the given angle (radians)
	std::optional<glm::vec2> facePoint;
	float turnWithin {0.0f};
	/// An action to play once, when its body plays no action; an action to play on top of its body
	std::optional<size_t> playOnce;
	std::optional<size_t> overlay;
	/// The place to walk back to, when it is too far from its place
	std::optional<glm::vec2> walkTo;
	/// A dance to start, with the milliseconds of each dance move
	std::optional<float> danceMoveMs;
	/// A pose to start: the loop, with the animations before and after it as its start and end
	std::optional<size_t> pose;
	/// A face to pull, by its animation, and for how many milliseconds
	std::optional<size_t> face;
	float faceMs {0.0f};
	/// A gesture to draw in the air, by its number. When the drawing is done, the move of the group becomes a dance.
	std::optional<uint32_t> gesture;
	/// Whether the move stops a pose, and a dance, that the body plays, and an arm that points
	bool endPose {false};
	bool endDance {false};
	bool endPoint {false};
};
/// The move of a group that dances. When a creature finishes a gesture in a dance, the move of its group becomes this.
inline constexpr uint32_t k_DanceMove = 34;
/// The turn of a creature in its dance
[[nodiscard]] DanceTurn Turn(const creature_mind::Senses::DanceView& view, glm::vec2 position);

/// The music of a creature's dance: the chant of the tribe of the first dancing villager, with voices when more than
/// eight dance
[[nodiscard]] int32_t MusicType(int32_t tribe, uint32_t dancers);
/// The camera hears the music of a creature's dance only when nearer to it than this, along the land
constexpr float k_MusicFurthest = 75.0f;
/// The camera hears the music only when its height is within this of the land under the creature
constexpr float k_MusicHighest = 60.0f;

} // namespace openblack::creature_dance
