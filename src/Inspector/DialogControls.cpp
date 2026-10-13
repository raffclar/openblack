/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DialogControls.h"

#include <cctype>
#include <cstdint>

#include <algorithm>
#include <optional>

#include "Gui/SkipBox.h"
#include "Story/NewGameChoice.h"

using namespace openblack;
using namespace openblack::inspector;

namespace
{

std::string Lower(std::string_view text)
{
	std::string lower(text);
	std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return lower;
}

bool Answers(const DialogControl& control, std::string_view wanted)
{
	return Lower(control.name) == wanted ||
	       std::ranges::any_of(control.aliases, [wanted](const std::string& alias) { return Lower(alias) == wanted; });
}

} // namespace

std::string openblack::inspector::PlainText(std::u16string_view text)
{
	std::string plain;
	bool space = false;
	for (const auto c : text)
	{
		if (c == u' ' || c == u'\n' || c == u'\r' || c == u'\t')
		{
			space = !plain.empty();
			continue;
		}
		if (space)
		{
			plain.push_back(' ');
			space = false;
		}
		plain.push_back(c < 0x80 ? static_cast<char>(c) : '?');
	}
	return plain;
}

std::vector<DialogControl> openblack::inspector::DialogControls(std::span<const gui::Dialog::NamedControl> controls)
{
	std::vector<DialogControl> found;
	found.reserve(controls.size());
	for (const auto& control : controls)
	{
		// A control labelled with spaces only (the tattoo editor's turning button) has no name to press it by
		if (PlainText(control.name).empty())
		{
			continue;
		}
		found.push_back({.name = PlainText(control.name),
		                 .kind = control.kind,
		                 .middle = (control.rect.min + control.rect.max) / 2,
		                 .aliases = {}});
	}
	return found;
}

std::vector<DialogControl> openblack::inspector::SkipBoxControls(const gui::SkipBox& box)
{
	auto controls = DialogControls(box.GetNamedControls());
	// The radio buttons come in the answers' order, those not on offer (keeping an old creature, for a profile without
	// one) left out, so each is matched to the next answer the box offers
	size_t answer = 0;
	for (auto& control : controls)
	{
		if (control.kind != "radio")
		{
			continue;
		}
		while (answer < new_game_choice::k_ChoiceCount && !box.IsOffered(static_cast<new_game_choice::Choice>(answer)))
		{
			++answer;
		}
		if (answer < new_game_choice::k_ChoiceCount)
		{
			control.aliases.emplace_back(new_game_choice::StartName(static_cast<new_game_choice::Choice>(answer++)));
		}
	}
	return controls;
}

std::string openblack::inspector::SkipBoxPicked(const gui::SkipBox& box)
{
	const auto controls = SkipBoxControls(box);
	const auto start = std::string(new_game_choice::StartName(box.GetSelection()));
	const auto picked = std::ranges::find_if(controls, [&start](const DialogControl& control) {
		return control.kind == "radio" && std::ranges::find(control.aliases, start) != control.aliases.end();
	});
	return picked != controls.end() ? picked->name : std::string();
}

std::optional<glm::ivec2> openblack::inspector::SkipBoxOk(const gui::SkipBox& box)
{
	const auto controls = SkipBoxControls(box);
	const auto ok = std::ranges::find(controls, std::string("button"), &DialogControl::kind);
	if (ok == controls.end())
	{
		return std::nullopt;
	}
	return ok->middle;
}

std::variant<glm::ivec2, std::string> openblack::inspector::FindDialogControl(std::span<const DialogControl> controls,
                                                                              const std::vector<ButtonPathStep>& path)
{
	if (path.empty() || path.size() > 2 || !std::holds_alternative<std::string>(path.back()) ||
	    (path.size() == 2 && !std::holds_alternative<int32_t>(path.front())))
	{
		return std::string("press a control by its name, with path [n] for the n-th of that name from 0");
	}
	const auto& label = std::get<std::string>(path.back());
	const auto wanted = Lower(label);
	auto which = path.size() == 2 ? std::get<int32_t>(path.front()) : 0;
	for (const auto& control : controls)
	{
		if (Answers(control, wanted) && which-- == 0)
		{
			return control.middle;
		}
	}
	return "no control " + label;
}

std::array<InputEvent, 3> openblack::inspector::ClickAt(glm::ivec2 screen)
{
	return {{
	    {.kind = InputEvent::Kind::PointerTo, .position = screen},
	    {.kind = InputEvent::Kind::ButtonDown, .button = 1},
	    {.kind = InputEvent::Kind::ButtonUp, .button = 1},
	}};
}
