/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameInterface.h"

#include <array>
#include <optional>

#include <SDL.h>
#include <bgfx/bgfx.h>
#include <spdlog/spdlog.h>

#include "Audio/Audio.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack::gui;
using openblack::Locator;
using openblack::filesystem::Path;

namespace
{
/// The scripts the game's text is in, in the order the game reads them
constexpr std::array k_TextScripts = {"InfoScript2.txt", "InfoScriptPatch2.txt", "InfoScriptMultiplayer2.txt"};
/// The dialogs' font, the first of the game's fonts
constexpr std::string_view k_Font = "j0";
constexpr uint16_t k_AtlasSize = 256;

std::vector<uint8_t> ReadIfExists(const std::filesystem::path& path)
{
	auto& fileSystem = Locator::filesystem::value();
	if (!fileSystem.Exists(path))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Missing {} for the interface", path.generic_string());
		return {};
	}
	// the byte cache, by the resolved path as the help text reads the same scripts
	return openblack::resources::LoadBlob(Locator::resources::value().GetBlobs(), fileSystem.FindPath(path));
}
} // namespace

namespace
{
/// A texture of the game made of a colour file and an alpha file, 256 pixels square, from the texture cache. Null when
/// they are missing
std::shared_ptr<openblack::graphics::Texture2D> LoadTexture(const std::string& name)
{
	using openblack::resources::Texture2DLoader;
	auto& fileSystem = Locator::filesystem::value();
	auto& textures = Locator::resources::value().GetTextures();
	const auto id = entt::hashed_string(("interface/" + name).c_str()).value();
	if (!textures.Contains(id))
	{
		try
		{
			textures.Load(id, Texture2DLoader::FromColourAlphaTag {}, name,
			              Texture2DLoader::ColourAlphaDesc {
			                  .colour = fileSystem.GetPath<Path::Textures>() / (name + ".raw"),
			                  .alpha = fileSystem.GetPath<Path::Textures>() / (name + "a.raw"),
			                  .size = k_AtlasSize,
			              });
		}
		catch (const std::exception& e)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Unable to read the texture {}: {}", name, e.what());
			return nullptr;
		}
	}
	return textures.Handle(id).handle();
}
} // namespace

std::unique_ptr<GameInterface> GameInterface::Create(std::u16string_view playerName, MenuSettings settings)
{
	auto& fileSystem = Locator::filesystem::value();

	TextDatabase texts;
	for (const auto* script : k_TextScripts)
	{
		const auto data = ReadIfExists(fileSystem.GetPath<Path::Scripts>() / script);
		texts.AddScript(data);
	}

	// the j0 font of the font cache, the one the renderer's help text draws with. (openblack guard, not the original's:
	// its fonts always exist) without it, as in the tests' data, there are no menus rather than a crash
	const auto font = GameFont::From(
	    openblack::resources::LoadGameFont(Locator::resources::value().GetFonts(), fileSystem.GetPath<Path::Data>() / k_Font));
	if (!font)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Unable to read the font {}", k_Font);
		return nullptr;
	}

	// The front end atlas, and the pictures of the player's symbols and of the mice
	auto atlas = LoadTexture("Front_end_buttons");
	if (!atlas)
	{
		return nullptr;
	}
	auto symbols = LoadTexture("ChooseSymbol");
	auto mice = LoadTexture("mousehelp");

	std::unique_ptr<graphics::Texture2D> fontTexture; // the font owns its atlas
	return std::unique_ptr<GameInterface>(new GameInterface(std::move(texts), std::move(*font), std::move(atlas),
	                                                        std::move(fontTexture), std::move(symbols), std::move(mice),
	                                                        playerName, std::move(settings)));
}

GameInterface::GameInterface(TextDatabase texts, GameFont font, std::shared_ptr<graphics::Texture2D> atlas,
                             std::unique_ptr<graphics::Texture2D> fontTexture, std::shared_ptr<graphics::Texture2D> symbols,
                             std::shared_ptr<graphics::Texture2D> mice, std::u16string_view playerName, MenuSettings settings)
    : _texts(std::move(texts))
    , _font(std::move(font))
    , _atlas(std::move(atlas))
    , _fontTexture(std::move(fontTexture))
    , _symbols(std::move(symbols))
    , _mice(std::move(mice))
    , _painter(_canvas, _font, *_atlas, _font.GetTexture())
    , _menu(std::make_unique<GameMenu>(_texts, _font, playerName, std::move(settings)))
{
	_painter.SetPictures(_symbols.get(), _mice.get());
}

GameInterface::~GameInterface() = default;

namespace
{
/// A dialog control's click: sound 159 G_MenuButton, mode 3, 2D, InGame bank (the force feedback that follows is not
/// ported)
void PlayButtonSound()
{
	openblack::audio::PlaySoundEffect(openblack::audio::Owner::None(), 159, 3, 0, false, false,
	                                  openblack::audio::SfxBank::InGame);
}
} // namespace

void GameInterface::ShowSkipBox()
{
	_skipBox.emplace(static_cast<float>(DialogPainter::MidTextSize()));
	_skipAnswer.reset();
}

bool GameInterface::ProcessEvent(const SDL_Event& event, glm::u16vec2 resolution)
{
	_painter.Begin(resolution);
	// the SkipBox takes every event while shown; it cannot be escaped out of: Escape does nothing
	if (IsSkipBoxShown())
	{
		if (event.type == SDL_MOUSEMOTION)
		{
			_skipBox->MouseMove(_painter.ToDialog({event.motion.x, event.motion.y}));
		}
		else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT)
		{
			PlayButtonSound(); // (inferred) the box's click on a control, as the menu's
			if (const auto answer = _skipBox->Click(_painter.ToDialog({event.button.x, event.button.y})); answer)
			{
				_skipAnswer = *answer;
			}
		}
		return event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP ||
		       event.type == SDL_MOUSEWHEEL || event.type == SDL_KEYDOWN || event.type == SDL_TEXTINPUT;
	}
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
	{
		if (_menu->IsOpen())
		{
			_action = _menu->Escape();
			if (_action == GameMenu::Action::Continue)
			{
				_menu->Close();
				SDL_StopTextInput();
			}
		}
		else
		{
			_menu->Open();
			// For the names typed into the menu
			SDL_StartTextInput();
		}
		return true;
	}
	if (!_menu->IsOpen())
	{
		return false;
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
			// Every control clicks as it acts
			if (_menu->TakeClicked())
			{
				PlayButtonSound();
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

GameMenu::Action GameInterface::TakeAction()
{
	return std::exchange(_action, GameMenu::Action::None);
}

void GameInterface::Update(float deltaSeconds)
{
	if (_skipBox)
	{
		_skipBox->Update(deltaSeconds);
	}
	_menu->Update(deltaSeconds);
}

void GameInterface::Draw(glm::u16vec2 resolution, glm::ivec2 mouse, uint32_t milliseconds)
{
	_canvas.Begin(resolution);
	_painter.Begin(resolution);
	// The words the temple's future room shows, under the dialogs
	if (_message.has_value())
	{
		_painter.DrawTextWrapped(DialogRect {{0, 0}, DialogPainter::k_Size}, true, _message->text, 60,
		                         glm::vec4(1.0f, 1.0f, 1.0f, _message->alpha));
	}
	if (IsSkipBoxShown())
	{
		_skipBox->Draw(_painter);
		_painter.DrawPointer(_canvas, mouse, milliseconds);
	}
	if (_menu->IsVisible())
	{
		_menu->Draw(_painter);
		if (_menu->IsOpen())
		{
			_painter.DrawPointer(_canvas, mouse, milliseconds);
		}
	}
	_canvas.End();
}
