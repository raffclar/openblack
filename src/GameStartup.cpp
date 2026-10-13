/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The screens the game shows as it starts and while a land loads: the logos, the pre-intro and the tips screen

#include <algorithm>
#include <chrono>
#include <string>
#include <thread>

#include <L3DFile.h>
#include <SDL.h>
#include <bgfx/bgfx.h>
#include <spdlog/spdlog.h>

#include "Audio/AudioManagerInterface.h"
#include "Audio/MusicPlayer.h"
#include "Common/MachineClock.h"
#include "Common/UserData.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureSkin.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/InspectorLoading.h"
#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/VideoSystemInterface.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "Graphics/RenderPass.h"
#include "Graphics/RendererInterface.h"
#include "Gui/GameInterface.h"
#include "Gui/LoadingScreen.h"
#include "Gui/LoadingScreenRules.h"
#include "Gui/MenuBackdrop.h"
#include "Gui/ScreenFit.h"
#include "Gui/StartupRules.h"
#include "Gui/TextDatabase.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "Video/StillPicture.h"
#include "Video/VideoRules.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;

namespace
{
constexpr std::string_view k_LogoFilm = "Data/logo.bik";
constexpr std::string_view k_PreIntroFilm = "Data/pre_intro.bik";
constexpr std::string_view k_TrailerMusic = "audio/music/intro/trailer.sad";
/// The pre-intro waits this long at most for a frame to be due before it shows it anyway
constexpr auto k_LongestFrameWait = std::chrono::seconds(25);
/// The colour the screen is cleared to between the game's own frames, and the sky's while the game runs
constexpr uint32_t k_StartupClearColour = 0x000000ff;
constexpr uint32_t k_GameClearColour = 0x274659ff;
/// The frames that the renderer can have in work on the GPU at one time (the bgfx default)
constexpr uint32_t k_RendererFramesInFlight = 3;

/// What the player did since the last frame
struct StartupInput
{
	/// A key went down
	bool key {false};
	/// The left or right mouse button is held
	bool mouse {false};
	/// The window was closed
	bool quit {false};
};

/// Takes the window's events, which the game's own handlers don't see during these screens
StartupInput PollStartupInput()
{
	StartupInput input;
	SDL_Event event;
	while (SDL_PollEvent(&event) != 0)
	{
		if (event.type == SDL_QUIT)
		{
			input.quit = true;
		}
		else if (event.type == SDL_KEYDOWN)
		{
			input.key = true;
		}
	}
	const auto buttons = SDL_GetMouseState(nullptr, nullptr);
	input.mouse = (buttons & (SDL_BUTTON(SDL_BUTTON_LEFT) | SDL_BUTTON(SDL_BUTTON_RIGHT))) != 0;
	return input;
}

/// The events, closing the window ending the game once it starts
StartupInput TakeStartupInput(Game& game)
{
	const auto input = PollStartupInput();
	if (input.quit)
	{
		game.RequestQuit();
	}
	return input;
}

/// Milliseconds since the program started, as the game counts frames with
uint32_t Milliseconds()
{
	return SDL_GetTicks();
}
} // namespace

bool Game::HasPlayerProfiles() noexcept
{
	return Locator::playerProfileSystem::has_value() && Locator::playerProfileSystem::value().GetProfileCount() > 0;
}

player_profiles::Store Game::ProfileStore() const
{
	auto store = player_profiles::StoreFor(
	    {.profileFolder = _profileFolder, .userFolder = user_data::Folder(), .automated = _automatedRun});
	if (store.developer)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "A developer's run: playing as the profile \"{}\", its creature kept in {}",
		                   player_profiles::k_DeveloperName,
		                   store.creatureMinds.has_value() ? store.creatureMinds->generic_string() : "no folder");
	}
	else if (store.folder.has_value())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The players' profiles are kept in {}", store.folder->generic_string());
	}
	return store;
}

bool Game::AskForNewPlayer()
{
	auto& profiles = Locator::playerProfileSystem::value();
	if (profiles.GetProfileCount() > 0)
	{
		return true;
	}
	if (!_interface || !Locator::windowing::has_value() || Locator::config::value().graphicsBackend == GraphicsBackend::Noop)
	{
		// With no box to ask in, the player gets the profile the game makes for one who gives no name: the account's
		profiles.CreateProfile(u"", 0);
		return !_quitRequested;
	}
	// Tools wait while the player is asked
	const ecs::systems::InspectorLoading asking("new player");
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "No player has a profile: asking the new player for their name");
	SDL_StartTextInput();
	_interface->ShowNewPlayerBox(false, [&profiles](std::u16string_view name) { return profiles.CheckNewName(name); });
	gui::menu_backdrop::Fade fade;
	auto last = Milliseconds();
	while (!gui::menu_backdrop::FadeOver(fade) && !_quitRequested)
	{
		const auto now = Milliseconds();
		const auto frame = static_cast<int>(now - last);
		last = now;
		auto& box = _interface->GetNewPlayerBox();
		const auto resolution = static_cast<glm::u16vec2>(Locator::windowing::value().GetSize());
		SDL_Event event;
		while (SDL_PollEvent(&event) != 0)
		{
			if (event.type == SDL_QUIT)
			{
				RequestQuit();
			}
			else
			{
				_interface->ProcessNewPlayerBoxEvent(event, resolution);
			}
		}
		// The profile is made as the box is answered
		if (const auto answer = box.TakeAnswer(); answer.has_value())
		{
			profiles.CreateProfile(answer->name, answer->symbol);
		}
		gui::menu_backdrop::StepFade(fade, !box.IsActive(), frame);
		glm::ivec2 mouse {0, 0};
		SDL_GetMouseState(&mouse.x, &mouse.y);
		PresentStartupFrame([this, mouse, frame, &fade](glm::u16vec2 size) {
			_interface->DrawNewPlayerScreen(size, mouse, machine_clock::Ticks(), static_cast<float>(frame), fade.alpha);
		});
	}
	SDL_StopTextInput();
	if (const auto profile = profiles.GetCurrentProfile(); profile.has_value())
	{
		_interface->SetPlayerProfile(gui::ToUtf16(profile->name), gui::ToUtf16(profile->creatureName), profile->symbol,
		                             profile->settings);
		RefreshPlayersPage();
	}
	return !_quitRequested;
}

void Game::PresentStartupFrame(const std::function<void(glm::u16vec2 resolution)>& draw)
{
	auto& renderer = Locator::rendererInterface::value();
	// The window may have been resized, maximised or restored since the last frame, whose events these screens take
	FollowWindowSize();
	const auto resolution = static_cast<glm::u16vec2>(Locator::windowing::value().GetSize());
	// Each frame starts from black
	renderer.ConfigureView(graphics::RenderPass::Main, resolution, k_StartupClearColour);
	bgfx::touch(static_cast<bgfx::ViewId>(graphics::SkyPassOf(graphics::RenderPass::Main)));
	draw(resolution);
	renderer.Frame();
}

void Game::PlayStartupScreens()
{
	if (!Locator::windowing::has_value() || Locator::config::value().graphicsBackend == GraphicsBackend::Noop)
	{
		return;
	}
	_loadingScreen = gui::LoadingScreen::Create();
	if (!_loadingScreen)
	{
		return;
	}
	if (!_skipLogos)
	{
		PlayLogoScreens();
	}
	if (!_quitRequested && startup::PlaysPreIntro(_preIntro, HasPlayerProfiles()))
	{
		PlayPreIntro();
	}
	// A new player's first film can't be skipped
	if (!HasPlayerProfiles())
	{
		Locator::videoSystem::value().SetNoSkip(true);
	}
	if (!_quitRequested)
	{
		StartTipOfTheDay();
	}
	// Setting the game up starts the loading time again
	_loadingScreen->GetClock().Reset();
	// These screens took the window's first events, which otherwise tell the game it is running
	Locator::config::value().running = true;
}

void Game::PlayLogoScreens()
{
	if (!Locator::filesystem::value().Exists(std::filesystem::path(k_LogoFilm)))
	{
		return;
	}
	TakeStartupInput(*this);
	for (int32_t still = 0; still < startup::k_LogoStills && !_quitRequested; ++still)
	{
		const auto picture = video::ReadStill(std::filesystem::path(k_LogoFilm), static_cast<uint32_t>(still));
		startup::LogoStill logo;
		auto last = Milliseconds();
		while (!logo.Ended() && !_quitRequested)
		{
			const auto now = Milliseconds();
			const auto input = TakeStartupInput(*this);
			const auto alpha = logo.Frame(static_cast<int32_t>(now - last), input.key || input.mouse);
			last = now;
			PresentStartupFrame([this, &picture, alpha](glm::u16vec2 resolution) {
				if (picture)
				{
					// Over the screen's 4:3 box from a pixel before its top left corner, black round it
					const auto box = gui::FitInside(gui::k_FourByThree, resolution.x, resolution.y);
					_loadingScreen->DrawPicture(resolution, picture->GetPlanes(),
					                            {.x = box.x - 1, .y = box.y - 1, .width = box.width, .height = box.height},
					                            alpha);
				}
			});
		}
	}
	TakeStartupInput(*this);
}

void Game::PlayPreIntro()
{
	auto film = video::VideoFile::Open(std::filesystem::path(k_PreIntroFilm));
	if (!film)
	{
		return;
	}
	// The trailer's music plays with it, at full volume, once
	auto& audio = Locator::audio::value();
	audio.MusicPlay(std::string(k_TrailerMusic), audio::MusicPlayOptions {.volume = 127, .startChunk = 1});

	using Clock = std::chrono::steady_clock;
	const auto start = Clock::now();
	const auto frameCount = static_cast<int32_t>(film->FrameCount());
	int32_t shown = 0;
	bool keyPressed = false;
	do
	{
		// Each frame waits until it is due by the film's own pace, and is never skipped
		const auto waitStart = Clock::now();
		while (!_quitRequested)
		{
			const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start);
			const auto due = video::FramesDue(elapsed, film->FpsNumerator(), film->FpsDenominator());
			if (static_cast<int32_t>(due) > shown || Clock::now() - waitStart > k_LongestFrameWait)
			{
				break;
			}
			keyPressed = TakeStartupInput(*this).key || keyPressed;
			std::this_thread::sleep_for(std::chrono::microseconds(500));
		}
		film->Decode(static_cast<uint32_t>(shown));
		++shown;
		PresentStartupFrame([this, &film](glm::u16vec2 resolution) {
			// Over the screen's 4:3 box, black round it
			_loadingScreen->DrawPicture(resolution, film->GetPlanes(),
			                            gui::FitInside(gui::k_FourByThree, resolution.x, resolution.y), 255);
		});
		keyPressed = TakeStartupInput(*this).key || keyPressed;
	} while (startup::PreIntroGoesOn(shown, frameCount, keyPressed) && !_quitRequested);

	// The music is cut off with it
	audio.MusicStop(false);
	TakeStartupInput(*this);
}

void Game::StartTipOfTheDay()
{
	if (!_loadingScreen)
	{
		return;
	}
	_loadingScreen->PrepareTip(Milliseconds(), HasPlayerProfiles());
	if (_tipFadedIn)
	{
		return;
	}
	// The first time, the screen fades in over about a second; this holds everything up
	_tipFadedIn = true;
	float fade = 0.0f;
	auto last = Milliseconds();
	while (fade < 1.0f && !_quitRequested)
	{
		PresentStartupFrame([this, fade](glm::u16vec2 resolution) { _loadingScreen->DrawTips(resolution, fade, 0.0f); });
		const auto now = Milliseconds();
		fade = startup::NextTipFade(fade, static_cast<int32_t>(now - last));
		last = now;
		TakeStartupInput(*this);
	}
}

void Game::BeginLoadingScreen(loading::LoadingClock::Mode look)
{
	if (!_loadingScreen)
	{
		return;
	}
	if (look == loading::LoadingClock::Mode::Tips)
	{
		// A load without a tip picks a new one
		if (!_loadingScreen->HasTipPicture())
		{
			StartTipOfTheDay();
		}
	}
	else
	{
		// A script's map counts its own time
		_loadingScreen->GetClock().Reset();
	}
	_loadingLook = look;
}

bool Game::RenderLoadingFrame(bool drawNow, const std::function<void()>& drawUnder)
{
	if (!_loadingScreen || !_loadingLook || !_loadingScreen->TipShowing())
	{
		return false;
	}
	auto& clock = _loadingScreen->GetClock();
	const auto look = *_loadingLook;
	if (!clock.Tick(Milliseconds(), look) && !drawNow)
	{
		return false;
	}
	// Nothing is drawn while the window is minimised, but the time still counts
	const bool shown = !Locator::windowing::value().IsMinimised();
	if (shown)
	{
		const float progress = clock.Progress();
		const auto time = clock.Time();
		PresentStartupFrame([this, look, progress, time, &drawUnder](glm::u16vec2 resolution) {
			if (drawUnder)
			{
				drawUnder();
			}
			if (look == loading::LoadingClock::Mode::Tips)
			{
				_loadingScreen->DrawTips(resolution, 1.0f, progress);
			}
			else if (const auto alpha = loading::PleaseWaitAlpha(time); alpha > 0)
			{
				// Black until the load has taken five seconds, then the banner fades in
				_loadingScreen->DrawPleaseWait(resolution, alpha);
			}
		});
	}
	// The window stays responsive; its events are left for the game
	SDL_PumpEvents();
	return shown;
}

void Game::FinishLoadingBehindScreen()
{
	auto& resources = Locator::resources::value();
	// If the game uses a resource before the loading threads load it, the resource loads in the middle of a frame and
	// makes that frame slow. Thus all the resources load before the first frame of the game, as in the original game.
	const auto start = std::chrono::steady_clock::now();
	const auto count = resources.LoadingCount();
	while (resources.LoadingCount() > 0 && !_quitRequested)
	{
		resources.UpdateLoading(k_LoadingScreenLoadBudget, graphics::UploadPacer::Allowance {});
		RenderLoadingFrame();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	const auto loaded = std::chrono::steady_clock::now();
	PrepareFirstFrames();
	WarmUpFirstFrame();
	// The renderer makes the last resources in one more loading frame, so that the first frame of the game does not
	// make them. The renderer releases the memory that it used to send the resources to the GPU only when it uses the
	// frame's command buffers again. Thus it draws more loading frames, one for each frame that the GPU can have in
	// work. Else the first frames of the game release that memory, which can take hundreds of milliseconds.
	for (uint32_t frame = 0; frame <= k_RendererFramesInFlight; ++frame)
	{
		RenderLoadingFrame(true);
	}
	const auto prepared = std::chrono::steady_clock::now();
	SPDLOG_LOGGER_INFO(spdlog::get("game"),
	                   "Load: {} prefetched resources finished behind the loading screen in {:.1f} ms, then the first "
	                   "frames prepared in {:.1f} ms; {} pipelines made",
	                   count, std::chrono::duration<double, std::milli>(loaded - start).count(),
	                   std::chrono::duration<double, std::milli>(prepared - loaded).count(),
	                   Locator::rendererInterface::has_value() ? Locator::rendererInterface::value().GetPipelinesMade() : 0);
}

void Game::PrepareFirstFrames()
{
	const auto& config = Locator::config::value();
	// The first scene draw bakes the shadows of the whole island into the land, which takes tens of milliseconds
	if (config.drawEntities && Locator::rendererInterface::has_value())
	{
		Locator::rendererInterface::value().BakeLandShadows(config.drawVegetation);
	}
	// The first frame of a creature makes a texture for each of its painted skins. Make enough for the species with the
	// most skins.
	if (Locator::rendererInterface::has_value())
	{
		auto& resources = Locator::resources::value();
		const auto& files = resources.GetL3DFiles();
		size_t skins = 0;
		resources.GetCreatureRigs().Each([&files, &skins](entt::id_type, const auto& rig) {
			if (const auto* file = files.Find(creature_skin::MeshFileId(rig->baseMeshName)); file != nullptr)
			{
				skins = std::max(skins, file->GetSkins().size());
			}
		});
		Locator::rendererInterface::value().PrepareCreatureSkins(skins);
	}
	// A creature's first move finds where creatures can walk on the whole land
	if (Locator::creatureLocomotionSystem::has_value())
	{
		[[maybe_unused]] const auto& land = Locator::creatureLocomotionSystem::value().GetWalkableLand();
	}
	// The first start of each effect reads its file, its models and its light maps
	if (Locator::particleSystem::has_value())
	{
		Locator::particleSystem::value().LoadAllTypes();
	}
	// The first frame loads the models of the globes and the gesture templates
	if (Locator::miracleFxSystem::has_value())
	{
		Locator::miracleFxSystem::value().LoadMeshes();
	}
	if (Locator::gestureSystem::has_value())
	{
		Locator::gestureSystem::value().LoadTemplates();
	}
}

void Game::EndLoadingScreen()
{
	if (!_loadingScreen || !_loadingLook)
	{
		return;
	}
	FinishLoadingBehindScreen();
	const auto look = *_loadingLook;
	_loadingLook.reset();
	// After the tips screen its tip goes before the game's first frame; the last loading frame stays up until then
	if (look == loading::LoadingClock::Mode::Tips)
	{
		_loadingScreen->ClearTip();
	}
	Locator::rendererInterface::value().ConfigureView(
	    graphics::RenderPass::Main, static_cast<glm::u16vec2>(Locator::windowing::value().GetSize()), k_GameClearColour);
}
