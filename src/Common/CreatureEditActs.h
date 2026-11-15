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
#include <optional>
#include <string_view>

/// The acts of the creature tools of the editor: the buttons, the boxes and the lists that tell a creature to do a thing,
/// or that change its mind, its leash or its fight. In a network game each act goes to every computer as a command of a
/// turn, so that all the computers do the same turn. These rules keep no state, so that the game and the tests use them.
///
/// To add an act:
/// 1. Add the act before Count, and its name in k_ActNames.
/// 2. Give the limits of its numbers in editor::ActRefusal, and make editor::ApplyCreatureAct do it.
namespace openblack::creature_edit_acts
{

enum class Act : uint8_t
{
	// The mind
	/// The mind stops or starts (the first argument: 1 stops it)
	PauseMind,
	/// The creature takes notice of reactions near it, or not (the first argument: 1 takes notice)
	TakesNotice,
	/// The body plays an action (the index: the action from the first action)
	PlayAction,
	/// The body plays a gesture (the index: the gesture from the first gesture)
	PlayGesture,
	/// The face pulls a face (the index: the face from the first face)
	PullFace,
	/// The face shows a feeling (the index: creature_face::Cue)
	ShowFeeling,
	SitDown,
	StandUp,
	/// The player strokes the creature or slaps it (the amount: from -1, a slap, to 1, a stroke)
	Feedback,
	/// The creature forgets what it learnt
	ClearLearning,
	/// The trainer strokes the creature for each thing it does to a kind of thing (the index: creature_edit_acts::k_Trainers)
	Trainer,
	/// The creature sees a skill at its place (the index: the skill)
	SeeSkill,
	/// The creature sees a miracle at its place (the index: the miracle)
	SeeMiracle,
	/// The player does a deed near the place (the index: the deed)
	PlayerDeed,

	// The body
	/// The body of every creature lives more game turns each game turn (the index: creature_edit_acts::k_TimeScales)
	BodyTime,
	/// Grown up creatures of a player faint, or not (the first argument: 1 faints)
	Fainting,
	/// The body is full, rested and well
	FillUp,
	Starve,
	TireOut,
	Parch,
	Bloat,
	/// The body shows its fatness at once
	ShowFatnessNow,
	Sleep,
	Wake,
	Eat,
	Drink,
	Poo,
	Puke,
	Faint,
	/// A pot of food comes at the place
	DropFood,

	// The movement
	/// The creature goes to the place (the first argument: 1 runs)
	GoTo,
	/// The creature runs away from the place
	FleeFrom,
	/// The creature turns to the place
	Face,
	/// The creature picks up the other thing
	PickUp,
	/// The creature knocks down the other thing
	KnockDown,
	/// The creature throws what it holds at the place
	ThrowAt,
	/// The creature points at the place
	PointAt,
	/// The creature stops its walk
	StopMoving,

	// The hands
	/// A thing comes at the place, for the creature to pick up (the index: MobileObjectInfo)
	PutThing,
	PutDown,
	TossAway,
	EatHeld,
	Drop,
	/// The creature looks over what it holds (the index: the keep animation from the first)
	LookOver,
	/// The creature stops what it does with its hands
	StopHands,

	// The leash
	/// The creature gets an owner (the index: PlayerNames)
	Owner,
	/// The creature is the one that its owner can lead, or not (the first argument: 1 is)
	Leashable,
	/// The creature knows a leash, or not (the index: LeashType, the first argument: 1 knows)
	KnowsLeash,
	GrantAllLeashes,
	/// The hand puts a leash on the creature (the index: LeashType)
	PutOnLeash,
	TakeOffLeash,
	/// The leash ties the creature to the other thing
	TieTo,
	UntieToHand,
	/// The leash works, or not (the first argument: 1 works)
	LeashWorks,
	/// The ropes of every leash show, or not (the first argument: 1 shows)
	LeashDrawn,
	KeepAtHome,
	LetRoam,
	/// Three posts of the owner come at the place
	PlacePosts,

	// The fight
	/// The creature that is knocked out gets up
	BringRound,
	/// The creature fights the other creature (the first argument: 1 fights by itself)
	StartFight,
	KnockOut,
	KillForGood,
	/// The creature fights by itself, or not (the first argument: 1 fights by itself)
	FightsByItself,
	AbortFight,
	/// The creature hits (the index: fight::Band, the amount: the charge in milliseconds)
	Blow,
	Block,
	Special,
	/// The creature steps (the index: fight::Step)
	FightStep,

	// The looks
	/// A wound comes on the skin (the arguments: u, v, skin, kind, column)
	AddWound,
	/// A trail of blood comes on the skin, down from the point (the arguments: u, v, skin)
	AddBlood,
	/// The skin heals (the first argument: 1 heals as the heal effect does, 0 heals a step)
	Heal,

	Count,
};

inline constexpr size_t k_ActCount = static_cast<size_t>(Act::Count);

/// The name of each act, for the inspector and the logs
inline constexpr std::array<std::string_view, k_ActCount> k_ActNames {
    "pause mind",   "takes notice",   "play action",    "play gesture",  "pull face",   "show feeling",  "sit down",
    "stand up",     "feedback",       "clear learning", "trainer",       "see skill",   "see miracle",   "player deed",
    "body time",    "fainting",       "fill up",        "starve",        "tire out",    "parch",         "bloat",
    "show fatness", "sleep",          "wake",           "eat",           "drink",       "poo",           "puke",
    "faint",        "drop food",      "go to",          "flee from",     "face",        "pick up",       "knock down",
    "throw at",     "point at",       "stop moving",    "put thing",     "put down",    "toss away",     "eat held",
    "drop",         "look over",      "stop hands",     "owner",         "leashable",   "knows leash",   "grant leashes",
    "put on leash", "take off leash", "tie to",         "untie to hand", "leash works", "leash drawn",   "keep at home",
    "let roam",     "place posts",    "bring round",    "start fight",   "knock out",   "kill for good", "fights by itself",
    "abort fight",  "blow",           "block",          "special",       "fight step",  "add wound",     "add blood",
    "heal",
};
static_assert(!k_ActNames.back().empty(), "Each act needs a name");

/// The body time scales that the creature tools offer
inline constexpr std::array<float, 5> k_TimeScales {1.0f, 10.0f, 100.0f, 1000.0f, 3600.0f};

/// The count of the trainers: none, and one for each kind of thing that it rewards (see editor::TrainerKind)
inline constexpr size_t k_TrainerCount = 6;

/// The act of a number on the wire, or none for a number that no act has
[[nodiscard]] constexpr std::optional<Act> ActFrom(uint8_t number) noexcept
{
	if (number >= k_ActCount)
	{
		return std::nullopt;
	}
	return static_cast<Act>(number);
}

[[nodiscard]] constexpr std::string_view NameOf(Act act) noexcept
{
	const auto index = static_cast<size_t>(act);
	return index < k_ActCount ? k_ActNames.at(index) : std::string_view {"unknown act"};
}

/// The act of a name, or none for a name that no act has
[[nodiscard]] constexpr std::optional<Act> ActNamed(std::string_view name) noexcept
{
	for (size_t index = 0; index < k_ActCount; ++index)
	{
		if (k_ActNames.at(index) == name)
		{
			return static_cast<Act>(index);
		}
	}
	return std::nullopt;
}

/// Whether the act changes every creature or the rules of the game, so that it has no creature
[[nodiscard]] constexpr bool ActsOnEveryCreature(Act act) noexcept
{
	return act == Act::BodyTime || act == Act::Fainting || act == Act::LeashDrawn;
}

} // namespace openblack::creature_edit_acts
