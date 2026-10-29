/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Locator.h"

#define LOCATOR_IMPLEMENTATIONS

#include <ctime>

#include <chrono>
#include <exception>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <spdlog/spdlog.h>

#include "3D/Implementations/LandIsland.h"
#include "3D/Implementations/Ocean.h"
#include "3D/Implementations/TempleInterior.h"
#include "3D/Implementations/UnloadedIsland.h"
#include "3D/LandData.h"
#include "Audio/AudioManager.h"
#include "Audio/AudioManagerNoOp.h"
#include "Audio/ClockedAudioPlayer.h"
#include "Audio/GameSoundRequests.h"
#include "Audio/QueuedAudioManager.h"
#include "CHLApi.h"
#include "Common/EventManager.h"
#include "Common/GameRandomProduction.h"
#include "Common/MachineClock.h"
#include "Common/MainThreadQueue.h"
#include "Common/PlayerProfiles.h"
#include "Common/RandomNumberManagerProduction.h"
#include "Debug/DebugGuiInterface.h"
#include "ECS/Archetypes/PlayerArchetype.h"
#include "ECS/DrawnRegistry.h"
#include "ECS/MapProduction.h"
#include "ECS/PhysicsGameHooks.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/AbodeKnockSystem.h"
#include "ECS/Systems/Implementations/AdvisorSystem.h"
#include "ECS/Systems/Implementations/AlignmentSystem.h"
#include "ECS/Systems/Implementations/AnimalSystem.h"
#include "ECS/Systems/Implementations/AnimatedStaticSystem.h"
#include "ECS/Systems/Implementations/AppleBarrelSystem.h"
#include "ECS/Systems/Implementations/BoatLaunchSystem.h"
#include "ECS/Systems/Implementations/BuildingDamageSystem.h"
#include "ECS/Systems/Implementations/CameraBookmarkSystem.h"
#include "ECS/Systems/Implementations/CameraHelpSystem.h"
#include "ECS/Systems/Implementations/CameraPathSystem.h"
#include "ECS/Systems/Implementations/CameraZoneSystem.h"
#include "ECS/Systems/Implementations/ChimneySmokeSystem.h"
#include "ECS/Systems/Implementations/CinematicDirectorSystem.h"
#include "ECS/Systems/Implementations/CloudSystem.h"
#include "ECS/Systems/Implementations/ComputerSettingsSystem.h"
#include "ECS/Systems/Implementations/CreatureAnimationLogSystem.h"
#include "ECS/Systems/Implementations/CreatureAnimationSystem.h"
#include "ECS/Systems/Implementations/CreatureAudioSystem.h"
#include "ECS/Systems/Implementations/CreatureCarryOverSystem.h"
#include "ECS/Systems/Implementations/CreatureCaveSystem.h"
#include "ECS/Systems/Implementations/CreatureFightSystem.h"
#include "ECS/Systems/Implementations/CreatureFizzSystem.h"
#include "ECS/Systems/Implementations/CreatureHairSystem.h"
#include "ECS/Systems/Implementations/CreatureHandSystem.h"
#include "ECS/Systems/Implementations/CreatureHelpSystem.h"
#include "ECS/Systems/Implementations/CreatureLocomotionSystem.h"
#include "ECS/Systems/Implementations/CreatureMindSystem.h"
#include "ECS/Systems/Implementations/CreatureModeSystem.h"
#include "ECS/Systems/Implementations/CreatureObjectActionSystem.h"
#include "ECS/Systems/Implementations/CreaturePenSystem.h"
#include "ECS/Systems/Implementations/CreaturePhysiologySystem.h"
#include "ECS/Systems/Implementations/CreatureSkinSystem.h"
#include "ECS/Systems/Implementations/CreatureWebPageSystem.h"
#include "ECS/Systems/Implementations/DialogueControlSystem.h"
#include "ECS/Systems/Implementations/DynamicsSystem.h"
#include "ECS/Systems/Implementations/EditorSystem.h"
#include "ECS/Systems/Implementations/ExplosionSystem.h"
#include "ECS/Systems/Implementations/FieldSystem.h"
#include "ECS/Systems/Implementations/FightTranscriptSystem.h"
#include "ECS/Systems/Implementations/FireSystem.h"
#include "ECS/Systems/Implementations/FireflySystem.h"
#include "ECS/Systems/Implementations/FishFarmSystem.h"
#include "ECS/Systems/Implementations/FloatingNumberSystem.h"
#include "ECS/Systems/Implementations/FootpathSystem.h"
#include "ECS/Systems/Implementations/FootprintSystem.h"
#include "ECS/Systems/Implementations/ForestSystem.h"
#include "ECS/Systems/Implementations/GestureSystem.h"
#include "ECS/Systems/Implementations/GuidanceSystem.h"
#include "ECS/Systems/Implementations/HandDemoSystem.h"
#include "ECS/Systems/Implementations/HandGrabSystem.h"
#include "ECS/Systems/Implementations/HandSystem.h"
#include "ECS/Systems/Implementations/HelpMessageSystem.h"
#include "ECS/Systems/Implementations/HelpProfileSystem.h"
#include "ECS/Systems/Implementations/HelpSpeechSystem.h"
#include "ECS/Systems/Implementations/HelpTextSystem.h"
#include "ECS/Systems/Implementations/HighDetailSystem.h"
#include "ECS/Systems/Implementations/InfluenceSystem.h"
#include "ECS/Systems/Implementations/IntroSystem.h"
#include "ECS/Systems/Implementations/LandShadeSystem.h"
#include "ECS/Systems/Implementations/LiquidParticleSystem.h"
#include "ECS/Systems/Implementations/LobbyClientSystem.h"
#include "ECS/Systems/Implementations/NetworkGameSystem.h"
#include "ECS/Systems/Implementations/ObjectDrawListSystem.h"
#include "ECS/Systems/Implementations/PlayerCommandSystem.h"
#include "ECS/Systems/Implementations/RealWeatherSystem.h"
#include "ECS/Systems/Implementations/SaveGameSystem.h"
#include "ECS/Systems/Implementations/ScriptHighlightSystem.h"
#include "ECS/Systems/Implementations/SyncChecksumSystem.h"
#include "ECS/Systems/Implementations/ThingsMadeSystem.h"
#include "ECS/Systems/Implementations/TipBubbleSystem.h"
#include "ECS/Systems/Implementations/WorkshopSystem.h"
#if defined(OPENBLACK_INSPECTOR)
#include "ECS/Systems/Implementations/InspectorSystem.h"
#endif
#include "ECS/IncomingMessageRules.h"
#include "ECS/Systems/GuidanceSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/Implementations/AbodeSoundSystem.h"
#include "ECS/Systems/Implementations/ArtefactSystem.h"
#include "ECS/Systems/Implementations/ChatSystem.h"
#include "ECS/Systems/Implementations/ComputerPlayerSystem.h"
#include "ECS/Systems/Implementations/DanceSystem.h"
#include "ECS/Systems/Implementations/FilmCreatureSystem.h"
#include "ECS/Systems/Implementations/FixedOutlineSystem.h"
#include "ECS/Systems/Implementations/GameEndSystem.h"
#include "ECS/Systems/Implementations/HeartbeatSystem.h"
#include "ECS/Systems/Implementations/IncomingMessageSystem.h"
#include "ECS/Systems/Implementations/LandSystem.h"
#include "ECS/Systems/Implementations/LeashSystem.h"
#include "ECS/Systems/Implementations/LivingActionSystem.h"
#include "ECS/Systems/Implementations/MagicShieldSystem.h"
#include "ECS/Systems/Implementations/MagicSystem.h"
#include "ECS/Systems/Implementations/MiracleFxSystem.h"
#include "ECS/Systems/Implementations/MistSystem.h"
#include "ECS/Systems/Implementations/MusicMoodSystem.h"
#include "ECS/Systems/Implementations/ParticleSystem.h"
#include "ECS/Systems/Implementations/PathfindingSystem.h"
#include "ECS/Systems/Implementations/PauseTextSystem.h"
#include "ECS/Systems/Implementations/PickingSystem.h"
#include "ECS/Systems/Implementations/PlayerProfileSystem.h"
#include "ECS/Systems/Implementations/PlayerSystem.h"
#include "ECS/Systems/Implementations/ProfileFolderSystem.h"
#include "ECS/Systems/Implementations/RainSystem.h"
#include "ECS/Systems/Implementations/ReactionSystem.h"
#include "ECS/Systems/Implementations/RenderingSystem.h"
#include "ECS/Systems/Implementations/ResourceStoreSystem.h"
#include "ECS/Systems/Implementations/RewardSystem.h"
#include "ECS/Systems/Implementations/ScriptControlSystem.h"
#include "ECS/Systems/Implementations/ScriptMusicSystem.h"
#include "ECS/Systems/Implementations/ScriptObjectsSystem.h"
#include "ECS/Systems/Implementations/SharkSystem.h"
#include "ECS/Systems/Implementations/SimulationSystem.h"
#include "ECS/Systems/Implementations/SkySystem.h"
#include "ECS/Systems/Implementations/SnowSystem.h"
#include "ECS/Systems/Implementations/SnowfallSystem.h"
#include "ECS/Systems/Implementations/SoundTagSystem.h"
#include "ECS/Systems/Implementations/SpellIconSystem.h"
#include "ECS/Systems/Implementations/TattooEditorSystem.h"
#include "ECS/Systems/Implementations/TattooMenuSystem.h"
#include "ECS/Systems/Implementations/TeleportSystem.h"
#include "ECS/Systems/Implementations/TempleDestructionSystem.h"
#include "ECS/Systems/Implementations/TempleExteriorSystem.h"
#include "ECS/Systems/Implementations/TimeSystem.h"
#include "ECS/Systems/Implementations/TornadoSystem.h"
#include "ECS/Systems/Implementations/TownDesireSystem.h"
#include "ECS/Systems/Implementations/TownSystem.h"
#include "ECS/Systems/Implementations/TutorialSkipSystem.h"
#include "ECS/Systems/Implementations/VegetationSystem.h"
#include "ECS/Systems/Implementations/VideoSystem.h"
#include "ECS/Systems/Implementations/VillageLightSystem.h"
#include "ECS/Systems/Implementations/VillageTotemSystem.h"
#include "ECS/Systems/Implementations/VortexSystem.h"
#include "ECS/Systems/Implementations/WalkPathSystem.h"
#include "ECS/Systems/Implementations/WaterRingSystem.h"
#include "ECS/Systems/Implementations/WeatherSystem.h"
#include "ECS/Systems/Implementations/WorshipSiteSystem.h"
#include "ECS/Systems/InspectorSystemInterface.h"
#include "ECS/Systems/NetworkGameSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ProfileFolderSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Graphics/HeadlessRenderer.h"
#include "Graphics/RendererInterface.h"
#include "Graphics/UploadMode.h"
#include "Gui/TextDatabase.h"
#include "Help/GuidanceRules.h"
#include "Input/GameActionMap.h"
#include "LHVM.h"
#include "Online/HttpClientInterface.h"
#include "Online/NetworkSession.h"
#include "Online/PushChannelInterface.h"
#include "Online/TokenStoreInterface.h"
#include "Online/WebSocketInterface.h"
#include "Online/WebSocketPushChannel.h"
#include "PlayerSlots.h"
#include "Profiler.h"
#if defined(__EMSCRIPTEN__)
#include "Web/BrowserWebSocketClient.h"
#include "Web/FetchHttpClient.h"
#else
#include "Online/Desktop/CurlHttpClient.h"
#include "Online/Desktop/CurlWebSocketClient.h"
#endif
#include "Resources/Resources.h"
#include "Windowing/Sdl2WindowingSystem.h"
#if __ANDROID__
#include "FileSystem/AndroidFileSystem.h"
#else
#include "FileSystem/DefaultFileSystem.h"
#endif

using namespace openblack::audio;
using namespace openblack::filesystem;
using openblack::GameRandomProduction;
using openblack::LandIsland;
using openblack::RandomNumberManagerProduction;
using openblack::TempleInterior;
using openblack::UnloadedIsland;
using openblack::chlapi::CHLApi;
using openblack::debug::gui::DebugGuiInterface;
using openblack::ecs::DrawnRegistry;
using openblack::ecs::MapProduction;
using openblack::ecs::Registry;
using openblack::ecs::systems::AbodeKnockSystem;
using openblack::ecs::systems::AbodeSoundSystem;
using openblack::ecs::systems::AdvisorSystem;
using openblack::ecs::systems::AlignmentSystem;
using openblack::ecs::systems::AnimalSystem;
using openblack::ecs::systems::AnimatedStaticSystem;
using openblack::ecs::systems::AppleBarrelSystem;
using openblack::ecs::systems::ArtefactSystem;
using openblack::ecs::systems::BoatLaunchSystem;
using openblack::ecs::systems::CameraBookmarkSystem;
using openblack::ecs::systems::CameraHelpSystem;
using openblack::ecs::systems::CameraPathSystem;
using openblack::ecs::systems::CameraZoneSystem;
using openblack::ecs::systems::ChatSystem;
using openblack::ecs::systems::ChimneySmokeSystem;
using openblack::ecs::systems::CinematicDirectorSystem;
using openblack::ecs::systems::CloudSystem;
using openblack::ecs::systems::ComputerPlayerSystem;
using openblack::ecs::systems::ComputerSettingsSystem;
using openblack::ecs::systems::CreatureAnimationSystem;
using openblack::ecs::systems::CreatureAudioSystem;
using openblack::ecs::systems::CreatureCarryOverSystem;
using openblack::ecs::systems::CreatureCaveSystem;
using openblack::ecs::systems::CreatureFightSystem;
using openblack::ecs::systems::CreatureFizzSystem;
using openblack::ecs::systems::CreatureHairSystem;
using openblack::ecs::systems::CreatureHandSystem;
using openblack::ecs::systems::CreatureHelpSystem;
using openblack::ecs::systems::CreatureLocomotionSystem;
using openblack::ecs::systems::CreatureMindSystem;
using openblack::ecs::systems::CreatureModeSystem;
using openblack::ecs::systems::CreatureObjectActionSystem;
using openblack::ecs::systems::CreaturePenSystem;
using openblack::ecs::systems::CreaturePhysiologySystem;
using openblack::ecs::systems::CreatureSkinSystem;
using openblack::ecs::systems::CreatureWebPageSystem;
using openblack::ecs::systems::DanceSystem;
using openblack::ecs::systems::DialogueControlSystem;
using openblack::ecs::systems::DynamicsSystem;
using openblack::ecs::systems::EditorSystem;
using openblack::ecs::systems::FieldSystem;
using openblack::ecs::systems::FilmCreatureSystem;
using openblack::ecs::systems::FireflySystem;
using openblack::ecs::systems::FishFarmSystem;
using openblack::ecs::systems::FixedOutlineSystem;
using openblack::ecs::systems::FootpathSystem;
using openblack::ecs::systems::FootprintSystem;
using openblack::ecs::systems::ForestSystem;
using openblack::ecs::systems::GameEndSystem;
using openblack::ecs::systems::GestureEventsInterface;
using openblack::ecs::systems::GestureSystem;
using openblack::ecs::systems::GuidanceSystem;
using openblack::ecs::systems::HandDemoSystem;
using openblack::ecs::systems::HandGrabSystem;
using openblack::ecs::systems::HandSystem;
using openblack::ecs::systems::HeartbeatSystem;
using openblack::ecs::systems::HelpMessageSystem;
using openblack::ecs::systems::HelpProfileSystem;
using openblack::ecs::systems::HelpSpeechSystem;
using openblack::ecs::systems::HelpTextSystem;
using openblack::ecs::systems::HighDetailSystem;
using openblack::ecs::systems::IncomingMessageSystem;
using openblack::ecs::systems::InfluenceSystem;
using openblack::ecs::systems::IntroSystem;
using openblack::ecs::systems::LandShadeSystem;
using openblack::ecs::systems::LandSystem;
using openblack::ecs::systems::LeashSystem;
using openblack::ecs::systems::LiquidParticleSystem;
using openblack::ecs::systems::LivingActionSystem;
using openblack::ecs::systems::LobbyClientConfig;
using openblack::ecs::systems::LobbyClientSystem;
using openblack::ecs::systems::MagicShieldSystem;
using openblack::ecs::systems::MagicSystem;
using openblack::ecs::systems::MiracleFxSystem;
using openblack::ecs::systems::MistSystem;
using openblack::ecs::systems::MusicMoodSystem;
using openblack::ecs::systems::NetworkGameSystem;
using openblack::ecs::systems::ObjectDrawListSystem;
using openblack::ecs::systems::ParticleSystem;
using openblack::ecs::systems::PathfindingSystem;
using openblack::ecs::systems::PauseTextSystem;
using openblack::ecs::systems::PickingSystem;
using openblack::ecs::systems::PlayerCommandSystem;
using openblack::ecs::systems::PlayerProfileSystem;
using openblack::ecs::systems::PlayerSystem;
using openblack::ecs::systems::ProfileFolderSystem;
using openblack::ecs::systems::RainSystem;
using openblack::ecs::systems::ReactionSystem;
using openblack::ecs::systems::RealWeatherSystem;
using openblack::ecs::systems::RenderingSystem;
using openblack::ecs::systems::ResourceStoreSystem;
using openblack::ecs::systems::SaveGameSystem;
using openblack::ecs::systems::ScriptControlSystem;
using openblack::ecs::systems::ScriptMusicSystem;
using openblack::ecs::systems::SharkSystem;
using openblack::ecs::systems::SnowfallSystem;
using openblack::ecs::systems::SnowSystem;
using openblack::ecs::systems::SoundTagSystem;
using openblack::ecs::systems::SpellIconSystem;
using openblack::ecs::systems::SyncChecksumSystem;
using openblack::ecs::systems::TattooEditorSystem;
using openblack::ecs::systems::TattooMenuSystem;
using openblack::ecs::systems::TeleportSystem;
using openblack::ecs::systems::TempleDestructionSystem;
using openblack::ecs::systems::TempleExteriorSystem;
using openblack::ecs::systems::ThingsMadeSystem;
using openblack::ecs::systems::TimeSystem;
using openblack::ecs::systems::TipBubbleSystem;
using openblack::ecs::systems::TornadoSystem;
using openblack::ecs::systems::TownDesireSystem;
using openblack::ecs::systems::TownSystem;
using openblack::ecs::systems::TutorialSkipSystem;
using openblack::ecs::systems::VegetationSystem;
using openblack::ecs::systems::VideoSystem;
using openblack::ecs::systems::VillageLightSystem;
using openblack::ecs::systems::VillageTotemSystem;
using openblack::ecs::systems::VortexSystem;
using openblack::ecs::systems::WalkPathSystem;
using openblack::ecs::systems::WaterRingSystem;
using openblack::ecs::systems::WeatherSystem;
using openblack::ecs::systems::WorkshopSystem;
using openblack::ecs::systems::WorshipSiteSystem;
using openblack::graphics::HeadlessRenderer;
using openblack::graphics::RendererInterface;
using openblack::input::GameActionMap;
using openblack::lhvm::LHVM;
using openblack::resources::Resources;
using openblack::windowing::DisplayMode;
using openblack::windowing::Sdl2WindowingSystem;

void openblack::InitializeWindow(const std::string& title, int width, int height, DisplayMode displayMode, uint32_t extraFlags,
                                 bool background)
{
	Locator::windowing::emplace<Sdl2WindowingSystem>(title, width, height, displayMode, extraFlags, background);
}

void openblack::InitializeSimulationCore()
{
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "EnTT version: {}", ENTT_VERSION);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), GLM_VERSION_COMPLETE);

	Locator::profiler::emplace();
	Locator::mainThreadQueue::emplace();
	Locator::events::emplace<EventManager>();
#if __ANDROID__
	Locator::filesystem::emplace<AndroidFileSystem>();
#else
	Locator::filesystem::emplace<DefaultFileSystem>();
#endif
	Locator::rng::emplace<RandomNumberManagerProduction>();
	Locator::gameRandom::emplace<GameRandomProduction>();
}

bool openblack::InitializeEngine(GraphicsBackend backend, bool vsync) noexcept
{
	Locator::rendererInterface::reset(RendererInterface::Create(backend, vsync).release());
	if (!Locator::rendererInterface::has_value())
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("graphics"), "Failed to create renderer");
		return false;
	}
	Locator::debugGui::reset(DebugGuiInterface::Create(graphics::RenderPass::ImGui).release());

	std::unique_ptr<audio::AudioManagerInterface> machineAudio;
	try
	{
		// A simulation run with a fixed frame time times its sounds by the game's clock
		if (Locator::config::has_value() && Locator::config::value().clockedAudio)
		{
			machineAudio = std::make_unique<AudioManager>(
			    std::make_unique<audio::ClockedAudioPlayer>([] { return machine_clock::Ticks(); }));
		}
		else
		{
			machineAudio = std::make_unique<AudioManager>();
		}
	}
	catch (std::runtime_error& error)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("audio"), "Falling back to no-op audio: {}", error.what());
		machineAudio = std::make_unique<AudioManagerNoOp>();
	}
	// The game's calls go to the audio through the queue of the main thread
	Locator::audio::emplace<audio::QueuedAudioManager>(std::move(machineAudio), Locator::mainThreadQueue::value());
	Locator::sounds::emplace<audio::GameSoundRequests>();
	return true;
}

bool openblack::InitializeHeadlessEngine() noexcept
{
	Locator::rendererInterface::reset(HeadlessRenderer::Create().release());
	if (!Locator::rendererInterface::has_value())
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("graphics"), "Failed to create the renderer with no device");
		return false;
	}
	Locator::audio::emplace<AudioManagerNoOp>();
	Locator::sounds::emplace<audio::GameSoundRequests>();
	return true;
}

namespace openblack
{
namespace
{

/// The lobby of the config, if the game has one
std::optional<LobbyClientConfig> LobbyConfig()
{
	if (!Locator::config::has_value() || !Locator::config::value().lobbyOrigin.has_value())
	{
		return std::nullopt;
	}
#if defined(__EMSCRIPTEN__)
	// The web build calls the lobby on the origin of the page
	return LobbyClientConfig {.origin = {}, .client = lobby_api::ClientKind::Browser};
#else
	auto origin = *Locator::config::value().lobbyOrigin;
	while (!origin.empty() && origin.back() == '/')
	{
		origin.pop_back();
	}
	constexpr std::string_view k_Https = "https://";
	if (!online::http_policy::IsHttpsAddress(origin) || origin.find('/', k_Https.size()) != std::string::npos ||
	    origin.find_first_of("?#", k_Https.size()) != std::string::npos)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The lobby must be an https:// origin with no path: {}", origin);
		return std::nullopt;
	}
	return LobbyClientConfig {.origin = std::move(origin), .client = lobby_api::ClientKind::Desktop};
#endif
}

/// The date of the computer's calendar at a time
lobby_api::Date LocalDate(int64_t unixTime)
{
	const auto time = static_cast<std::time_t>(unixTime);
	std::tm local {};
#if defined(_WIN32)
	localtime_s(&local, &time);
#else
	localtime_r(&time, &local);
#endif
	return {.year = local.tm_year + 1900, .month = local.tm_mon + 1, .day = local.tm_mday};
}

/// The web client, the WSS client, the lobby client and the real-world weather. They make no call and open no socket
/// until the game asks for one.
void InitializeOnline(const player_profiles::Store& profiles)
{
#if defined(__EMSCRIPTEN__)
	Locator::httpClient::emplace<web::FetchHttpClient>();
	Locator::webSocketClient::emplace<web::BrowserWebSocketClient>();
	// The browser cannot send headers in the handshake
	constexpr auto k_PushAuth = online::PushAuth::Subprotocol;
#else
	// Tests on this computer only: a lobby on a loopback address with a certificate of a test authority
	const auto testAuthorities = Locator::config::has_value() ? Locator::config::value().lobbyTestAuthorities : std::nullopt;
	const bool loopbackLobby = Locator::config::has_value() && Locator::config::value().lobbyOrigin.has_value() &&
	                           online::http_policy::IsLoopbackAddress(*Locator::config::value().lobbyOrigin);
	if (testAuthorities.has_value() && loopbackLobby)
	{
		Locator::httpClient::emplace<online::CurlHttpClient>(std::nullopt, std::filesystem::path(*testAuthorities));
		Locator::webSocketClient::emplace<online::CurlWebSocketClient>(*testAuthorities);
	}
	else
	{
		if (testAuthorities.has_value())
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The test authorities are only for a lobby on a loopback address");
		}
		Locator::httpClient::emplace<online::CurlHttpClient>();
		Locator::webSocketClient::emplace<online::CurlWebSocketClient>();
	}
	constexpr auto k_PushAuth = online::PushAuth::Header;
#endif
	Locator::networkGame::emplace<NetworkGameSystem>(
	    Locator::webSocketClient::value(),
	    [](const std::string& authorities) -> std::unique_ptr<online::WebSocketClientInterface> {
#if defined(__EMSCRIPTEN__)
		    static_cast<void>(authorities);
		    return nullptr;
#else
		    return std::make_unique<online::CurlWebSocketClient>(authorities);
#endif
	    },
	    [creatureMinds = profiles.creatureMinds]() -> std::vector<uint8_t> {
		    // The creature of the player's profile goes to the other players before a network game starts
		    const auto file =
		        Locator::playerProfileSystem::has_value() ? Locator::playerProfileSystem::value().CreatureFile() : std::nullopt;
		    if (!file.has_value() || !creatureMinds.has_value())
		    {
			    return {};
		    }
		    auto& fileSystem = Locator::filesystem::value();
		    const auto path = *creatureMinds / *file;
		    return fileSystem.Exists(path) ? fileSystem.ReadAll(path) : std::vector<uint8_t> {};
	    });
	Locator::lobbyClient::emplace<LobbyClientSystem>(
	    LobbyConfig(), Locator::httpClient::value(),
	    std::make_unique<online::WebSocketPushChannel>(Locator::webSocketClient::value(), k_PushAuth),
	    online::MakeTokenStore(Locator::filesystem::value(), profiles.folder),
	    []() { return std::chrono::system_clock::now(); });
	const bool internetWeather = Locator::config::has_value() && Locator::config::value().useInternetWeather;
	const auto readScript = [](std::string_view file) -> std::optional<std::vector<uint8_t>> {
		auto& files = Locator::filesystem::value();
		const auto path = files.GetPath<filesystem::Path::Scripts>() / file;
		try
		{
			if (!files.Exists(path))
			{
				return std::nullopt;
			}
			return files.ReadAll(path);
		}
		catch (const std::exception& error)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "Could not read {}: {}", file, error.what());
			return std::nullopt;
		}
	};
	Locator::realWeatherSystem::emplace<RealWeatherSystem>(
	    internetWeather, Locator::lobbyClient::value(), [readScript]() { return readScript("weatherinfo.lhw"); },
	    []() { return LocalDate(Locator::time::has_value() ? Locator::time::value().GetUnixTime() : std::time(nullptr)); },
	    []() { return std::chrono::steady_clock::now(); }, [readScript]() { return readScript("country.lhw"); },
	    // The days of weather go to the folder of the current profile. A developer's run keeps no files for its player.
	    [developer = profiles.developer](std::span<const uint8_t> bytes) {
		    if (developer || !Locator::playerProfileSystem::has_value() || !Locator::profileFolderSystem::has_value())
		    {
			    return;
		    }
		    const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile();
		    if (profile.has_value() &&
		        !Locator::profileFolderSystem::value().WritePlayerFile(gui::ToUtf16(profile->name), "weather.dat", bytes))
		    {
			    SPDLOG_LOGGER_WARN(spdlog::get("game"), "Could not write the days of the real weather");
		    }
	    },
	    // The message of the real weather goes into the list of incoming messages
	    [](std::string_view place, uint8_t picture) {
		    if (Locator::incomingMessageSystem::has_value())
		    {
			    Locator::incomingMessageSystem::value().AddWeather(gui::ToUtf16(place), picture);
		    }
	    });
}

/// The chat of a network game, and what it asks of the rest of the game
void EmplaceChat(bool developer)
{
	ChatSystem::Services services;
	services.submit = [](player_commands::Action action) {
		if (Locator::playerCommandSystem::has_value())
		{
			Locator::playerCommandSystem::value().Submit(std::move(action));
		}
	};
	services.localPlayer = []() -> std::optional<PlayerNames> {
		if (!Locator::handSystem::has_value())
		{
			return std::nullopt;
		}
		const auto& hands = Locator::handSystem::value();
		const auto own = hands.GetOwnPlayer();
		return hands.IsOwnPlayer(own) ? std::optional(own) : std::nullopt;
	};
	services.creatureOf = [](PlayerNames player) -> std::optional<entt::entity> {
		return Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetPrimaryCreature(player) : std::nullopt;
	};
	services.players = []() {
		std::vector<PlayerNames> players;
		for (size_t slot = 0; slot < player_slots::k_MaxPlayers; ++slot)
		{
			players.push_back(static_cast<PlayerNames>(slot));
		}
		return players;
	};
	services.incoming = [](std::u16string_view line) {
		if (Locator::incomingMessageSystem::has_value())
		{
			Locator::incomingMessageSystem::value().Add(incoming_messages::Kind::Chat, line, 0);
		}
	};
	// A taunt is said as the remark of the first kind, as the original game does
	services.advisorsSay = [](uint32_t text) {
		if (Locator::guidanceSystem::has_value())
		{
			Locator::guidanceSystem::value().SayText(text, guidance::Kind::TownDesireSound);
		}
	};
	services.yackingRemark = []() {
		if (Locator::guidanceSystem::has_value())
		{
			Locator::guidanceSystem::value().SayOneOff(guidance::OneOff::QuitTheYacking);
		}
	};
	// The lines go to the chat log in the folder of the current profile. A developer's run keeps no files for its player.
	services.log = [developer](std::string_view line) {
		if (developer || !Locator::playerProfileSystem::has_value() || !Locator::profileFolderSystem::has_value())
		{
			return;
		}
		const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile();
		if (profile.has_value())
		{
			const auto* bytes = reinterpret_cast<const uint8_t*>(line.data());
			Locator::profileFolderSystem::value().AppendPlayerFile(gui::ToUtf16(profile->name), "chat.log",
			                                                       {bytes, line.size()});
		}
	};
	services.names = []() {
		std::map<PlayerNames, std::u16string> names;
		if (!Locator::networkGame::has_value() || Locator::networkGame::value().Session() == nullptr)
		{
			return names;
		}
		const auto& session = *Locator::networkGame::value().Session();
		const auto& start = session.GetStart();
		const auto& joined = session.GetJoined();
		const auto* gods = start.has_value()                                 ? &start->gods
		                   : joined.has_value() && joined->start.has_value() ? &joined->start->gods
		                                                                     : nullptr;
		if (gods != nullptr)
		{
			for (const auto& god : *gods)
			{
				names[static_cast<PlayerNames>(god.slot)] = gui::ToUtf16(god.name);
			}
		}
		return names;
	};
	services.turn = []() { return Locator::time::has_value() ? Locator::time::value().GetTurn() : 0u; };
	Locator::chatSystem::emplace<ChatSystem>(std::move(services));
}

} // namespace
} // namespace openblack

bool openblack::InitializeSimulation(const player_profiles::Store& profiles) noexcept
{
	// The script machine and its natives
	Locator::chlapi::emplace<CHLApi>();
	Locator::vm::emplace<LHVM>();
	Locator::terrainSystem::emplace<UnloadedIsland>();
	Locator::resources::emplace<Resources>();
	Locator::playerSystem::emplace<PlayerSystem>();
	Locator::entitiesRegistry::emplace<Registry>();
	Locator::drawnRegistry::emplace<DrawnRegistry>();
	Locator::handSystem::emplace<HandSystem>();
	Locator::handGrabSystem::emplace<HandGrabSystem>();
	Locator::temple::emplace<TempleInterior>();
	Locator::skySystem::emplace<ecs::systems::SkySystem>();
	Locator::alignmentSystem::emplace<AlignmentSystem>();
	Locator::cameraHelpSystem::emplace<CameraHelpSystem>();
	Locator::tutorialSkipSystem::emplace<TutorialSkipSystem>();
	Locator::playerProfileSystem::emplace<PlayerProfileSystem>(profiles);
	Locator::profileFolderSystem::emplace<ProfileFolderSystem>(Locator::filesystem::value(), profiles.folder);
	Locator::musicMoodSystem::emplace<MusicMoodSystem>();
	// The music that the scripts start, which the world times by its turns
	Locator::scriptMusicSystem::emplace<ScriptMusicSystem>();
	Locator::templeExteriorSystem::emplace<TempleExteriorSystem>();
	Locator::templeDestructionSystem::emplace<TempleDestructionSystem>();
	Locator::computerPlayerSystem::emplace<ComputerPlayerSystem>();
	Locator::time::emplace<TimeSystem>();
	Locator::simulation::emplace<ecs::systems::SimulationSystem>();
	Locator::vegetation::emplace<VegetationSystem>();
	Locator::abodeSoundSystem::emplace<AbodeSoundSystem>();
	Locator::villageTotemSystem::emplace<VillageTotemSystem>();
	Locator::spellIconSystem::emplace<SpellIconSystem>();
	Locator::artefactSystem::emplace<ArtefactSystem>();
	Locator::heartbeatSystem::emplace<HeartbeatSystem>();
	Locator::gameEndSystem::emplace<GameEndSystem>();
	Locator::fieldSystem::emplace<FieldSystem>();
	Locator::fishFarmSystem::emplace<FishFarmSystem>();
	Locator::workshopSystem::emplace<WorkshopSystem>();
	Locator::animalSystem::emplace<AnimalSystem>();
	Locator::snowSystem::emplace<SnowSystem>();
	Locator::snowfallSystem::emplace<SnowfallSystem>();
	Locator::waterRingSystem::emplace<WaterRingSystem>();
	Locator::creatureAnimationSystem::emplace<CreatureAnimationSystem>();
	Locator::creatureMindSystem::emplace<CreatureMindSystem>();
	Locator::creaturePhysiologySystem::emplace<CreaturePhysiologySystem>();
	Locator::creatureHairSystem::emplace<CreatureHairSystem>();
	Locator::creatureAudioSystem::emplace<CreatureAudioSystem>();
	Locator::creatureObjectActionSystem::emplace<CreatureObjectActionSystem>();
	Locator::creatureHandSystem::emplace<CreatureHandSystem>();
	Locator::footprintSystem::emplace<FootprintSystem>();
	Locator::footpathSystem::emplace<FootpathSystem>();
	Locator::creatureSkinSystem::emplace<CreatureSkinSystem>();
	Locator::creaturePenSystem::emplace<CreaturePenSystem>();
	Locator::leashSystem::emplace<LeashSystem>();
	Locator::creatureFightSystem::emplace<CreatureFightSystem>();
	Locator::creatureModeSystem::emplace<CreatureModeSystem>();
	Locator::creatureCaveSystem::emplace<CreatureCaveSystem>();
	Locator::tattooEditorSystem::emplace<TattooEditorSystem>();
	Locator::tattooMenuSystem::emplace<TattooMenuSystem>();
	Locator::filmCreatureSystem::emplace<FilmCreatureSystem>();
	Locator::cinematicDirectorSystem::emplace<CinematicDirectorSystem>();
	Locator::scriptControlSystem::emplace<ScriptControlSystem>();
	Locator::dialogueControlSystem::emplace<DialogueControlSystem>();
	Locator::helpSpeechSystem::emplace<HelpSpeechSystem>();
	Locator::highDetailSystem::emplace<HighDetailSystem>();
	Locator::introSystem::emplace<IntroSystem>();
	Locator::boatLaunchSystem::emplace<BoatLaunchSystem>();
	Locator::walkPathSystem::emplace<WalkPathSystem>();
	Locator::danceSystem::emplace<DanceSystem>();
	Locator::sharkSystem::emplace<SharkSystem>();
	Locator::helpTextSystem::emplace<HelpTextSystem>();
	Locator::helpProfileSystem::emplace<HelpProfileSystem>();
	Locator::creatureHelpSystem::emplace<CreatureHelpSystem>();
	Locator::helpMessageSystem::emplace<HelpMessageSystem>();
	Locator::guidanceSystem::emplace<GuidanceSystem>();
	Locator::tipBubbleSystem::emplace<TipBubbleSystem>();
	Locator::handDemoSystem::emplace<HandDemoSystem>();
	Locator::playerCommandSystem::emplace<PlayerCommandSystem>();
	Locator::syncChecksumSystem::emplace<SyncChecksumSystem>();
	Locator::advisorSystem::emplace<AdvisorSystem>();
	Locator::soundTagSystem::emplace<SoundTagSystem>();
	Locator::rainSystem::emplace<RainSystem>();
	Locator::chimneySmokeSystem::emplace<ChimneySmokeSystem>();
	// The count of things made goes with the world: a new land starts it again and a saved game keeps it
	Locator::thingsMadeSystem::emplace<ThingsMadeSystem>(
	    []() -> uint32_t& { return Locator::entitiesRegistry::value().Context().thingsMade; });
	Locator::abodeKnockSystem::emplace<AbodeKnockSystem>();
	Locator::influenceSystem::emplace<InfluenceSystem>();
	Locator::townDesireSystem::emplace<TownDesireSystem>();
	Locator::particleSystem::emplace<ParticleSystem>();
	Locator::reactionSystem::emplace<ReactionSystem>();
	Locator::teleportSystem::emplace<TeleportSystem>();
	Locator::vortexSystem::emplace<VortexSystem>();
	Locator::fixedOutlineSystem::emplace<FixedOutlineSystem>();
	Locator::creatureFizzSystem::emplace<CreatureFizzSystem>();
	Locator::liquidParticleSystem::emplace<LiquidParticleSystem>();
	Locator::landShadeSystem::emplace<LandShadeSystem>();
	// The player's creature is kept under their profile's file
	Locator::creatureCarryOverSystem::emplace<CreatureCarryOverSystem>(
	    profiles.creatureMinds, []() { return Locator::playerProfileSystem::value().CreatureFile(); });
	Locator::saveGameSystem::emplace<SaveGameSystem>();
	// The creature's web page and its pictures go to the folder of the profile picked now
	Locator::creatureWebPageSystem::emplace<CreatureWebPageSystem>([]() -> std::optional<std::u16string> {
		if (!Locator::playerProfileSystem::has_value())
		{
			return std::nullopt;
		}
		const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile();
		return profile.has_value() ? std::optional(gui::ToUtf16(profile->name)) : std::nullopt;
	});
	Locator::tornadoSystem::emplace<TornadoSystem>();
	Locator::magicShieldSystem::emplace<MagicShieldSystem>();
	Locator::forestSystem::emplace<ForestSystem>();
	Locator::fireflySystem::emplace<FireflySystem>();
	Locator::gestureSystem::emplace<GestureSystem>();
	// The miracles take the gestures the gesture system recognises; the system stays the owner
	Locator::gestureEvents::reset(static_cast<GestureEventsInterface*>(&Locator::gestureSystem::value()),
	                              [](GestureEventsInterface* /*unowned*/) {});
	Locator::magicSystem::emplace<MagicSystem>();
	Locator::miracleFxSystem::emplace<MiracleFxSystem>();
	Locator::pauseTextSystem::emplace<PauseTextSystem>();
	Locator::incomingMessageSystem::emplace<IncomingMessageSystem>([]() { return machine_clock::Ticks(); });
	EmplaceChat(profiles.developer);
	Locator::fireSystem::emplace<ecs::systems::FireSystem>();
	Locator::explosionSystem::emplace<ecs::systems::ExplosionSystem>();
	Locator::floatingNumberSystem::emplace<ecs::systems::FloatingNumberSystem>();
	Locator::rewardSystem::emplace<ecs::systems::RewardSystem>();
	Locator::scriptObjects::emplace<ecs::systems::ScriptObjectsSystem>();
	Locator::buildingDamageSystem::emplace<ecs::systems::BuildingDamageSystem>();
	Locator::scriptHighlightSystem::emplace<ecs::systems::ScriptHighlightSystem>();
	// The land loads and plays its turns for the game and for the dedicated server both
	Locator::landSystem::emplace<LandSystem>();
	return true;
}

void openblack::InitializePresentation()
{
	// The sea, the mists, the clouds and the village lights, which the turns and the scripts do not read
	Locator::oceanSystem::emplace<Ocean>(graphics::ConfiguredUploads());
	Locator::mistSystem::emplace<MistSystem>();
	Locator::cloudSystem::emplace<CloudSystem>();
	Locator::villageLightSystem::emplace<VillageLightSystem>();
}

void openblack::InitializeClient(const player_profiles::Store& profiles)
{
	// The input of the player, the drawing of the world, the editor, the videos and the online services
	Locator::gameActionSystem::emplace<GameActionMap>();
	Locator::rendereringSystem::emplace<RenderingSystem>();
	InitializeOnline(profiles);
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	// The editor is a developer tool: a build without the developer tools has no editor
	Locator::editorSystem::emplace<EditorSystem>();
#endif
	Locator::videoSystem::emplace<VideoSystem>();
}

bool openblack::InitializeGame(const player_profiles::Store& profiles) noexcept
{
	if (!InitializeSimulation(profiles))
	{
		return false;
	}
	InitializePresentation();
	InitializeClient(profiles);
	return true;
}

namespace openblack
{
namespace
{
/// Starts the services of a new level. makeLand gives the level its land, at the place in the order where the land
/// changes.
template <typename MakeLand>
void InitializeLevelWith(MakeLand&& makeLand)
{
	// Both seeds go to 0 with every map, as the game clears them
	Locator::gameRandom::value().SetSeeds({0, 0});
	// The game's own streams start again from the run's seed with every map too, so that the same land loaded with the
	// same seed draws the same numbers
	auto& rng = Locator::rng::value();
	rng.SetRunSeed(rng.GetRunSeed());
	// In a seeded run the machine's clock, as the game reads it, counts from 0 again with every map, from the pinned date
	if (Locator::time::has_value())
	{
		if (const auto date = Locator::time::value().GetPinnedDate(); date.has_value())
		{
			Locator::time::value().RestartClock(*date);
		}
	}
	Locator::entitiesMap::emplace<MapProduction>();
	Locator::dynamicsSystem::emplace<DynamicsSystem>();
	// What lines and the cursor meet, as the game finds them
	Locator::pickingSystem::emplace<PickingSystem>();
	// The game's own kinds of thing in the physics
	Locator::dynamicsSystem::value().SetClassHooks(std::make_unique<openblack::ecs::PhysicsGameHooks>());
	Locator::livingActionSystem::emplace<LivingActionSystem>();
	Locator::townSystem::emplace<TownSystem>();
	Locator::worshipSiteSystem::emplace<WorshipSiteSystem>();
	Locator::resourceStoreSystem::emplace<ResourceStoreSystem>();
	Locator::animatedStaticSystem::emplace<AnimatedStaticSystem>();
	// The apple barrel with its worm beside the local player's storage pit
	Locator::appleBarrelSystem::emplace<AppleBarrelSystem>();
	Locator::weatherSystem::emplace<WeatherSystem>();
	Locator::pathfindingSystem::emplace<PathfindingSystem>();
	// Where creatures can walk is sorted anew for each land
	Locator::creatureLocomotionSystem::emplace<CreatureLocomotionSystem>();
	Locator::cameraBookmarkSystem::emplace<CameraBookmarkSystem>();
	// A land starts with no camera zones
	Locator::cameraZoneSystem::emplace<CameraZoneSystem>();
	makeLand();
	// The things on the land's blocks that are drawn, near to far
	Locator::objectDrawListSystem::emplace<ObjectDrawListSystem>();
	Locator::cameraPathSystem::emplace<CameraPathSystem>();
}
} // namespace
} // namespace openblack

void openblack::InitializeLevel(const std::filesystem::path& path)
{
	InitializeLevelWith([&path] { Locator::terrainSystem::emplace<LandIsland>(path); });
}

void openblack::InitializeLevel(const LandData& land)
{
	InitializeLevelWith([&land] { Locator::terrainSystem::emplace<LandIsland>(land); });
}

void openblack::InitializeLevelOnOpenLand()
{
	InitializeLevelWith([] {
		const auto restored = Locator::terrainSystem::value().RestoreBuiltAltitudes();
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The level keeps its land: {} cells went back to their built altitude",
		                   restored);
	});
}

void openblack::InitializeComputerSettings(std::optional<std::filesystem::path> file)
{
	Locator::computerSettingsSystem::emplace<ComputerSettingsSystem>(std::move(file));
}

void openblack::ShutDownServices()
{
	// The files of the creature that wait go to the disk while the services that write them are still there
	if (Locator::creatureCarryOverSystem::has_value())
	{
		Locator::creatureCarryOverSystem::value().FinishWrites();
	}
	if (Locator::creatureWebPageSystem::has_value())
	{
		Locator::creatureWebPageSystem::value().FinishWrites();
	}

	// Stop all sounds
	if (Locator::audio::has_value())
	{
		Locator::audio::value().Stop();
	}

	// Manually delete the assets here before BGFX renderer clears its buffers resulting in invalid handles in our assets
	if (Locator::resources::has_value())
	{
		auto& resources = Locator::resources::value();
		// Nothing more may be made on the loading threads once the renderer goes
		resources.StopLoading();
		resources.GetMeshes().Clear();
		resources.GetTextures().Clear();
		resources.GetAnimations().Clear();
		resources.GetSounds().Clear();
		// The advisors' models hold meshes of their own
		resources.GetAdvisorModels().Clear();
		// The thumbnails of the network lands hold textures of their own
		resources.GetLandThumbnails().Clear();
	}
	Locator::advisorSystem::reset();

	// The audio resources have been cleared and all sounds have been stopped. It is now safe to reset audio
	Locator::sounds::reset();
	if (Locator::audio::has_value())
	{
		Locator::audio::reset();
	}

	// The inspector goes first: its providers read the services below
	Locator::inspector::reset();
	Locator::landSystem::reset();
	// The lobby client goes before the web client that it uses, and the real-world weather before the lobby client
	Locator::realWeatherSystem::reset();
	Locator::lobbyClient::reset();
	Locator::networkGame::reset();
	Locator::webSocketClient::reset();
	Locator::httpClient::reset();
	Locator::fightTranscript::reset();
	Locator::creatureAnimationLog::reset();
	Locator::rendereringSystem::reset();
	Locator::dynamicsSystem::reset();
	Locator::templeDestructionSystem::reset();
	Locator::computerPlayerSystem::reset();
	Locator::pickingSystem::reset();
	Locator::editorSystem::reset();
	Locator::cameraBookmarkSystem::reset();
	Locator::cameraZoneSystem::reset();
	Locator::livingActionSystem::reset();
	Locator::townSystem::reset();
	Locator::worshipSiteSystem::reset();
	Locator::resourceStoreSystem::reset();
	Locator::animatedStaticSystem::reset();
	Locator::objectDrawListSystem::reset();
	Locator::abodeSoundSystem::reset();
	Locator::appleBarrelSystem::reset();
	Locator::weatherSystem::reset();
	Locator::handGrabSystem::reset();
	Locator::handSystem::reset();
	Locator::pathfindingSystem::reset();
	Locator::creatureLocomotionSystem::reset();
	Locator::cinematicDirectorSystem::reset();
	Locator::scriptControlSystem::reset();
	Locator::dialogueControlSystem::reset();
	Locator::helpSpeechSystem::reset();
	Locator::highDetailSystem::reset();
	Locator::introSystem::reset();
	Locator::boatLaunchSystem::reset();
	Locator::walkPathSystem::reset();
	Locator::danceSystem::reset();
	Locator::sharkSystem::reset();
	Locator::videoSystem::reset();
	Locator::helpTextSystem::reset();
	Locator::helpProfileSystem::reset();
	Locator::computerSettingsSystem::reset();
	Locator::musicMoodSystem::reset();
	Locator::scriptMusicSystem::reset();
	Locator::simulation::reset();
	Locator::profileFolderSystem::reset();
	Locator::creatureHelpSystem::reset();
	Locator::guidanceSystem::reset();
	Locator::helpMessageSystem::reset();
	Locator::tipBubbleSystem::reset();
	Locator::handDemoSystem::reset();
	Locator::playerCommandSystem::reset();
	Locator::syncChecksumSystem::reset();
	Locator::influenceSystem::reset();
	Locator::chimneySmokeSystem::reset();
	Locator::thingsMadeSystem::reset();
	Locator::abodeKnockSystem::reset();
	Locator::rainSystem::reset();
	Locator::snowfallSystem::reset();
	Locator::waterRingSystem::reset();
	Locator::filmCreatureSystem::reset();
	Locator::tattooMenuSystem::reset();
	Locator::tattooEditorSystem::reset();
	Locator::creatureCaveSystem::reset();
	Locator::creatureModeSystem::reset();
	Locator::creatureFightSystem::reset();
	Locator::leashSystem::reset();
	Locator::creatureMindSystem::reset();
	Locator::creaturePhysiologySystem::reset();
	Locator::creaturePenSystem::reset();
	Locator::creatureSkinSystem::reset();
	Locator::creatureHairSystem::reset();
	Locator::footprintSystem::reset();
	Locator::footpathSystem::reset();
	Locator::creatureAudioSystem::reset();
	Locator::creatureObjectActionSystem::reset();
	Locator::creatureHandSystem::reset();
	Locator::creatureAnimationSystem::reset();
	Locator::snowSystem::reset();
	Locator::fieldSystem::reset();
	Locator::fishFarmSystem::reset();
	Locator::workshopSystem::reset();
	Locator::animalSystem::reset();
	Locator::soundTagSystem::reset();
	Locator::villageTotemSystem::reset();
	Locator::spellIconSystem::reset();
	Locator::artefactSystem::reset();
	Locator::heartbeatSystem::reset();
	Locator::gameEndSystem::reset();
	Locator::townDesireSystem::reset();
	Locator::miracleFxSystem::reset();
	Locator::pauseTextSystem::reset();
	Locator::incomingMessageSystem::reset();
	Locator::chatSystem::reset();
	Locator::fireSystem::reset();
	Locator::explosionSystem::reset();
	Locator::floatingNumberSystem::reset();
	Locator::rewardSystem::reset();
	Locator::scriptObjects::reset();
	Locator::buildingDamageSystem::reset();
	Locator::scriptHighlightSystem::reset();
	Locator::magicSystem::reset();
	Locator::gestureEvents::reset();
	Locator::reactionSystem::reset();
	Locator::teleportSystem::reset();
	Locator::vortexSystem::reset();
	Locator::fixedOutlineSystem::reset();
	Locator::creatureFizzSystem::reset();
	Locator::liquidParticleSystem::reset();
	Locator::landShadeSystem::reset();
	Locator::creatureWebPageSystem::reset();
	Locator::creatureCarryOverSystem::reset();
	Locator::saveGameSystem::reset();
	Locator::tornadoSystem::reset();
	Locator::magicShieldSystem::reset();
	Locator::forestSystem::reset();
	Locator::fireflySystem::reset();
	Locator::gestureSystem::reset();
	Locator::particleSystem::reset();
	Locator::terrainSystem::reset();
	Locator::filesystem::reset();
	Locator::gameActionSystem::reset();

	Locator::oceanSystem::reset();
	Locator::skySystem ::reset();
	Locator::cameraHelpSystem::reset();
	Locator::cameraPathSystem::reset();
	Locator::cloudSystem::reset();
	Locator::mistSystem::reset();
	Locator::villageLightSystem::reset();
	Locator::tutorialSkipSystem::reset();
	Locator::templeExteriorSystem::reset();
	Locator::temple::reset();
	Locator::vegetation::reset();
	Locator::alignmentSystem::reset();
	Locator::playerProfileSystem::reset();
	Locator::playerSystem::reset();
	Locator::debugGui::reset();
	// The map listens to the registry's signals, so it goes first: a map left behind would later let go of signals of a
	// registry that is gone
	Locator::entitiesMap::reset();
	Locator::drawnRegistry::reset();
	Locator::entitiesRegistry::reset();
	Locator::rendererInterface::reset();
	Locator::windowing::reset();
	Locator::events::reset();
	Locator::camera::reset();
	Locator::config::reset();
	Locator::infoConstants::reset();
	Locator::profiler::reset();
	Locator::mainThreadQueue::reset();

	Locator::vm::reset();
	Locator::gameRandom::reset();
	Locator::rng::reset();
}

bool openblack::StartInspector([[maybe_unused]] uint16_t port)
{
#if defined(OPENBLACK_INSPECTOR)
	std::string error;
	auto server = inspector::Server::Listen(port, error);
	if (server == nullptr)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The inspector couldn't start: {}", error);
		return false;
	}
	// The fights keep a transcript for the inspector's tools
	Locator::fightTranscript::emplace<ecs::systems::FightTranscriptSystem>();
	// The creatures' bodies keep a log of their clips for the inspector's tools
	Locator::creatureAnimationLog::emplace<ecs::systems::CreatureAnimationLogSystem>();
	Locator::inspector::emplace<ecs::systems::InspectorSystem>(std::move(server));
	return true;
#else
	SPDLOG_LOGGER_ERROR(spdlog::get("game"), "This build has no inspector: configure with OPENBLACK_INSPECTOR on");
	return false;
#endif
}
