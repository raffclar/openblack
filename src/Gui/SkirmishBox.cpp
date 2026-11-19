/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SkirmishBox.h"

#include <algorithm>

#include "Controls.h"
#include "SkirmishBoxRules.h"
#include "TextDatabase.h"

using namespace openblack::gui;

namespace
{
constexpr float k_FadeInSeconds = 0.5f;
constexpr float k_FadeOutSeconds = 0.2f;
/// The band under the title: light grey at half the opacity of the box
constexpr glm::vec4 k_BandColour {224.0f / 255.0f, 224.0f / 255.0f, 224.0f / 255.0f, 0.5f};

constexpr std::string_view k_TitleText = "HELP_TEXT_DIALOG_ADDITION_26";
constexpr std::string_view k_BackText = "HELP_TEXT_DIALOG_BACK";
constexpr std::string_view k_StartText = "HELP_TEXT_DIALOG_LOBBYSTARTGAME";
} // namespace

SkirmishBox::SkirmishBox(const TextDatabase& texts, const GameFont& font)
{
	using Look = BigButton::Look;
	using LabelSide = BigButton::LabelSide;
	// The box makes its controls in the order of the game. The box's text is the big size. The title takes the clicks of
	// the mouse but does nothing.
	auto& title = _dialog.Add<StaticText>(k_Title, std::u16string(texts.Get(k_TitleText)), StaticText::Layout::Wrapped);
	title.takesClicks = true;

	auto& back = _dialog.Add<BigButton>(font, k_BackPosition, k_ArrowSize, std::u16string(texts.Get(k_BackText)),
	                                    LabelSide::Right, Look::LeftArrow);
	back.SetLabelSize(DialogPainter::k_BigTextSize);
	back.onClick = [this] {
		_answer = {.choice = Choice::Back, .land = std::nullopt};
		Hide();
	};

	_start = &_dialog.Add<BigButton>(font, k_StartPosition, k_ArrowSize, std::u16string(texts.Get(k_StartText)),
	                                 LabelSide::Left, Look::RightArrow);
	_start->SetLabelSize(DialogPainter::k_BigTextSize);
	_start->onClick = [this] {
		if (!StartShows())
		{
			return;
		}
		_answer = {.choice = Choice::Start, .land = _list->GetSelection()};
		Hide();
	};

	// The scroll bar shows only while the lands do not fit the box
	_list = &_dialog.Add<List>(font, k_List, DialogPainter::k_BigTextSize, false, true, true);
	_list->ShowScrollBarWhenNeeded();
	_start->visible = false;
}

SkirmishBox::~SkirmishBox() = default;

void SkirmishBox::Show(std::vector<std::u16string> lands)
{
	std::vector<List::Item> items;
	items.reserve(lands.size());
	for (auto& land : lands)
	{
		items.push_back({.text = std::move(land), .mouseCell = std::nullopt, .mouseMirrored = false, .colour = std::nullopt});
	}
	// The list drops a pick past its end
	_list->SetItems(std::move(items));
	_answer = {};
	_active = true;
	_dialog.Reset();
	FollowSelection();
	_fade.Reset(0.0f);
	_fade.SetDestination(1.0f, k_FadeInSeconds);
}

void SkirmishBox::Hide()
{
	_active = false;
	_dialog.Reset();
	_fade.SetDestination(0.0f, k_FadeOutSeconds);
}

std::optional<size_t> SkirmishBox::GetSelection() const noexcept
{
	return _list->GetSelection();
}

void SkirmishBox::Select(std::optional<size_t> selection)
{
	_list->SetSelection(selection.has_value() && *selection < _list->GetItems().size() ? selection : std::nullopt);
	_list->ScrollToSelection();
	FollowSelection();
}

bool SkirmishBox::StartShows() const noexcept
{
	return skirmish_box::StartShows(_list->GetSelection(), _list->GetItems().size());
}

void SkirmishBox::FollowSelection()
{
	_start->visible = StartShows();
}

void SkirmishBox::MouseMove(glm::ivec2 point)
{
	if (_active)
	{
		_dialog.MouseMove(point);
	}
}

void SkirmishBox::MouseDown(glm::ivec2 point)
{
	if (_active)
	{
		_dialog.MouseDown(point);
		FollowSelection();
	}
}

bool SkirmishBox::MouseUp(glm::ivec2 point)
{
	if (!_active)
	{
		return false;
	}
	const bool acted = _dialog.MouseUp(point);
	FollowSelection();
	return acted;
}

void SkirmishBox::Wheel(glm::ivec2 point, int steps)
{
	if (_active)
	{
		_dialog.Wheel(point, steps);
	}
}

void SkirmishBox::KeyDown(int key, bool shift)
{
	if (_active)
	{
		_dialog.KeyDown(key, shift);
		FollowSelection();
	}
}

void SkirmishBox::Update(float deltaSeconds)
{
	_fade.Update(deltaSeconds);
	if (_active)
	{
		_dialog.Update(deltaSeconds);
		FollowSelection();
	}
}

void SkirmishBox::Draw(const DialogPainter& painter) const
{
	if (!IsVisible())
	{
		return;
	}
	const auto alpha = std::clamp(_fade.GetValue(), 0.0f, 1.0f);
	painter.SetAlpha(alpha);
	// The band under the title comes first, under the box
	const auto& band = k_Title;
	painter.DrawShape(
	    {glm::vec2(band.min), glm::vec2(band.max.x, band.min.y), glm::vec2(band.max), glm::vec2(band.min.x, band.max.y)},
	    {k_BandColour, k_BandColour, k_BandColour, k_BandColour});
	painter.DrawBackground(k_Box, glm::vec3(1.0f), false, DialogPainter::All);
	painter.SetAlpha(alpha);
	// While the box fades out, no control lights up
	_dialog.Draw(painter, _active);
	painter.SetAlpha(1.0f);
}

std::vector<Dialog::NamedControl> SkirmishBox::GetNamedControls() const
{
	if (!_active)
	{
		return {};
	}
	auto controls = _dialog.GetNamedControls();
	// The lands that show in the list, each over the height of its line
	const auto& items = _list->GetItems();
	std::optional<size_t> last;
	for (int y = _list->rect.min.y; y < _list->rect.max.y; ++y)
	{
		const auto item = _list->ItemAt(y);
		if (!item.has_value() || item == last || *item >= items.size())
		{
			if (item.has_value() && item == last && !controls.empty())
			{
				controls.back().rect.max.y = y + 1;
			}
			continue;
		}
		last = item;
		controls.push_back({.name = items.at(*item).text,
		                    .kind = "list item",
		                    .rect = {.min = {_list->rect.min.x, y}, .max = {_list->GetBoxRight(), y + 1}}});
	}
	return controls;
}
