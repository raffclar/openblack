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
#include <bit>
#include <optional>
#include <string_view>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/AllMeshes.h"

/// The rules of the explorers' boat, the film the Land 1 silver scroll of the explorers ends with once they have their
/// wood, grain and cows: five sailors push the boat down the beach into the sea, then the boat sails away to the south
/// west with its crew and cargo on deck. The boat follows two recorded clips of one bone each; everything else is placed
/// on or beside it.
namespace openblack::ecs::boat_launch
{

/// Where the ark's dry dock stands on the beach, from which the boat is pushed
inline constexpr glm::vec3 k_LaunchPlace {std::bit_cast<float>(0x44EB22AAu), std::bit_cast<float>(0x41021B09u),
                                          std::bit_cast<float>(0x454521C0u)};
/// Where the boat's sailing clip is played from, far out to the west
inline constexpr glm::vec3 k_SailingStart {std::bit_cast<float>(0x44B61148u), 0.0f, std::bit_cast<float>(0x454BF0F6u)};

/// The clips the boat follows, in the misc folder: pushed down the beach, then rocking at sea
inline constexpr std::string_view k_LaunchClip = "boat1.anm";
inline constexpr std::string_view k_SailingClip = "boat2.anm";

/// The sea mirrors the boat, and nothing else of the film, in this colour of its own (0xAARRGGBB): flat, unshaded by
/// the sun and unfaded by the haze, with the land's colour where the boat is added, however far off it is
inline constexpr uint32_t k_SeaReflectionColour = 0xFF303070u;
/// That colour's red, green and blue, 0 to 255
[[nodiscard]] constexpr glm::vec3 SeaReflectionColour() noexcept
{
	return {static_cast<float>((k_SeaReflectionColour >> 16) & 0xFFu), static_cast<float>((k_SeaReflectionColour >> 8) & 0xFFu),
	        static_cast<float>(k_SeaReflectionColour & 0xFFu)};
}

/// The milliseconds the sailors push before the boat starts to move
inline constexpr int32_t k_PushFor = 3000;
/// Once the boat is this far into its launch clip the sailors stop pushing
inline constexpr int32_t k_PushEnds = 850;
/// The launch ends this many milliseconds before its clip does, and the sailing starts
inline constexpr int32_t k_LaunchEndsEarly = 400;
/// The sailing lasts a minute
inline constexpr int32_t k_SailingLasts = 60000;
/// The boat's speed at sea, metres a millisecond
inline constexpr float k_SailingSpeed = std::bit_cast<float>(0x3BA3D70Au);

/// The spray thrown up as the boat goes into the water, a pair of puffs every so often
inline constexpr int32_t k_SprayEvery = 200;

/// The two parts of the film
enum class Stage : uint8_t
{
	/// The sailors push the boat down the beach into the sea
	Launch,
	/// The boat sails away with its crew and cargo
	Sailing,
};

/// The film's clocks, in milliseconds of game time
struct Clocks
{
	Stage stage {Stage::Launch};
	/// Since the stage began; at the launch, since the sailors stopped pushing once they have
	int32_t elapsed {0};
	/// Where the boat is in its clip, and where it was before this frame
	int32_t boatTime {0};
	int32_t previousBoatTime {0};
	/// The sailors are still pushing: until they stop, the boat waits for them before it moves
	bool pushing {true};
	/// Since the last spray
	int32_t sinceSpray {0};
	/// Each wake sprite's place in its life
	std::array<int32_t, 5> wakes {0, 1200, 2400, 3600, 4800};
};

/// What happened to the film in a frame
enum class Step : uint8_t
{
	/// It goes on in its stage
	Continues,
	/// The launch ended and the sailing began: the boat isn't drawn at its new place until the next frame
	SailingBegan,
	/// The sailing ended, and with it the film
	Ended,
};

/// The two clips' lengths, in milliseconds, and whether the sailing clip comes round
struct ClipLengths
{
	int32_t launch {0};
	int32_t sailing {0};
	bool sailingLoops {true};
};

/// The film moved on by a frame of game time
Step Advance(Clocks& clocks, int32_t milliseconds, const ClipLengths& lengths);
/// Whether a spray is due at the launch: once more than its interval has passed since the last, the wait starts again
/// whether or not the boat throws any spray then
[[nodiscard]] bool TakeSpray(Clocks& clocks);

/// The sounds of the story's sound bank the boat makes as its clip passes their times: a creak as it starts, a splash,
/// a big splash as it meets the sea. Those passed between the boat's time before this frame and now, in order.
[[nodiscard]] std::vector<int32_t> SoundsPassed(int32_t before, int32_t now);

/// A pair of puffs of spray: brown sand while the boat runs down the beach, white water as it goes into the sea
struct Spray
{
	float size;
	/// 0xRRGGBB
	uint32_t colour;
	/// From the boat's place, besides the random spread
	glm::vec3 offset;
};
/// The spray thrown up at a time of the launch clip, none before or after
[[nodiscard]] std::optional<Spray> SprayAt(int32_t boatTime);
/// The spray is spread about the boat, across by up to two metres and along by up to twenty
inline constexpr float k_SpraySpreadX = 2.0f;
inline constexpr float k_SpraySpreadZ = 20.0f;

/// The boat's root bone between two keyframes: the two matrices mixed element by element, not turned between
[[nodiscard]] glm::mat4 RootBetween(const glm::mat4& from, const glm::mat4& to, float t);
/// The boat on the beach: its clip's matrix placed at the dry dock, turned a quarter and mirrored across its length.
/// `liftedBy` the land's height under the boat less that under the dry dock.
[[nodiscard]] glm::mat4 LaunchMatrix(const glm::mat4& root, const glm::vec3& dock, float liftedBy);
/// Where the boat's clip puts it on the beach before the land lifts it, from which the land's height is read
[[nodiscard]] glm::vec3 LaunchPlace(const glm::mat4& root, const glm::vec3& dock);
/// The boat at sea: its clip's matrix played from the sailing start, turned an eighth to the south west and carried that
/// way at its speed for the milliseconds of sailing, then turned a quarter and mirrored as on the beach
[[nodiscard]] glm::mat4 SailingMatrix(const glm::mat4& root, const glm::vec3& start, int32_t elapsed);

/// One of the five sailors pushing the boat, then worn out or cheering beside the water
struct Sailor
{
	/// Where it stands on the beach, before the land's height there
	glm::vec2 xz;
	/// The way it faces: the game's angle, a quarter turn back
	float heading;
	AnimId clip;
	/// Its place in its clip, before it comes round
	int32_t clipTime;
};
inline constexpr size_t k_LaunchSailors = 5;
/// The sailor `index` at the launch's clocks, beside the dry dock
[[nodiscard]] Sailor LaunchSailor(size_t index, const Clocks& clocks, const glm::vec3& dock);

/// One of the things on the boat's deck at sea
struct DeckPart
{
	MeshId mesh;
	/// Its clip, none for one that doesn't move
	std::optional<AnimId> clip;
	/// From its place on the boat, in the boat's frame
	glm::mat4 local;
	/// How far ahead in its clip it plays
	int32_t clipOffset;
};
/// The two cows eating, the grain, the man and woman at the bow, the two sitting swinging their legs and the one
/// scrubbing the deck
[[nodiscard]] const std::array<DeckPart, 8>& DeckParts();
/// Where a deck part is drawn: its own place carried by the boat
[[nodiscard]] glm::mat4 OnDeck(const DeckPart& part, const glm::mat4& boat);

/// One of the five sprites of the boat's wake, which run back from the bow and fade as they spread on the sea
struct Wake
{
	glm::vec3 position;
	float halfWidth;
	/// Its depth across, over its width
	float aspect;
	/// Turned about the vertical, radians
	float angle;
	/// 0..255
	uint8_t alpha;
	/// Its cell of the smoke sheet, 8 by 8
	uint8_t cell;
	bool shown;
};
/// The wake sprite `index` behind the boat, from its place in its life
[[nodiscard]] Wake WakeAt(int32_t phase, const glm::mat4& boat);
/// A wake sprite's life, after which it starts again at the bow
inline constexpr int32_t k_WakeLife = 6000;

} // namespace openblack::ecs::boat_launch
