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
#include <string_view>
#include <vector>

#include "Enums.h"
#include "Gestures/MiracleSelection.h"

/// Which gestures the hand is waiting for, and what each would do, as the game decides it once a frame. The game never
/// recognises a gesture it isn't waiting for: a circle only sizes a storm or a shield in the hand, a scribble only does
/// something when there is something for it to do. The requests are listed in the game's order; the first one drawn
/// is acted on, the path is forgotten and the hand rests a moment before it records again.
namespace openblack::gesture
{

/// After a gesture is recognised, the hand's path isn't recorded for this long
constexpr float k_RecognisedPauseSeconds = 0.4f;
/// A circle drawn for a storm or a shield is remembered this long
constexpr float k_CircleSeconds = 5.0f;

/// What a recognised gesture does
enum class Purpose : uint8_t
{
	/// A circle sizing the storm or shield held in the hand, drawn while the Action button is held
	SizeCircle,
	/// Powering the held seed up to a level
	PowerUp,
	/// A scribble with a seed in the hand: the power-up asked for is called off, or else the seed is dropped back where
	/// it came from
	DropSeed,
	/// A scribble with the leash held in the empty hand: the leash comes off
	ShakeOffLeash,
	/// A scribble with a thing in the hand, in the player's influence: the hand lets go of it where it is
	ShakeOffHeld,
	/// The leash gesture: the leash goes on, and the leash picker opens when the creature knows more than one
	LeashGesture,
	/// In the leash picker, a leash's gesture: the creature's leash changes to it
	PickLeash,
	/// In the leash picker, a scribble: the picker closes, leaving the leash on
	ClosePicker,
	/// While the player's creature duels: its special move is added to its fight's queue
	FightSpecialMove,
	/// While the player's creature duels: a miracle it knows is added to its fight's queue
	FightMiracle,
	/// In the leash picker on Land 1, the square wave: the hidden script starts. The picker stays open.
	HiddenScript,
	/// With the hand ready, a spiral or an inverse spiral: the miracle selection opens for the miracles of that gesture
	OpenSelection,
	/// In the open miracle selection, a gesture of the chain of a miracle
	SelectMiracle,
	/// In the open miracle selection, a scribble: the selection closes
	CloseSelection,
	/// With the hand ready, the R: the last miracle that came into the hand comes again from its icon
	RepeatMiracle,
	/// A scribble with the empty hand, while an icon of a worship site of the player charges a miracle for the hand: the
	/// icon that started its charge last stops it
	CancelCharge,
};

[[nodiscard]] std::string_view Name(Purpose purpose);
/// Whether recognising a gesture for a purpose is shown, with its sound and its trail on the land: every gesture but a
/// scribble, which only shakes something off the hand
[[nodiscard]] bool ShowsRecognition(Purpose purpose);
/// Whether the hand stops for a short time after it recognises a gesture for a purpose. Only the gesture that starts
/// the hidden script lets the hand record again at once.
[[nodiscard]] bool RestsAfter(Purpose purpose);
/// A recognised gesture is shown once at least this many of its points lie on the land
inline constexpr size_t k_RecognitionShownPoints = 2;
/// A point of the path lies on the land once any of its coordinates is further than this from 0
inline constexpr float k_OnLandEpsilon = 1e-4f;
/// A gesture's name, as the game's debug messages give it
[[nodiscard]] std::string_view Name(GestureType gesture);

/// A gesture waited for
struct Request
{
	GestureType gesture {GestureType::None};
	Purpose purpose {Purpose::DropSeed};
	/// PowerUp: the level, 0 to 2. DropSeed with a seed that can power up: the level that the seed has, -1 for none.
	int powerUpLevel {-1};
	/// PickLeash: the leash; LeashGesture: the leash worn, else the one picked to put on next
	LeashType leash {LeashType::None};
	/// FightMiracle: the miracle, and the seed of the miracle that its hint shows
	MagicType magic {MagicType::None};
	/// FightMiracle and RepeatMiracle: the seed that the hint of the gesture shows. SelectMiracle: the seed that the
	/// gesture selects. PowerUp, and DropSeed with a seed that can power up: the seed in the hand.
	SpellSeedType seed {SpellSeedType::None};
	/// OpenSelection: the gesture opens the selection for the miracles of the creature
	bool forCreature {false};

	bool operator==(const Request&) const = default;
};

/// What the requests depend on
struct HandContext
{
	/// The seed held in the hand
	struct Seed
	{
		SpellSeedType type {SpellSeedType::None};
		/// Held long enough to cast, and not yet cast
		bool ready {true};
		bool cast {false};
		/// The gesture that sizes what it casts, a circle for the storm and the shields
		GestureType sizingGesture {GestureType::None};
		/// The gestures that power it up to levels 0, 1 and 2, none where it has no such level
		std::array<GestureType, 3> powerUpGestures {GestureType::None, GestureType::None, GestureType::None};
		/// The level it casts at, -1 for its plain miracle
		int powerUp {-1};
		/// Whether it can be powered up at all. The game only powers up a seed charged at a worship site's icon.
		bool canPowerUp {true};
	};
	std::optional<Seed> seed;
	/// The point under the hand is in the player's influence: a seed that can't be powered up is only dropped by a
	/// scribble there
	bool inInfluence {true};
	/// A circle is remembered for the seed
	bool circleRemembered {false};
	/// The Action button is held: a storm or a shield is readied, and sized while it is held
	bool actionHeld {false};
	/// The hand holds a thing that isn't a miracle's seed
	bool holdingObject {false};
	/// An icon at a worship site of the player charges a miracle for this hand
	bool iconCharging {false};

	/// The player's creature
	struct Creature
	{
		bool fighting {false};
		bool leashed {false};
		/// The leash is tied to something rather than held in the hand
		bool tied {false};
		/// The leashes it knows: aggression, learning, compassion
		std::array<bool, 3> knows {false, false, false};
		LeashType worn {LeashType::None};
		/// The leash the leash gesture is about: the one worn, else the one picked to put on next
		LeashType current {LeashType::Rope};
		/// It duels and takes more moves: then the hand waits only for the fight's gestures
		bool takesFightMoves {false};
		/// The miracles it knows that can be cast in a fight, each with its gesture, in the order of the miracles
		struct FightMiracle
		{
			GestureType gesture {GestureType::None};
			MagicType magic {MagicType::None};
			SpellSeedType seed {SpellSeedType::None};
		};
		std::vector<FightMiracle> fightMiracles;
	};
	std::optional<Creature> creature;
	/// The leash picker is open
	bool pickerOpen {false};
	/// The land is Land 1 of the story, not a skirmish. Then the leash picker also waits for the square wave.
	bool hiddenScriptLand {false};

	/// The hand has space for a miracle: it holds nothing
	bool handReady {false};
	/// The miracle selection, while it is open
	struct Selection
	{
		/// The gesture that opened it
		GestureType openedWith {GestureType::None};
		/// The gestures that it waits for
		std::vector<SelectionWait> waiting;
		/// It stays open after this frame. When its time runs out in a frame, it still waits for its gestures in that
		/// frame, but the other gestures act as if it were closed.
		bool open {true};
	};
	std::optional<Selection> selection;
	/// The gestures that open the selection for the player's own miracles and for the miracles of the creature, and
	/// the gesture that calls the last miracle again
	GestureType ownSelectionGesture {GestureType::Spiral};
	GestureType creatureSelectionGesture {GestureType::InverseSpiral};
	GestureType repeatGesture {GestureType::RShape};
	/// An icon of the player can give a miracle for the opening gesture of the player's own miracles, and of the
	/// creature's miracles
	bool ownSelectionOffered {false};
	bool creatureSelectionOffered {false};
	/// The last miracle that came into the hand, and whether an icon of the player can give it again
	SpellSeedType lastSeed {SpellSeedType::None};
	bool repeatOffered {false};

	/// The gesture that calls a creature's special move in a fight
	GestureType specialMoveGesture {GestureType::Star};
	/// The gesture that puts the leash on, and each leash's gesture in the picker, by leash number (0 is unused)
	GestureType leashGesture {GestureType::SquareSpirial};
	std::array<GestureType, 4> leashGestures {GestureType::None, GestureType::VerticalScribble, GestureType::EShape,
	                                          GestureType::Heart};
};

/// How many leashes a creature knows
[[nodiscard]] int KnownLeashes(const HandContext::Creature& creature);
/// The script that the square wave starts
inline constexpr std::string_view k_HiddenScript = "HiddenScript";
/// The gestures waited for, in the order they are tried
[[nodiscard]] std::vector<Request> Requests(const HandContext& context);
/// The event that the help counts when the hand draws the gesture of a request, none for most gestures. A scribble
/// counts only when it is about a miracle that the hand can power up.
[[nodiscard]] std::optional<uint32_t> HelpEventOf(const Request& request, const HandContext& context);
/// What a gesture of the miracle selection, or the R, did
enum class SelectionStep : uint8_t
{
	/// The selection had nothing to offer and stays closed, or no miracle of the open selection has the gesture
	Nothing,
	/// A spiral or an inverse spiral opens the selection
	Opened,
	/// A miracle of the selection has the gesture, and more gestures follow it in the chain of the miracle
	Advanced,
	/// The gesture is the last of the chain of a miracle: the hand asks the icon of the miracle for it
	Chosen,
	/// A scribble closes the selection
	Closed,
	/// The R asks the icon again for the last miracle that came into the hand
	Repeated,
};
/// The events that the help counts, in their order, for a step of the miracle selection. A choice counts as a step of
/// the chain first.
[[nodiscard]] std::vector<uint32_t> HelpEventsOf(SelectionStep step);
/// Whether the chain follows the hand. It follows the hand while the hand holds a miracle that it can power up or that a
/// circle sizes. After a miracle did its first turn in the hand on the land, it follows the hand while it holds any
/// miracle.
[[nodiscard]] bool ChainFollows(const std::optional<HandContext::Seed>& seed, bool seedTurnedOnLand);
/// Whether the help counts a cast of the miracle in the hand as a cast with a power-up. The miracle must be ready, not
/// cast, from an icon and powered up. A miracle that a gesture sizes must have that gesture as the last gesture drawn.
[[nodiscard]] bool CountsPoweredUpCast(const HandContext::Seed& seed, GestureType lastGesture);

/// The leash picker: open after the leash gesture while the creature knows more than one leash, offering each other
/// leash it knows as a gesture to draw. It closes when a leash is picked, on a scribble, when the leash comes off, or
/// after a while.
struct LeashPicker
{
	bool open {false};
	float seconds {0.0f};

	void Open();
	void Close();
	/// Time passes: it closes once open longer than the timeout, or once the leash is off
	void Update(float elapsed, float timeoutSeconds, bool leashed);
};

} // namespace openblack::gesture
