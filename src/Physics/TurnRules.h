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

#include <array>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/WaterRings.h"

/// The rules the physics follows over a game turn, around the bodies' own steps: how hard a turn's knocks were, which
/// knocks make a sound and how loud, which of the map's cells a moving body wakes, the dust and rings it leaves.
namespace openblack::physics::turn
{

/// A body's impact is its turn's summed force times this: the mean of the turn's twenty steps
inline constexpr float k_ImpactScale = 0.05f;
/// No impact at all when the summed force's square is no more than this
inline constexpr float k_LeastForceSquared = 0.0001f;

/// The impact of a turn's summed force (the total force on the body over the steps it touched something, its weight
/// among it: a body lying still has next to none), none when it is too small to count
[[nodiscard]] std::optional<float> Impact(glm::vec3 forceSum);
/// How hard a knock was, in multiples of the body's own weight
[[nodiscard]] float GLoad(float impact, float mass);

/// Whether a knock makes a sound: the body is moving and something hit it, or it was knocked harder than half its
/// weight
[[nodiscard]] bool WantsCollisionSound(bool resting, bool hitByBody, float impact, float mass);

/// How loud a knock sounds, by how many of the object's own table weights it was: hard, medium or soft
enum class SoundLevel : int32_t
{
	Hard = 1,
	Medium = 2,
	Soft = 3,
};
/// Soft under 1.25 of its weight, hard over 3. Grain never sounds hard.
[[nodiscard]] SoundLevel CollisionLevel(float impact, float infoWeight, bool grain);

/// The sounds' codes of each kind of thing, as what hits and as what is hit, by its collision sound type
inline constexpr std::array<int32_t, 33> k_HitterCodes = {21, 24, 20, 20, 20, 20, 20, 19, 19, 19, 19, 20, 25, 22, 22, 22, 22,
                                                          22, 30, 25, 35, 31, 24, 23, 24, 25, 25, 21, 21, 21, 33, 34, 42};
inline constexpr std::array<int32_t, 33> k_HitCodes = {11, 14, 10, 10, 10, 10, 10, 9,  9,  9,  9,  10, 15, 12, 12, 12, 12,
                                                       12, 20, 15, 23, 19, 14, 13, 14, 15, 15, 16, 18, 17, 21, 22, 24};
/// The action key every collision sound has in the bank
inline constexpr int32_t k_CollisionAction = 75;
/// The keys of a collision sound: its level, what hit and what it hit (their collision sound types)
[[nodiscard]] std::array<int32_t, 5> CollisionKeys(SoundLevel level, int32_t hitterType, int32_t hitType);
/// Whether a collision sound follows the thing that made it as it moves: all do but those whose hitter's code is the one
/// the bank keeps for sounds that stay where they were made
inline constexpr int32_t k_StayingHitterCode = 0x16;
[[nodiscard]] constexpr bool CollisionSoundFollows(const std::array<int32_t, 5>& keys)
{
	return keys[2] != k_StayingHitterCode;
}

/// The pairs of things that have just sounded against each other: the same pair stays quiet for two turns, and no
/// more than 128 pairs are kept
class SoundPairs
{
public:
	static constexpr size_t k_MostPairs = 128;
	static constexpr int32_t k_QuietTurns = 2;

	/// Whether a pair may sound now, either way round
	[[nodiscard]] bool MaySound(uint32_t a, uint32_t b) const;
	void Add(uint32_t a, uint32_t b);
	/// A turn has gone by
	void EndTurn();
	void Clear() { _pairs.clear(); }
	[[nodiscard]] size_t Size() const { return _pairs.size(); }

private:
	struct Pair
	{
		uint32_t a;
		uint32_t b;
		int32_t turns;

		/// Gives the values that a saved game keeps to a visitor
		template <typename Self, typename Visitor>
		static void VisitSaved(Self& self, Visitor&& visit)
		{
			visit(self.a, self.b, self.turns);
		}
	};
	std::vector<Pair> _pairs;

public:
	/// Gives the values that a saved game keeps to a visitor
	template <typename Self, typename Visitor>
	static void VisitSaved(Self& self, Visitor&& visit)
	{
		visit(self._pairs);
	}
};

/// A range of the map's cells, both ends included
struct CellRange
{
	glm::ivec2 low {0};
	glm::ivec2 high {-1};
};
/// The map's cells a square of half a side about a centre covers, its corners clamped onto the map
[[nodiscard]] CellRange SquareCells(glm::vec3 centre, float half);
/// The cells a moving body wakes the objects of: a square of its radius plus a tenth of a second of its travel across
/// the ground
[[nodiscard]] CellRange WakeCells(glm::vec3 centre, glm::vec3 velocity, float radius);

/// The kinds of puff the shared pool of puffs holds. Each kind has its own life, growth and frames.
enum class PuffKind : uint8_t
{
	/// Dust that a landing, a footstep or a fart throws up. It lives one second.
	Dust,
	/// Dark smoke that rises off an evil creature. It lives two seconds.
	Smoke,
	/// A light mote that shows on a good creature. It lives two seconds and is added to what is behind it.
	Mote,
	/// The same mote, but its frames play in the opposite order
	MoteReversed,
	/// A drop of blood thrown by a blow. It falls until it meets the land, then slides on it, slowing, and lives three
	/// seconds from each of those starts. It shows one of eight frames at random each time it is drawn, keeps its size,
	/// and hides what is behind it.
	Blood,
};

/// One of the puffs in the shared pool: dust, smoke or a mote
struct DustPuff
{
	glm::vec3 position {0.0f};
	glm::vec3 velocity {0.0f};
	/// Its full size
	float size {0.0f};
	/// Seconds of game time it has lived
	float age {0.0f};
	/// Which of its sixteen looks it starts on
	int32_t variant {0};
	/// 0xAARRGGBB
	uint32_t argb {0};
	PuffKind kind {PuffKind::Dust};
	/// A drop of blood that has met the land and slides on it
	bool onLand {false};

	/// Gives the values that a saved game keeps to a visitor
	template <typename Self, typename Visitor>
	static void VisitSaved(Self& self, Visitor&& visit)
	{
		visit(self.position, self.velocity, self.size, self.age, self.variant, self.argb, self.kind, self.onLand);
	}
};
/// How many puffs a landing makes, the most there can be at once, and how long one lives
inline constexpr int32_t k_PuffsPerLanding = 6;
/// The biggest a landing's puffs are, and their colours over land and over the sea
inline constexpr float k_LargestPuff = 5.0f;
inline constexpr uint32_t k_LandDust = 0x50806040u;
inline constexpr uint32_t k_SeaFoam = 0x28C8F0F4u;
/// Each axis of a puff's speed is (r - 100) times this, r a random number from 0 to 200
inline constexpr float k_PuffSpeed = 0.02f;
/// The size of a landing's puffs for a body of a radius
[[nodiscard]] float LandingPuffSize(float radius);
/// A colour blended towards another by an amount from 0 to 255, keeping its own alpha
[[nodiscard]] uint32_t BlendColour(uint32_t argb, uint32_t towards, int32_t amount);

/// The ring a body leaves where it hits the water, and the smaller one each time it bobs
[[nodiscard]] water_rings::Ring ImpactRing(glm::vec3 centre, float radius);
[[nodiscard]] water_rings::Ring BobRing(glm::vec3 centre, float radius);

/// Whether a body's step took it into the camera's ten metres fast enough to whoosh past it
[[nodiscard]] bool PassesCamera(glm::vec3 before, glm::vec3 after, glm::vec3 camera, glm::vec3 velocity);
/// The five whooshes are picked by the tick count
inline constexpr int32_t k_Whooshes = 5;

/// Whether a body is awake as a turn starts, before what moves wakes what is near it: a moving body is, a resting one only
/// if its kind always stays in the physics. A body still asleep after the waking leaves the physics, so a body at rest
/// with nothing moving near it costs nothing from the next turn on.
[[nodiscard]] constexpr bool AwakeAtTurnStart(bool resting, bool alwaysStays)
{
	return !resting || alwaysStays;
}

/// Whether a moving body is low enough for the sea: its centre under half its radius above the sea's level. There it
/// bobs, and it has sunk if it is denser than water and its kind says so.
[[nodiscard]] bool NearSeaLevel(bool resting, float centreHeight, float radius);
/// Whether a moving body has sunk out of sight: its centre at least its radius under the sea's level, so no point of
/// it is above the surface. It is drawn no more from then on, though it sinks on until it is deleted deep under the sea.
[[nodiscard]] bool SunkOutOfSight(float centreHeight, float radius);
/// Whether a body bobbed this step: its upward speed changed sign
[[nodiscard]] bool Bobbed(float upwardSpeedBefore, float upwardSpeedAfter);

/// A dead thing in the physics grows this much denser each turn, so that corpses sink
inline constexpr float k_CorpseSoaking = 0.01f;
inline constexpr float k_DeadLife = 0.01f;

/// A felled tree has fallen once its up axis has tipped this far from upright; taller than this it makes a sound
inline constexpr float k_FelledUpright = 0.98f;
inline constexpr float k_FelledSoundHeight = 10.0f;
/// Whether a felled tree has toppled: its up axis has tipped past k_FelledUpright
[[nodiscard]] constexpr bool FelledTreeToppled(float upAxisHeight)
{
	return upAxisHeight < k_FelledUpright;
}
/// Whether a felled tree that topples makes the sound of a tree that falls: only a tree taller than k_FelledSoundHeight
[[nodiscard]] constexpr bool FelledTreeFallSounds(float treeHeight)
{
	return treeHeight > k_FelledSoundHeight;
}

/// The force a living thing pushes an object out of its way with: the object's weight
[[nodiscard]] float PushForce(float weight);

} // namespace openblack::physics::turn
