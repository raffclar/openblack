/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "EndGameBox.h"

#include <algorithm>

#include "ConditionsBox.h"
#include "GameFont.h"
#include "TextDatabase.h"

using namespace openblack::gui;

namespace
{
// The box fades in when it opens and out when it closes, as the other boxes of the game
constexpr float k_FadeInSeconds = 0.5f;
constexpr float k_FadeOutSeconds = 0.2f;

// The tabs: Game Over, Multiplayer, Statistics and four empty tabs, labelled in small text
constexpr size_t k_TabCount = 7;
constexpr size_t k_MultiplayerTab = 1;
constexpr size_t k_StatisticsTab = 2;
constexpr int k_TabTextSize = DialogPainter::k_SmallTextSize;

// The title's text size
constexpr int k_TitleTextSize = 49;

// The arrows at the bottom of the box
constexpr int k_ArrowSize = 40;
constexpr glm::ivec2 k_LeftArrow {20, 530};
constexpr glm::ivec2 k_RightArrow {740, 530};

// The band under the title is drawn at half the opacity of the box
constexpr float k_BandOpacity = 0.5f;
} // namespace

EndGameBox::EndGameBox(const TextDatabase& texts, const GameFont& font)
{
	std::vector<Dialog::Tab> tabs {
	    {.label = std::u16string(texts.Get("HELP_TEXT_GAME_OVER")), .onSelect = {}},
	    {.label = std::u16string(texts.Get("HELP_TEXT_MULTIPLAYER_MAIN_TAB")), .onSelect = [this] { OpenConditions(); }},
	    {.label = std::u16string(texts.Get("HELP_TEXT_PAUSE_STATS_151")), .onSelect = [this] { OpenStatistics(); }},
	};
	tabs.resize(k_TabCount);
	for (auto& tab : tabs)
	{
		tab.textSize = k_TabTextSize;
	}
	_dialog = std::make_unique<Dialog>(std::move(tabs), 0, k_Box);
	auto& dialog = *_dialog;

	// The band of the colour of the player's god under the title, at half opacity
	dialog.drawUnder = [this](const DialogPainter& painter) {
		auto colour = _contents.colour;
		colour.a *= k_BandOpacity;
		const auto& rect = k_TitleRect;
		painter.DrawShape(
		    {glm::vec2(rect.min), glm::vec2(rect.max.x, rect.min.y), glm::vec2(rect.max), glm::vec2(rect.min.x, rect.max.y)},
		    {colour, colour, colour, colour});
	};
	_title = &dialog.Add<StaticText>(k_TitleRect, std::u16string(), StaticText::Layout::Wrapped, k_TitleTextSize);

	using Look = BigButton::Look;
	using LabelSide = BigButton::LabelSide;
	_backButton =
	    &dialog.Add<BigButton>(font, k_LeftArrow, k_ArrowSize, std::u16string(texts.Get("HELP_TEXT_DIALOG_ADDITION_22")),
	                           LabelSide::Right, Look::LeftArrow);
	_backButton->onClick = [this] { Close(); };
	_watchButton =
	    &dialog.Add<BigButton>(font, k_LeftArrow, k_ArrowSize, std::u16string(texts.Get("HELP_TEXT_DIALOG_ADDITION_147")),
	                           LabelSide::Right, Look::LeftArrow);
	_watchButton->onClick = [this] { Close(); };
	_leaveButton =
	    &dialog.Add<BigButton>(font, k_RightArrow, k_ArrowSize, std::u16string(texts.Get("HELP_TEXT_DIALOG_LEAVE_GAME")),
	                           LabelSide::Left, Look::RightArrow);
	_leaveButton->onClick = [this] {
		_action = Action::Leave;
		Close();
	};

	// The Game Statistics box and the box of the winning conditions come back to the end box by their first tab and by
	// Back
	_statistics = std::make_unique<StatisticsBox>(texts, font, [this] { BackFromTab(); });
	_conditions = std::make_unique<ConditionsBox>(texts, font, [this] { BackFromTab(); });
}

EndGameBox::~EndGameBox() = default;

void EndGameBox::Open(Contents contents)
{
	_contents = std::move(contents);
	_title->text = _contents.title;
	// Back to Game shows only for the question to leave, and Watch Game only while the player can watch
	_backButton->visible = _contents.box.kind == end_game_box::Kind::LeaveQuestion;
	_watchButton->visible = _contents.box.kind == end_game_box::Kind::Lost && _contents.box.watch;
	_leaveButton->visible = true;
	_showsStatistics = false;
	_showsConditions = false;
	_dialog->SelectTab(0);
	_dialog->Reset();
	if (!_open)
	{
		_open = true;
		_fade.SetDestination(1.0f, k_FadeInSeconds);
	}
	if (_onShown)
	{
		_onShown(_contents);
	}
}

void EndGameBox::Close()
{
	if (!_open)
	{
		return;
	}
	_open = false;
	_fade.SetDestination(0.0f, k_FadeOutSeconds);
}

void EndGameBox::OpenStatistics()
{
	auto [figures, local] =
	    _statisticsSource ? _statisticsSource() : std::pair<statistics::BoxFigures, PlayerNames> {{}, PlayerNames::PLAYER_ONE};
	_dialog->SelectTab(k_StatisticsTab);
	_statistics->Open(std::move(figures), local, true);
	_showsStatistics = true;
}

bool EndGameBox::OpenTab(size_t tab)
{
	if (!_open)
	{
		return false;
	}
	switch (tab)
	{
	case k_MultiplayerTab:
		OpenConditions();
		return true;
	case k_StatisticsTab:
		OpenStatistics();
		return true;
	default:
		return false;
	}
}

void EndGameBox::OpenConditions()
{
	auto figures = _conditionsSource ? _conditionsSource() : conditions_box::Figures {};
	_dialog->SelectTab(k_MultiplayerTab);
	_conditions->Open(std::move(figures), true);
	_showsConditions = true;
}

void EndGameBox::BackFromTab()
{
	// The end box shows again as it opened, on its Game Over tab
	_showsStatistics = false;
	_showsConditions = false;
	_dialog->SelectTab(0);
	_dialog->Reset();
	if (_onShown)
	{
		_onShown(_contents);
	}
}

Dialog& EndGameBox::CurrentDialog() const
{
	if (_showsStatistics)
	{
		return _statistics->GetDialog();
	}
	return _showsConditions ? _conditions->GetDialog() : *_dialog;
}

void EndGameBox::Update(float deltaSeconds)
{
	_fade.Update(deltaSeconds);
	if (_showsStatistics)
	{
		_statistics->Update(deltaSeconds);
	}
	if (_open)
	{
		CurrentDialog().Update(deltaSeconds);
	}
}

void EndGameBox::MouseMove(glm::ivec2 point)
{
	if (_open)
	{
		CurrentDialog().MouseMove(point);
	}
}

void EndGameBox::MouseDown(glm::ivec2 point)
{
	if (_open)
	{
		CurrentDialog().MouseDown(point);
	}
}

bool EndGameBox::MouseUp(glm::ivec2 point)
{
	return _open && CurrentDialog().MouseUp(point);
}

std::vector<Dialog::NamedControl> EndGameBox::GetNamedControls() const
{
	return _open ? CurrentDialog().GetNamedControls() : std::vector<Dialog::NamedControl> {};
}

void EndGameBox::Draw(const DialogPainter& painter) const
{
	const auto fade = std::clamp(_fade.GetValue(), 0.0f, 1.0f);
	if (fade <= 0.0f)
	{
		return;
	}
	painter.SetAlpha(fade);
	CurrentDialog().Draw(painter, _open);
	painter.SetAlpha(1.0f);
}
