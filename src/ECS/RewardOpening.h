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
#include <functional>
#include <optional>
#include <span>

#include "Enums.h"

/// Opening a reward chest. Its player taps it with the Action button once it is on the land: a tap sound, then two
/// opening sounds as its lid lifts, and what the game's reward table says it holds comes out. An opened chest stays 200
/// game turns and then goes.
///
/// What comes out, in the order the table is read, the first that applies:
/// - food, then wood: poured at the chest, into the stores and piles about it and the rest onto a new pile;
/// - belief: given to the chest's town, with a sparkle on the chest for 20 turns; with no town, nothing;
/// - an object (a singing stone, an idol, a weeping stone or a toy), made where the chest stands;
/// - a scaffold, for its town's workshop when the town has one;
/// - the "next miracle" chest: the next miracle of the player's reward progress, or a random one as a one-shot globe;
/// - a power-up with no seed: its gesture shown on the land beside the chest;
/// - a miracle seed: a one-shot globe 2 m above the chest when the player can't cast it and there's no town; the town
///   learning it when there is one (a globe when it already knew it); and when the player can already cast it, the
///   next power-up level of it the player lacks, with its gesture shown on the land, or a globe when there is none.
///
/// Pure rules, tested on their own with a fake world; the reward system applies them to the game.
namespace openblack::ecs::reward
{

/// How many game turns an opened chest stays before it goes
inline constexpr int32_t k_OpenedTurns = 200;
/// A one-shot globe from a chest floats this far above it
inline constexpr float k_GlobeAbove = 2.0f;
/// A belief chest's sparkle lasts this many game turns
inline constexpr int k_BeliefSparkleTurns = 20;
/// A gesture shown on the land is this many metres across, laid from the chest towards the east and south
inline constexpr float k_GestureSize = 30.0f;
/// The "next miracle" chest looks for a town whose village centre shows fewer miracles than this
inline constexpr size_t k_CentreMostMiracles = 6;

/// What a kind of chest holds: its row of the game's reward table
struct Contents
{
	float food {0.0f};
	uint32_t wood {0};
	float belief {0.0f};
	MobileStaticInfo object {MobileStaticInfo::None};
	bool scaffold {false};
	/// It gives the next miracle of the player's reward progress
	bool nextMiracle {false};
	SpellSeedType seed {SpellSeedType::None};
	/// The magic type of a power-up it gives, none for none
	MagicType powerUp {MagicType::None};
};

/// The game the chest opens into, as seen from the chest
class OpeningWorld
{
public:
	virtual ~OpeningWorld() = default;

	/// Food or wood poured at the chest
	virtual void Pour(ResourceType type, uint32_t amount) = 0;
	/// Whether the chest has a town
	[[nodiscard]] virtual bool HasTown() const = 0;
	/// The chest's town is given belief in its player, and the chest sparkles
	virtual void GiveTownBelief(float belief) = 0;
	/// An object made where the chest stands
	virtual void MakeObject(MobileStaticInfo object) = 0;
	/// A scaffold for the chest's town's workshop, if it has a town with a workshop
	virtual void MakeScaffold() = 0;
	/// The chest takes the player's first town whose village centre shows fewer than six miracles, if any
	virtual void FindTownForMiracle() = 0;
	/// The first miracle of the player's reward progress allowed on this land that the player can't cast yet, or none
	[[nodiscard]] virtual MagicType NextMiracle() = 0;
	/// A random miracle of the player's reward progress allowed on this land
	[[nodiscard]] virtual MagicType RandomMiracle() = 0;
	/// The first seed that casts a magic type
	[[nodiscard]] virtual SpellSeedType FirstSeedOf(MagicType type) const = 0;
	/// How many levels a seed has: its plain miracle and each power-up
	[[nodiscard]] virtual int LevelCount(SpellSeedType seed) const = 0;
	/// The magic type the game reads for a seed's level as it looks for a power-up: the plain miracle at the base level,
	/// each power-up after it (see MagicAtLevel)
	[[nodiscard]] virtual MagicType MagicAt(SpellSeedType seed, int level) const = 0;
	/// Whether the chest's player can cast a magic type
	[[nodiscard]] virtual bool PlayerMay(MagicType type) const = 0;
	/// The chest's player can cast a magic type from now on
	virtual void PlayerLearns(MagicType type) = 0;
	/// The chest's town learns a magic type: whether it didn't know it before
	virtual bool TownLearns(MagicType type) = 0;
	/// The seed of a miracle the town has learnt shown spinning above the chest
	virtual void ShowTownSeed(SpellSeedType seed) = 0;
	/// A one-shot globe of a seed floats above the chest
	virtual void OneShotGlobe(SpellSeedType seed) = 0;
	/// The gesture of a magic type, if it has one, is shown on the land beside the chest
	virtual void ShowGesture(MagicType type) = 0;
};

/// Opens a chest that holds these contents into the world
void Open(const Contents& contents, OpeningWorld& world);

/// The magic type the game reads for a seed's level as it looks for a power-up the player lacks: the plain miracle at
/// the base level (-1), and level n the seed's (n + 1)th magic type. Its count of levels takes in the plain miracle, so
/// it reads one level past the last power-up: none for a seed with fewer than three power-ups, and, for one with three,
/// whatever follows its magic types in the table, its first power-up gesture read as a magic type.
[[nodiscard]] MagicType MagicAtLevel(const std::array<MagicType, 4>& magicTypes,
                                     const std::array<GestureType, 3>& powerUpGestures, int level);

/// One entry of a reward progress list: a miracle and the lands it may be given on
struct ProgressEntry
{
	MagicType magic {MagicType::None};
	std::array<uint32_t, 6> onLand {};
};

/// Which reward progress list a player of this alignment takes: the good one above nothing, the evil one below, and a
/// coin toss (heads, good) at exactly nothing
[[nodiscard]] bool TakesGoodList(float alignment, const std::function<bool()>& coinIsHeads);

/// The first entry allowed on the land that the player can't cast yet, or none
[[nodiscard]] MagicType NextMiracle(std::span<const ProgressEntry> list, int32_t land,
                                    const std::function<bool(MagicType)>& playerMay);

/// Entries drawn at random (draw(n) gives 0 to n - 1) until one allowed on the land with a miracle comes up; none
/// when no entry is allowed there, which the game would draw for ever
[[nodiscard]] MagicType RandomMiracle(std::span<const ProgressEntry> list, int32_t land,
                                      const std::function<uint32_t(uint32_t)>& draw);

/// Where an opened chest's clip has got to after a frame of so many milliseconds: round and round when it loops,
/// otherwise held at its last millisecond
[[nodiscard]] uint32_t StepOpenedClip(uint32_t place, uint32_t milliseconds, uint32_t playTime, bool looping);

} // namespace openblack::ecs::reward
