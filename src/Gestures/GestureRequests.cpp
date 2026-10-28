/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GestureRequests.h"

#include <algorithm>
#include <tuple>
#include <utility>

#include "Help/HelpProfile.h"

using namespace openblack;
using namespace openblack::gesture;

std::string_view gesture::Name(Purpose purpose)
{
	switch (purpose)
	{
	case Purpose::SizeCircle:
		return "size the miracle";
	case Purpose::PowerUp:
		return "power up";
	case Purpose::DropSeed:
		return "drop the seed";
	case Purpose::ShakeOffLeash:
		return "shake the leash off";
	case Purpose::ShakeOffHeld:
		return "shake off what is held";
	case Purpose::LeashGesture:
		return "leash";
	case Purpose::PickLeash:
		return "pick a leash";
	case Purpose::ClosePicker:
		return "close the picker";
	case Purpose::FightSpecialMove:
		return "special move";
	case Purpose::FightMiracle:
		return "fight miracle";
	case Purpose::HiddenScript:
		return "hidden script";
	case Purpose::OpenSelection:
		return "open the miracle selection";
	case Purpose::SelectMiracle:
		return "select a miracle";
	case Purpose::CloseSelection:
		return "close the miracle selection";
	case Purpose::RepeatMiracle:
		return "repeat the last miracle";
	}
	return "";
}

bool gesture::ShowsRecognition(Purpose purpose)
{
	switch (purpose)
	{
	case Purpose::DropSeed:
	case Purpose::ShakeOffLeash:
	case Purpose::ShakeOffHeld:
	case Purpose::ClosePicker:
	case Purpose::HiddenScript:
	case Purpose::CloseSelection:
		return false;
	case Purpose::SizeCircle:
	case Purpose::PowerUp:
	case Purpose::LeashGesture:
	case Purpose::PickLeash:
	case Purpose::FightSpecialMove:
	case Purpose::FightMiracle:
	case Purpose::OpenSelection:
	case Purpose::SelectMiracle:
	case Purpose::RepeatMiracle:
		return true;
	}
	return false;
}

bool gesture::RestsAfter(Purpose purpose)
{
	return purpose != Purpose::HiddenScript;
}

std::string_view gesture::Name(GestureType gesture)
{
	constexpr std::array<std::string_view, 24> k_Names {
	    "none",
	    "spiral",
	    "inverse spiral",
	    "S",
	    "circle",
	    "scribble",
	    "three",
	    "vertical scribble",
	    "star",
	    "fork right",
	    "fork up",
	    "fork left",
	    "fork down",
	    "heart",
	    "R",
	    "square spiral",
	    "Cyrillic L",
	    "E",
	    "reverse S",
	    "infinity",
	    "W",
	    "house",
	    "inverse square spiral",
	    "square wave",
	};
	const auto index = static_cast<size_t>(gesture);
	return index < k_Names.size() ? k_Names.at(index) : "unknown";
}

int gesture::KnownLeashes(const HandContext::Creature& creature)
{
	return static_cast<int>(std::ranges::count(creature.knows, true));
}

std::vector<Request> gesture::Requests(const HandContext& context)
{
	std::vector<Request> requests;
	const auto& seed = context.seed;

	// While the player's creature duels, the hand waits only for its special move and the fight miracles it knows
	if (context.creature.has_value() && context.creature->takesFightMoves)
	{
		if (context.specialMoveGesture != GestureType::None)
		{
			requests.push_back({.gesture = context.specialMoveGesture, .purpose = Purpose::FightSpecialMove});
		}
		for (const auto& miracle : context.creature->fightMiracles)
		{
			if (miracle.gesture != GestureType::None)
			{
				requests.push_back({.gesture = miracle.gesture,
				                    .purpose = Purpose::FightMiracle,
				                    .magic = miracle.magic,
				                    .seed = miracle.seed});
			}
		}
		return requests;
	}

	// A circle for the storm or shield in the hand, drawn while the Action button is held, until one is remembered
	if (context.actionHeld && seed.has_value() && seed->sizingGesture != GestureType::None && !context.circleRemembered)
	{
		requests.push_back({.gesture = seed->sizingGesture, .purpose = Purpose::SizeCircle});
	}

	const bool poweringUp = seed.has_value() && seed->ready && !seed->cast && seed->canPowerUp;
	if (poweringUp)
	{
		// A scribble calls off the power-up asked for, or drops the seed; each other level's gesture powers it up
		requests.push_back({.gesture = GestureType::Scribble,
		                    .purpose = Purpose::DropSeed,
		                    .powerUpLevel = seed->powerUp,
		                    .seed = seed->type});
		for (int level = 0; level < static_cast<int>(seed->powerUpGestures.size()); ++level)
		{
			const auto gesture = seed->powerUpGestures.at(static_cast<size_t>(level));
			if (gesture != GestureType::None && level != seed->powerUp)
			{
				requests.push_back(
				    {.gesture = gesture, .purpose = Purpose::PowerUp, .powerUpLevel = level, .seed = seed->type});
			}
		}
	}
	else if (context.selection.has_value())
	{
		// The selection: a scribble closes it; each gesture that follows in the chain of a miracle it offers is waited for
		requests.push_back({.gesture = GestureType::Scribble, .purpose = Purpose::CloseSelection});
		for (const auto& wait : context.selection->waiting)
		{
			requests.push_back({.gesture = wait.gesture, .purpose = Purpose::SelectMiracle, .seed = wait.seed});
		}
	}
	else if (context.pickerOpen && context.creature.has_value() && context.creature->leashed)
	{
		// The picker: a scribble closes it; each other leash the creature knows is a gesture away
		requests.push_back({.gesture = GestureType::Scribble, .purpose = Purpose::ClosePicker});
		// On Land 1, the square wave starts the hidden script. The picker shows no hint for it.
		if (context.hiddenScriptLand)
		{
			requests.push_back({.gesture = GestureType::SquareWave, .purpose = Purpose::HiddenScript});
		}
		for (size_t i = 0; i < context.creature->knows.size(); ++i)
		{
			const auto leash = static_cast<LeashType>(i + 1);
			const auto gesture = context.leashGestures.at(i + 1);
			if (context.creature->knows.at(i) && leash != context.creature->worn && gesture != GestureType::None)
			{
				requests.push_back({.gesture = gesture, .purpose = Purpose::PickLeash, .leash = leash});
			}
		}
	}

	if (!poweringUp)
	{
		if (!seed.has_value() && context.holdingObject)
		{
			// A thing in the hand is shaken out of it, only in the player's influence
			if (context.inInfluence)
			{
				requests.push_back({.gesture = GestureType::Scribble, .purpose = Purpose::ShakeOffHeld});
			}
		}
		else if (!seed.has_value())
		{
			// The empty hand shakes off the leash it holds; one tied to something stays
			if (context.creature.has_value() && context.creature->leashed && !context.creature->tied)
			{
				requests.push_back({.gesture = GestureType::Scribble, .purpose = Purpose::ShakeOffLeash});
			}
		}
		else if (context.inInfluence)
		{
			// Out of the player's influence the seed can't be scribbled away
			requests.push_back({.gesture = GestureType::Scribble, .purpose = Purpose::DropSeed});
		}
	}

	// While the player's creature fights, no gesture opens the selection, calls a miracle again or puts the leash on
	const bool fighting = context.creature.has_value() && context.creature->fighting;
	const bool selectionOpen = context.selection.has_value() && context.selection->open;

	// With the hand ready, a spiral opens the selection for the player's own miracles and an inverse spiral for the
	// creature's. The gesture that opened the selection is not waited for again while it is open.
	if (!fighting && context.handReady)
	{
		for (const auto& [gesture, offered, forCreature] :
		     {std::tuple {context.ownSelectionGesture, context.ownSelectionOffered, false},
		      std::tuple {context.creatureSelectionGesture, context.creatureSelectionOffered, true}})
		{
			if (offered && gesture != GestureType::None && (!selectionOpen || context.selection->openedWith != gesture))
			{
				requests.push_back({.gesture = gesture, .purpose = Purpose::OpenSelection, .forCreature = forCreature});
			}
		}
	}

	// With the hand ready and the selection closed, the R calls the last miracle again
	if (!fighting && !selectionOpen && context.handReady && context.repeatGesture != GestureType::None && context.repeatOffered)
	{
		requests.push_back({.gesture = context.repeatGesture, .purpose = Purpose::RepeatMiracle, .seed = context.lastSeed});
	}

	// The leash gesture: on an unleashed creature that knows a leash, or to change the leash of one that knows two
	if (context.creature.has_value() && !context.creature->fighting && !context.pickerOpen && !selectionOpen &&
	    context.leashGesture != GestureType::None)
	{
		const auto known = KnownLeashes(*context.creature);
		if ((context.creature->leashed && known > 1) || (!context.creature->leashed && known > 0))
		{
			requests.push_back(
			    {.gesture = context.leashGesture, .purpose = Purpose::LeashGesture, .leash = context.creature->current});
		}
	}
	return requests;
}

namespace
{
/// Whether the hand holds a miracle that its gestures can power up
bool PowersUp(const HandContext& context)
{
	const auto& seed = context.seed;
	return seed.has_value() && seed->ready && !seed->cast && seed->canPowerUp;
}
} // namespace

std::optional<uint32_t> gesture::HelpEventOf(const Request& request, const HandContext& context)
{
	switch (request.purpose)
	{
	case Purpose::PowerUp:
		return help::profile::k_GesturePowerUp;
	case Purpose::FightSpecialMove:
		return help::profile::k_GestureSpecialMove;
	case Purpose::DropSeed:
		// A scribble shakes a miracle that the hand cannot power up out of the hand, and the help does not count it
		if (!PowersUp(context))
		{
			return std::nullopt;
		}
		return context.seed->powerUp >= 0 ? help::profile::k_GestureStopPowerUp : help::profile::k_GestureDropMiracle;
	default:
		return std::nullopt;
	}
}

std::vector<uint32_t> gesture::HelpEventsOf(SelectionStep step)
{
	namespace profile = help::profile;
	switch (step)
	{
	case SelectionStep::Opened:
		return {profile::k_GestureOpenSelection};
	case SelectionStep::Advanced:
		return {profile::k_GestureSelectionStep};
	case SelectionStep::Chosen:
		return {profile::k_GestureSelectionStep, profile::k_GestureSelectionChosen};
	case SelectionStep::Closed:
		return {profile::k_GestureCloseSelection};
	case SelectionStep::Repeated:
		return {profile::k_GestureRepeatMiracle};
	case SelectionStep::Nothing:
		break;
	}
	return {};
}

bool gesture::ChainFollows(const std::optional<HandContext::Seed>& seed, bool seedTurnedOnLand)
{
	if (!seed.has_value())
	{
		return false;
	}
	return (seed->ready && !seed->cast && seed->canPowerUp) || seed->sizingGesture == GestureType::Circle || seedTurnedOnLand;
}

bool gesture::CountsPoweredUpCast(const HandContext::Seed& seed, GestureType lastGesture)
{
	if (!seed.ready || seed.cast || !seed.canPowerUp || seed.powerUp < 0)
	{
		return false;
	}
	return seed.sizingGesture == GestureType::None || lastGesture == seed.sizingGesture;
}

void LeashPicker::Open()
{
	open = true;
	seconds = 0.0f;
}

void LeashPicker::Close()
{
	open = false;
	seconds = 0.0f;
}

void LeashPicker::Update(float elapsed, float timeoutSeconds, bool leashed)
{
	if (!open)
	{
		return;
	}
	seconds += elapsed;
	if (seconds > timeoutSeconds || !leashed)
	{
		Close();
	}
}
