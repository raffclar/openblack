/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "LoadingScreenSystem.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#include <intrin.h>
#elif defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>
#endif

#include <SDL.h>
#include <bgfx/bgfx.h>
#include <spdlog/spdlog.h>

#include "Audio/AudioManagerInterface.h"
#include "Audio/MusicPlayer.h"
#include "Camera/Camera.h"
#include "Common/GameRandom.h"
#include "ECS/Systems/InspectorLoading.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/VideoSystemInterface.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/LogoEffectView.h"
#include "Graphics/RendererInterface.h"
#include "Gui/GameInterface.h"
#include "Gui/LoadingScreen.h"
#include "Gui/LogoEffect.h"
#include "Gui/ScreenFit.h"
#include "Gui/StartupRules.h"
#include "Input/WindowPointer.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"
#include "Sound/SoundId.h"
#include "Video/StillPicture.h"
#include "Video/VideoRules.h"
#include "Visual/RenderPass.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;

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

constexpr std::string_view k_LionheadLogo = "Data/lhlogo.raw";
constexpr std::string_view k_LogoSky = "Data/D_SKY.l3d";
constexpr entt::hashed_string k_LogoSkyId = entt::hashed_string("logo/d_sky");
/// The player's own logo, in the temporary folder of the system
constexpr std::string_view k_UserLogo = "userlogo.raw";

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

/// Takes the window's events, which the game's own handlers don't see during these screens. `keys` gets the keys and
/// the clicks that the logo stills and the pre-intro read
StartupInput PollStartupInput(startup::StartupKeys* keys)
{
	StartupInput input;
	SDL_Event event;
	while (SDL_PollEvent(&event) != 0)
	{
		// On the web the mouse events get the place of the pointer of the game
		input::PointWindowEvent(event);
		if (event.type == SDL_QUIT)
		{
			input.quit = true;
		}
		else if (event.type == SDL_KEYDOWN)
		{
			input.key = true;
			if (keys != nullptr)
			{
				keys->KeyDown(event.key.keysym.scancode == SDL_SCANCODE_ESCAPE);
			}
		}
		else if (event.type == SDL_KEYUP && keys != nullptr)
		{
			keys->KeyUp();
		}
		else if (event.type == SDL_MOUSEBUTTONDOWN && keys != nullptr &&
		         (event.button.button == SDL_BUTTON_LEFT || event.button.button == SDL_BUTTON_RIGHT))
		{
			keys->Click();
		}
	}
	const auto buttons = SDL_GetMouseState(nullptr, nullptr);
	input.mouse = (buttons & (SDL_BUTTON(SDL_BUTTON_LEFT) | SDL_BUTTON(SDL_BUTTON_RIGHT))) != 0;
	return input;
}

/// The events, closing the window ending the game once it starts
StartupInput TakeStartupInput(const LoadingScreenHooks& hooks, startup::StartupKeys* keys = nullptr)
{
	const auto input = PollStartupInput(keys);
	if (input.quit)
	{
		hooks.requestQuit();
	}
	return input;
}

/// Milliseconds since the program started, as the game counts frames with
uint32_t Milliseconds()
{
	return SDL_GetTicks();
}

/// Whether the player has a profile. A first start has none.
bool HasPlayerProfiles() noexcept
{
	return Locator::playerProfileSystem::has_value() && Locator::playerProfileSystem::value().GetProfileCount() > 0;
}

/// What the player did during a frame of the logo effect
struct LogoInput
{
	StartupInput startup;
	glm::ivec2 mouseMove {0, 0};
	int32_t wheel {0};
};

/// Takes the window's events for a frame of the logo effect, with the movement of the mouse and the turns of the wheel
LogoInput TakeLogoInput(const LoadingScreenHooks& hooks)
{
	LogoInput input;
	SDL_Event event;
	while (SDL_PollEvent(&event) != 0)
	{
		// On the web the mouse events get the place of the pointer of the game
		input::PointWindowEvent(event);
		if (event.type == SDL_QUIT)
		{
			input.startup.quit = true;
		}
		else if (event.type == SDL_KEYDOWN)
		{
			input.startup.key = true;
		}
		else if (event.type == SDL_MOUSEMOTION)
		{
			input.mouseMove += glm::ivec2(event.motion.xrel, event.motion.yrel);
		}
		else if (event.type == SDL_MOUSEWHEEL)
		{
			// The game counts 120 for each notch, positive away from the player
			constexpr int32_t k_WheelNotch = 120;
			const auto notches = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event.wheel.y : event.wheel.y;
			input.wheel += notches * k_WheelNotch;
		}
	}
	const auto buttons = SDL_GetMouseState(nullptr, nullptr);
	input.startup.mouse = (buttons & (SDL_BUTTON(SDL_BUTTON_LEFT) | SDL_BUTTON(SDL_BUTTON_RIGHT))) != 0;
	if (input.startup.quit)
	{
		hooks.requestQuit();
	}
	return input;
}

/// The version word and the feature word of the processor, none where the processor cannot tell them
std::optional<std::pair<uint32_t, uint32_t>> ProcessorWords()
{
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
	std::array<int, 4> words {};
	__cpuid(words.data(), 1);
	return std::pair {static_cast<uint32_t>(words[0]), static_cast<uint32_t>(words[3])};
#elif defined(__x86_64__) || defined(__i386__)
	unsigned int a = 0;
	unsigned int b = 0;
	unsigned int c = 0;
	unsigned int d = 0;
	if (__get_cpuid(1, &a, &b, &c, &d) == 0)
	{
		return std::nullopt;
	}
	return std::pair {static_cast<uint32_t>(a), static_cast<uint32_t>(d)};
#else
	return std::nullopt;
#endif
}

/// The picture of the logo effect: on one start in four, the player's own logo if there is one, else the Lionhead logo
std::optional<logo_effect::Picture> LogoPicture()
{
	// The time since the computer started, as the game reads it
	const auto uptime =
	    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch());
	if (logo_effect::TriesUserLogo(static_cast<uint64_t>(uptime.count())))
	{
		std::error_code error;
		const auto folder = std::filesystem::temp_directory_path(error);
		if (!error)
		{
			std::ifstream file(folder / k_UserLogo, std::ios::binary);
			const std::vector<uint8_t> bytes {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
			if (auto picture = logo_effect::ReadUserLogo(bytes); picture.has_value())
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "The logo effect shows the player's own logo");
				return picture;
			}
		}
	}
	auto& fileSystem = Locator::filesystem::value();
	const auto path = std::filesystem::path(k_LionheadLogo);
	if (!fileSystem.Exists(path))
	{
		return std::nullopt;
	}
	return logo_effect::LionheadLogo(fileSystem.ReadAll(path));
}
} // namespace

LoadingScreenSystem::LoadingScreenSystem(LoadingScreenHooks hooks)
    : _hooks(std::move(hooks))
{
}

LoadingScreenSystem::~LoadingScreenSystem() = default;

void LoadingScreenSystem::StartStartupTimer(LoadTimer::Clock::time_point launch)
{
	_startupTimer.emplace("startup", launch);
	_startupTimer->Step("launch");
}

void LoadingScreenSystem::StartupStep(std::string_view step)
{
	if (_startupTimer.has_value())
	{
		_startupTimer->Step(step);
	}
}

void LoadingScreenSystem::FinishStartupTimer(std::string_view lastStep)
{
	if (_startupTimer.has_value())
	{
		_startupTimer->Step(lastStep);
		_startupTimer.reset();
	}
}

void LoadingScreenSystem::StartPrefetchTimer()
{
	_prefetchTimer.emplace("of every resource");
}

bool LoadingScreenSystem::TipShowing() const
{
	return _loadingScreen != nullptr && _loadingScreen->TipShowing();
}

void LoadingScreenSystem::ReleaseScreen()
{
	_loadingScreen.reset();
}

void LoadingScreenSystem::PresentStartupFrame(const Draw& draw)
{
	auto& renderer = Locator::rendererInterface::value();
	// The window may have been resized, maximised or restored since the last frame, whose events these screens take
	_hooks.followWindowSize();
	const auto resolution = static_cast<glm::u16vec2>(Locator::windowing::value().GetSize());
	// Each frame starts from black
	renderer.ConfigureView(graphics::RenderPass::Main, resolution, k_StartupClearColour);
	bgfx::touch(static_cast<bgfx::ViewId>(graphics::SkyPassOf(graphics::RenderPass::Main)));
	draw(resolution);
	renderer.Frame();
}

void LoadingScreenSystem::PlayStartupScreens(const StartupScreenOptions& options)
{
	if (!Locator::windowing::has_value() || Locator::config::value().graphicsBackend == GraphicsBackend::Noop)
	{
		// A window with no renderer shows no screens, so the game is running at once. With no window at all, the game
		// does not run frames
		if (Locator::windowing::has_value())
		{
			Locator::config::value().running = true;
		}
		return;
	}
	_loadingScreen = gui::LoadingScreen::Create();
	if (!_loadingScreen)
	{
		return;
	}
	_loadingScreen->SetBuild(options.build);
	if (!options.skipLogos)
	{
		PlayLogoScreens();
	}
	if (!QuitRequested() && startup::PlaysPreIntro({.requested = options.preIntro,
	                                                .hasProfiles = HasPlayerProfiles(),
	                                                .toolRun = options.skipLogos || options.scenario}))
	{
		PlayPreIntro();
	}
	// A new player's first film can't be skipped
	if (!HasPlayerProfiles())
	{
		Locator::videoSystem::value().SetNoSkip(true);
	}
	if (!QuitRequested())
	{
		StartTipOfTheDay();
	}
	// Setting the game up starts the loading time again
	_loadingScreen->GetClock().Reset();
	// These screens took the window's first events, which otherwise tell the game it is running
	Locator::config::value().running = true;
}

void LoadingScreenSystem::PlayLogoScreens()
{
	if (!Locator::filesystem::value().Exists(std::filesystem::path(k_LogoFilm)))
	{
		return;
	}
	// The game forgets the earlier keys and clicks before the first still and after the last
	TakeStartupInput(_hooks);
	startup::StartupKeys keys;
	for (int32_t still = 0; still < startup::k_LogoStills && !QuitRequested(); ++still)
	{
		const auto picture = video::ReadStill(std::filesystem::path(k_LogoFilm), static_cast<uint32_t>(still));
		startup::LogoStill logo;
		auto last = Milliseconds();
		while (!logo.Ended() && !QuitRequested())
		{
			const auto now = Milliseconds();
			TakeStartupInput(_hooks, &keys);
			const auto alpha = logo.Frame(static_cast<int32_t>(now - last), keys);
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
	TakeStartupInput(_hooks);
}

void LoadingScreenSystem::PlayLogoEffect()
{
	if (!Locator::gameInterface::has_value() || !Locator::windowing::has_value() ||
	    Locator::config::value().graphicsBackend == GraphicsBackend::Noop)
	{
		return;
	}
	const auto picture = LogoPicture();
	auto& files = Locator::resources::value().GetL3DFiles();
	if (!files.Contains(k_LogoSkyId.value()))
	{
		try
		{
			files.Load(k_LogoSkyId.value(), resources::L3DFileLoader::FromDiskTag {}, std::filesystem::path(k_LogoSky));
		}
		catch (const std::exception& error)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The logo effect has no sky: {}", error.what());
		}
	}
	if (!picture.has_value() || !files.Contains(k_LogoSkyId.value()))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The logo effect cannot start: {} is missing",
		                    picture.has_value() ? k_LogoSky : k_LionheadLogo);
		return;
	}
	// Tools wait while the effect plays
	const InspectorLoading playing("logo effect");
	_hooks.followWindowSize();
	const auto resolution = static_cast<glm::u16vec2>(Locator::windowing::value().GetSize());
	const auto box = gui::FitInside(gui::k_FourByThree, resolution.x, resolution.y);
	const logo_effect::Screen screen {.width = box.width, .height = box.height};
	// The effect is laid out on a 4:3 screen in the middle of the window, as the game laid it out on its screen. Its
	// picture covers the whole window, so the sky and the dots continue past the 4:3 screen.
	const glm::ivec2 origin {box.x, box.y};
	const logo_effect::Box visible {.min = -origin, .max = glm::ivec2(resolution) - origin};
	const glm::vec2 clipFit {static_cast<float>(box.width) / static_cast<float>(resolution.x),
	                         static_cast<float>(box.height) / static_cast<float>(resolution.y)};
	const glm::vec2 clipShift {
	    (((static_cast<float>(box.x) + (static_cast<float>(box.width) * 0.5f)) * 2.0f) / static_cast<float>(resolution.x)) -
	        1.0f,
	    1.0f - (((static_cast<float>(box.y) + (static_cast<float>(box.height) * 0.5f)) * 2.0f) /
	            static_cast<float>(resolution.y))};
	auto view = graphics::LogoEffectView::Create(resolution, *files.Handle(k_LogoSkyId.value()));
	if (!view)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The logo effect cannot start: its sky has no sphere");
		return;
	}
	const auto words = ProcessorWords();
	const bool sponsor = words.has_value() && logo_effect::ShowsSponsorLogo(words->first, words->second);
	Locator::gameInterface::value().PrepareLogoEffect();
	const auto& camera = Locator::camera::value();
	const logo_effect::Random random = [](float a, float b) { return Locator::gameRandom::value().CrtRandom(a, b); };
	// The dots are placed with the game camera's lens as it is now
	logo_effect::Effect effect(*picture, screen, logo_effect::LensOf(camera.GetLens(), camera.GetNearClip(), screen), random,
	                           visible);
	auto& audio = Locator::audio::value();
	const auto sound = static_cast<entt::id_type>(audio::SoundId::Logo);
	audio.PlaySoundEffect(sound, std::nullopt);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The logo effect starts ({}x{}, the sponsor's logo {})", screen.width,
	                   screen.height, sponsor ? "shows" : "does not show");

	std::vector<graphics::LogoEffectView::Dot> dots;
	dots.reserve(logo_effect::k_DotCount);
	TakeLogoInput(_hooks);
	auto last = Milliseconds();
	LogoInput input;
	while (logo_effect::GoesOn(input.startup.mouse, effect.Ended(), input.startup.key) && !QuitRequested())
	{
		// One frame for each refresh of a 60 Hz screen at most
		auto now = Milliseconds();
		while (static_cast<float>(now - last) < logo_effect::k_FrameMilliseconds)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			now = Milliseconds();
		}
		const auto milliseconds = std::max(1, static_cast<int32_t>(now - last));
		last = now;
		input = TakeLogoInput(_hooks);
		const auto pointer = input::WindowPointer();
		const auto frame = effect.Step({.milliseconds = milliseconds,
		                                .pointer = pointer - glm::ivec2(box.x, box.y),
		                                .mouseMove = input.mouseMove,
		                                .wheel = input.wheel,
		                                .viewAcross = camera.GetLens()},
		                               random);
		dots.clear();
		for (const auto& dot : effect.GetDots())
		{
			if (const auto corners = logo_effect::DotCorners(dot, frame.view, frame.lens.nearClip); corners.has_value())
			{
				dots.push_back({.corners = *corners, .argb = dot.colour, .additive = dot.additive});
			}
		}
		const auto* dotTexture = Locator::gameInterface::value().GetLogoDotTexture();
		PresentStartupFrame([&](glm::u16vec2 size) {
			view->Draw({.position = frame.view.position,
			            .right = frame.view.right,
			            .up = frame.view.up,
			            .forward = frame.view.forward,
			            .offset = frame.view.offset,
			            .clipScaleX = frame.lens.clipScaleX * clipFit.x,
			            .clipScaleY = frame.lens.clipScaleY * clipFit.y,
			            .clipShift = clipShift,
			            .nearClip = frame.lens.nearClip,
			            .skyColour = frame.skyColour,
			            .dots = dots,
			            .dotUvMin = logo_effect::k_DotUvMin,
			            .dotUvMax = logo_effect::k_DotUvMax,
			            .dotTexture = dotTexture});
			Locator::gameInterface::value().DrawLogoEffectOverlay({.resolution = resolution,
			                                                       .origin = origin,
			                                                       .screen = screen,
			                                                       .text = picture->text,
			                                                       .tradeMark = picture->lionhead,
			                                                       .sponsor = sponsor,
			                                                       .textAlpha = frame.textAlpha,
			                                                       .logoAlpha = frame.logoAlpha,
			                                                       .fade = frame.fade});
			Locator::gameInterface::value().DrawLogoEffectPicture(size, view->GetPicture());
		});
	}
	SPDLOG_LOGGER_INFO(
	    spdlog::get("game"), "The logo effect ends at time {:.3f} ({})", effect.GetTime(),
	    effect.Ended() ? "its end" : (input.startup.key ? "a key" : (input.startup.mouse ? "a mouse button" : "the window")));
	view.reset();
	Locator::gameInterface::value().ReleaseLogoEffect();
	// The screen stays black while a mouse button is held
	while (input.startup.mouse && !QuitRequested())
	{
		PresentStartupFrame([](glm::u16vec2 /*size*/) {});
		input = TakeLogoInput(_hooks);
	}
	audio.StopSoundEffect(sound);
}

void LoadingScreenSystem::PlayPreIntro()
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
	// Only Escape stops the pre-intro, while it is the last key down
	startup::StartupKeys keys;
	do
	{
		// Each frame waits until it is due by the film's own pace, and is never skipped
		const auto waitStart = Clock::now();
		while (!QuitRequested())
		{
			const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start);
			const auto due = video::FramesDue(elapsed, film->FpsNumerator(), film->FpsDenominator());
			if (static_cast<int32_t>(due) > shown || Clock::now() - waitStart > k_LongestFrameWait)
			{
				break;
			}
			TakeStartupInput(_hooks, &keys);
			std::this_thread::sleep_for(std::chrono::microseconds(500));
		}
		film->Decode(static_cast<uint32_t>(shown));
		++shown;
		PresentStartupFrame([this, &film](glm::u16vec2 resolution) {
			// Over the screen's 4:3 box, black round it
			_loadingScreen->DrawPicture(resolution, film->GetPlanes(),
			                            gui::FitInside(gui::k_FourByThree, resolution.x, resolution.y), 255);
		});
		TakeStartupInput(_hooks, &keys);
	} while (startup::PreIntroGoesOn(shown, frameCount, keys.EscapeDown()) && !QuitRequested());

	// The music is cut off with it
	audio.MusicStop(false);
	TakeStartupInput(_hooks);
}

void LoadingScreenSystem::StartTipOfTheDay()
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
	// The first time, the screen fades in over about a second. This stops all other work until the fade is complete.
	_tipFadedIn = true;
	startup::TipFadeIn fadeIn(Milliseconds());
	while (!fadeIn.Over() && !QuitRequested())
	{
		const float fade = fadeIn.Fade();
		PresentStartupFrame([this, fade](glm::u16vec2 resolution) { _loadingScreen->DrawTips(resolution, fade, 0.0f); });
		fadeIn.Step(Milliseconds());
		TakeStartupInput(_hooks);
	}
}

void LoadingScreenSystem::BeginLoading(loading::LoadingClock::Mode look)
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

bool LoadingScreenSystem::RenderLoadingFrame(bool drawNow, const std::function<void()>& drawUnder)
{
	if (!_loadingScreen || !_loadingLook || !_loadingScreen->TipShowing())
	{
		return false;
	}
	const auto look = *_loadingLook;
	// A load that shows nothing keeps the last frame of the game on the screen
	if (look == loading::LoadingClock::Mode::None)
	{
		return false;
	}
	auto& clock = _loadingScreen->GetClock();
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

bool LoadingScreenSystem::IsLoading() const
{
	return _loadingScreen != nullptr && _loadingLook.has_value();
}

void LoadingScreenSystem::EndLoading()
{
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

void LoadingScreenSystem::ShowSavingBox(const SavingBoxFrame& box)
{
	Locator::gameInterface::value().DrawSavingBox(static_cast<glm::u16vec2>(Locator::windowing::value().GetSize()), box.loading,
	                                              box.darken);
	// A picture asked for this frame shows the box, as the screen does
	if (box.screenshot.has_value())
	{
		Locator::rendererInterface::value().RequestScreenshot(*box.screenshot);
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The {} box shows at frame {}", box.loading ? "loading" : "saving", box.frame);
	Locator::rendererInterface::value().Frame();
}
