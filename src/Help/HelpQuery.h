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
#include <utility>
#include <variant>

#include "Enums.h"
#include "Help/HelpMessages.h"

/// The answers to the Help key: what the advisors say about the thing in the hand or under it, or about the ground
/// under the hand. Each kind of thing has its own help texts, mostly from its row in the info tables, with a few kinds
/// choosing by what the thing is doing or whose it is; people get stock answers about who they are; the ground is
/// told by what it is made of.
namespace openblack::help_query
{

using help_messages::Texts;

/// A message set is said about the thing, with the thing's texts for a set that says them
struct SaySet
{
	size_t set;
};

/// The texts from the first to the last are said through a help script; the town needs' script also says the need's
/// text and how much the town wants it, in percent
struct SayTexts
{
	Texts texts;
	help_messages::Script script {help_messages::Script::JustTalkWithText};
	std::optional<std::pair<uint32_t, float>> townNeed;
};

using Answer = std::variant<SaySet, SayTexts>;

/// What the help sees of a person
struct Person
{
	/// A script controls the person
	bool scriptControlled {false};
	/// A disciple of a kind that goes about its work: it is answered by its texts, never as one of the people below
	bool workingDisciple {false};
	/// Dead or dying
	bool dead {false};
	bool child {false};
	/// Belongs to no town: neither housed nor among a town's homeless
	bool vagrant {false};
	/// Belongs to the player asking
	bool players {false};
};

/// What kind of answer a thing's texts give
enum class Role : uint8_t
{
	/// Its texts, said plainly
	Plain,
	/// A town's centre or its totem: said by the town's script
	Town,
	/// A flag flying a town's need: said with the need and how much the town wants it
	TownNeed,
};

/// What the help sees of a thing
struct Thing
{
	/// A person, for the people's stock answers
	std::optional<Person> person;
	/// A wonder belonging to another player
	bool othersWonder {false};
	/// Its help texts; a first text of 0 is none
	Texts texts {.first = 0, .last = 0};
	Role role {Role::Plain};
	/// For a town's need: the need's own text, and how much its town wants it (none for a flag with no town)
	uint32_t needText {0};
	std::optional<float> rawDesire;
};

/// The help's answer about a thing
[[nodiscard]] Answer About(const Thing& thing);

/// What the help sees of the ground under the hand
struct Ground
{
	/// The hand points at the land or the sea
	bool hasPoint {false};
	/// The fish of a fish farm swim there
	bool fishFarm {false};
	/// The height of the point, in metres
	float altitude {0.0f};
	/// The point is over a part of the map with land in it
	bool onLand {false};
	/// The help texts of what the ground there is made of
	Texts material {.first = 0, .last = 0};
};

/// The help's answer about the ground
[[nodiscard]] Answer AboutGround(const Ground& ground);

/// The texts said about a person under a script's control, about a dead person, about a fish farm's water and about
/// shallow water
constexpr uint32_t k_ScriptControlledText = 6120;
constexpr uint32_t k_DeadText = 3036;
constexpr uint32_t k_FishFarmText = 2850;
constexpr uint32_t k_ShallowWaterText = 2949;
/// Ground at or below this height, in metres, is shallow water to the help
constexpr double k_ShallowWaterHeight = 0.15;
/// The texts of a temple's entrance
constexpr Texts k_TempleEntranceTexts {.first = 2000, .last = 2000};

/// A villager's texts: what it is doing when its activity has a text, else what its disciple's work is, else its
/// kind's
[[nodiscard]] Texts VillagerTexts(uint32_t activityText, std::optional<VillagerDisciple> disciple, Texts kind);

/// A creature's text: its species' first text, the next one when it belongs to another player (the chimp's and the
/// ogre's only text serves everyone)
[[nodiscard]] Texts CreatureTexts(Texts species, uint32_t speciesRow, bool players);

/// A feature's texts: a finished ark dry dock tells of the ark, others their kind's
[[nodiscard]] Texts FeatureTexts(FeatureInfo type, float built, Texts kind);

/// A pile of food's texts: its kind's in a finished storage pit, else a food pile's
[[nodiscard]] Texts FoodPileTexts(bool inFinishedStore, Texts kind);
constexpr uint32_t k_FoodPileText = 2872;

/// A miracle seed's texts: its miracle's, each of the two falling back to the seed object's when the miracle has none
[[nodiscard]] Texts SeedTexts(Texts miracle, Texts object);

} // namespace openblack::help_query
