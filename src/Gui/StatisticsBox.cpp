/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "StatisticsBox.h"

#include <cmath>
#include <cstdio>

#include <algorithm>
#include <chrono>
#include <utility>

#include <glm/geometric.hpp>

#include "TextDatabase.h"

namespace openblack::gui
{

namespace statistics_box
{
namespace
{
using Justify = DialogPainter::Justify;

constexpr int k_Small = DialogPainter::k_SmallTextSize;
/// How long a graph's bars take to grow to their full size
constexpr float k_GrowSeconds = 0.5f;
/// The see-through foot of a vertical bar, and the edges of a row's bar
constexpr float k_BarFoot = static_cast<float>(0x3F) / 255.0f;
constexpr float k_RowBarEdge = static_cast<float>(0x7F) / 255.0f;
constexpr glm::vec4 k_White {1.0f};
constexpr glm::vec4 k_Shadow {0.0f, 0.0f, 0.0f, 1.0f};

std::u16string Widen(const std::string& text)
{
	return {text.begin(), text.end()};
}

/// A box of one colour at its top and another at its bottom
void FillBox(const DialogPainter& painter, glm::vec2 min, glm::vec2 max, glm::vec4 top, glm::vec4 bottom)
{
	painter.DrawShape({min, glm::vec2 {max.x, min.y}, max, glm::vec2 {min.x, max.y}}, {top, top, bottom, bottom});
}

/// The game cuts a value towards nothing to place it
int Cut(float value)
{
	return static_cast<int>(value);
}

glm::ivec2 PointerIn(const DialogRect& rect, glm::ivec2 pointer)
{
	return {std::clamp(pointer.x, rect.min.x, rect.max.x), std::clamp(pointer.y, rect.min.y, rect.max.y)};
}

/// A graph's name under it with a shadow, orange while the pointer is over the graph
void DrawCaption(const DialogPainter& painter, const DialogRect& rect, const std::u16string& caption, bool hovered)
{
	if (caption.empty())
	{
		return;
	}
	painter.DrawTextWrapped({.min = {rect.min.x + 1, rect.max.y + 1}, .max = {rect.max.x + 1, rect.max.y + 1 + k_Small}}, true,
	                        caption, k_Small, k_Shadow);
	painter.DrawTextWrapped({.min = {rect.min.x, rect.max.y}, .max = {rect.max.x, rect.max.y + k_Small}}, true, caption,
	                        k_Small, hovered ? DialogPainter::k_HoverColour : k_White);
}

/// The value at the pointer's height, written with a shadow by the graph's left edge
void DrawReadout(const DialogPainter& painter, const DialogRect& rect, int pointerY, const std::string& text)
{
	constexpr int k_Width = 100;
	const auto words = Widen(text);
	painter.DrawText({rect.min.x - 1, pointerY - (k_Small / 2) + 1}, k_Width, Justify::Right, words, k_Small, k_Shadow);
	painter.DrawText({rect.min.x - 2, pointerY - (k_Small / 2)}, k_Width, Justify::Right, words, k_Small, k_White);
}

std::string Printed(const char* format, double value)
{
	std::array<char, 32> text {};
	std::snprintf(text.data(), text.size(), format, value);
	return {text.data()};
}
std::string PrintedWhole(const char* format, int value)
{
	std::array<char, 32> text {};
	std::snprintf(text.data(), text.size(), format, value);
	return {text.data()};
}
} // namespace

/// A graph box: one bar for each player in their colour, from a row of the list. Each bar grows to its height in half a
/// second each time it is filled
class BarGraph final: public Control
{
public:
	explicit BarGraph(DialogRect rect)
	    : Control(rect)
	{
	}

	void Draw(const DialogPainter& painter, bool hovered, bool /*focused*/, bool /*pressed*/) const override
	{
		if (!visible)
		{
			return;
		}
		painter.DrawBevelBox(rect, 1, DialogPainter::All, k_White);
		DrawCaption(painter, rect, caption, hovered);
		const auto x0 = static_cast<float>(rect.min.x);
		const auto y1 = static_cast<float>(rect.max.y);
		const auto width = static_cast<float>(rect.Width() - 4);
		const auto height = static_cast<float>(rect.Height() - 4);
		const auto range = _scale.highest - _scale.lowest;
		const auto grown = _growth.GetValue();
		if (!_values.empty())
		{
			const int barWidth = Cut(width) / static_cast<int>(_values.size());
			for (size_t i = 0; i < _values.size(); ++i)
			{
				const auto left = static_cast<float>(rect.min.x + 2 + (static_cast<int>(i) * barWidth) + 2);
				const auto right = static_cast<float>(rect.min.x + 2 + ((static_cast<int>(i) + 1) * barWidth) - 2);
				const auto top = std::clamp(Cut((y1 - 2.0f) - (height * grown * (_values[i] - _scale.lowest) / range)),
				                            rect.min.y + 2, rect.max.y - 2);
				const auto foot = Cut((y1 - 2.0f) - (height * grown * -_scale.lowest / range)) - 2;
				const auto colour = _colours[i];
				FillBox(painter, {left, static_cast<float>(top)}, {right, static_cast<float>(foot)}, colour,
				        glm::vec4(glm::vec3(colour), k_BarFoot));
			}
		}
		// The guide lines across, over the bars
		const auto step = GuideStep(range, static_cast<float>(rect.Height()));
		const auto first = static_cast<float>(static_cast<int64_t>(_scale.lowest / step)) * step;
		auto index = static_cast<int64_t>(first / step) & 1;
		for (auto value = first; value < _scale.highest; value += step, ++index)
		{
			const auto y = static_cast<float>(
			    std::clamp(Cut((y1 - 2.0f) - (height * (value - _scale.lowest) / range)), rect.min.y + 2, rect.max.y - 2));
			FillBox(painter, {x0 + 2.0f, y}, {static_cast<float>(rect.max.x - 2), y + 1.0f}, GuideColour(index, false),
			        GuideColour(index, false));
		}
		if (hovered)
		{
			const auto pointer = PointerIn(rect, _pointer);
			const auto value = std::clamp((y1 - static_cast<float>(pointer.y) - 2.0f) / height * range + _scale.lowest,
			                              _scale.lowest, _scale.highest);
			constexpr float k_WholeAbove = 10.0f;
			DrawReadout(painter, rect, pointer.y,
			            std::abs(range) > k_WholeAbove ? PrintedWhole("%d", Cut(value))
			                                           : Printed("%0.1f", static_cast<double>(value)));
		}
	}
	[[nodiscard]] bool IsInteractive() const override { return visible; }
	void MouseMove(glm::ivec2 point) override { _pointer = point; }
	void Update(float deltaSeconds) override { _growth.Update(deltaSeconds); }

	/// Shows a row, or nothing; its bars grow again
	void Show(const Row* row, int32_t landNumber)
	{
		_values.clear();
		_colours.clear();
		caption.clear();
		_growth.Reset(0.0f);
		_growth.SetDestination(1.0f, k_GrowSeconds);
		_scale = {};
		if (row == nullptr)
		{
			return;
		}
		_values = row->values;
		for (const auto player : row->players)
		{
			_colours.push_back(PlayerColour(player, landNumber));
		}
		_scale = VerticalScale(*row);
	}
	/// Empty, and the bars grow again, when the player picks a new tab
	void Clear() { Show(nullptr, 0); }

	std::u16string caption;

private:
	std::vector<float> _values;
	std::vector<glm::vec4> _colours;
	BarScale _scale;
	Zoomer _growth;
	glm::ivec2 _pointer {-1, -1};
};

/// A line graph of the players' histories, one line each in their colour. Shown as shares, the lines are stacked
/// bands that fill the graph; shown as values, they are lines over guide lines. A click turns one into the other.
class LineGraph final: public Control
{
public:
	LineGraph(DialogRect rect, std::u16string caption, bool shares)
	    : Control(rect)
	    , _caption(std::move(caption))
	    , _shares(shares)
	{
	}

	void Draw(const DialogPainter& painter, bool hovered, bool /*focused*/, bool /*pressed*/) const override
	{
		if (!visible)
		{
			return;
		}
		painter.DrawBevelBox(rect, 1, DialogPainter::All, k_White);
		DrawCaption(painter, rect, _caption, hovered);
		if (_shares)
		{
			DrawShares(painter);
		}
		else
		{
			DrawValues(painter);
		}
		if (hovered)
		{
			const auto pointer = PointerIn(rect, _pointer);
			const auto height = static_cast<float>(rect.Height() - 4);
			const auto share = std::clamp((static_cast<float>(rect.max.y - pointer.y) - 2.0f) / height, 0.0f, 1.0f);
			if (_shares)
			{
				constexpr float k_Hundred = 100.0f;
				DrawReadout(painter, rect, pointer.y, PrintedWhole("%d%%", Cut(share * k_Hundred)));
			}
			else
			{
				const auto range = _scale.highest - _scale.lowest;
				const auto value = std::clamp((share * range) + _scale.lowest, _scale.lowest, _scale.highest);
				constexpr float k_DecimalsUpTo = 10.0f;
				DrawReadout(painter, rect, pointer.y,
				            _scale.highest <= k_DecimalsUpTo ? Printed("%0.2f", static_cast<double>(value))
				                                             : PrintedWhole("%d", Cut(value)));
			}
		}
	}
	void MouseMove(glm::ivec2 point) override { _pointer = point; }
	void Activate(glm::ivec2 /*point*/) override { _shares = !_shares; }

	void SetLines(std::vector<std::vector<float>> lines, std::vector<glm::vec4> colours)
	{
		_lines = std::move(lines);
		_colours = std::move(colours);
		_scale = LinesScale(_lines);
	}

private:
	void DrawShares(const DialogPainter& painter) const
	{
		if (_lines.empty())
		{
			return;
		}
		// The fewest points of any line; the first line sets the first number
		size_t points = _lines.front().size();
		for (const auto& line : _lines)
		{
			points = std::min(points, line.size());
		}
		if (points < 2)
		{
			return;
		}
		const auto bottom = static_cast<float>(rect.max.y - 2);
		const auto height = static_cast<float>(rect.Height() - 4);
		const auto step = static_cast<float>(rect.Width() - 4) / static_cast<float>(points - 1);
		const auto sum = [this](size_t i) {
			float total = 0.0f;
			for (const auto& line : _lines)
			{
				total += line[i];
			}
			return total != 0.0f ? 1.0f / total : 0.0f;
		};
		for (size_t i = 0; i + 1 < points; ++i)
		{
			const auto left = static_cast<float>(rect.min.x + 2) + (step * static_cast<float>(i));
			const auto right = left + step;
			const auto inverseA = sum(i);
			const auto inverseB = sum(i + 1);
			float shareA = 0.0f;
			float shareB = 0.0f;
			auto lowA = bottom;
			auto lowB = bottom;
			for (size_t l = 0; l < _lines.size(); ++l)
			{
				shareA += _lines[l][i] * inverseA;
				shareB += _lines[l][i + 1] * inverseB;
				const auto highA = static_cast<float>(Cut(bottom - (height * shareA)));
				const auto highB = static_cast<float>(Cut(bottom - (height * shareB)));
				const auto solid = _colours[l];
				const auto clear = glm::vec4(glm::vec3(solid), 0.0f);
				painter.DrawShape(
				    {glm::vec2 {left, highA}, glm::vec2 {right, highB}, glm::vec2 {right, lowB}, glm::vec2 {left, lowA}},
				    {solid, solid, clear, clear});
				lowA = highA;
				lowB = highB;
			}
		}
	}

	void DrawValues(const DialogPainter& painter) const
	{
		const auto bottom = static_cast<float>(rect.max.y - 2);
		const auto height = static_cast<float>(rect.Height() - 4);
		const auto range = _scale.highest - _scale.lowest;
		const auto yOf = [&](float value) {
			return static_cast<float>(
			    std::clamp(Cut(bottom - ((value - _scale.lowest) / range * height)), rect.min.y + 4, rect.max.y - 4));
		};
		for (size_t l = 0; l < _lines.size(); ++l)
		{
			const auto& line = _lines[l];
			if (line.size() < 2)
			{
				continue;
			}
			const auto step = static_cast<float>(rect.Width() - 4) / static_cast<float>(line.size() - 1);
			const auto solid = _colours[l];
			const auto clear = glm::vec4(glm::vec3(solid), 0.0f);
			for (size_t i = 0; i + 1 < line.size(); ++i)
			{
				const glm::vec2 a {static_cast<float>(rect.min.x + 2) + (step * static_cast<float>(i)), yOf(line[i])};
				const glm::vec2 b {a.x + step, yOf(line[i + 1])};
				// The line itself, one pixel of the screen wide
				const auto along = b - a;
				const auto across =
				    glm::vec2 {-along.y, along.x} / std::max(glm::length(along), 1e-3f) / std::max(painter.GetScale(), 1e-3f);
				painter.DrawShape({a, b, b + across, a + across}, {solid, solid, solid, solid});
				// A soft edge two units either side of the line
				painter.DrawShape({a - glm::vec2 {0.0f, 2.0f}, b - glm::vec2 {0.0f, 2.0f}, b, a}, {clear, clear, solid, solid});
				painter.DrawShape({a + glm::vec2 {0.0f, 2.0f}, b + glm::vec2 {0.0f, 2.0f}, b, a}, {clear, clear, solid, solid});
			}
		}
		// The guide lines across, over the lines; the line at nothing white
		const auto step = GuideStep(range, static_cast<float>(rect.Height()));
		const auto first = static_cast<float>(static_cast<int64_t>(_scale.lowest / step)) * step;
		auto index = static_cast<int64_t>(first / step);
		for (auto value = first; value <= _scale.highest; value += step, ++index)
		{
			const auto y = Cut(bottom - ((value - _scale.lowest) / range * height));
			if (y > rect.min.y + 2 && y < rect.max.y - 2)
			{
				const auto colour = GuideColour(index, true);
				FillBox(painter, {static_cast<float>(rect.min.x + 2), static_cast<float>(y)},
				        {static_cast<float>(rect.max.x - 2), static_cast<float>(y + 1)}, colour, colour);
			}
		}
	}

	std::u16string _caption;
	bool _shares;
	std::vector<std::vector<float>> _lines;
	std::vector<glm::vec4> _colours;
	LineScale _scale;
	glm::ivec2 _pointer {-1, -1};
};

/// Each player's creature on the Creature Stats tab, in a box of the player's colour
class CreatureViews final: public Control
{
public:
	CreatureViews()
	    : Control({.min = {40, 100}, .max = {530, 330}})
	{
	}

	void Draw(const DialogPainter& painter, bool /*hovered*/, bool /*focused*/, bool /*pressed*/) const override
	{
		if (!visible)
		{
			return;
		}
		// The boxes are drawn at half the box's opacity
		const auto boxes = CreatureBoxes(colours.size());
		for (size_t i = 0; i < boxes.size(); ++i)
		{
			const auto colour = glm::vec4(glm::vec3(colours[i]), 0.5f);
			FillBox(painter, glm::vec2(boxes[i].first), glm::vec2(boxes[i].second), colour, colour);
		}
		// TODO(raffclar): draw each player's creature in 3D in its box, and turn it
	}
	[[nodiscard]] bool IsInteractive() const override { return false; }
	[[nodiscard]] bool IsOnTop() const override { return true; }

	std::vector<glm::vec4> colours;
};

} // namespace statistics_box

using namespace statistics_box;

StatisticsBox::StatisticsBox(const TextDatabase& texts, const GameFont& font, std::function<void()> back)
    : _texts(texts)
    , _font(font)
    , _mainMenuLabel(texts.Get("HELP_TEXT_DIALOG_ADDITION_88"))
    , _gameOverLabel(texts.Get("HELP_TEXT_GAME_OVER"))
    , _totalLabel(texts.Get("HELP_TEXT_PAUSE_STATS_03"))
{
	std::vector<Dialog::Tab> tabs {
	    {.label = _mainMenuLabel, .onSelect = back},
	    {.label = std::u16string(texts.Get("HELP_TEXT_DIALOG_ADDITION_111")), .onSelect = [this] { SetTab(Tab::Player); }},
	    {.label = std::u16string(texts.Get("HELP_TEXT_DIALOG_ADDITION_46")), .onSelect = [this] { SetTab(Tab::Creature); }},
	    {.label = std::u16string(texts.Get("HELP_TEXT_DIALOG_ADDITION_138")), .onSelect = [this] { SetTab(Tab::Town); }},
	    {.label = std::u16string(texts.Get("HELP_TEXT_DIALOG_ADDITION_87")), .onSelect = [this] { SetTab(Tab::Miracle); }},
	    {.label = std::u16string(texts.Get("HELP_TEXT_DIALOG_ADDITION_124")), .onSelect = [this] { SetTab(Tab::Tech); }},
	};
	_dialog = std::make_unique<Dialog>(std::move(tabs), static_cast<size_t>(Tab::Player), k_Box);
	auto& dialog = *_dialog;

	// A dark band under the heading, drawn before the box at half its opacity
	dialog.drawUnder = [this](const DialogPainter& painter) {
		_totalHeadingDrawn = false;
		constexpr float k_Band = static_cast<float>(0x20) / 255.0f;
		const glm::vec4 colour {k_Band, k_Band, k_Band, 0.5f};
		painter.DrawShape({glm::vec2 {40, 60}, glm::vec2 {760, 60}, glm::vec2 {760, 88}, glm::vec2 {40, 88}},
		                  {colour, colour, colour, colour});
	};
	dialog.Add<StaticText>(DialogRect {.min = {40, 60}, .max = {760, 88}},
	                       std::u16string(texts.Get("HELP_TEXT_DIALOG_ADDITION_61")), StaticText::Layout::Centre,
	                       DialogPainter::k_MidTextSize);
	dialog
	    .Add<BigButton>(font, glm::ivec2 {20, 530}, 40, std::u16string(texts.Get("HELP_TEXT_DIALOG_TATOOBACK")),
	                    BigButton::LabelSide::Right, BigButton::Look::LeftArrow)
	    .onClick = back;
	constexpr std::array<int, k_GraphCount> k_GraphLeft {40, 290, 540};
	for (size_t i = 0; i < k_GraphCount; ++i)
	{
		_graphs.at(i) =
		    &dialog.Add<BarGraph>(DialogRect {.min = {k_GraphLeft.at(i), 100}, .max = {k_GraphLeft.at(i) + 220, 330}});
	}
	_influence = &dialog.Add<LineGraph>(DialogRect {.min = {40, 100}, .max = {510, 200}},
	                                    std::u16string(texts.Get("HELP_TEXT_PAUSE_STATS_05")), true);
	_population = &dialog.Add<LineGraph>(DialogRect {.min = {40, 230}, .max = {510, 330}},
	                                     std::u16string(texts.Get("HELP_TEXT_PAUSE_STATS_06")), false);
	_list = &dialog.Add<List>(font, DialogRect {.min = {40, 365}, .max = {760, 515}}, DialogPainter::k_SmallTextSize, false,
	                          true, true);
	_list->drawOver = [this](const DialogPainter& painter, const List::ItemPlace& place) { DrawRow(painter, place); };
	_list->onClick = [this] {
		if (const auto row = _list->GetSelection(); row.has_value())
		{
			PickRow(*row);
		}
	};
	_graphicBox = &dialog.Add<CheckBox>(font, glm::ivec2 {290, 530}, 20, std::u16string(texts.Get("HELP_TEXT_PAUSE_STATS_07")),
	                                    BigButton::LabelSide::Right, true, true);
	_numericBox = &dialog.Add<CheckBox>(font, glm::ivec2 {490, 530}, 20, std::u16string(texts.Get("HELP_TEXT_PAUSE_STATS_08")),
	                                    BigButton::LabelSide::Left, false, true);
	_graphicBox->onChange = [this](bool) { SetGraphic(true); };
	_numericBox->onChange = [this](bool) { SetGraphic(false); };
	_creatures = &dialog.Add<CreatureViews>();
}

StatisticsBox::~StatisticsBox() = default;

void StatisticsBox::Open(statistics::BoxFigures figures, PlayerNames local, bool fromGameOver)
{
	_figures = std::move(figures);
	_local = local;
	_dialog->SetTabLabel(0, fromGameOver ? _gameOverLabel : _mainMenuLabel);
	// It opens on Player Stats, as it was. Its graphs do not grow again
	_tab = Tab::Player;
	SetTab(Tab::Player);
	_dialog->Reset();
}

void StatisticsBox::SetTab(Tab tab)
{
	_dialog->SelectTab(static_cast<size_t>(tab));
	if (tab != _tab)
	{
		for (auto* graph : _graphs)
		{
			graph->Clear();
		}
	}
	_tab = tab;
	_page = MakePage(tab, _figures, _local);
	for (size_t i = 0; i < k_GraphCount; ++i)
	{
		_graphs.at(i)->visible = _page.graphShown.at(i);
	}
	_influence->visible = _page.lineGraphsShown;
	_population->visible = _page.lineGraphsShown;
	_creatures->visible = _page.creaturesShown;

	std::vector<List::Item> items;
	items.reserve(_page.rows.size());
	for (const auto& row : _page.rows)
	{
		items.push_back({.text = std::u16string(_texts.Get(row.label)), .mouseCell = std::nullopt, .mouseMirrored = false});
	}
	_list->SetItems(std::move(items));
	_shownRow.reset();
	// The graph boxes are filled in the order the rows are listed; the right one, emptied on the Creature Stats tab,
	// stays empty
	std::vector<std::pair<size_t, Graph>> fills;
	for (size_t i = 0; i < k_GraphCount; ++i)
	{
		if (const auto row = _page.graphs.at(i); row.has_value())
		{
			fills.emplace_back(*row, static_cast<Graph>(i));
		}
	}
	std::ranges::sort(fills);
	for (const auto& [row, graph] : fills)
	{
		Fill(graph, row);
	}
	if (tab == Tab::Creature)
	{
		_graphs.at(static_cast<size_t>(Graph::Right))->Clear();
	}
	_list->SetSelection(_page.selected);
	_shownRow = _page.selected;

	// The line graphs: each player's influence and population over the whole game
	std::vector<std::vector<float>> influence;
	std::vector<std::vector<float>> population;
	std::vector<glm::vec4> colours;
	for (const auto& player : _figures.players)
	{
		influence.push_back(player.influenceHistory);
		population.push_back(player.populationHistory);
		colours.push_back(PlayerColour(player.player, _figures.landNumber));
	}
	_influence->SetLines(std::move(influence), colours);
	_population->SetLines(std::move(population), colours);
	_creatures->colours = std::move(colours);
}

void StatisticsBox::Fill(Graph graph, std::optional<size_t> row)
{
	auto& box = *_graphs.at(static_cast<size_t>(graph));
	box.Show(row.has_value() && *row < _page.rows.size() ? &_page.rows.at(*row) : nullptr, _figures.landNumber);
	if (row.has_value() && *row < _page.rows.size())
	{
		box.caption = std::u16string(_texts.Get(_page.rows.at(*row).label));
	}
	_shownRow = row;
	_list->SetSelection(row);
}

void StatisticsBox::PickRow(size_t row)
{
	if (_shownRow == row)
	{
		return;
	}
	Fill(Graph::Right, row);
}

void StatisticsBox::SetGraphic(bool graphic)
{
	_graphic = graphic;
	_graphicBox->SetChecked(graphic);
	_numericBox->SetChecked(!graphic);
}

void StatisticsBox::Update(float deltaSeconds)
{
	_frameSeconds = deltaSeconds;
}

void StatisticsBox::DrawRow(const DialogPainter& painter, const List::ItemPlace& place)
{
	if (place.index >= _page.rows.size())
	{
		return;
	}
	const auto& row = _page.rows.at(place.index);
	const auto values = row.values.size();
	if (DrawnAsBar(row, _graphic))
	{
		// One bar split between the players, over the right hand 300 of the row
		const auto top = place.shownTop + 2;
		const auto bottom = place.shownBottom - 2;
		if (place.shownBottom - place.shownTop <= 10 || values < 1 || values > statistics::k_BoxPlayers)
		{
			return;
		}
		auto& growth = _rowGrowth.at(values - 1);
		if (growth.GetDuration() == 0.0f && growth.GetValue() == 0.0f)
		{
			growth.SetDestination(1.0f, k_GrowSeconds);
		}
		growth.Update(_frameSeconds);
		const DialogRect bar {.min = {place.right - k_ValuesWidth, top}, .max = {place.right - 4, bottom}};
		painter.DrawBevelBox(bar, 1, DialogPainter::All, glm::vec4(1.0f));
		const auto parts = BarParts(row);
		const auto left = static_cast<float>(bar.min.x + 2);
		const auto width = static_cast<float>(bar.Width() - 4);
		const auto middle = static_cast<float>((bar.min.y + bar.max.y) / 2);
		auto start = left;
		for (size_t i = 0; i < parts.size(); ++i)
		{
			const auto end = static_cast<float>(
			    std::clamp(static_cast<int>(left + (width * growth.GetValue() * parts[i])), bar.min.x + 2, bar.max.x - 2));
			auto solid = PlayerColour(row.players[i], _figures.landNumber);
			auto edge = glm::vec4(glm::vec3(solid), k_RowBarEdge);
			if (row.values[i] < 0.0f)
			{
				std::swap(solid, edge);
			}
			FillBox(painter, {start, static_cast<float>(bar.min.y + 2)}, {end, middle}, edge, solid);
			FillBox(painter, {start, middle}, {end, static_cast<float>(bar.max.y - 2)}, solid, edge);
			start = end;
		}
		// The guide lines down the bar, every fourth brighter
		float scale = row.most;
		if (values != 1 || scale <= 0.0f)
		{
			scale = 0.0f;
			for (const auto value : row.values)
			{
				scale += std::abs(value);
			}
			scale = scale == 0.0f ? 1.0f : scale;
		}
		const auto step = GuideStep(scale, static_cast<float>(bar.Width()));
		int64_t index = 1;
		for (auto value = step; value < scale; value += step, ++index)
		{
			const auto x = static_cast<float>(Cut(left + (width * value / scale)));
			const auto colour = GuideColour(index, false);
			FillBox(painter, {x, static_cast<float>(bar.min.y + 2)}, {x + 1.0f, static_cast<float>(bar.max.y - 2)}, colour,
			        colour);
		}
		return;
	}

	// Numbers: each player's value in their colour, then the total; with one value, the total alone
	const auto y = place.top + 2;
	const auto width = ColumnWidth(values);
	if (values > 1)
	{
		for (size_t i = 0; i < values; ++i)
		{
			painter.DrawText({ColumnCentre(place.right, values, i), y}, width * 5, Justify::Centre,
			                 Widen(Format(row, row.values[i])), k_Small, PlayerColour(row.players[i], _figures.landNumber));
		}
	}
	if (const auto total = Total(row); total.has_value())
	{
		painter.DrawText({ColumnCentre(place.right, values, values), y}, width * 5, Justify::Centre, Widen(Format(row, *total)),
		                 k_Small, k_TotalColour);
	}
	// "Total" over the first row of one value shown
	if (values == 1 && !_totalHeadingDrawn)
	{
		_totalHeadingDrawn = true;
		painter.DrawText({ColumnCentre(place.right, values, values), place.top - k_Small}, width * 5, Justify::Centre,
		                 _totalLabel, k_Small, k_TotalColour);
	}
}

} // namespace openblack::gui
