/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "EndGameBoxRules.h"

#include <array>
#include <string>

namespace openblack::end_game_box
{

namespace
{
/// An end box opens: it closes the box with OK
void OpenEndBox(Opened& opened, State& state, Box box)
{
	opened.godOut.reset();
	opened.endBox = box;
	state.endBoxShowing = true;
}
} // namespace

Opened Follow(std::span<const game_end::Event> events, PlayerNames local, State& state)
{
	using game_end::Event;
	Opened opened;
	for (const auto& event : events)
	{
		switch (event.kind)
		{
		case Event::Kind::Won:
			// The game is decided by its conditions or its time: each other god among the first four lost
			OpenEndBox(opened, state,
			           {.kind = event.god != local && !LooksOn(local) ? Kind::LostAndDecided : Kind::Won, .watch = false});
			break;
		case Event::Kind::GodOut:
			if (event.god == local)
			{
				state.gameGoesOn = true;
				OpenEndBox(opened, state, {.kind = Kind::Lost, .watch = true});
			}
			else if (!state.endBoxShowing)
			{
				opened.godOut = event.god;
			}
			break;
		case Event::Kind::GodOutAndDecided:
			if (event.god == local)
			{
				state.gameGoesOn = false;
				OpenEndBox(opened, state, {.kind = Kind::LostAndDecided, .watch = false});
			}
			else
			{
				// Also when the god of this computer is out before (vanilla-bugs 469)
				OpenEndBox(opened, state, {.kind = Kind::Won, .watch = false});
			}
			break;
		}
	}
	return opened;
}

Box AskToLeave(State& state)
{
	state.gameGoesOn = true;
	state.endBoxShowing = true;
	return {.kind = Kind::LeaveQuestion, .watch = false};
}

void StoppedShowing(State& state)
{
	state.endBoxShowing = false;
}

bool StopsTheGame(const Box& box, const State& state)
{
	return box.kind != Kind::LeaveQuestion && !state.gameGoesOn;
}

std::u16string Format(std::u16string_view text, std::span<const std::u16string_view> words)
{
	std::u16string out;
	size_t next = 0;
	for (size_t i = 0; i < text.size(); ++i)
	{
		if (text[i] == u'%' && i + 1 < text.size() && text[i + 1] == u's')
		{
			if (next < words.size())
			{
				out += words[next];
			}
			++next;
			++i;
			continue;
		}
		out += text[i];
	}
	return out;
}

std::u16string Title(Kind kind, bool looksOn, std::u16string_view winner, const Titles& titles)
{
	switch (kind)
	{
	case Kind::LeaveQuestion:
		return std::u16string(titles.leave);
	case Kind::Won:
		if (looksOn)
		{
			const std::array<std::u16string_view, 2> words {titles.gameOver, winner};
			return Format(titles.winnerFormat, words);
		}
		return std::u16string(titles.won);
	case Kind::Lost:
	case Kind::LostAndDecided:
		break;
	}
	return std::u16string(titles.lost);
}

std::u16string GodName(PlayerNames god, std::optional<std::u16string_view> humanName)
{
	if (humanName.has_value() && !humanName->empty())
	{
		return std::u16string(*humanName);
	}
	std::u16string name = u"Player[";
	for (const auto digit : std::to_string(static_cast<int>(god) + 1))
	{
		name += static_cast<char16_t>(digit);
	}
	return name + u"]";
}

} // namespace openblack::end_game_box
