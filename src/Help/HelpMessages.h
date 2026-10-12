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
#include <string_view>

/// The help messages the game starts by itself: sets of texts the advisors say through the help scripts, the advisors'
/// banter when the player has had no help for a while, and the answers to the Help key.
namespace openblack::help_messages
{

/// The help scripts that say them
enum class Script : uint8_t
{
	/// Says the texts from the first to the last
	JustTalkWithText,
	JustTalkWithNoText,
	/// The advisors chat
	Banter,
	JustTalkMaybeWithSpirits,
	/// About a town: its centre
	Town,
	/// About one of a town's needs: says the texts, then the need's own text with how much the town wants it
	TownNeeds,
};
[[nodiscard]] std::string_view ScriptName(Script script);

/// How a set's texts are picked
enum class Pick : uint8_t
{
	/// All of its texts, from the first to the last
	Fixed,
	/// One of them at random
	Random,
	/// The texts of the thing it is about
	Thing,
	/// The script alone, with no texts
	NoText,
};

struct MessageSet
{
	uint32_t first;
	uint32_t last;
	Pick pick;
	/// The kind of help it is, whose time is kept when the set is said
	uint32_t category;
	Script script;
};

/// The sets the game has, by their numbers
constexpr size_t k_SetCount = 107;
[[nodiscard]] const MessageSet& Set(size_t set);

/// The advisors' banter, sets 1 to 25
constexpr size_t k_FirstBanter = 1;
constexpr size_t k_BanterCount = 25;
/// The answers about things the help has nothing to say of, and about people
constexpr size_t k_NotInterested = 33;
constexpr size_t k_NonBeliever = 34;
constexpr size_t k_Child = 35;
constexpr size_t k_Vagrant = 38;
/// About another player's wonder
constexpr size_t k_OthersWonder = 97;

/// A range of texts, said in order
struct Texts
{
	uint32_t first;
	uint32_t last;
};
/// The texts a set says, none for a script with no texts; a thing's set says the thing's (none there: text 0)
[[nodiscard]] std::optional<Texts> TextsOf(size_t set, std::optional<Texts> thing,
                                           const std::function<uint32_t(int32_t)>& localRand);

/// How many times each set has been said
using SetCounts = std::array<uint32_t, k_SetCount>;
/// The banter to say next: one at random of the banter sets not said yet, which counts as used; when all have been
/// used (or none is left), their counts are cleared and the first is said
[[nodiscard]] size_t PickBanter(SetCounts& sent, uint16_t& banterUsed, const std::function<uint32_t(int32_t)>& localRand);

/// The help's event that counts any help given or asked for, and the one counting a set said
constexpr uint32_t k_AnyHelpEvent = 43;
constexpr uint32_t k_SetSaidEvent = 42;
/// The advisors banter when the player has had no help for this long
constexpr float k_BanterIdleSeconds = 120.0f;
/// Whether it is time for banter: no help for two minutes, and no script's cut scene
[[nodiscard]] bool BanterDue(float secondsSinceHelp, bool scriptCutScene);

/// How much a town wants one of its needs, told as a percentage: its raw desire, at most 1, times 100
[[nodiscard]] float TownNeed(float rawDesire);

} // namespace openblack::help_messages
