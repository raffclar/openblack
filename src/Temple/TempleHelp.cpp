/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleHelp.h"

#include <array>

namespace openblack::temple_help
{

namespace
{
constexpr std::string_view k_WorldRoomHelp = "CitadelWorldRoomHelp";
constexpr std::string_view k_WorldScrollHelp = "CitadelWorldRoomScrollHelp";
constexpr std::string_view k_ChallengeScrollHelp = "CitadelChallengeRoomScrollHelp";
constexpr std::string_view k_SaveGameScrollHelp = "CitadelSaveGameRoomScrollHelp";
constexpr std::string_view k_AttributesScrollHelp = "CitadelCreatureRoomAttributesScroll";
constexpr std::string_view k_ActionsScrollHelp = "CitadelCreatureRoomActionsLearntScroll";
constexpr std::string_view k_LikesScrollHelp = "CitadelCreatureRoomLikesScroll";
constexpr std::string_view k_MagicScrollHelp = "CitadelCreatureRoomMagicScroll";

/// The main room's camera stops its scroll's help
constexpr std::array k_MainRoomScrollHelps {k_WorldScrollHelp};
/// The challenge and save game rooms share a camera, which stops both rooms' scroll help
constexpr std::array k_ChallengeRoomScrollHelps {k_ChallengeScrollHelp, k_SaveGameScrollHelp};
/// The creature's room's camera stops the help of all four of its scrolls
constexpr std::array k_CreatureRoomScrollHelps {k_AttributesScrollHelp, k_LikesScrollHelp, k_MagicScrollHelp,
                                                k_ActionsScrollHelp};
} // namespace

std::optional<std::string_view> RoomHelp(TempleRoom room)
{
	switch (room)
	{
	case TempleRoom::Main:
	case TempleRoom::Options:
		return k_WorldRoomHelp;
	case TempleRoom::CreatureCave:
		return "CitadelCreatureRoomHelp";
	case TempleRoom::Challenge:
		return "CitadelChallengeRoomHelp";
	case TempleRoom::SaveGame:
		return "CitadelSaveGameRoomHelp";
	case TempleRoom::Credits:
		return "CitadelCreditsRoomHelp";
	default:
		// The multiplayer room has none
		return std::nullopt;
	}
}

std::optional<std::string_view> ScrollHelp(TempleScrolls::Content content)
{
	using Content = TempleScrolls::Content;
	switch (content)
	{
	case Content::World:
		return k_WorldScrollHelp;
	case Content::CreatureAttributes:
		return k_AttributesScrollHelp;
	case Content::CreatureActions:
		return k_ActionsScrollHelp;
	case Content::CreatureMind:
		return k_LikesScrollHelp;
	case Content::CreatureMiracles:
		return k_MagicScrollHelp;
	case Content::Challenge:
		return k_ChallengeScrollHelp;
	case Content::SaveGame:
		return k_SaveGameScrollHelp;
	default:
		// The library's scrolls start no help
		return std::nullopt;
	}
}

std::span<const std::string_view> ScrollHelpsStoppedIn(TempleRoom room)
{
	switch (room)
	{
	case TempleRoom::Main:
		return k_MainRoomScrollHelps;
	case TempleRoom::Challenge:
	case TempleRoom::SaveGame:
		return k_ChallengeRoomScrollHelps;
	case TempleRoom::CreatureCave:
		return k_CreatureRoomScrollHelps;
	default:
		return {};
	}
}

std::optional<std::string_view> CaveTargetHelp(CreatureCaveTargets::Target target)
{
	using CreatureCaveTargets::Target;
	switch (target)
	{
	case Target::Belts:
		return "CitadelCreatureRoomAttackDummies";
	case Target::Medals:
		return "CitadelCreatureRoomMagicPlinths";
	default:
		return std::nullopt;
	}
}

void EnterRoom(Scripts& scripts, TempleRoom room, bool helpSystemOn)
{
	scripts.StopHelp();
	if (const auto help = RoomHelp(room); help.has_value() && helpSystemOn)
	{
		scripts.Start(*help);
	}
}

void LookAtScroll(Scripts& scripts, TempleScrolls::Content content)
{
	if (const auto help = ScrollHelp(content); help.has_value())
	{
		scripts.Start(*help);
	}
}

void LeaveScroll(Scripts& scripts, TempleRoom room)
{
	for (const auto name : ScrollHelpsStoppedIn(room))
	{
		scripts.Stop(name);
	}
}

void ZoomToCaveTarget(Scripts& scripts, CreatureCaveTargets::Target target)
{
	if (const auto help = CaveTargetHelp(target); help.has_value())
	{
		scripts.Start(*help);
	}
}

void Leave(Scripts& scripts)
{
	scripts.StopHelp();
}

bool TurnDue(std::optional<uint32_t>& last, uint32_t now)
{
	if (!last.has_value())
	{
		last = now;
	}
	if (now - *last <= k_TurnMilliseconds)
	{
		return false;
	}
	*last += k_TurnMilliseconds;
	if (now - *last > 2 * k_TurnMilliseconds)
	{
		last = now;
	}
	return true;
}

} // namespace openblack::temple_help
