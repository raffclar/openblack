/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

/// What of the last land goes, and what stays, when another land is loaded. Black & White clears the land the same way
/// every time, but around that it does more or less depending on why the land changes: the story moving on keeps
/// everything that isn't the land's, the land menu starts the scripts again first, and a new game starts everything.
namespace openblack::land_change
{

/// Why a land is being loaded
enum class Kind
{
	/// The story moves on to its next land: the scripts go on running
	Story,
	/// A land loaded from the land menu: the challenge's scripts start again from scratch first
	FreshScripts,
	/// A new game, the game's first start included: everything starts afresh
	NewGame,
	/// The developers' testbed: a workbench outside the game, where nothing carries over
	Testbed,
};

/// What becomes of the two advisors
enum class Advisors
{
	/// They carry on as they were: out, flying home or at home
	CarryOn,
	/// They are sent home, flying there as they always do
	SentHome,
	/// They start at home, out of sight, as they do in a new game
	Afresh,
};

struct Rules
{
	Advisors advisors;
	/// The scripts' hold on the screen stays: their fade, their cinema bars, their texts on screen, the interface
	/// switched off
	bool scriptScreenKept;
	/// Each player's heartbeat beats on as it was, rather than starting calm
	bool heartbeatsCarried;
	/// The players go on as they were: their alignment, the miracles they have ever had, their histories. Otherwise they
	/// start afresh, neither good nor evil, with no miracles and no history
	bool playersCarried;
	/// The player starts as good or evil as their profile last kept them
	bool alignmentFromProfile;
	/// A new game starts: it is played as the profile picked now, which keeps its creature from now on, the last game's
	/// creature having been kept for the profile that game was played as
	bool gameStarts;
};

[[nodiscard]] constexpr Rules RulesFor(Kind kind)
{
	switch (kind)
	{
	case Kind::Story:
		return {.advisors = Advisors::CarryOn,
		        .scriptScreenKept = true,
		        .heartbeatsCarried = true,
		        .playersCarried = true,
		        .alignmentFromProfile = false,
		        .gameStarts = false};
	case Kind::FreshScripts:
		return {.advisors = Advisors::SentHome,
		        .scriptScreenKept = false,
		        .heartbeatsCarried = true,
		        .playersCarried = true,
		        .alignmentFromProfile = false,
		        .gameStarts = false};
	case Kind::NewGame:
		return {.advisors = Advisors::Afresh,
		        .scriptScreenKept = false,
		        .heartbeatsCarried = false,
		        .playersCarried = false,
		        .alignmentFromProfile = true,
		        .gameStarts = true};
	case Kind::Testbed:
		break;
	}
	return {.advisors = Advisors::Afresh,
	        .scriptScreenKept = false,
	        .heartbeatsCarried = false,
	        .playersCarried = false,
	        .alignmentFromProfile = false,
	        .gameStarts = false};
}

} // namespace openblack::land_change
