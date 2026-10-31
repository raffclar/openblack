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
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace openblack::temple_scrolls
{

/// What the player's creature's scrolls tell of it
struct CreatureFacts
{
	std::u16string name;
	int32_t age {0};
	/// Percentages
	int32_t health {0};
	int32_t energy {0};
	int32_t strength {0};
	int32_t fatness {0};
	int32_t exhaustion {0};
	int32_t dehydration {0};
	int32_t poo {0};
	/// From -1, evil, to 1, good
	float alignment {0.0f};
	/// From -1, cold, to 1, hot
	float warmth {0.0f};
	/// From 0, healthy, to 1, ill
	float illness {0.0f};
	/// The miracle cast on the creature, of the 16 HELP_TEXT_CREATURE_RECEIVED_SPELL texts
	std::optional<int32_t> miracleApplied;
	/// People killed, animals killed, creatures defeated, battles fought, battles won, poos and mushrooms eaten
	std::array<int32_t, 7> tallies {};
	/// Taking food from the fields, the totem, the village store, fishing and dancing
	std::array<bool, 5> actionsKnown {};
	/// Each desire the creature has, the text of it ("HELP_TEXT_ROOM_PERSONALITY_GREEDY" and so on) and how much,
	/// of the ten HELP_TEXT_CREATURE_ATTITUDE texts from "extremely"
	struct Desire
	{
		std::string_view text;
		int32_t attitude;
	};
	std::vector<Desire> desires;
	/// What the creature thinks of its god, from -1 to 1, if anything
	std::optional<float> opinionOfGod;
	/// How much attention its god has paid it, of the five HELP_TEXT_HOW_MUCH texts from 0
	int32_t attention {0};
	/// Each other creature it knows: its name, what it thinks of it, from -1 to 1, and how much attention that
	/// creature has paid it, of the five HELP_TEXT_HOW_MUCH texts, if any
	struct KnownCreature
	{
		std::u16string name;
		float opinion {0.0f};
		std::optional<int32_t> attention;
	};
	std::vector<KnownCreature> creatures;
	/// Whether it thinks it knows its god's dominant desire
	bool knowsGodsDesire {false};
	/// The creatures, temples, flocks and forests it knows about
	std::array<int32_t, 4> known {};
	/// How it leans in its fights, from -1 to 1, which the belts on the attack dummies show
	float fightBalance {0.0f};
	/// The miracles it knows of, the text of each and how much of it it has learnt
	struct Miracle
	{
		/// The number of the game's help text for having learnt it
		uint32_t text;
		int32_t percent;
		/// Its place in the game's table of miracles, and how far it is learnt, 0 to 100, which the medals and
		/// seeds by the magic plinths show
		uint32_t miracle {0};
		float learnt {0.0f};
	};
	std::vector<Miracle> miracles;
};

/// The text of a scroll, written as its room writes it
enum class Content
{
	World,
	CreatureAttributes,
	CreatureActions,
	CreatureMind,
	CreatureMiracles,
	Challenge,
	SaveGame,
	LibraryStaff,
	LibraryControl,
	LibraryCreature,
	LibraryVillageLife,
	LibraryMiracles,
	LibraryDidYouKnow,
	LibraryHistory,
};

} // namespace openblack::temple_scrolls
