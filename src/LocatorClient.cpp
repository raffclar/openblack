/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The services of the game on the player's computer: the window, the graphics device, the renderer, the audio device,
// the systems that only show the world and the drawing of the world. The dedicated server does not link this file.

#define LOCATOR_IMPLEMENTATIONS

#include <memory>
#include <stdexcept>
#include <string>

#include <spdlog/spdlog.h>

#include "3D/Implementations/Ocean.h"
#include "3D/TempleInteriorInterface.h"
#include "Audio/AudioManager.h"
#include "Audio/AudioManagerNoOp.h"
#include "Audio/ClockedAudioPlayer.h"
#include "Audio/GameSoundRequests.h"
#include "Audio/QueuedAudioManager.h"
#include "Common/MachineClock.h"
#include "Common/MainThreadQueue.h"
#include "Common/PlayerProfiles.h"
#include "Debug/DebugGuiInterface.h"
#include "ECS/Systems/Implementations/AbodeSoundSystem.h"
#include "ECS/Systems/Implementations/AdvisorVoiceSystem.h"
#include "ECS/Systems/Implementations/CloudSystem.h"
#include "ECS/Systems/Implementations/CreatureHairSystem.h"
#include "ECS/Systems/Implementations/FloatingNumberSystem.h"
#include "ECS/Systems/Implementations/IncomingMessageSystem.h"
#include "ECS/Systems/Implementations/LandShadeSystem.h"
#include "ECS/Systems/Implementations/LiquidParticleSystem.h"
#include "ECS/Systems/Implementations/ParticleSystem.h"
#include "ECS/Systems/Implementations/PauseTextSystem.h"
#include "ECS/Systems/Implementations/RainSystem.h"
#include "ECS/Systems/Implementations/RenderingSystem.h"
#include "ECS/Systems/Implementations/RenderingSystemTemple.h"
#include "ECS/Systems/Implementations/SkySystem.h"
#include "ECS/Systems/Implementations/SnowfallSystem.h"
#include "ECS/Systems/Implementations/TurnCheerSystem.h"
#include "ECS/Systems/Implementations/VillageLightSystem.h"
#include "ECS/Systems/Implementations/WaterRingSystem.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "EngineConfig.h"
#include "Graphics/BgfxGraphicsDevice.h"
#include "Graphics/RendererInterface.h"
#include "Input/SdlInputDevice.h"
#include "Locator.h"
#include "Visual/UploadMode.h"
#include "Windowing/Sdl2WindowingSystem.h"

using namespace openblack::audio;
using openblack::debug::gui::DebugGuiInterface;
using openblack::ecs::systems::AbodeSoundSystem;
using openblack::ecs::systems::AdvisorVoiceSystem;
using openblack::ecs::systems::CloudSystem;
using openblack::ecs::systems::CreatureHairSystem;
using openblack::ecs::systems::FloatingNumberSystem;
using openblack::ecs::systems::IncomingMessageSystem;
using openblack::ecs::systems::LandShadeSystem;
using openblack::ecs::systems::LiquidParticleSystem;
using openblack::ecs::systems::ParticleSystem;
using openblack::ecs::systems::PauseTextSystem;
using openblack::ecs::systems::RainSystem;
using openblack::ecs::systems::RenderingSystem;
using openblack::ecs::systems::RenderingSystemTemple;
using openblack::ecs::systems::SnowfallSystem;
using openblack::ecs::systems::TurnCheerSystem;
using openblack::ecs::systems::VillageLightSystem;
using openblack::ecs::systems::WaterRingSystem;
using openblack::graphics::RendererInterface;
using openblack::windowing::DisplayMode;
using openblack::windowing::Sdl2WindowingSystem;

void openblack::InitializeWindow(const std::string& title, int width, int height, DisplayMode displayMode, uint32_t extraFlags,
                                 bool background)
{
	Locator::windowing::emplace<Sdl2WindowingSystem>(title, width, height, displayMode, extraFlags, background);
}

bool openblack::InitializeEngine(GraphicsBackend backend, bool vsync) noexcept
{
	// The keyboard, the mouse and the event queue of the window system. The world reads them through this device.
	Locator::inputDevice::emplace<input::SdlInputDevice>();
	// The device goes first: the renderer makes its buffers and textures on it. Its service is made before the services of
	// the resources, so that it stays until the last texture and buffer of the resources is deleted.
	Locator::graphicsDevice::emplace<graphics::BgfxGraphicsDevice>();
	EmplaceRenderer(RendererInterface::Create(backend, vsync));
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

void openblack::InitializePresentation()
{
	// The snow and the rain that fall in the view. They draw local numbers when they start, so they stay first and in this
	// order.
	Locator::snowfallSystem::emplace<SnowfallSystem>();
	Locator::rainSystem::emplace<RainSystem>();
	// The hair of the creatures, which swings from their bodies in the frames
	Locator::creatureHairSystem::emplace<CreatureHairSystem>();
	// The sounds of the abodes near the camera, the shade of the land, the text of the pause and the strip of messages
	Locator::abodeSoundSystem::emplace<AbodeSoundSystem>();
	Locator::landShadeSystem::emplace<LandShadeSystem>();
	Locator::pauseTextSystem::emplace<PauseTextSystem>();
	Locator::incomingMessageSystem::emplace<IncomingMessageSystem>([]() { return machine_clock::Ticks(); });
	// The sea, the sky, the clouds and the village lights, which the turns and the scripts do not read
	Locator::oceanSystem::emplace<Ocean>(graphics::ConfiguredUploads());
	Locator::skySystem::emplace<ecs::systems::SkySystem>();
	Locator::cloudSystem::emplace<CloudSystem>();
	Locator::villageLightSystem::emplace<VillageLightSystem>();
	// The sound, the gestures and the lip sync of the advisors' lines. The scripts time the lines on their own clock.
	Locator::advisorVoiceSystem::emplace<AdvisorVoiceSystem>();
	// The measure of how fast this computer's camera turns, and the advisors' cheers at it
	Locator::turnCheerSystem::emplace<TurnCheerSystem>();
	// The draw of the particle effects, which reads the effects from their rules
	Locator::particleSystem::emplace<ParticleSystem>(
	    []() -> const ecs::systems::ParticleRuleSystemInterface* {
		    return Locator::particleRuleSystem::has_value() ? &Locator::particleRuleSystem::value() : nullptr;
	    },
	    &ParticleSystem::TextureStemsOnDisk);
	// The rings on the water, the drops and the dust, and the numbers that float up. The turns ask for them, but they
	// keep only what this computer shows, so the turns make no change when they are not there.
	Locator::waterRingSystem::emplace<WaterRingSystem>();
	Locator::liquidParticleSystem::emplace<LiquidParticleSystem>();
	Locator::floatingNumberSystem::emplace<FloatingNumberSystem>();
}

void openblack::InitializeClient(const player_profiles::Store& profiles)
{
	// The input of the player, the drawing of the world, the editor, the videos and the online services
	InitializePlayerInput();
	Locator::rendereringSystem::emplace<RenderingSystem>();
	// The temple makes the draw of its rooms, and the draw of the island again, with these functions
	if (Locator::temple::has_value())
	{
		Locator::temple::value().SetDrawMakers({
		    .temple = [] { Locator::rendereringSystem::emplace<RenderingSystemTemple>(); },
		    .island = [] { Locator::rendereringSystem::emplace<RenderingSystem>(); },
		});
	}
	InitializeOnlineAndTools(profiles);
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
