/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>
#include <string>
#include <string_view>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Magic/HeldCastRules.h"

/// The words and numbers of the tooltips the hand shows over things in the world: a village centre's belief and
/// population, a temple's health and way in, a store's food and wood, and the amount a pile gives. Each is one of the
/// game's 170 tooltips (from text 0xE73).
namespace openblack::gui::world_tooltip
{

/// "Enter Temple", over the way into the player's own temple, by the Apply button
constexpr uint32_t k_EnterTemple = 88;
/// Over the way into a temple, the hand offers to enter it only for the player's own temple, standing fully built, and
/// only while the hand is within the player's influence
[[nodiscard]] constexpr bool OffersEnterTemple(bool ownTemple, bool templeBuilt, bool handInInfluence)
{
	return ownTemple && templeBuilt && handInInfluence;
}
/// "%3.0f Belief Left", over any village centre: the belief the town holds in the player beyond any other player's
constexpr uint32_t k_BeliefLeft = 89;
/// "%3.0f Belief Needed": the words the belief tooltip shows instead while another player is ahead in the town, with
/// how far behind the player is
constexpr uint32_t k_BeliefNeeded = 109;
/// "Amount: %3.0f", forced as the hand takes food or wood from a pile
constexpr uint32_t k_HeldAmount = 119;
/// "Temple Health: %3.0f%%"
constexpr uint32_t k_TempleHealth = 117;
/// "Stored - Food: %3.0f Wood: %3.0f", over a village store
constexpr uint32_t k_Stored = 134;
/// "Population: %3.0f", written as "Population:  12/34": its people over the room of its homes
constexpr uint32_t k_Population = 137;
/// "Food Amount: %3.0f" and "Wood Amount: %3.0f", over a pile
constexpr uint32_t k_FoodAmount = 138;
constexpr uint32_t k_WoodAmount = 139;

/// "Build", what a scaffold held anywhere but over a planned building says dropping it does
constexpr uint32_t k_Build = 125;
/// "Graveyard", what a scaffold held over a planned graveyard says: its info's text for building it. The buildings'
/// names (Abode, Totem, Village Store, Creche, Workshop, Wonder, Graveyard, Village Centre, Football Pitch, Miracle
/// Dispenser, Field) are shown this way only, never over a standing building.
constexpr uint32_t k_Graveyard = 147;

/// The game's text of its first tooltip, and how many there are
constexpr uint32_t k_FirstToolTipText = 0xE73;
constexpr uint32_t k_ToolTipCount = 170;

/// The tooltip a game text is, none for a text that is not a tooltip (the game shows nothing for it)
[[nodiscard]] std::optional<uint32_t> ToolTipOfText(uint32_t text);

/// What a held scaffold says dropping it does: over a building its town plans, that building's text for building it
/// (its name: "Graveyard" over a planned graveyard); anywhere else "Build"
[[nodiscard]] std::optional<uint32_t> ScaffoldDrop(std::optional<uint32_t> plannedBuildText);

/// A belief tooltip is shown in thousandths
constexpr double k_BeliefScale = 1000.0;
/// Shares and lives show as percentages
constexpr double k_PercentScale = 100.0;

/// A tooltip and the numbers written into its words
struct Numbered
{
	uint32_t index;
	double first;
	std::optional<double> second;
};

/// A village centre's belief tooltip, from how much more the town believes in the player than in the player it believes
/// in most besides, in thousandths. It is always the "Belief Left" tooltip, whoever is ahead; only its words change
/// when it is drawn (see Worded).
[[nodiscard]] Numbered Belief(double margin);

/// The words a tooltip is drawn with and the number written into them
struct Wording
{
	uint32_t words;
	double number;
};

/// The words and number a numbered tooltip is drawn with: the belief tooltip reads "Belief Needed" with the shortfall
/// while the player trails in the town; every other tooltip keeps its own words and number
[[nodiscard]] Wording Worded(uint32_t index, double number);

/// A temple's health is its heart's life, 0 to 1, as a percentage
[[nodiscard]] Numbered TempleHealth(float life);

/// A village centre's people, and the places in the homes of its town
[[nodiscard]] Numbered Population(uint32_t people, uint32_t places);

/// A village store's food and wood
[[nodiscard]] Numbered Stored(uint32_t food, uint32_t wood);

/// A pile's food or wood amount, by what it holds
[[nodiscard]] Numbered PileAmount(bool food, uint32_t amount);

/// The amount shown as the hand takes from a pile
[[nodiscard]] Numbered HeldAmount(uint32_t amount);

/// A tooltip's words with its numbers written in, as the game writes them
[[nodiscard]] std::u16string Compose(uint32_t index, std::u16string_view words, double first, std::optional<double> second);

/// Where the hand's tooltip hangs: the whole pixel the drawn hand's origin (at its fingertips) projects to, wherever
/// the pointer is. While the hand stays put and the pointer moves on (pouring food or wood, gripping the land, holding a
/// totem) the tooltip stays with the hand. None while the hand is behind the camera; off the screen it still counts,
/// to be kept on the screen by KeptOnScreen.
[[nodiscard]] std::optional<glm::ivec2> HandOnScreen(const glm::mat4& worldToClip, float nearClip, glm::vec3 hand,
                                                     glm::ivec2 screen);

/// The tooltip's corner by the hand, kept on the screen by its size
[[nodiscard]] glm::ivec2 KeptOnScreen(glm::ivec2 hand, glm::ivec2 screen, int size);

/// "Cast", by the action button, where the miracle held in the hand may be cast on the thing or the land under the
/// pointer
constexpr uint32_t k_Cast = 14;
/// "Cast Circle": the same over the land for the miracles cast at a circle drawn (the storms, the tornado, the shields)
constexpr uint32_t k_CastCircle = 126;

/// The tooltip of what the hand offers to do with the miracle it holds, none where it offers nothing
[[nodiscard]] std::optional<uint32_t> HeldMiracle(magic::HeldCastOffer offer);

/// Whether the hand offers the tooltips of what is under the pointer (a pile's amount, a store's contents, a temple, a
/// village centre, a flag's need, the creature's "Interact"). Only with nothing in it: with a miracle held, poured or
/// not, it offers to cast the miracle where it may, and elsewhere only what it would over nothing (the leash's tips).
/// Tooltips forced by what the hand does (the amount it takes from a pile, a totem's share) still show.
[[nodiscard]] bool OffersOverToolTips(bool miracleInHand);

} // namespace openblack::gui::world_tooltip
