/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameInterface.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <SDL.h>
#include <bgfx/bgfx.h>
#include <spdlog/spdlog.h>

#include "3D/TempleInteriorInterface.h"
#include "Audio/AudioManagerInterface.h"
#include "Audio/GameSoundEffects.h"
#include "Camera/Camera.h"
#include "Common/FixedFormat.h"
#include "Common/GameRandom.h"
#include "Common/MachineClock.h"
#include "Creature/CreatureSkin.h"
#include "ECS/FloatingNumber.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/TattooEditorSystemInterface.h"
#include "ECS/Systems/TipBubbleSystemInterface.h"
#include "ECS/Systems/VideoSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/TexelDepth.h"
#include "Graphics/Texture2D.h"
#include "Graphics/VideoOverlay.h"
#include "Gui/CinemaBars.h"
#include "Gui/CinemaHides.h"
#include "Gui/GlyphTexture.h"
#include "Gui/ScreenFit.h"
#include "Gui/ScriptTexts.h"
#include "Gui/WorldToolTipRules.h"
#include "Help/ClickCue.h"
#include "Help/ControlIcons.h"
#include "Help/DialogueText.h"
#include "Help/HelpTextDisplay.h"
#include "Help/TipBubble.h"
#include "InfoConstants.h"
#include "Input/GameActionMapInterface.h"
#include "Input/KeyBindings.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "Temple/ChallengeRoomRules.h"

using namespace openblack::gui;
using openblack::Locator;
using openblack::filesystem::Path;

namespace
{
/// The scripts the game's text is in, in the order the game reads them
constexpr std::array k_TextScripts = {"InfoScript2.txt", "InfoScriptPatch2.txt", "InfoScriptMultiplayer2.txt"};
/// The dialogs' font, the first of the game's fonts
constexpr std::string_view k_Font = "j0";
/// The good advisor's font and the evil one's: as in the original game, each advisor's dialogue lines are drawn in a
/// font of its own, the other narrators' in j0
constexpr std::string_view k_GoodAdvisorFont = "f1";
constexpr std::string_view k_EvilAdvisorFont = "f3";
constexpr uint16_t k_AtlasSize = 256;

std::vector<uint8_t> ReadIfExists(const std::filesystem::path& path)
{
	auto& fileSystem = Locator::filesystem::value();
	if (!fileSystem.Exists(path))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Missing {} for the interface", path.generic_string());
		return {};
	}
	return fileSystem.ReadAll(path);
}

/// A key's name as the control pictures show it ("Shift", "Left", "Space")
openblack::help::control_icons::KeyNameFn ControlKeyName()
{
	return [](SDL_Scancode key) { return openblack::gui::ToUtf16(openblack::input::KeyChordName({.key = key})); };
}

/// One of the game's fonts, from its glyph metrics and bitmaps. Nothing when they are missing or don't fit together.
std::optional<GameFont> LoadFont(std::string_view name)
{
	auto& fileSystem = Locator::filesystem::value();
	const auto met = ReadIfExists(fileSystem.GetPath<Path::Data>() / (std::string(name) + ".met"));
	const auto fnt = ReadIfExists(fileSystem.GetPath<Path::Data>() / (std::string(name) + ".fnt"));
	auto font = GameFont::Load(met, fnt);
	if (!font)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Unable to read the font {}", name);
	}
	return font;
}

/// A font's glyphs, white with their coverage in alpha
std::unique_ptr<openblack::graphics::Texture2D> MakeFontTexture(const GameFont& font, std::string_view name)
{
	return openblack::gui::MakeGlyphTexture(font.GetAtlas(), font.GetAtlasSize(), std::string("Font ") + std::string(name));
}
} // namespace

namespace
{
/// A texture of the game made of a colour file and an alpha file, 256 pixels square. Null when they are missing.
std::unique_ptr<openblack::graphics::Texture2D>
LoadTexture(const std::string& name, openblack::graphics::Wrapping wrapping = openblack::graphics::Wrapping::ClampEdge,
            const std::function<uint8_t(uint8_t)>& level = {})
{
	auto& fileSystem = Locator::filesystem::value();
	const auto colour = ReadIfExists(fileSystem.GetPath<Path::Textures>() / (name + ".raw"));
	const auto alpha = ReadIfExists(fileSystem.GetPath<Path::Textures>() / (name + "a.raw"));
	constexpr size_t k_Pixels = static_cast<size_t>(k_AtlasSize) * k_AtlasSize;
	if (colour.size() != k_Pixels * 3 || alpha.size() != k_Pixels)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Unable to read the texture {}", name);
		return nullptr;
	}
	const auto* data = bgfx::alloc(static_cast<uint32_t>(k_Pixels * 4));
	for (size_t i = 0; i < k_Pixels; ++i)
	{
		data->data[(i * 4) + 0] = colour[(i * 3) + 0];
		data->data[(i * 4) + 1] = colour[(i * 3) + 1];
		data->data[(i * 4) + 2] = colour[(i * 3) + 2];
		data->data[(i * 4) + 3] = alpha[i];
		if (level)
		{
			for (size_t channel = 0; channel < 4; ++channel)
			{
				data->data[(i * 4) + channel] = level(data->data[(i * 4) + channel]);
			}
		}
	}
	// Kept four bits a channel, as the game keeps every texture it loads with an alpha
	openblack::graphics::texel_depth::Reduce(std::span(data->data, data->size),
	                                         openblack::graphics::texel_depth::Depth::FourBits);
	auto texture = std::make_unique<openblack::graphics::Texture2D>(name);
	texture->Create(k_AtlasSize, k_AtlasSize, 1, openblack::graphics::TextureFormat::RGBA8, wrapping,
	                openblack::graphics::Filter::Linear, data);
	return texture;
}

/// The tooltips' priorities and times from the info script, or the most common of them without it
std::array<ToolTipInfo, ToolTips::k_Count> ReadToolTipsInfo()
{
	std::array<ToolTipInfo, ToolTips::k_Count> info {};
	info.fill({.priority = 0.5f, .displayTime = 1.0f, .displayTimeAfterFocus = 0.0f});
	if (Locator::infoConstants::has_value())
	{
		const auto& toolTips = Locator::infoConstants::value().toolTips;
		for (size_t i = 0; i < info.size(); ++i)
		{
			info.at(i) = {.priority = toolTips.at(i).priority,
			              .displayTime = toolTips.at(i).displayTime,
			              .displayTimeAfterFocus = toolTips.at(i).displayTimeAfterFocus};
		}
	}
	return info;
}

/// atmos.raw's glow, which the tooltips' glow boxes are cut from, a ninth at a time
constexpr std::array k_GlowU = {0.0f, 0.078125f, 0.16797f, 0.24609f};
constexpr std::array k_GlowV = {0.25f, 0.32813f, 0.41797f, 0.49609f};
/// atmos.raw's arrows about a tooltip's mouse
struct ToolTipArrow
{
	uint32_t bit;
	glm::vec2 uvMin;
	glm::vec2 uvMax;
};
constexpr std::array k_ToolTipArrows = {
    ToolTipArrow {ToolTipArrows::k_Left, {0.7539f, 0.1289f}, {0.8711f, 0.2461f}},
    ToolTipArrow {ToolTipArrows::k_Right, {0.8789f, 0.0039f}, {0.9961f, 0.1211f}},
    ToolTipArrow {ToolTipArrows::k_Up, {0.7539f, 0.0039f}, {0.8711f, 0.1211f}},
    ToolTipArrow {ToolTipArrows::k_Down, {0.8789f, 0.1289f}, {0.9961f, 0.2461f}},
};
/// The tooltips' mice for a wheel mouse, the third row of mousehelp.raw
constexpr int k_ToolTipMouseRow = 2;

/// What the interface picked under the pointer at the last frame drawn, as depths in front of the camera
openblack::help::tip_bubble::UnderHand WhatIsUnderHand()
{
	openblack::help::tip_bubble::UnderHand under;
	if (!Locator::pickingSystem::has_value() || !Locator::camera::has_value())
	{
		return under;
	}
	const auto& pick = Locator::pickingSystem::value().GetPick();
	if (pick.object.has_value())
	{
		under.objectDepth = pick.distance;
	}
	if (pick.land.has_value())
	{
		under.landDepth = (Locator::camera::value().GetViewProjectionMatrix() * glm::vec4(*pick.land, 1.0f)).w;
	}
	return under;
}
} // namespace

std::unique_ptr<GameInterface> GameInterface::Create(std::u16string_view playerName, MenuSettings settings)
{
	auto& fileSystem = Locator::filesystem::value();

	TextDatabase texts;
	for (const auto* script : k_TextScripts)
	{
		const auto data = ReadIfExists(fileSystem.GetPath<Path::Scripts>() / script);
		// The first is the help texts' script, whose texts the game's scripts refer to by number
		if (script == k_TextScripts.front())
		{
			texts.AddHelpScript(data);
		}
		else
		{
			texts.AddScript(data);
		}
	}

	auto font = LoadFont(k_Font);
	if (!font)
	{
		return nullptr;
	}

	// The front end atlas, and the pictures of the player's symbols and of the tooltips' mice
	auto atlas = LoadTexture("Front_end_buttons");
	if (!atlas)
	{
		return nullptr;
	}
	auto symbols = LoadTexture("ChooseSymbol");
	auto mice = LoadTexture("mousehelp");
	// The atmosphere texture's glows and the tooltips' arrows
	auto atmos = LoadTexture("ATMOS");

	auto fontTexture = MakeFontTexture(*font, k_Font);

	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Interface: {} texts, font {} with {} glyphs", texts.GetCount(), font->GetName(),
	                    font->GetGlyphs().size());
	auto created = std::unique_ptr<GameInterface>(new GameInterface(std::move(texts), std::move(*font), std::move(atlas),
	                                                                std::move(fontTexture), std::move(symbols), std::move(mice),
	                                                                std::move(atmos), playerName, std::move(settings)));
	// The advisors' fonts for the dialogue, j0 standing in for one that is missing
	if (auto good = LoadFont(k_GoodAdvisorFont))
	{
		auto texture = MakeFontTexture(*good, k_GoodAdvisorFont);
		created->_goodAdvisorFont = FontFace {.font = std::move(*good), .texture = std::move(texture)};
	}
	if (auto evil = LoadFont(k_EvilAdvisorFont))
	{
		auto texture = MakeFontTexture(*evil, k_EvilAdvisorFont);
		created->_evilAdvisorFont = FontFace {.font = std::move(*evil), .texture = std::move(texture)};
	}
	// The tip bubble's box and tail
	created->_gatheringText = LoadTexture("gatheringtext");
	// The front end's swirling backdrop, its texture repeating across the screen, kept as the game kept it: 4 bits to a
	// colour and to its alpha
	created->_backdrop = LoadTexture("pin", graphics::Wrapping::Repeat, menu_backdrop::SixteenBitLevel);
	// The gestures' pictures, for the tips that show one
	for (size_t i = 0; i < created->_gestureSheets.size(); ++i)
	{
		created->_gestureSheets.at(i) = LoadTexture("S_Gesture" + std::to_string(i));
	}
	created->_hintDotSheet = LoadTexture("S_SpriteSheet2");
	return created;
}

GameInterface::GameInterface(TextDatabase texts, GameFont font, std::unique_ptr<graphics::Texture2D> atlas,
                             std::unique_ptr<graphics::Texture2D> fontTexture, std::unique_ptr<graphics::Texture2D> symbols,
                             std::unique_ptr<graphics::Texture2D> mice, std::unique_ptr<graphics::Texture2D> atmos,
                             std::u16string_view playerName, MenuSettings settings)
    : _texts(std::move(texts))
    , _font(std::move(font))
    , _atlas(std::move(atlas))
    , _fontTexture(std::move(fontTexture))
    , _symbols(std::move(symbols))
    , _mice(std::move(mice))
    , _atmos(std::move(atmos))
    , _painter(_canvas, _font, *_atlas, *_fontTexture)
    , _menu(std::make_unique<GameMenu>(_texts, _font, playerName, std::move(settings)))
    , _skipBox(std::make_unique<SkipBox>(_texts, _font, DialogPainter::k_MidTextSize))
    , _tattooEditor(std::make_unique<TattooEditorDialog>(_texts, _font))
    , _newPlayerBox(std::make_unique<NewPlayerBox>(_texts, _font))
    , _toolTips(ReadToolTipsInfo())
{
	_painter.SetPictures(_symbols.get(), _mice.get());
	_fullFontTexture = MakeGlyphTexture(_font.GetFullAtlas(), _font.GetFullAtlasSize(), "Font full height");
	_painter.SetFullGlyphs(_fullFontTexture.get());
}

GameInterface::~GameInterface() = default;

void GameInterface::ShowSkipBox(bool currentProfileHasCreature)
{
	_skipBoxAnswer.reset();
	_skipBox->Show(currentProfileHasCreature);
	// The control under the pointer lights up at once
	glm::ivec2 mouse;
	SDL_GetMouseState(&mouse.x, &mouse.y);
	_skipBox->MouseMove(_painter.ToDialog(mouse));
}

void GameInterface::ShowNewPlayerBox(bool canCancel, NewPlayerBox::CheckName checkName)
{
	_newPlayerBox->checkName = std::move(checkName);
	_newPlayerBox->Show(canCancel);
	// The control under the pointer lights up at once
	glm::ivec2 mouse;
	SDL_GetMouseState(&mouse.x, &mouse.y);
	_newPlayerBox->MouseMove(_painter.ToDialog(mouse));
}

void GameInterface::ShowNewPlayerBoxOverGame(bool canCancel, NewPlayerBox::CheckName checkName)
{
	_boxOverGameFade = menu_backdrop::Fade {};
	ShowNewPlayerBox(canCancel, std::move(checkName));
}

bool GameInterface::ProcessNewPlayerBoxEvent(const SDL_Event& event, glm::u16vec2 resolution)
{
	_painter.Begin(resolution);
	switch (event.type)
	{
	case SDL_MOUSEMOTION:
		_newPlayerBox->MouseMove(_painter.ToDialog({event.motion.x, event.motion.y}));
		return true;
	case SDL_MOUSEBUTTONDOWN:
		if (event.button.button == SDL_BUTTON_LEFT)
		{
			_newPlayerBox->MouseDown(_painter.ToDialog({event.button.x, event.button.y}));
		}
		return true;
	case SDL_MOUSEBUTTONUP:
		// Every control clicks as it is let go over, the texts too
		if (event.button.button == SDL_BUTTON_LEFT &&
		    _newPlayerBox->MouseUp(_painter.ToDialog({event.button.x, event.button.y})))
		{
			audio::PlayGameSoundEffect(static_cast<entt::id_type>(audio::SoundId::G_MenuButton), std::nullopt);
		}
		return true;
	case SDL_TEXTINPUT:
		_newPlayerBox->TextInput(ToUtf16(event.text.text));
		return true;
	case SDL_KEYDOWN:
		_newPlayerBox->KeyDown(event.key.keysym.sym);
		return true;
	default:
		return false;
	}
}

void GameInterface::DrawNewPlayerScreen(glm::u16vec2 resolution, glm::ivec2 mouse, uint32_t milliseconds,
                                        float frameMilliseconds, uint8_t black)
{
	_canvas.Begin(resolution);
	_pointerCanvas.Begin(resolution);
	_painter.Begin(resolution);
	_newPlayerBox->Update(frameMilliseconds / 1000.0f);
	DrawMenuBackdrop(resolution, frameMilliseconds);
	_newPlayerBox->Draw(_painter);
	_painter.DrawPointer(_pointerCanvas, mouse, milliseconds);
	// The screen fades from and to black over it all, the pointer too
	if (black > 0)
	{
		_pointerCanvas.DrawQuad(glm::vec2(0.0f), glm::vec2(resolution), glm::vec2(0.0f), glm::vec2(1.0f),
		                        glm::vec4(0.0f, 0.0f, 0.0f, static_cast<float>(black) / 255.0f), nullptr);
	}
	_canvas.End();
	_pointerCanvas.End();
}

void GameInterface::DrawMenuBackdrop(glm::u16vec2 resolution, float frameMilliseconds)
{
	// The swirls turn on at most once a screen refresh; they start at random the first time
	if (!_backdropSwirls.has_value())
	{
		_backdropSwirls = menu_backdrop::Start([](float a, float b) { return Locator::gameRandom::value().CrtRandom(a, b); });
		_backdropPace.sinceStep = 0.0f;
		for (int layer = 0; layer < menu_backdrop::k_Layers; ++layer)
		{
			menu_backdrop::PlaceLayer(*_backdropSwirls, layer, 0.0f, _backdropLayers.at(static_cast<size_t>(layer)));
		}
	}
	if (const auto step = menu_backdrop::StepPace(_backdropPace, frameMilliseconds); step.has_value())
	{
		const auto turn = static_cast<float>(*step) * menu_backdrop::k_TurnPerMillisecond;
		for (int layer = 0; layer < menu_backdrop::k_Layers; ++layer)
		{
			menu_backdrop::PlaceLayer(*_backdropSwirls, layer, turn, _backdropLayers.at(static_cast<size_t>(layer)));
		}
	}
	// The backdrop covers the screen's 4:3 box, black round it
	if (_backdrop)
	{
		const auto box = FitInside(k_FourByThree, resolution.x, resolution.y);
		const auto size = glm::ivec2(box.width, box.height);
		const auto origin = glm::vec2(static_cast<float>(box.x), static_cast<float>(box.y));
		const auto point = [&size, &origin](int column, int row) {
			return origin + glm::vec2(menu_backdrop::PointOnScreen(column, row, size));
		};
		const auto white = glm::vec4(1.0f);
		for (const auto& layer : _backdropLayers)
		{
			const auto uv = [&layer](int column, int row) {
				return layer.at((static_cast<size_t>(column) * menu_backdrop::k_Rows) + static_cast<size_t>(row));
			};
			for (int column = 1; column < menu_backdrop::k_Columns; ++column)
			{
				for (int row = 1; row < menu_backdrop::k_Rows; ++row)
				{
					// Two triangles a square, split from its top left corner to its bottom right
					_canvas.DrawShape(
					    {point(column - 1, row - 1), point(column, row - 1), point(column, row), point(column - 1, row)},
					    {uv(column - 1, row - 1), uv(column, row - 1), uv(column, row), uv(column - 1, row)},
					    {white, white, white, white}, _backdrop.get());
				}
			}
		}
	}
}

void GameInterface::SetPlayerProfile(std::u16string_view name, std::u16string_view creatureName, int symbol,
                                     const player_profiles::Settings& kept)
{
	auto settings = _menu->GetSettings();
	settings.creatureName = creatureName;
	settings.symbol = symbol;
	TakeProfileSettings(settings, kept);
	const bool insideTemple = _menu->IsInsideTemple();
	_menu = std::make_unique<GameMenu>(_texts, _font, name, std::move(settings));
	_menu->SetInsideTemple(insideTemple);
}

bool GameInterface::ProcessSkipBoxEvent(const SDL_Event& event)
{
	switch (event.type)
	{
	case SDL_MOUSEMOTION:
		_skipBox->MouseMove(_painter.ToDialog({event.motion.x, event.motion.y}));
		return true;
	case SDL_MOUSEBUTTONDOWN:
		if (event.button.button == SDL_BUTTON_LEFT)
		{
			_skipBox->MouseDown(_painter.ToDialog({event.button.x, event.button.y}));
		}
		return true;
	case SDL_MOUSEBUTTONUP:
		if (event.button.button == SDL_BUTTON_LEFT)
		{
			const auto result = _skipBox->MouseUp(_painter.ToDialog({event.button.x, event.button.y}));
			// Every control clicks as it is let go over, the texts too
			if (result.clicked)
			{
				audio::PlayGameSoundEffect(static_cast<entt::id_type>(audio::SoundId::G_MenuButton), std::nullopt);
			}
			if (result.answer.has_value())
			{
				_skipBoxAnswer = result.answer;
			}
		}
		return true;
	// No key answers the box or gets past it, Escape neither; keys let go of still reach the game
	case SDL_MOUSEWHEEL:
	case SDL_TEXTINPUT:
	case SDL_KEYDOWN:
		return true;
	default:
		return false;
	}
}

void GameInterface::OpenMenu()
{
	_menu->Open();
	// For the names typed into the menu
	SDL_StartTextInput();
}

bool GameInterface::ProcessEvent(const SDL_Event& event, glm::u16vec2 resolution)
{
	_painter.Begin(resolution);
	if (_skipBox->IsActive())
	{
		return ProcessSkipBoxEvent(event);
	}
	// The new player's box, from the Players page, takes the input while it is up; nothing else does while its screen
	// fades
	if (_newPlayerBox->IsActive())
	{
		return ProcessNewPlayerBoxEvent(event, resolution);
	}
	if (_boxOverGameFade.has_value())
	{
		return true;
	}
	if (_tattooEditor->IsOpen() && Locator::tattooEditorSystem::has_value())
	{
		return ProcessTattooEditorEvent(event);
	}
	// Escape opening the menu is the game's to decide (OpenMenu)
	if (!_menu->IsOpen())
	{
		return false;
	}
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
	{
		_action = _menu->Escape();
		if (_action == GameMenu::Action::Continue)
		{
			_menu->Close();
			SDL_StopTextInput();
		}
		return true;
	}

	switch (event.type)
	{
	case SDL_MOUSEMOTION:
		_menu->MouseMove(_painter.ToDialog({event.motion.x, event.motion.y}));
		return true;
	case SDL_MOUSEBUTTONDOWN:
		if (event.button.button == SDL_BUTTON_LEFT)
		{
			_menu->MouseDown(_painter.ToDialog({event.button.x, event.button.y}));
		}
		return true;
	case SDL_MOUSEBUTTONUP:
		if (event.button.button == SDL_BUTTON_LEFT)
		{
			_action = _menu->MouseUp(_painter.ToDialog({event.button.x, event.button.y}));
			// In the game's dialogs every control clicks as it acts
			if (_menu->TakeClicked())
			{
				audio::PlayGameSoundEffect(static_cast<entt::id_type>(audio::SoundId::G_MenuButton), std::nullopt);
			}
			if (_action == GameMenu::Action::Continue)
			{
				_menu->Close();
				SDL_StopTextInput();
			}
		}
		return true;
	case SDL_MOUSEWHEEL:
	{
		glm::ivec2 mouse;
		SDL_GetMouseState(&mouse.x, &mouse.y);
		_menu->Wheel(_painter.ToDialog(mouse), event.wheel.y);
		return true;
	}
	case SDL_TEXTINPUT:
		_menu->TextInput(ToUtf16(event.text.text));
		return true;
	// Keys let go of still reach the game, so that none stays held down
	case SDL_KEYDOWN:
		_menu->KeyDown(event.key.keysym.sym);
		return true;
	default:
		return false;
	}
}

bool GameInterface::ProcessTattooEditorEvent(const SDL_Event& event)
{
	auto& editor = Locator::tattooEditorSystem::value();
	switch (event.type)
	{
	case SDL_MOUSEMOTION:
		_tattooEditor->MouseMove(_painter.ToDialog({event.motion.x, event.motion.y}));
		return true;
	case SDL_MOUSEBUTTONDOWN:
		if (event.button.button == SDL_BUTTON_LEFT)
		{
			_leftButtonDown = true;
			_tattooEditor->MouseDown(_painter.ToDialog({event.button.x, event.button.y}));
		}
		return true;
	case SDL_MOUSEBUTTONUP:
		if (event.button.button == SDL_BUTTON_LEFT)
		{
			_leftButtonDown = false;
		}
		if (event.button.button == SDL_BUTTON_LEFT &&
		    _tattooEditor->MouseUp(_painter.ToDialog({event.button.x, event.button.y}), editor))
		{
			// In the game's dialogs every control clicks as it acts
			audio::PlayGameSoundEffect(static_cast<entt::id_type>(audio::SoundId::G_MenuButton), std::nullopt);
		}
		return true;
	case SDL_MOUSEWHEEL:
		return true;
	case SDL_KEYDOWN:
		_tattooEditor->KeyDown(event.key.keysym.sym, editor);
		return true;
	default:
		return false;
	}
}

GameMenu::Action GameInterface::TakeAction()
{
	return std::exchange(_action, GameMenu::Action::None);
}

void GameInterface::Update(float deltaSeconds)
{
	_menu->Update(deltaSeconds);
	_skipBox->Update(deltaSeconds);
	_newPlayerBox->Update(deltaSeconds);
	UpdateTattooEditor(deltaSeconds);
	_toolTips.Update(deltaSeconds);
	_screenFade.Update(deltaSeconds);
}

void GameInterface::UpdateTattooEditor(float deltaSeconds)
{
	if (!Locator::tattooEditorSystem::has_value())
	{
		return;
	}
	auto& editor = Locator::tattooEditorSystem::value();
	if (editor.IsOpen() && !_tattooEditor->IsOpen())
	{
		// In the temple the dialog stands over the cave without a frame
		_tattooEditor->Open(false);
	}
	std::span<const std::array<uint8_t, 3>> palette;
	auto& arts = Locator::resources::value().GetCreatureSkinArt();
	if (arts.Contains(creature_skin::k_ArtId))
	{
		palette = arts.Handle(creature_skin::k_ArtId)->palette;
	}
	if (!_tattooEditor->IsOpen())
	{
		_leftButtonDown = false;
	}
	_tattooEditor->Update(deltaSeconds, editor, palette, _leftButtonDown);
}

void GameInterface::Draw(glm::u16vec2 resolution, glm::ivec2 mouse, uint32_t milliseconds, bool overDebugWindow)
{
	_canvas.Begin(resolution);
	_pointerCanvas.Begin(resolution);
	_painter.Begin(resolution);
	// The villagers' labels and the tip bubble are part of the world's picture: under the interface, the cinema bars
	// and the fades
	DrawVillagerLabels(resolution);
	DrawTipBubble(resolution, mouse);
	// The gestures' hints are drawn with the world too, over all of it
	DrawGestureHints(resolution);
	const bool menuOpen =
	    (_menu->IsVisible() && _menu->IsOpen()) || _skipBox->IsActive() || _tattooEditor->IsOpen() || _newPlayerBox->IsActive();
	// A cut scene leaves some of the interface off the screen
	const auto& cinema = Locator::cinematicDirectorSystem::value();
	const cinema_hides::Bars cinemaBars {.in = cinema.IsWideScreenOn(), .heldByScript = cinema.GetWideScreenOwner() != 0};
	if (cinema_hides::ShowsFloatingNumbers(cinemaBars))
	{
		DrawFloatingNumbers(resolution);
	}
	if (_message.has_value())
	{
		_painter.DrawTextWrapped(DialogRect {{0, 0}, DialogPainter::k_Size}, true, _message->text, 60,
		                         glm::vec4(1.0f, 1.0f, 1.0f, _message->alpha));
	}
	DrawCaption(resolution);
	// The game leaves the creature's panel and the tooltip out under a dialog
	if (!menuOpen)
	{
		DrawFightPanel(resolution);
		if (cinema_hides::ShowsCreaturePanel(cinemaBars))
		{
			DrawCreaturePanel(resolution);
		}
		if (cinema_hides::ShowsToolTip(cinemaBars))
		{
			DrawToolTip(resolution);
		}
	}
	if (_tattooEditor->IsVisible())
	{
		// The marker on the place under the pointer while the mouse button is down
		std::optional<glm::ivec2> marker;
		if (Locator::tattooEditorSystem::has_value() && _leftButtonDown)
		{
			if (const auto point = Locator::tattooEditorSystem::value().GetHoveredPoint(); point.has_value())
			{
				marker = _painter.ToDialog(*point);
			}
		}
		_tattooEditor->Draw(_painter, marker, milliseconds);
	}
	// The new player's box from the Players page takes the screen as the first start's does: the front end's swirling
	// backdrop on black, coming up from black and going back to black, the box and the pointer under the black too.
	// The menu comes back once it is over.
	const auto backdropMilliseconds = menu_backdrop::FrameMilliseconds(_lastBackdropTicks, milliseconds);
	_lastBackdropTicks = _boxOverGameFade.has_value() ? std::optional(milliseconds) : std::nullopt;
	if (_boxOverGameFade.has_value())
	{
		menu_backdrop::StepFade(*_boxOverGameFade, !_newPlayerBox->IsActive(), static_cast<int>(backdropMilliseconds));
		_canvas.DrawQuad(glm::vec2(0.0f), glm::vec2(resolution), glm::vec2(0.0f), glm::vec2(1.0f),
		                 glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), nullptr);
		DrawMenuBackdrop(resolution, backdropMilliseconds);
		_newPlayerBox->Draw(_painter);
		_painter.DrawPointer(_pointerCanvas, mouse, milliseconds);
		if (const auto black = _boxOverGameFade->alpha; black > 0)
		{
			_pointerCanvas.DrawQuad(glm::vec2(0.0f), glm::vec2(resolution), glm::vec2(0.0f), glm::vec2(1.0f),
			                        glm::vec4(0.0f, 0.0f, 0.0f, static_cast<float>(black) / 255.0f), nullptr);
		}
		if (menu_backdrop::FadeOver(*_boxOverGameFade))
		{
			_boxOverGameFade.reset();
		}
	}
	else
	{
		if (_menu->IsVisible())
		{
			_menu->Draw(_painter);
		}
		_skipBox->Draw(_painter);
		if (menuOpen || overDebugWindow)
		{
			_painter.DrawPointer(_pointerCanvas, mouse, milliseconds);
		}
	}
	// A video on a screen wider than its picture has black bars at its sides, which go with the cinema bars
	const auto sides = static_cast<float>(DrawVideo(resolution));
	// The scripts' fade covers the picture between the cinema bars, which are black
	const auto& director = Locator::cinematicDirectorSystem::value();
	const auto bars = static_cast<float>(CinemaBars::BarHeight(resolution.x, resolution.y, director.GetWideScreenFraction()));
	const auto screen = glm::vec2(resolution);
	const auto drawBars = [&screen, bars, sides](Canvas& canvas) {
		const auto black = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		if (bars > 0.0f)
		{
			canvas.DrawQuad(glm::vec2(0.0f), {screen.x, bars}, glm::vec2(0.0f), glm::vec2(1.0f), black, nullptr);
			canvas.DrawQuad({0.0f, screen.y - bars}, screen, glm::vec2(0.0f), glm::vec2(1.0f), black, nullptr);
		}
		if (sides > 0.0f)
		{
			canvas.DrawQuad(glm::vec2(0.0f), {sides, screen.y}, glm::vec2(0.0f), glm::vec2(1.0f), black, nullptr);
			canvas.DrawQuad({screen.x - sides, 0.0f}, screen, glm::vec2(0.0f), glm::vec2(1.0f), black, nullptr);
		}
	};
	// The bars cover the scene in a view of their own, which the advisors near the screen are drawn over: clinging to
	// the bottom of the screen during a cut scene, an advisor shows over the bottom bar
	_cinemaBarsCanvas.Begin(resolution);
	drawBars(_cinemaBarsCanvas);
	_cinemaBarsCanvas.End();
	// Inside the temple the temple draws its own fade, never the scripts': a scene that faded to black and took the
	// player into the temple, as the game's end does, shows the room
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	if (const auto fade = director.GetFadeColour(); !inTemple && (fade >> 24u) != 0)
	{
		const auto colour = glm::vec4(static_cast<float>((fade >> 16u) & 0xFFu), static_cast<float>((fade >> 8u) & 0xFFu),
		                              static_cast<float>(fade & 0xFFu), static_cast<float>(fade >> 24u)) /
		                    255.0f;
		_canvas.DrawQuad({sides, bars}, {screen.x - sides, screen.y - bars}, glm::vec2(0.0f), glm::vec2(1.0f), colour, nullptr);
		// While a fade shows, the bars are drawn again over it and over the advisors
		drawBars(_canvas);
	}
	DrawDialogue(resolution, static_cast<int>(bars));
	// The scripts' words go over the advisors' dialogue and the scripts' fade, but not while a video plays
	if (!Locator::videoSystem::has_value() || !Locator::videoSystem::value().IsPlaying())
	{
		DrawScriptTexts(resolution);
	}
	// The game covers the frame with the fade's colour last of all
	if (const auto fade = _screenFade.GetColour(); fade.a > 0.0f)
	{
		_canvas.DrawQuad(glm::vec2(0.0f), glm::vec2(resolution), glm::vec2(0.0f), glm::vec2(1.0f), fade, nullptr);
	}
	_canvas.End();
	_pointerCanvas.End();
}

void GameInterface::DrawScriptTexts(glm::u16vec2 resolution)
{
	const auto screen = glm::vec2(resolution);
	for (const auto& text : Locator::cinematicDirectorSystem::value().GetTexts())
	{
		const auto box = script_text::Place(text, screen);
		const auto lines = _font.Wrap(text.words, box.size, box.max.x - box.min.x);
		const auto laid =
		    script_text::Lay(box, lines, [this, &box](std::u16string_view words) { return _font.GetWidth(words, box.size); });
		// The text is drawn in the scripts' colour at its fade's alpha. The game also draws it a pixel down and right
		// first, as a shadow, but with no alpha at all, so that nothing of it shows.
		const auto colour = glm::vec3(text.colour) / 255.0f;
		const auto alpha = static_cast<float>(script_text::AlphaByte(text.alpha)) / 255.0f;
		for (const auto& line : laid)
		{
			_painter.DrawString(line.at, line.words, box.size, glm::vec4(colour, alpha * line.topShare),
			                    glm::vec4(colour, alpha * line.bottomShare));
		}
	}
}

int GameInterface::DrawVideo(glm::u16vec2 resolution)
{
	if (!Locator::videoSystem::has_value())
	{
		return 0;
	}
	const auto& videos = Locator::videoSystem::value();
	const auto picture = videos.GetPicture();
	if (!picture.has_value())
	{
		return 0;
	}
	const auto rect = video::LetterboxRect(resolution.x, resolution.y);
	// Over the interface, under the scripts' fade and the cinema bars: what has been drawn so far goes first
	_canvas.End();
	if (!_video)
	{
		_video = std::make_unique<graphics::VideoOverlay>();
	}
	_video->Draw(graphics::RenderPass::Interface, resolution,
	             {
	                 .y = picture->y,
	                 .u = picture->u,
	                 .v = picture->v,
	                 .yStride = picture->yStride,
	                 .chromaStride = picture->chromaStride,
	                 .width = picture->width,
	                 .height = picture->height,
	                 .serial = picture->serial,
	             },
	             rect, picture->alpha, videos.IsSixteenBitColour());
	_canvas.Begin(resolution);
	return rect.x;
}

void GameInterface::DrawGlow(glm::vec2 min, glm::vec2 max, glm::vec4 colour)
{
	// A ninth of the glow at each corner, a square of half the box's height (in whole pixels) about each of its corners
	if (_atmos == nullptr || colour.a <= 0.0f)
	{
		return;
	}
	const auto corner = static_cast<float>((static_cast<int>(max.y) - static_cast<int>(min.y)) / 4);
	const std::array x = {min.x - corner, min.x + corner, max.x - corner, max.x + corner};
	const std::array y = {min.y - corner, min.y + corner, max.y - corner, max.y + corner};
	_canvas.SetBlend(Canvas::Blend::Additive);
	for (size_t row = 0; row < 3; ++row)
	{
		for (size_t column = 0; column < 3; ++column)
		{
			_canvas.DrawQuad({x.at(column), y.at(row)}, {x.at(column + 1), y.at(row + 1)},
			                 {k_GlowU.at(column), k_GlowV.at(row)}, {k_GlowU.at(column + 1), k_GlowV.at(row + 1)}, colour,
			                 _atmos.get());
		}
	}
	_canvas.SetBlend(Canvas::Blend::Alpha);
}

std::pair<const GameFont*, const openblack::graphics::Texture2D*> GameInterface::DialogueFont(help::TextFont font) const
{
	if (font == help::TextFont::F1 && _goodAdvisorFont.has_value())
	{
		return {&_goodAdvisorFont->font, _goodAdvisorFont->texture.get()};
	}
	if (font == help::TextFont::F3 && _evilAdvisorFont.has_value())
	{
		return {&_evilAdvisorFont->font, _evilAdvisorFont->texture.get()};
	}
	return {&_font, _fontTexture.get()};
}

void GameInterface::DrawDialogue(glm::u16vec2 resolution, int barPixels)
{
	// Neither the box nor its words show while a video plays; the texts still move on underneath
	const bool videoPlaying = Locator::videoSystem::has_value() && Locator::videoSystem::value().IsPlaying();
	const auto* frame = [&]() -> const help::TextFrame* {
		if (!Locator::helpTextSystem::has_value() || videoPlaying)
		{
			return nullptr;
		}
		const help::WidthFn widthOf = [this](help::TextFont font, std::u16string_view text, float size) {
			return DialogueFont(font).first->GetWidth(text, size);
		};
		return Locator::helpTextSystem::value().Layout(glm::ivec2(resolution), barPixels, widthOf);
	}();
	_dialogueWordsDrawn = frame != nullptr && !frame->runs.empty();
	if (frame != nullptr)
	{
		// The box covers whole pixels, its right and bottom ones included
		if (frame->boxShown)
		{
			const auto& box = frame->band;
			const auto colour = glm::vec4(0.0f, 0.0f, 0.0f, static_cast<float>(frame->boxAlpha) / 255.0f);
			_canvas.DrawQuad({static_cast<float>(box.left), static_cast<float>(box.top)},
			                 {static_cast<float>(box.right + 1), static_cast<float>(box.bottom + 1)}, glm::vec2(0.0f),
			                 glm::vec2(1.0f), colour, nullptr);
		}
		// Over the box and under the words, the "Continue" cue while the text waits for a click, whether or not the
		// texts are drawn
		if (const auto* dialogue = Locator::helpTextSystem::value().GetDialogue(); dialogue != nullptr)
		{
			if (const auto share = dialogue->GetClickCueShare(); share.has_value())
			{
				DrawClickCue(frame->box, *share);
			}
			DrawControlIcons(*frame, *dialogue);
		}
	}
	// The hand's helper icons go over the box and under the words
	DrawHandTricons(resolution);
	if (frame != nullptr)
	{
		for (const auto& run : frame->runs)
		{
			DrawDialogueRun(run);
		}
	}
}

void GameInterface::DrawHandTricons(glm::u16vec2 resolution)
{
	namespace tricons = hand_tricons;
	// Not under a dialog; otherwise the fades hold where they are until the icons are drawn again
	const bool menuOpen = (_menu->IsVisible() && _menu->IsOpen()) || _skipBox->IsActive() || _tattooEditor->IsOpen();
	if (!_handTricons.has_value() || menuOpen || _atmos == nullptr)
	{
		return;
	}
	const auto& in = *_handTricons;
	const bool toolTipsOn = _menu->GetSettings().toolTips != 0;
	tricons::Fade(_tricons, {
	                            .icons = tricons::Shown(in.icons, toolTipsOn, in.handStateShowsIcons),
	                            .seconds = in.seconds,
	                            .demonstration = in.demonstration,
	                            .cameraBusy = in.cameraBusy,
	                            .crossNudge = in.crossNudge,
	                        });
	const auto screen = glm::ivec2(resolution);
	const auto centre = tricons::Place(in.hand, in.cursor, screen, in.cinemaBars, in.leans);
	for (const auto& sprite : tricons::Sprites(glm::vec2(centre), in.halfSize, _tricons, in.rotateAngle))
	{
		if (sprite.has_value())
		{
			const auto colour = glm::vec4(1.0f, 1.0f, 1.0f, sprite->alpha);
			_canvas.DrawShape(sprite->corners, sprite->uvs, {colour, colour, colour, colour}, _atmos.get());
		}
	}

	// The demonstration's mouse beside them, whether or not an icon shows
	if (!in.demonstration)
	{
		return;
	}
	// The languages that need bigger text aren't chosen in openblack yet
	constexpr bool k_BiggerText = false;
	_demoMouseOnLeft = tricons::LabelOnLeft(centre.x, screen.x, _demoMouseOnLeft);
	const auto& label = _texts.GetHelpText(tricons::k_DemoText).text;
	// Sized for the original's default screen, and scaled with the screen's height on wide screens
	const auto scale = tricons::DemoScale(glm::ivec2(screen));
	const auto labelSize = static_cast<float>(tricons::DemoLabelSize(k_BiggerText, scale));
	const auto mouse = tricons::LayoutDemoMouse(centre, _demoMouseOnLeft, _font.GetWidth(label, labelSize), k_BiggerText,
	                                            in.moveHeld, in.actionHeld, scale);
	DrawGlow(mouse.mouseMin, mouse.mouseMax, mouse.mouseGlow);
	if (!label.empty())
	{
		DrawGlow(mouse.labelGlowMin, mouse.labelGlowMax, mouse.labelGlow);
	}
	if (_mice != nullptr)
	{
		_canvas.DrawQuad(mouse.mouseMin, mouse.mouseMax, mouse.uvMin, mouse.uvMax, glm::vec4(1.0f), _mice.get());
	}
	const auto shadow = glm::vec2(mouse.shadowOffset);
	_painter.DrawString(mouse.labelAt - shadow, label, mouse.labelSize, mouse.shadowColour);
	_painter.DrawString(mouse.labelAt + shadow, label, mouse.labelSize, mouse.shadowColour);
	_painter.DrawString(mouse.labelAt, label, mouse.labelSize, mouse.labelColour);
}

void GameInterface::DrawTipBubble(glm::u16vec2 resolution, glm::ivec2 mouse)
{
	namespace tip = help::tip_bubble;
	if (!Locator::tipBubbleSystem::has_value() || !Locator::camera::has_value() || _gatheringText == nullptr)
	{
		return;
	}
	auto& bubble = Locator::tipBubbleSystem::value();
	// Only a bubble drawn this frame can be under the hand
	bubble.SetUnderHand(false);
	const auto anchor = bubble.GetAnchor();
	if (!bubble.IsUp() || !anchor.has_value() || bubble.GetDisplayTime() <= 0.0f)
	{
		return;
	}
	const auto clip = Locator::camera::value().GetViewProjectionMatrix() * glm::vec4(*anchor, 1.0f);
	if (clip.w <= 0.0f)
	{
		return;
	}
	const auto screen = glm::vec2(resolution);
	const glm::vec2 pixel {(clip.x / clip.w + 1.0f) * screen.x * 0.5f, (1.0f - clip.y / clip.w) * screen.y * 0.5f};
	const auto placement = tip::Place(pixel, clip.w, screen.x, ReferenceScale(resolution.x, resolution.y));
	if (!placement.has_value())
	{
		return;
	}
	auto& scroll = bubble.GetScroll();
	tip::ClampScroll(scroll, *placement);
	const float alpha = std::min(bubble.GetDisplayTime(), 1.0f) * placement->distanceAlpha;
	// After the words are drawn: gripped, they follow the hand from the next frame on; the pointer over the bubble keeps
	// it showing, and the hand is on it unless something under the pointer is nearer
	const auto pointer = glm::vec2(mouse);
	const auto afterDrawing = [&bubble, &scroll, &placement, pointer]() {
		if (bubble.IsGripped())
		{
			help::tip_bubble::FollowHand(scroll, pointer.y);
		}
		if (help::tip_bubble::Inside(*placement, pointer))
		{
			bubble.Hover();
			bubble.SetUnderHand(help::tip_bubble::HandOnBubble(*placement, pointer, WhatIsUnderHand()));
		}
	};
	if (alpha <= 0.0f)
	{
		afterDrawing();
		return;
	}

	// The box and its tail
	for (const auto& quad : tip::BoxShape(*placement, alpha))
	{
		std::array<glm::vec2, 4> corners {};
		std::array<glm::vec2, 4> uvs {};
		std::array<glm::vec4, 4> colours {};
		for (size_t i = 0; i < quad.size(); ++i)
		{
			corners.at(i) = quad.at(i).position;
			uvs.at(i) = quad.at(i).uv;
			colours.at(i) = glm::vec4(tip::k_BoxColour, quad.at(i).alpha);
		}
		_canvas.DrawShape(corners, uvs, colours, _gatheringText.get());
	}
	// The arrows, blinking, when there is more to read
	if (_atmos != nullptr && tip::ArrowsLit(machine_clock::Ticks()))
	{
		for (const auto& [shown, arrow] :
		     {std::pair {scroll.moreAbove, tip::UpArrow(*placement)}, std::pair {scroll.moreBelow, tip::DownArrow(*placement)}})
		{
			if (shown)
			{
				_canvas.DrawQuad(arrow.min, arrow.max, arrow.uvMin, arrow.uvMax, tip::k_ArrowColour, _atmos.get());
			}
		}
	}
	// The words: the tip at the bottom, a blank line and the title on top
	const auto& tipText = _texts.GetHelpText(bubble.GetText()).text;
	const auto& title = _texts.GetHelpText(tip::k_TitleText).text;
	const std::array<std::u16string_view, 3> items {tipText, u" ", title};
	const tip::WidthFn measure = [this](std::u16string_view text, float size) { return BubbleTextWidth(text, size); };
	const tip::WrapFn wrap = [this](std::u16string_view text, float width, float size) {
		const auto lines = _font.Wrap(text, size, width);
		return std::vector<std::u16string>(lines.begin(), lines.end());
	};
	// A tip's control codes show the player's bindings
	std::span<const input::KeyBinding> bindings;
	if (Locator::gameActionSystem::has_value())
	{
		bindings = Locator::gameActionSystem::value().GetKeyBindings();
	}
	const auto keyName = ControlKeyName();
	const tip::ControlShownFn controlShown = [bindings, &keyName](int action) {
		return help::control_icons::PictureFor(action, bindings, keyName).has_value();
	};
	const auto words = tip::LayOutWords(*placement, items, scroll.Pixels(), alpha, wrap, measure, controlShown);
	scroll.contentHeight = words.contentHeight;
	scroll.lineHeight = placement->sizes.lineHeight;
	for (const auto& gesture : words.gestures)
	{
		const auto sheet = static_cast<size_t>(gesture.sheet);
		if (sheet < _gestureSheets.size() && _gestureSheets.at(sheet) != nullptr)
		{
			_canvas.DrawQuad(gesture.min, gesture.max, gesture.uvMin, gesture.uvMax, gesture.colour,
			                 _gestureSheets.at(sheet).get());
		}
	}
	for (const auto& control : words.controls)
	{
		DrawBubbleControl(control, bindings, placement->pixelScale);
	}
	for (const auto& line : words.shadows)
	{
		DrawBubbleLine(line, glm::vec3(0.0f));
	}
	for (const auto& line : words.lines)
	{
		DrawBubbleLine(line, glm::vec3(1.0f));
	}
	afterDrawing();
}

void GameInterface::DrawBubbleControl(const help::tip_bubble::ControlIcon& icon, std::span<const input::KeyBinding> bindings,
                                      float pixelScale)
{
	namespace icons = help::control_icons;
	const auto picture = icons::PictureFor(icon.action, bindings, ControlKeyName());
	if (!picture.has_value())
	{
		return;
	}
	// The gap is in the default screen's pixels
	const float gap = help::tip_bubble::k_ControlGap * pixelScale;
	const auto colour = glm::vec4(1.0f, 1.0f, 1.0f, icon.alpha);
	if (const auto* text = std::get_if<std::u16string>(&*picture); text != nullptr)
	{
		// A key cap as wide as its words need
		const float textWidth = _font.GetWidth(*text, icons::KeyTextSize(icon.size));
		const auto box = help::tip_bubble::PlaceControl(icon, icons::KeyCapWidth(icon.size, textWidth), gap);
		const icons::Placed placed {.icon = {box.min, box.max}, .glowColour = {}, .alpha = icon.alpha, .size = icon.size};
		const auto cap = icons::LayoutKeyCap(placed);
		if (_atmos != nullptr)
		{
			for (size_t i = 0; i < cap.slices.size(); ++i)
			{
				_canvas.DrawQuad(cap.slices.at(i).min, cap.slices.at(i).max, cap.uvs.at(i).min, cap.uvs.at(i).max, colour,
				                 _atmos.get());
			}
		}
		_painter.DrawString(cap.textAt, *text, cap.textSize, cap.textColour);
		return;
	}
	if (_mice == nullptr)
	{
		return;
	}
	const auto box = help::tip_bubble::PlaceControl(icon, static_cast<float>(icon.size), gap);
	const auto cell =
	    icons::CellFor(std::get<icons::MouseClick>(*picture), help::click_cue::k_WheelMouseRow, machine_clock::Ticks());
	_canvas.DrawQuad(box.min, box.max, cell.uvMin, cell.uvMax, colour, _mice.get());
}

void GameInterface::DrawVillagerLabels(glm::u16vec2 resolution)
{
	namespace labels = villager_labels;
	if (_villagerLabels.empty() || _gatheringText == nullptr)
	{
		return;
	}
	labels::Board board;
	const labels::WidthFn width = [this](std::u16string_view text, float size) { return BubbleTextWidth(text, size); };
	const auto pixelScale = labels::PixelScale(glm::vec2(resolution));
	for (const auto& request : _villagerLabels)
	{
		board.Add(request, static_cast<float>(resolution.x), width, pixelScale);
	}
	const auto draw = [this](const labels::Quad& quad, glm::vec4 colour) {
		_canvas.DrawShape({quad[0].position, quad[1].position, quad[2].position, quad[3].position},
		                  {quad[0].uv, quad[1].uv, quad[2].uv, quad[3].uv}, {colour, colour, colour, colour},
		                  _gatheringText.get());
	};
	for (const auto* label : board.DrawOrder())
	{
		// The see-through black box, then the black pointer down to the head, then the words
		const auto boxColour = glm::vec4(0.0f, 0.0f, 0.0f, static_cast<float>(label->boxAlpha) / 255.0f);
		for (const auto& quad : labels::BoxShape(*label))
		{
			draw(quad, boxColour);
		}
		for (const auto& quad : labels::PointerShape(*label))
		{
			draw(quad, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
		}
		const float alpha = static_cast<float>(label->wordsAlpha) / 255.0f;
		const auto colour = glm::vec3(label->colour) / 255.0f;
		for (size_t i = 0; i < label->lines.size(); ++i)
		{
			const help::tip_bubble::Line line {
			    .text = label->lines[i],
			    .at = {label->left + label->inset, label->top + label->inset + (static_cast<float>(i) * label->lineHeight)},
			    .size = label->lineHeight,
			    .topAlpha = alpha,
			    .bottomAlpha = alpha,
			    .clipTop = 0.0f,
			    .clipBottom = static_cast<float>(resolution.y),
			};
			DrawBubbleLine(line, colour);
		}
	}
}

float GameInterface::BubbleTextWidth(std::u16string_view text, float size) const
{
	return _font.GetWidth(text, size);
}

void GameInterface::DrawBubbleLine(const help::tip_bubble::Line& line, glm::vec3 colour)
{
	const float scale = line.size / static_cast<float>(_font.GetHeight());
	const auto atlasSize = glm::vec2(_font.GetAtlasSize());
	const float top = line.at.y;
	const float bottom = line.at.y + line.size;
	// The line fades from its top row to its bottom
	const auto alphaAt = [&](float y) {
		return line.topAlpha + ((line.bottomAlpha - line.topAlpha) * (y - top) / (bottom - top));
	};
	float pen = line.at.x;
	for (const auto c : line.text)
	{
		const auto* glyph = _font.Find(c);
		if (glyph == nullptr)
		{
			continue;
		}
		// The atlas holds the glyph at half size; it spans the whole line, cut to the box
		const float width = static_cast<float>(glyph->atlasMax.x - glyph->atlasMin.x) * 2.0f * scale;
		const float left = pen + (glyph->left * scale);
		pen += (glyph->left + glyph->ink + glyph->right) * scale;
		const float clippedTop = std::max(top, line.clipTop);
		const float clippedBottom = std::min(bottom, line.clipBottom);
		if (clippedBottom <= clippedTop)
		{
			continue;
		}
		auto uvMin = glm::vec2(glyph->atlasMin) / atlasSize;
		auto uvMax = glm::vec2(glyph->atlasMax) / atlasSize;
		const float vPerPixel = (uvMax.y - uvMin.y) / (bottom - top);
		uvMin.y += (clippedTop - top) * vPerPixel;
		uvMax.y -= (bottom - clippedBottom) * vPerPixel;
		const auto topColour = glm::vec4(colour, alphaAt(clippedTop));
		const auto bottomColour = glm::vec4(colour, alphaAt(clippedBottom));
		_canvas.DrawShape({glm::vec2(left, clippedTop), glm::vec2(left + width, clippedTop),
		                   glm::vec2(left + width, clippedBottom), glm::vec2(left, clippedBottom)},
		                  {glm::vec2(uvMin.x, uvMin.y), glm::vec2(uvMax.x, uvMin.y), glm::vec2(uvMax.x, uvMax.y),
		                   glm::vec2(uvMin.x, uvMax.y)},
		                  {topColour, topColour, bottomColour, bottomColour}, _fontTexture.get());
	}
}

void GameInterface::DrawClickCue(const help::TextRegion& box, float share)
{
	// The languages that need bigger text aren't chosen in openblack yet
	constexpr bool k_BiggerText = false;
	const auto& label = _texts.GetHelpText(help::click_cue::k_LabelText).text;
	const auto labelSize = static_cast<float>(help::click_cue::LabelSize(box.top, box.bottom, k_BiggerText));
	// Inset from the strip's right end, which is the screen's on 4:3
	const auto cue = help::click_cue::Layout(box.right, box.top, box.bottom, share, _font.GetWidth(label, labelSize),
	                                         k_BiggerText, machine_clock::Ticks());
	if (!cue.has_value())
	{
		return;
	}
	DrawGlow(cue->mouseGlow.min, cue->mouseGlow.max, cue->mouseGlowColour);
	if (!label.empty())
	{
		DrawGlow(cue->labelGlow.min, cue->labelGlow.max, cue->labelGlowColour);
	}
	if (_mice != nullptr)
	{
		_canvas.DrawQuad(cue->mouse.min, cue->mouse.max, cue->uvMin, cue->uvMax, cue->mouseColour, _mice.get());
	}
	_painter.DrawString(cue->labelAt - 1.0f, label, cue->labelSize, cue->shadowColour);
	_painter.DrawString(cue->labelAt + 1.0f, label, cue->labelSize, cue->shadowColour);
	_painter.DrawString(cue->labelAt, label, cue->labelSize, cue->labelColour);
}

void GameInterface::DrawControlIcons(const help::TextFrame& frame, const help::DialogueText& dialogue)
{
	namespace icons = help::control_icons;
	if (frame.controlIcons.empty() || !Locator::gameActionSystem::has_value())
	{
		return;
	}
	const auto bindings = Locator::gameActionSystem::value().GetKeyBindings();
	const auto keyName = ControlKeyName();
	const auto& box = frame.box;
	const int size = icons::IconSize(box.top, box.bottom);
	// Inset from the strip's right end, which is the screen's on 4:3
	const auto drawKeyCap = [this, &box, size](int top, std::u16string_view text, float share) {
		const float textWidth = _font.GetWidth(text, icons::KeyTextSize(size));
		const auto placed = icons::Place(box.right, top, size, icons::KeyCapWidth(size, textWidth), share);
		if (!placed.has_value())
		{
			return;
		}
		DrawGlow(placed->icon.min, placed->icon.max, placed->glowColour);
		const auto cap = icons::LayoutKeyCap(*placed);
		if (_atmos != nullptr)
		{
			const auto colour = glm::vec4(1.0f, 1.0f, 1.0f, placed->alpha);
			for (size_t i = 0; i < cap.slices.size(); ++i)
			{
				_canvas.DrawQuad(cap.slices.at(i).min, cap.slices.at(i).max, cap.uvs.at(i).min, cap.uvs.at(i).max, colour,
				                 _atmos.get());
			}
		}
		_painter.DrawString(cap.textAt, text, cap.textSize, cap.textColour);
	};

	// The camera keys' cap in the box's top third
	if (const auto text = icons::KeyIconText(frame.controlIcons, bindings, keyName); text.has_value())
	{
		drawKeyCap(icons::KeyIconTop(box.top, box.bottom), *text, dialogue.GetKeyIconShare());
	}
	// Any other action's mouse or key in the bottom third, unless the "Continue" cue is there
	const auto share = dialogue.GetActionIconShare();
	if (!share.has_value())
	{
		return;
	}
	const auto picture = icons::ActionIcon(frame.controlIcons, bindings, keyName);
	if (!picture.has_value())
	{
		return;
	}
	const int top = icons::ActionIconTop(box.top, box.bottom);
	if (const auto* text = std::get_if<std::u16string>(&*picture); text != nullptr)
	{
		drawKeyCap(top, *text, *share);
		return;
	}
	const auto placed = icons::Place(box.right, top, size, static_cast<float>(size), *share);
	if (!placed.has_value())
	{
		return;
	}
	DrawGlow(placed->icon.min, placed->icon.max, placed->glowColour);
	if (_mice != nullptr)
	{
		const auto cell =
		    icons::CellFor(std::get<icons::MouseClick>(*picture), help::click_cue::k_WheelMouseRow, machine_clock::Ticks());
		_canvas.DrawQuad(placed->icon.min, placed->icon.max, cell.uvMin, cell.uvMax, glm::vec4(1.0f, 1.0f, 1.0f, placed->alpha),
		                 _mice.get());
	}
}

void GameInterface::DrawDialogueRun(const help::TextRun& run)
{
	const auto [font, texture] = DialogueFont(run.font);
	const auto colour = glm::vec4(run.r, run.g, run.b, run.a) / 255.0f;
	const auto scale = run.size / static_cast<float>(font->GetHeight());
	const auto atlasSize = glm::vec2(font->GetAtlasSize());
	const float clipTop = run.clipTop;
	const float clipBottom = run.clipBottom + 1.0f;
	auto pen = glm::floor(glm::vec2(run.x, run.y));
	for (const auto c : run.text)
	{
		const auto* glyph = font->Find(c);
		if (glyph == nullptr)
		{
			continue;
		}
		// The atlas holds the glyph at half size
		const auto cellSize = glm::vec2(glyph->atlasMax - glyph->atlasMin) * 2.0f * scale;
		auto min = glm::vec2(pen.x + (glyph->left * scale), pen.y);
		auto max = min + cellSize;
		auto uvMin = glm::vec2(glyph->atlasMin) / atlasSize;
		auto uvMax = glm::vec2(glyph->atlasMax) / atlasSize;
		pen.x += (glyph->left + glyph->ink + glyph->right) * scale;
		// Cut to the box, the glyph's picture with it
		const float top = std::max(min.y, clipTop);
		const float bottom = std::min(max.y, clipBottom);
		if (bottom <= top)
		{
			continue;
		}
		const float vPerPixel = (uvMax.y - uvMin.y) / (max.y - min.y);
		uvMin.y += (top - min.y) * vPerPixel;
		uvMax.y -= (max.y - bottom) * vPerPixel;
		min.y = top;
		max.y = bottom;
		_canvas.DrawQuad(min, max, uvMin, uvMax, colour, texture);
	}
}

void GameInterface::DrawCreaturePanel(glm::u16vec2 resolution)
{
	if (!_creaturePanel.has_value())
	{
		return;
	}
	using creature_panel::Row;
	const auto& values = *_creaturePanel;
	const bool withReward = values.reward.has_value();
	const int rowPitch = creature_panel::RowPitch(resolution);
	const auto textSize = static_cast<float>(rowPitch / 2);

	// The labels' column is as wide as the widest label
	std::array<std::u16string_view, creature_panel::k_RowCount> labels {};
	std::array<float, creature_panel::k_RowCount> widths {};
	for (size_t i = 0; i < labels.size(); ++i)
	{
		labels.at(i) = _texts.Get(creature_panel::k_LabelNames.at(i));
		widths.at(i) = _font.GetWidth(labels.at(i), textSize);
	}
	const auto layout =
	    creature_panel::Compute(resolution, withReward, creature_panel::LabelColumnWidth(widths, static_cast<float>(rowPitch)));
	const creature_panel::RewardTexts rewardTexts {
	    .bad = _texts.Get(creature_panel::k_BadBoyName),
	    .good = _texts.Get(creature_panel::k_GoodBoyName),
	    .none = _texts.Get(creature_panel::k_NoRewardName),
	};

	// See-through black at the left, fading out to the right
	const glm::vec4 shade {0.0f, 0.0f, 0.0f, 95.0f / 255.0f};
	const glm::vec4 clear {0.0f, 0.0f, 0.0f, 0.0f};
	const auto& box = layout.box;
	_canvas.DrawShape({box.min, glm::vec2(box.max.x, box.min.y), box.max, glm::vec2(box.min.x, box.max.y)},
	                  {shade, clear, clear, shade});

	const glm::vec4 white {1.0f, 1.0f, 1.0f, 1.0f};
	const glm::vec4 black {0.0f, 0.0f, 0.0f, 1.0f};
	// Each text in white over its black shadow, two pixels of the default screen down and right
	const float shadow = 2.0f * ReferenceScale(resolution.x, resolution.y);
	const auto drawText = [&](glm::vec2 at, std::u16string_view text) {
		_painter.DrawString(at + shadow, text, layout.textSize, black);
		_painter.DrawString(at, text, layout.textSize, white);
	};
	for (const auto& row : layout.Rows())
	{
		const auto label = labels.at(static_cast<size_t>(row.row));
		drawText({row.labelRight.x - std::floor(_font.GetWidth(label, layout.textSize)), row.labelRight.y}, label);
		drawText(row.valueLeft, creature_panel::FormatValue(row.row, values, rewardTexts));

		const float fill = creature_panel::Fill(row.row, values);
		const auto bright = creature_panel::BarColour(row.row, fill);
		DrawStatBar(row.bar, fill, glm::u8vec3(glm::vec3(bright) * 255.0f), row.row == Row::Reward);
	}
}

void GameInterface::DrawStatBar(const creature_panel::Rect& bar, float fill, glm::u8vec3 colour, bool fromMiddle)
{
	// The frame: a three-quarters black box with a white line round it two pixels in
	const glm::vec4 frame {0.0f, 0.0f, 0.0f, 191.0f / 255.0f};
	const glm::vec4 line {254.0f / 255.0f, 254.0f / 255.0f, 254.0f / 255.0f, 1.0f};
	const glm::vec4 black {0.0f, 0.0f, 0.0f, 1.0f};
	const glm::vec4 clear {0.0f, 0.0f, 0.0f, 0.0f};
	const auto quad = [this](glm::vec2 min, glm::vec2 max, const std::array<glm::vec4, 4>& colours) {
		_canvas.DrawShape({min, glm::vec2(max.x, min.y), max, glm::vec2(min.x, max.y)}, colours);
	};
	quad(bar.min, bar.max, {frame, frame, frame, frame});
	const auto inner = glm::ivec2(bar.min) + 2;
	const auto innerMax = glm::ivec2(bar.max) - 2;
	_canvas.DrawLine(inner, {innerMax.x, inner.y}, line);
	_canvas.DrawLine({inner.x, innerMax.y}, innerMax, line);
	_canvas.DrawLine(inner, {inner.x, innerMax.y}, line);
	_canvas.DrawLine({innerMax.x, inner.y}, innerMax, line);

	// The fill, from half the colour where it starts to the whole colour where it ends
	const auto filled = creature_panel::FillOf(bar, fill, fromMiddle);
	if (!filled.Empty())
	{
		const auto bright = glm::vec4(glm::vec3(colour) / 255.0f, 1.0f);
		const auto dim = glm::vec4(glm::vec3(colour / glm::u8vec3(2)) / 255.0f, 1.0f);
		quad({filled.from, filled.top}, {filled.to, filled.bottom}, {dim, bright, bright, dim});
	}
	if (filled.middle.has_value())
	{
		_canvas.DrawLine(glm::ivec2(static_cast<int>(*filled.middle), static_cast<int>(filled.top)),
		                 glm::ivec2(static_cast<int>(*filled.middle), static_cast<int>(filled.bottom) - 1), black);
	}

	// Shadows inside the frame along its top and its left, along the whole bar
	const float inset = creature_panel::k_BarInset;
	const glm::vec2 top {bar.min.x + inset, bar.min.y + inset};
	const glm::vec2 bottomRight {bar.max.x - inset, bar.max.y - inset};
	quad(top, {bottomRight.x, top.y + inset}, {black, black, clear, clear});
	quad(top, {top.x + inset, bottomRight.y}, {black, clear, clear, black});
}

void GameInterface::DrawFightPanel(glm::u16vec2 resolution)
{
	if (!_fightPanel.has_value())
	{
		return;
	}
	const auto layout = creature_fight_hud::Compute(resolution);
	const glm::vec4 shade {0.0f, 0.0f, 0.0f, 95.0f / 255.0f};
	const glm::vec4 clear {0.0f, 0.0f, 0.0f, 0.0f};
	const glm::vec4 white {1.0f, 1.0f, 1.0f, 1.0f};
	const glm::vec4 black {0.0f, 0.0f, 0.0f, 1.0f};
	// The shade is darkest in the screen's corner and clear at the box's other three corners, split from its top right
	// to its bottom left, so the lower right half of the box is clear
	const auto& box = layout.box;
	_canvas.DrawShape({glm::vec2(box.max.x, box.min.y), box.max, glm::vec2(box.min.x, box.max.y), box.min},
	                  {clear, clear, clear, shade});
	const auto widthOf = [this, &layout](std::u16string_view text) { return _font.GetWidth(text, layout.textSize); };
	for (size_t i = 0; i < layout.rows.size(); ++i)
	{
		const auto& row = layout.rows.at(i);
		const auto& side = _fightPanel->sides.at(i);
		const auto toPanel = [](const creature_fight_hud::Rect& rect) {
			return creature_panel::Rect {.min = rect.min, .max = rect.max};
		};
		// The stamina bar first, then the health bar over its top
		DrawStatBar(toPanel(row.stamina), side.stamina, creature_fight_hud::StaminaColour(side.stamina), false);
		DrawStatBar(toPanel(row.health), side.health, creature_fight_hud::HealthColour(side.health), false);
		// The name to the bars' right in white over its black shadow, two pixels of the default screen down and right
		const auto name = creature_fight_hud::FitName(side.name, layout.nameWidth, widthOf);
		_painter.DrawString(row.name + (2.0f * ReferenceScale(resolution.x, resolution.y)), name, layout.textSize, black);
		_painter.DrawString(row.name, name, layout.textSize, white);
	}
}

void GameInterface::DrawCaption(glm::u16vec2 resolution)
{
	if (!_caption.has_value())
	{
		return;
	}
	namespace rules = temple::challenge_room;
	// The sizes are pixels of the default screen; each line is centred on the screen, the second a line's height below
	// the first
	const float scale = ReferenceScale(resolution.x, resolution.y);
	const float size = rules::k_TextSize * scale;
	const float shadow = rules::k_ShadowOffset * scale;
	const float alpha = static_cast<float>(_caption->alpha) / 255.0f;
	float top = rules::k_TextTop * scale;
	for (const auto& line : _caption->lines)
	{
		const auto width = static_cast<int32_t>(_font.GetWidth(line, size));
		const auto x = static_cast<float>((static_cast<int32_t>(resolution.x) - width) / 2);
		_painter.DrawString({x, top}, line, size, glm::vec4(0.0f, 0.0f, 0.0f, alpha), scale > 1.0f);
		_painter.DrawString({x + shadow, top + shadow}, line, size, glm::vec4(1.0f, 1.0f, 0.0f, alpha), scale > 1.0f);
		top += size;
	}
}

void GameInterface::DrawFloatingNumbers(glm::u16vec2 resolution)
{
	// The numbers' sizes are pixels of the default screen
	const float scale = ReferenceScale(resolution.x, resolution.y);
	const float textSize = ecs::floating_number::k_TextSize * scale;
	const float shadow = ecs::floating_number::k_ShadowOffset * scale;
	for (const auto& number : _floatingNumbers)
	{
		const float width = _font.GetWidth(number.text, textSize);
		const auto at = ecs::floating_number::TextPlace(number.screen, width);
		const float alpha = static_cast<float>(number.alpha) / 255.0f;
		const glm::vec4 colour {static_cast<float>((number.colour >> 16u) & 0xFFu) / 255.0f,
		                        static_cast<float>((number.colour >> 8u) & 0xFFu) / 255.0f,
		                        static_cast<float>(number.colour & 0xFFu) / 255.0f, alpha};
		_painter.DrawString({at.x - shadow, at.y + shadow}, number.text, textSize, glm::vec4(0.0f, 0.0f, 0.0f, alpha));
		_painter.DrawString(at, number.text, textSize, colour);
	}
}

void GameInterface::DrawToolTip(glm::u16vec2 resolution)
{
	const auto shown = _toolTips.GetShown();
	if (!shown.has_value() || !_handOnScreen.has_value())
	{
		return;
	}
	// The colours' alpha goes in 256ths, and below 4 nothing is drawn
	const float alpha = std::floor(shown->alpha * 255.0f) / 255.0f;
	if (alpha * 255.0f < 4.0f)
	{
		return;
	}
	// A numbered tooltip may read in other words by its number (the belief tooltip while the player trails)
	const auto worded =
	    shown->number.has_value() ? std::optional(world_tooltip::Worded(shown->index, *shown->number)) : std::nullopt;
	const auto words = _texts.Get(ToolTips::TextName(shown->words.value_or(worded.has_value() ? worded->words : shown->index)));
	const std::u16string text =
	    worded.has_value() ? world_tooltip::Compose(shown->index, words, worded->number, shown->second) : std::u16string(words);
	const glm::vec4 yellow {1.0f, 1.0f, 0.0f, alpha};
	const glm::vec4 white {1.0f, 1.0f, 1.0f, alpha};
	const glm::vec4 shadow {0.0f, 0.0f, 0.0f, alpha};

	// Sized by the screen's height, and kept on it by the hand
	const int width = resolution.x;
	const int height = resolution.y;
	const int size = height / 25;
	const auto kept = world_tooltip::KeptOnScreen(glm::ivec2(*_handOnScreen), glm::ivec2(width, height), size);
	const int handX = kept.x;
	const int handY = kept.y;

	// The words, then the mouse with its arrows either side
	const int iconWidth = shown->action != ToolTipAction::None && _mice != nullptr ? size : 0;
	const bool left = (shown->arrows & ToolTipArrows::k_Left) != 0;
	const bool right = (shown->arrows & ToolTipArrows::k_Right) != 0;
	const int iconsWidth = iconWidth + (left ? size / 2 : 0) + (right ? size / 2 : 0);
	const int textSize = size * 2 / 3;
	const float textWidth = _font.GetWidth(text, static_cast<float>(textSize));
	// The gap after the words and the shadows' offset are pixels of the default screen
	const float pixel = ReferenceScale(width, height);
	const float gap = 2.0f * pixel;
	const float total = textWidth + static_cast<float>(iconsWidth) + gap;

	// Right of the hand, or left of it in the right third of the screen, until it reaches the left third
	if (handX > width * 2 / 3)
	{
		_toolTipOnLeft = true;
	}
	else if (handX < width / 3)
	{
		_toolTipOnLeft = false;
	}
	const float x = _toolTipOnLeft ? static_cast<float>(handX + (size / 2)) - total : static_cast<float>(handX - (size / 2));
	const float textX = x;
	const float iconX = std::trunc(x + textWidth + gap) + (left ? static_cast<float>(size / 2) : 0.0f);
	const auto y = static_cast<float>(handY);
	const auto sizeF = static_cast<float>(size);
	const auto textOffset = static_cast<float>((size - textSize) / 2);

	// The glows behind: white behind the mouse, and faint yellow behind the words and arrows
	const float glowAlpha = std::trunc(128.0f * shown->alpha) / 255.0f;
	const float faintAlpha = std::trunc(128.0f * shown->alpha / 3.0f) / 255.0f;
	if (iconWidth != 0)
	{
		DrawGlow({iconX, y}, {iconX + static_cast<float>(iconWidth), y + sizeF}, glm::vec4(1.0f, 1.0f, 1.0f, glowAlpha));
	}
	if (!text.empty())
	{
		DrawGlow({textX, y + textOffset}, {textX + textWidth, y + sizeF - textOffset}, glm::vec4(1.0f, 1.0f, 0.0f, faintAlpha));
	}
	const float half = sizeF * 0.5f;
	const float quarter = sizeF * 0.25f;
	const auto arrowRect = [&](uint32_t bit) -> std::pair<glm::vec2, glm::vec2> {
		const float iconRight = iconX + static_cast<float>(iconWidth);
		switch (bit)
		{
		case ToolTipArrows::k_Left:
			return {{iconX - half, y + quarter}, {iconX, y + (3.0f * quarter)}};
		case ToolTipArrows::k_Right:
			return {{iconRight, y + quarter}, {iconRight + half, y + (3.0f * quarter)}};
		case ToolTipArrows::k_Up:
			return {{iconX + quarter, y - half}, {iconX + (3.0f * quarter), y}};
		default:
			return {{iconX + quarter, y + sizeF}, {iconX + (3.0f * quarter), y + (1.5f * sizeF)}};
		}
	};
	for (const auto& arrow : k_ToolTipArrows)
	{
		if ((shown->arrows & arrow.bit) != 0)
		{
			const auto [min, max] = arrowRect(arrow.bit);
			DrawGlow(min, max, glm::vec4(1.0f, 1.0f, 0.0f, faintAlpha));
		}
	}

	// The mouse, its button the action's, and the arrows over the glows
	if (iconWidth != 0)
	{
		constexpr float k_Grid = 0.25f;
		const bool selecting = shown->action == ToolTipAction::Select;
		const auto uvMin = glm::vec2(1.0f, static_cast<float>(k_ToolTipMouseRow)) * k_Grid;
		const auto uvMax = uvMin + k_Grid;
		// The left button is the right one mirrored
		_canvas.DrawQuad({iconX, y}, {iconX + sizeF, y + sizeF}, selecting ? glm::vec2(uvMax.x, uvMin.y) : uvMin,
		                 selecting ? glm::vec2(uvMin.x, uvMax.y) : uvMax, white, _mice.get());
	}
	if (_atmos != nullptr)
	{
		const float grow = static_cast<float>(size / 9);
		for (const auto& arrow : k_ToolTipArrows)
		{
			if ((shown->arrows & arrow.bit) != 0)
			{
				const auto [min, max] = arrowRect(arrow.bit);
				_canvas.DrawQuad(min - grow, max + grow, arrow.uvMin, arrow.uvMax, white, _atmos.get());
			}
		}
	}

	// The words, in yellow over two black shadows
	const auto textSizeF = static_cast<float>(textSize);
	_painter.DrawString({textX - pixel, y - pixel + textOffset}, text, textSizeF, shadow);
	_painter.DrawString({textX + pixel, y + pixel + textOffset}, text, textSizeF, shadow);
	_painter.DrawString({textX, y + textOffset}, text, textSizeF, yellow);
}
