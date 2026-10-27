/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "StatisticsBoxRules.h"

#include <cmath>
#include <cstdio>

#include <algorithm>
#include <functional>
#include <string>

#include "ECS/PlayerColour.h"

namespace openblack::gui::statistics_box
{

namespace
{
using statistics::BoxFigures;
using statistics::PlayerFigures;

/// The most a figure of decimals is held to, either way
constexpr float k_Held = 1e20f;

/// Makes a tab's rows, one value for each player shown or one of the game's own
class PageMaker
{
public:
	PageMaker(const BoxFigures& figures, PlayerNames local, Page& page)
	    : _figures(figures)
	    , _local(local)
	    , _page(page)
	{
	}

	/// A figure of each player's, whole numbers
	void Players(std::string_view label, const std::function<uint32_t(const PlayerFigures&)>& value, float most, ShowMost show,
	             Display display)
	{
		Row row = Start(label, most, 0, show, display);
		for (const auto& player : _figures.players)
		{
			row.values.push_back(static_cast<float>(value(player)));
			row.players.push_back(player.player);
		}
		Add(std::move(row));
	}
	/// A figure of each player's with decimals
	void Players(std::string_view label, const std::function<float(const PlayerFigures&)>& value, int decimals, float most,
	             ShowMost show, Display display)
	{
		Row row = Start(label, most, decimals, show, display);
		for (const auto& player : _figures.players)
		{
			row.values.push_back(Held(value(player)));
			row.players.push_back(player.player);
		}
		Add(std::move(row));
	}
	/// One of the game's own figures, whole, in the local player's colour
	void Single(std::string_view label, uint32_t value, float most, ShowMost show)
	{
		Row row = Start(label, most, 0, show, Display::Number);
		row.values.push_back(static_cast<float>(value));
		row.players.push_back(_local);
		Add(std::move(row));
	}
	/// One of the game's own figures with decimals, in the local player's colour
	void Single(std::string_view label, float value, int decimals, float most, ShowMost show)
	{
		Row row = Start(label, most, decimals, show, Display::Number);
		row.values.push_back(Held(value));
		row.players.push_back(_local);
		Add(std::move(row));
	}

	/// The last row made fills a graph box, and is picked in the list
	void Copy(Graph graph)
	{
		const auto row = _page.rows.size() - 1;
		_page.graphs.at(static_cast<size_t>(graph)) = row;
		_page.selected = row;
	}

	/// The first player's figure, which the rows of men and women are out of for every player
	[[nodiscard]] float FirstPlayers(const std::function<uint32_t(const PlayerFigures&)>& value) const
	{
		return _figures.players.empty() ? 0.0f : static_cast<float>(value(_figures.players.front()));
	}

private:
	static Row Start(std::string_view label, float most, int decimals, ShowMost show, Display display)
	{
		return {.label = std::string(label), .most = most, .decimals = decimals, .show = show, .display = display};
	}

	void Add(Row row)
	{
		// A row out of its highest value takes the highest player's value as its most
		if (row.show == ShowMost::NumberToHighest)
		{
			row.most = row.values.empty() ? 0.0f : *std::ranges::max_element(row.values);
		}
		_page.rows.push_back(std::move(row));
	}

	const BoxFigures& _figures;
	PlayerNames _local;
	Page& _page;
};

void PlayerPage(PageMaker& make, const BoxFigures& figures)
{
	using P = const PlayerFigures&;
	constexpr auto k_Highest = ShowMost::NumberToHighest;
	constexpr auto k_Ticked = Display::AsTicked;
	make.Players("HELP_TEXT_PAUSE_STATS_110", [](P p) { return p.influence; }, 0, 0.0f, k_Highest, k_Ticked);
	make.Copy(Graph::Right);
	const auto others = static_cast<float>(figures.playersInGame - 1);
	make.Players("HELP_TEXT_PAUSE_STATS_111", [](P p) { return p.godsDefeated; }, others, ShowMost::OutOfMost, k_Ticked);
	make.Players(
	    "HELP_TEXT_PAUSE_STATS_112", [](P p) { return p.alignmentChange; }, 2, 1.0f, ShowMost::Number, Display::Number);
	make.Players("HELP_TEXT_PAUSE_STATS_113", [](P p) { return p.peopleKilled; }, 0.0f, k_Highest, k_Ticked);
	make.Players("HELP_TEXT_PAUSE_STATS_114", [](P p) { return p.belief; }, 0, 0.0f, k_Highest, k_Ticked);
	make.Players("HELP_TEXT_PAUSE_STATS_115", [](P p) { return p.maxBelief; }, 0, 0.0f, k_Highest, k_Ticked);
	make.Players("HELP_TEXT_PAUSE_STATS_116", [](P p) { return p.minBelief; }, 0, 0.0f, k_Highest, k_Ticked);
	make.Players("HELP_TEXT_PAUSE_STATS_117", [](P p) { return p.artefacts; }, 0.0f, k_Highest, k_Ticked);
	make.Players("HELP_TEXT_PAUSE_STATS_118", [](P p) { return p.creatureGrowth; }, 2, 1.0f, ShowMost::Percent, k_Ticked);
	make.Players(
	    "HELP_TEXT_PAUSE_STATS_119", [](P p) { return p.creatureAlignmentChange; }, 2, 1.0f, ShowMost::Number, Display::Number);
	make.Players("HELP_TEXT_PAUSE_STATS_120", [](P p) { return p.buildingsDestroyed; }, 0.0f, k_Highest, k_Ticked);
}

void CreaturePage(PageMaker& make)
{
	using P = const PlayerFigures&;
	constexpr auto k_Highest = ShowMost::NumberToHighest;
	constexpr auto k_Ticked = Display::AsTicked;
	constexpr float k_Hundred = 100.0f;
	make.Players(
	    "HELP_TEXT_DIALOG_ADDITION_48", [](P p) { return p.creatureDamage; }, 2, k_Hundred, ShowMost::Percent, k_Ticked);
	make.Players(
	    "HELP_TEXT_DIALOG_ADDITION_68", [](P p) { return p.creatureHunger; }, 2, k_Hundred, ShowMost::Percent, k_Ticked);
	make.Players(
	    "HELP_TEXT_DIALOG_ADDITION_137", [](P p) { return p.creatureTiredness; }, 2, k_Hundred, ShowMost::Percent, k_Ticked);
	make.Players("HELP_TEXT_DIALOG_ADDITION_158", [](P p) { return p.creatureAge; }, 0.0f, k_Highest, k_Ticked);
	make.Players("HELP_TEXT_DIALOG_ADDITION_159", [](P p) { return p.creatureAlignment; }, 2, 1.0f, ShowMost::Number, k_Ticked);
	make.Players("HELP_TEXT_DIALOG_ADDITION_161", [](P p) { return p.creature.animalsKilled; }, 0.0f, k_Highest, k_Ticked);
	make.Players("HELP_TEXT_DIALOG_ADDITION_163", [](P p) { return p.creature.battlesFought; }, 0.0f, k_Highest, k_Ticked);
	make.Copy(Graph::Middle);
	make.Players("HELP_TEXT_DIALOG_ADDITION_164", [](P p) { return p.creature.battlesWon; }, 0.0f, k_Highest, k_Ticked);
	make.Copy(Graph::Left);
	make.Players("HELP_TEXT_DIALOG_ADDITION_165", [](P p) { return p.creature.poos; }, 0.0f, k_Highest, k_Ticked);
	make.Players("HELP_TEXT_DIALOG_ADDITION_166", [](P p) { return p.creature.mushroomsEaten; }, 0.0f, k_Highest, k_Ticked);
}

void TownPage(PageMaker& make)
{
	using P = const PlayerFigures&;
	constexpr auto k_Highest = ShowMost::NumberToHighest;
	constexpr auto k_OutOf = ShowMost::OutOfMost;
	constexpr auto k_Ticked = Display::AsTicked;
	make.Players("HELP_TEXT_PAUSE_STATS_30", [](P p) { return p.births; }, 0.0f, k_Highest, k_Ticked);
	make.Players("HELP_TEXT_PAUSE_STATS_31", [](P p) { return p.deaths; }, 0.0f, k_Highest, k_Ticked);
	make.Players("HELP_TEXT_PAUSE_STATS_32", [](P p) { return p.population; }, 0.0f, k_Highest, k_Ticked);
	make.Copy(Graph::Right);
	// Every player's men and women are out of the first player's people
	const auto population = make.FirstPlayers([](P p) { return p.population; });
	make.Players("HELP_TEXT_PAUSE_STATS_33", [](P p) { return p.men; }, population, k_OutOf, k_Ticked);
	make.Players("HELP_TEXT_PAUSE_STATS_34", [](P p) { return p.women; }, population, k_OutOf, k_Ticked);
	make.Players("HELP_TEXT_PAUSE_STATS_35", [](P p) { return p.capacity; }, 0.0f, k_Highest, k_Ticked);
	make.Players("HELP_TEXT_PAUSE_STATS_36", [](P p) { return p.populationExtremes.highest; }, 0.0f, k_Highest, k_Ticked);
	const auto most = make.FirstPlayers([](P p) { return p.populationExtremes.highest; });
	make.Players("HELP_TEXT_PAUSE_STATS_37", [](P p) { return p.menExtremes.highest; }, most, k_OutOf, k_Ticked);
	make.Players("HELP_TEXT_PAUSE_STATS_38", [](P p) { return p.womenExtremes.highest; }, most, k_OutOf, k_Ticked);
	make.Players("HELP_TEXT_PAUSE_STATS_39", [](P p) { return p.populationExtremes.lowest; }, 0.0f, k_Highest, k_Ticked);
	const auto fewest = make.FirstPlayers([](P p) { return p.populationExtremes.lowest; });
	make.Players("HELP_TEXT_PAUSE_STATS_40", [](P p) { return p.menExtremes.lowest; }, fewest, k_OutOf, k_Ticked);
	make.Players("HELP_TEXT_PAUSE_STATS_41", [](P p) { return p.womenExtremes.lowest; }, fewest, k_OutOf, k_Ticked);
	make.Players("HELP_TEXT_PAUSE_STATS_42", [](P p) { return p.converted; }, 0.0f, k_Highest, k_Ticked);
	make.Players("HELP_TEXT_PAUSE_STATS_43", [](P p) { return p.satisfaction; }, 2, 1.0f, ShowMost::Number, k_Ticked);
	make.Copy(Graph::Left);
	make.Players("HELP_TEXT_PAUSE_STATS_44", [](P p) { return p.towns; }, 0.0f, k_Highest, k_Ticked);
	make.Players("HELP_TEXT_PAUSE_STATS_45", [](P p) { return p.totalBuildings; }, 0.0f, k_Highest, k_Ticked);
	make.Players("HELP_TEXT_PAUSE_STATS_46", [](P p) { return p.abodes; }, 0.0f, k_Highest, k_Ticked);
	make.Players("HELP_TEXT_PAUSE_STATS_47", [](P p) { return p.civic; }, 0.0f, k_Highest, k_Ticked);
	make.Players("HELP_TEXT_PAUSE_STATS_48", [](P p) { return p.wonders; }, 0.0f, k_Highest, k_Ticked);
	make.Players("HELP_TEXT_PAUSE_STATS_49", [](P p) { return p.built.total; }, 0.0f, k_Highest, k_Ticked);
	make.Copy(Graph::Middle);
	make.Players("HELP_TEXT_PAUSE_STATS_50", [](P p) { return p.built.abodes; }, 0.0f, k_Highest, k_Ticked);
	make.Players("HELP_TEXT_PAUSE_STATS_51", [](P p) { return p.built.civic; }, 0.0f, k_Highest, k_Ticked);
	make.Players("HELP_TEXT_PAUSE_STATS_52", [](P p) { return p.built.wonders; }, 0.0f, k_Highest, k_Ticked);
	// The disciples made, in the order they are kept: farmers, foresters, fishermen, builders, breeders, traders,
	// missionaries and craftsmen
	constexpr std::array<std::string_view, 8> k_Disciples {
	    "HELP_TEXT_PAUSE_STATS_53", "HELP_TEXT_PAUSE_STATS_54", "HELP_TEXT_PAUSE_STATS_55", "HELP_TEXT_PAUSE_STATS_56",
	    "HELP_TEXT_PAUSE_STATS_57", "HELP_TEXT_PAUSE_STATS_58", "HELP_TEXT_PAUSE_STATS_59", "HELP_TEXT_PAUSE_STATS_60",
	};
	for (size_t i = 0; i < k_Disciples.size(); ++i)
	{
		make.Players(k_Disciples.at(i), [i](P p) { return p.disciplesMade.at(i); }, 0.0f, k_Highest, k_Ticked);
	}
	make.Players("HELP_TEXT_PAUSE_STATS_61", [](P p) { return p.foodUsed; }, 0.0f, k_Highest, k_Ticked);
	make.Players("HELP_TEXT_PAUSE_STATS_62", [](P p) { return p.woodUsed; }, 0.0f, k_Highest, k_Ticked);
}

void MiraclePage(PageMaker& make)
{
	using P = const PlayerFigures&;
	constexpr auto k_Highest = ShowMost::NumberToHighest;
	constexpr auto k_Ticked = Display::AsTicked;
	const auto total = [](statistics::MiracleKind kind) {
		return [kind](P p) { return statistics::MiraclesCast(p.castsOfType, kind); };
	};
	make.Players("HELP_TEXT_PAUSE_STATS_63", total(statistics::MiracleKind::Aggressive), 0.0f, k_Highest, k_Ticked);
	make.Copy(Graph::Left);
	make.Players("HELP_TEXT_PAUSE_STATS_64", total(statistics::MiracleKind::Nice), 0.0f, k_Highest, k_Ticked);
	make.Copy(Graph::Middle);
	make.Players("HELP_TEXT_PAUSE_STATS_65", total(statistics::MiracleKind::Creature), 0.0f, k_Highest, k_Ticked);
	make.Copy(Graph::Right);
	// One row for each magic type from the fireball to the angry creature miracle, in the order of the magic types,
	// then the itchy creature miracle
	constexpr auto k_FirstListed = static_cast<size_t>(MagicType::Fireball);
	constexpr auto k_LastListed = static_cast<size_t>(MagicType::CreatureSpellAngry);
	constexpr size_t k_FirstLabel = 66;
	const auto castsOf = [&make](size_t type, size_t label) {
		const auto name = "HELP_TEXT_PAUSE_STATS_" + std::to_string(label);
		make.Players(name, [type](P p) { return p.castsOfType.at(type); }, 0.0f, ShowMost::NumberToHighest, Display::AsTicked);
	};
	for (size_t type = k_FirstListed; type <= k_LastListed; ++type)
	{
		castsOf(type, k_FirstLabel + type - k_FirstListed);
	}
	castsOf(static_cast<size_t>(MagicType::CreatureSpellItchy), 106);
	make.Players("HELP_TEXT_PAUSE_STATS_107", [](P p) { return p.chantsUsed; }, 1, 0.0f, k_Highest, k_Ticked);
	make.Players("HELP_TEXT_PAUSE_STATS_108", [](P p) { return p.sacrifices; }, 0.0f, k_Highest, k_Ticked);
}

void TechPage(PageMaker& make, const statistics::TechFigures& tech)
{
	constexpr auto k_Highest = ShowMost::NumberToHighest;
	constexpr float k_FrameRateMost = 50.0f;
	make.Single("HELP_TEXT_PAUSE_STATS_09", tech.linesOfCode, 0.0f, k_Highest);
	make.Copy(Graph::Right);
	make.Single("HELP_TEXT_PAUSE_STATS_10", tech.maxFrameRate, 1, k_FrameRateMost, ShowMost::Number);
	make.Copy(Graph::Middle);
	make.Single("HELP_TEXT_PAUSE_STATS_11", tech.minFrameRate, 1, k_FrameRateMost, ShowMost::Number);
	make.Copy(Graph::Left);
	make.Single("HELP_TEXT_PAUSE_STATS_13", tech.objectsCreated, 0.0f, k_Highest);
	make.Single("HELP_TEXT_PAUSE_STATS_14", tech.timePlayed, 0.0f, k_Highest);
	make.Single("HELP_TEXT_PAUSE_STATS_15", tech.scriptInstructions, 0.0f, k_Highest);
	make.Single("HELP_TEXT_PAUSE_STATS_16", tech.statisticsCount, 0.0f, k_Highest);
	const auto& help = tech.help;
	make.Single("HELP_TEXT_PAUSE_STATS_17", help.distanceDragged, 0.0f, k_Highest);
	make.Single("HELP_TEXT_PAUSE_STATS_18", help.timeLookingAtSky, 0.0f, k_Highest);
	make.Single("HELP_TEXT_PAUSE_STATS_19", help.timeLookingAtGround, 0.0f, k_Highest);
	make.Single("HELP_TEXT_PAUSE_STATS_20", help.gestures, 0.0f, k_Highest);
	make.Single("HELP_TEXT_PAUSE_STATS_21", help.zooms, 0.0f, k_Highest);
	make.Single("HELP_TEXT_PAUSE_STATS_22", help.rotations, 0.0f, k_Highest);
	make.Single("HELP_TEXT_PAUSE_STATS_23", help.pitches, 0.0f, k_Highest);
	make.Single("HELP_TEXT_PAUSE_STATS_24", help.interfaceActions, 0.0f, k_Highest);
	make.Single("HELP_TEXT_PAUSE_STATS_25", help.thrown, 0.0f, k_Highest);
	make.Single("HELP_TEXT_PAUSE_STATS_26", help.pickedUp, 0.0f, k_Highest);
	make.Single("HELP_TEXT_PAUSE_STATS_27", help.fightAttacks, 0.0f, k_Highest);
	make.Single("HELP_TEXT_PAUSE_STATS_28", help.fightBlocks, 0.0f, k_Highest);
	make.Single("HELP_TEXT_PAUSE_STATS_29", help.fightSteps, 0.0f, k_Highest);
}

} // namespace

Page MakePage(Tab tab, const statistics::BoxFigures& figures, PlayerNames local)
{
	Page page;
	PageMaker make(figures, local, page);
	switch (tab)
	{
	case Tab::Player:
		PlayerPage(make, figures);
		page.graphShown = {false, false, true};
		page.lineGraphsShown = true;
		break;
	case Tab::Creature:
		CreaturePage(make);
		page.graphShown = {false, false, true};
		page.creaturesShown = true;
		// The right box stays empty until a row is picked, though the battles won stays picked in the list
		page.graphs.at(static_cast<size_t>(Graph::Right)).reset();
		break;
	case Tab::Town:
		TownPage(make);
		break;
	case Tab::Miracle:
		MiraclePage(make);
		break;
	case Tab::Tech:
		TechPage(make, figures.tech);
		break;
	case Tab::Back:
	case Tab::_Count:
		break;
	}
	return page;
}

float Held(float value)
{
	return std::clamp(value, -k_Held, k_Held);
}

std::string Format(const Row& row, float value)
{
	std::array<char, 64> text {};
	const auto decimals = row.decimals;
	switch (row.show)
	{
	case ShowMost::OutOfMost:
		std::snprintf(text.data(), text.size(), "%0.*f/%0.*f", decimals, static_cast<double>(value), decimals,
		              static_cast<double>(row.most));
		break;
	case ShowMost::Percent:
	{
		constexpr float k_Hundred = 100.0f;
		const auto share = row.most != 0.0f ? value / row.most * k_Hundred : value;
		std::snprintf(text.data(), text.size(), "%0.*f%%", decimals, static_cast<double>(share));
		break;
	}
	case ShowMost::Number:
	case ShowMost::NumberToHighest:
		std::snprintf(text.data(), text.size(), "%0.*f", decimals, static_cast<double>(value));
		break;
	}
	return {text.data()};
}

std::optional<float> Total(const Row& row)
{
	if (row.show == ShowMost::Percent)
	{
		return std::nullopt;
	}
	float total = 0.0f;
	for (const auto value : row.values)
	{
		total += value;
	}
	return total;
}

bool DrawnAsBar(const Row& row, bool graphicTicked)
{
	return row.display == Display::Bar || (row.display == Display::AsTicked && graphicTicked);
}

int ColumnWidth(size_t values)
{
	return k_ValuesWidth / static_cast<int>(values + 1);
}

int ColumnCentre(int right, size_t values, size_t column)
{
	const auto width = ColumnWidth(values);
	return right - k_ValuesWidth + (static_cast<int>(column) * width) + (width / 2);
}

std::vector<float> BarParts(const Row& row)
{
	float scale = row.most;
	if (row.values.size() != 1 || scale <= 0.0f)
	{
		scale = 0.0f;
		for (const auto value : row.values)
		{
			scale += std::abs(value);
		}
		if (scale == 0.0f)
		{
			scale = 1.0f;
		}
	}
	std::vector<float> ends;
	float sum = 0.0f;
	for (const auto value : row.values)
	{
		sum += std::abs(value);
		ends.push_back(std::clamp(sum / scale, 0.0f, 1.0f));
	}
	return ends;
}

BarScale VerticalScale(const Row& row)
{
	BarScale scale {.lowest = 0.0f, .highest = 0.0f};
	if (row.values.size() == 1 && row.most > 0.0f)
	{
		scale.highest = row.most;
	}
	else
	{
		for (const auto value : row.values)
		{
			scale.highest = std::max(scale.highest, value);
		}
		if (scale.highest <= 0.0f)
		{
			scale.highest = 1.0f;
		}
	}
	for (const auto value : row.values)
	{
		scale.lowest = std::min(scale.lowest, value);
	}
	return scale;
}

float GuideStep(float range, float length)
{
	constexpr float k_FirstStep = 0.25f;
	const float quarter = length / 4.0f;
	float step = k_FirstStep;
	while (range / step > quarter)
	{
		step *= 2.0f;
	}
	return step;
}

glm::vec4 GuideColour(int64_t index, bool zeroWhite)
{
	if (zeroWhite && index == 0)
	{
		return glm::vec4(1.0f);
	}
	constexpr float k_Bright = static_cast<float>(0x50) / 255.0f;
	constexpr float k_Faint = static_cast<float>(0x10) / 255.0f;
	return {1.0f, 1.0f, 1.0f, (index & 3) == 0 ? k_Bright : k_Faint};
}

glm::vec4 PlayerColour(PlayerNames player, int32_t landNumber)
{
	const auto colour = player_colour::OnLand(player, landNumber);
	constexpr float k_Channel = 255.0f;
	return {static_cast<float>((colour >> 16u) & 0xFFu) / k_Channel, static_cast<float>((colour >> 8u) & 0xFFu) / k_Channel,
	        static_cast<float>(colour & 0xFFu) / k_Channel, 1.0f};
}

std::vector<std::pair<glm::ivec2, glm::ivec2>> CreatureBoxes(size_t players)
{
	// The creatures stand over the left and middle graph boxes, as high as the right one
	constexpr int k_Left = 40;
	constexpr int k_Top = 100;
	constexpr int k_Bottom = 330;
	constexpr int k_Gap = 10;
	std::vector<std::pair<glm::ivec2, glm::ivec2>> boxes;
	if (players == 0)
	{
		return boxes;
	}
	const auto count = static_cast<int>(players);
	if (players < 3)
	{
		const int width = (50 - count) * 10 / count;
		for (int i = 0; i < count; ++i)
		{
			const int x = k_Left + (i * (width + k_Gap));
			boxes.emplace_back(glm::ivec2 {x, k_Top}, glm::ivec2 {x + width, k_Bottom});
		}
		return boxes;
	}
	// Two rows: the third and fourth go below, the third alone centred
	constexpr int k_Width = 240;
	constexpr int k_Half = (k_Bottom - k_Top) / 2;
	for (int i = 0; i < count; ++i)
	{
		const bool below = i >= 2;
		int x = k_Left + ((i % 2) * (k_Width + k_Gap));
		if (below && count == 3)
		{
			x += 120;
		}
		const int top = below ? k_Top + k_Half + 5 : k_Top;
		boxes.emplace_back(glm::ivec2 {x, top}, glm::ivec2 {x + k_Width, top + k_Half - 5});
	}
	return boxes;
}

float CreatureTurn(uint32_t milliseconds)
{
	constexpr uint32_t k_Period = 0x2000;
	constexpr float k_PerMillisecond = 0.00076699f;
	return static_cast<float>(milliseconds & (k_Period - 1)) * k_PerMillisecond;
}

LineScale LinesScale(const std::vector<std::vector<float>>& lines)
{
	constexpr float k_FirstLowest = 1e10f;
	LineScale scale {.lowest = k_FirstLowest, .highest = 0.0f};
	for (const auto& line : lines)
	{
		for (const auto point : line)
		{
			scale.highest = std::max(scale.highest, point);
			scale.lowest = std::min(scale.lowest, point);
		}
	}
	if (!(scale.lowest < scale.highest))
	{
		scale.lowest = 0.0f;
	}
	if (scale.highest <= scale.lowest)
	{
		scale.highest = scale.lowest + 1.0f;
	}
	return scale;
}

} // namespace openblack::gui::statistics_box
