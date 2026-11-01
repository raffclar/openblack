/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ConditionsBox.h"

#include <utility>
#include <vector>

#include "GameFont.h"
#include "StatisticsBox.h"
#include "TextDatabase.h"

using namespace openblack::gui;
using namespace openblack::gui::conditions_box;

namespace
{
// The tabs: the first, Multiplayer and three empty tabs, labelled in small text. Multiplayer is open.
constexpr size_t k_TabCount = 5;
constexpr size_t k_MultiplayerTab = 1;

// The heading, with a dark band under it at half the opacity of the box
constexpr DialogRect k_HeadingRect {.min = {40, 60}, .max = {760, 88}};
constexpr float k_Band = static_cast<float>(0x20) / 255.0f;

// The graph and the list, each 65 less wide than the box
constexpr DialogRect k_GraphRect {.min = {40, 100}, .max = {755, 280}};
constexpr DialogRect k_ListRect {.min = {40, 345}, .max = {755, 525}};

// A check box for each of the first four players, labelled "Player n" below it, 100 apart
constexpr int k_CheckBoxTop = 295;
constexpr int k_FirstCheckBoxLeft = 100;
constexpr int k_CheckBoxSpacing = 100;
// The window of gods of a game of more than 4 gods, on the line of the check boxes, right of them
constexpr DialogRect k_GodWindowLine {.min = {520, 295}, .max = {755, 320}};

// The values of the gods in the list
constexpr int k_ValueTextSize = 26;
} // namespace

ConditionsBox::ConditionsBox(const TextDatabase& texts, const GameFont& font, std::function<void()> back)
    : _mainMenuLabel(texts.Get("HELP_TEXT_DIALOG_ADDITION_88"))
    , _gameOverLabel(texts.Get("HELP_TEXT_GAME_OVER"))
    , _doneText(texts.Get("HELP_TEXT_MULTIPLAYER_CONDITION_DONE"))
{
	for (size_t number = 0; number < _names.size(); ++number)
	{
		_names.at(number) = texts.Get(ConditionTextName(number));
	}
	std::vector<Dialog::Tab> tabs {
	    {.label = _mainMenuLabel, .onSelect = back},
	    {.label = std::u16string(texts.Get("HELP_TEXT_MULTIPLAYER_MAIN_TAB")), .onSelect = {}},
	};
	tabs.resize(k_TabCount);
	for (auto& tab : tabs)
	{
		tab.textSize = DialogPainter::SmallTextSize();
	}
	_dialog = std::make_unique<Dialog>(std::move(tabs), k_MultiplayerTab, k_Box);
	auto& dialog = *_dialog;

	dialog.drawUnder = [](const DialogPainter& painter) {
		const glm::vec4 colour {k_Band, k_Band, k_Band, 0.5f};
		const auto& rect = k_HeadingRect;
		painter.DrawShape(
		    {glm::vec2(rect.min), glm::vec2(rect.max.x, rect.min.y), glm::vec2(rect.max), glm::vec2(rect.min.x, rect.max.y)},
		    {colour, colour, colour, colour});
	};
	dialog.Add<StaticText>(k_HeadingRect, std::u16string(texts.Get("HELP_TEXT_MULTIPLAYER_STATS_CAPTION")),
	                       StaticText::Layout::Centre, DialogPainter::MidTextSize());
	dialog
	    .Add<BigButton>(font, glm::ivec2 {20, 530}, 40, std::u16string(texts.Get("HELP_TEXT_DIALOG_TATOOBACK")),
	                    BigButton::LabelSide::Right, BigButton::Look::LeftArrow)
	    .onClick = back;
	_graph = &statistics_box::AddLineGraph(dialog, k_GraphRect, std::u16string(), false);
	_list = &dialog.Add<List>(font, k_ListRect, DialogPainter::MidTextSize(), false, true, true);
	_list->ShowScrollBarWhenNeeded();
	_list->drawUnder = [this](const DialogPainter& painter, const List::ItemPlace& place) { DrawValues(painter, place); };
	for (size_t i = 0; i < k_Gods; ++i)
	{
		auto label = std::u16string(u"Player ") + static_cast<char16_t>(u'1' + i);
		auto& box = dialog.Add<CheckBox>(
		    font, glm::ivec2 {k_FirstCheckBoxLeft + (k_CheckBoxSpacing * static_cast<int>(i)), k_CheckBoxTop},
		    CheckBox::k_DefaultSize, std::move(label), BigButton::LabelSide::Below, true, false);
		box.onChange = [this, i](bool ticked) {
			if (const auto god = _checkBoxGods.at(i); god.has_value() && *god < _ticked.size())
			{
				_ticked.at(*god) = ticked;
			}
			FillGraph();
		};
		_checkBoxes.at(i) = &box;
	}
	_window = std::make_unique<GodWindow>(dialog, font, k_GodWindowLine, [this] {
		ShowCheckBoxes();
		FillGraph();
	});
}

ConditionsBox::~ConditionsBox() = default;

void ConditionsBox::Open(Figures figures, bool fromGameOver)
{
	_figures = std::move(figures);
	_dialog->SetTabLabel(0, fromGameOver ? _gameOverLabel : _mainMenuLabel);
	_dialog->SelectTab(k_MultiplayerTab);
	// Every god is ticked again, and only the check boxes of the gods that play show
	_ticked.assign(_figures.gods.size(), true);
	_window->SetGods(_figures.gods.size());
	ShowCheckBoxes();
	std::vector<List::Item> items;
	items.reserve(_figures.conditions.size());
	for (const auto& condition : _figures.conditions)
	{
		const auto name =
		    condition.number < _names.size() ? std::u16string_view(_names.at(condition.number)) : std::u16string_view();
		items.push_back({.text = LineText(name, condition.target),
		                 .mouseCell = std::nullopt,
		                 .mouseMirrored = false,
		                 .colour = std::nullopt});
	}
	_list->SetItems(std::move(items));
	_list->SetSelection(std::nullopt);
	FillGraph();
	_dialog->Reset();
}

void ConditionsBox::ShowCheckBoxes()
{
	_checkBoxGods = GodsOfCheckBoxes(_figures, _window->GetFirst());
	for (size_t i = 0; i < k_Gods; ++i)
	{
		auto& box = *_checkBoxes.at(i);
		const auto god = _checkBoxGods.at(i);
		box.visible = god.has_value();
		box.SetChecked(god.has_value() && *god < _ticked.size() && _ticked.at(*god));
		box.label = CheckBoxLabel(_figures, _checkBoxGods, i);
	}
}

void ConditionsBox::FillGraph()
{
	std::vector<std::vector<float>> lines;
	std::vector<glm::vec4> colours;
	for (const auto index : ShownGods(_checkBoxGods, _ticked))
	{
		const auto& god = _figures.gods.at(index);
		lines.push_back(god.scores);
		colours.push_back(god.colour);
	}
	// The graph goes from 0 to 1
	statistics_box::SetLines(*_graph, std::move(lines), std::move(colours),
	                         statistics_box::LineScale {.lowest = 0.0f, .highest = 1.0f});
}

void ConditionsBox::DrawValues(const DialogPainter& painter, const List::ItemPlace& place) const
{
	// Only a line that the list shows whole
	if (place.index >= _figures.conditions.size() || place.top < _list->rect.min.y || place.bottom > _list->rect.max.y)
	{
		return;
	}
	const auto shown = ShownGods(_checkBoxGods, _ticked);
	const auto columns = ValueColumns(_list->rect.min.x, place.right, shown.size());
	const auto number = _figures.conditions.at(place.index).number;
	auto x = columns.first;
	for (const auto index : shown)
	{
		const auto& god = _figures.gods.at(index);
		if (const auto& condition = god.conditions.at(number); condition.has_value())
		{
			painter.DrawText({x, place.top}, columns.width, DialogPainter::Justify::Centre, ValueText(*condition, _doneText),
			                 k_ValueTextSize, god.colour);
		}
		x += columns.width;
	}
}
