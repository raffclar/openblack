/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The steps of the game's frame: the turns, the interface, the entities, the screen marks, the labels, the temple and
// the draws before the first frame

#include <cstdlib>

#include <algorithm>
#include <chrono>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "3D/DayNightClock.h"
#include "3D/HandAnimation.h"
#include "3D/HandMorph.h"
#include "3D/L3DMesh.h"
#include "3D/TempleInteriorInterface.h"
#include "3D/WaterRings.h"
#include "Audio/AtmosAudio.h"
#include "Audio/AudioManagerInterface.h"
#include "CHLApi.h"
#include "Camera/Camera.h"
#include "Camera/CreatureCameraModel.h"
#include "Camera/ScriptCameraModel.h"
#include "Common/BuildVersion.h"
#include "Game.h"
#include "GameDetail.h"
#include "Sound/GameMusic.h"
#include "Sound/GameSoundEffects.h"
#include "Sound/HelpSpeech.h"
#include "Sound/ScriptSoundEffect.h"
#if defined(OPENBLACK_DEVELOPER_TOOLS)
#include "Common/DrawTrace.h"
#endif
#include "Common/GameLanguage.h"
#include "Common/GameRandom.h"
#include "Common/MachineClock.h"
#include "Common/MainThreadQueue.h"
#include "Common/RandomNumberManager.h"
#include "Common/SideThread.h"
#include "Creature/CreatureFightHud.h"
#include "Debug/DebugGuiInterface.h"
#include "Debug/FrameStatsLog.h"
#include "ECS/Archetypes/SkyArchetype.h"
#include "ECS/ChallengePictures.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/FloatingNumber.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/LearningMarker.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/FloatingNumber.h"
#include "ECS/GamePlayers.h"
#include "ECS/LearningMarker.h"
#include "ECS/Map.h"
#include "ECS/OtherPlayerSparkles.h"
#include "ECS/ProfileAlignment.h"
#include "ECS/Registry.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/AbodeSoundSystemInterface.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/AnimatedStaticSystemInterface.h"
#include "ECS/Systems/BoatLaunchSystemInterface.h"
#include "ECS/Systems/CameraBookmarkSystemInterface.h"
#include "ECS/Systems/CameraPathSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureFizzSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/CreatureWebPageSystemInterface.h"
#include "ECS/Systems/DesignedWaterfallSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/ExplosionSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/FloatingNumberSystemInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/HighDetailSystemInterface.h"
#include "ECS/Systems/LandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/LiquidParticleSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "ECS/Systems/NetworkGameSystemInterface.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PauseTextSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/PuzzleTotemSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "ECS/Systems/RewardSystemInterface.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "ECS/Systems/ScriptControlSystemInterface.h"
#include "ECS/Systems/ScriptHighlightSystemInterface.h"
#include "ECS/Systems/ScriptMusicSystemInterface.h"
#include "ECS/Systems/SnowSystemInterface.h"
#include "ECS/Systems/TeleportSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TornadoSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/Systems/TurnCheerSystemInterface.h"
#include "ECS/Systems/VideoSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/Systems/VortexSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/TownDesireFlags.h"
#include "ECS/TreeRoots.h"
#include "ECS/VillageTotem.h"
#include "ECS/VillagerLabelSource.h"
#include "EngineConfig.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/RendererInterface.h"
#include "Graphics/WarmUpViews.h"
#include "Gui/Canvas.h"
#include "Gui/GameInterface.h"
#include "Gui/HandToolTips.h"
#include "Gui/MultiplayerFrontEnd.h"
#include "Gui/PingDisplay.h"
#include "Gui/StartupRules.h"
#include "Gui/WorldToolTipRules.h"
#include "Gui/WorldToolTips.h"
#include "Hand/HandBlend.h"
#include "Hand/HandGrabRules.h"
#include "Hand/HandTrace.h"
#include "Help/DialogueText.h"
#include "Help/HelpProfile.h"
#include "Help/SingAlong.h"
#include "Help/Spirits.h"
#include "Help/TurnCheer.h"
#include "Input/GameActionMapInterface.h"
#include "Locator.h"
#include "Magic/TownBelief.h"
#include "Online/HttpClientInterface.h"
#include "Online/NetworkSession.h"
#include "Particles/LiquidParticles.h"
#include "Profiler.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"
#include "Serializer/FotFile.h"
#include "Simulation/ChecksumLog.h"
#include "Simulation/CommandLog.h"
#include "Simulation/FrameBody.h"
#include "Simulation/ProfileMenu.h"
#include "Simulation/TurnBody.h"
#include "Simulation/WorldStart.h"

#if defined(__EMSCRIPTEN__)
#include "Web/WebPlatform.h"
#endif

using namespace openblack;
using namespace openblack::game_detail;

namespace
{

/// The player of this computer is in the temple. In a network game the world's turns go on there, and their audio
/// plays the temple's sounds.
bool PlayerInTemple()
{
	return Locator::temple::has_value() && Locator::temple::value().Active();
}

} // namespace

bool Game::PlayFrameStep(game_frame::Step step, FrameState& frame)
{
	using game_frame::Step;
	// While the window is minimised the turns, the keys and the film's clock continue, but the camera, the start of the draw,
	// the hand and the world's draw stop
	if (!game_frame::DoesStep(step, FrameGraphicsWindow()))
	{
		return true;
	}
	switch (step)
	{
	case Step::FrameClock:
		StartFrame(frame);
		break;
	case Step::Input:
		UpdateInput();
		break;
	case Step::KeyCommands:
		SubmitKeyCommands();
		break;
	case Step::ScreenMarks:
		UpdateScreenMarks();
		break;
	case Step::DebugGui:
		return UpdateDebugGui();
	case Step::Interface:
		UpdateInterfaceFrame(frame.deltaTime);
		break;
	case Step::Turns:
	{
		// A scenario asked for is laid out on a fresh testbed before the turns, as a load is
		if (_scenarioStager && _scenarioRequest.has_value())
		{
			_scenarioStager();
		}
		// The turns of this frame played beside the draw of the last frame (deviation row 153)
		if (_frameLoop != nullptr && std::exchange(_frameLoop->turnsPlayedBeside, false))
		{
			return true;
		}
		return PlayTurns();
	}
	case Step::SendCommands:
		// A tool's run that plays the commands of a file does no command of its own player
		if (_simulation.has_value() && _simulation->playCommandsPath.has_value())
		{
			Locator::playerCommandSystem::value().Reset();
			if (Locator::helpProfileSystem::has_value())
			{
				std::ignore = Locator::helpProfileSystem::value().MakeReport(false, std::nullopt, false);
			}
			std::ignore = TakeDialogueClick();
			break;
		}
		// A tool's run can keep the camera out of the commands, so that two runs give the same commands
		if (!_simulation.has_value() || !_simulation->settings.noHandReports)
		{
			ReportHand();
		}
		ReportHelp();
		SendDialogueClick();
		Locator::playerCommandSystem::value().SendOpenBlock();
		break;
	case Step::FrameTime:
		// The frame's game time: none while paused, quicker or slower with the game speed
		Locator::time::value().UpdateFrame();
		// The steps of a sixtieth of a second that the hand, the strokes, the throws and the camera take in this frame
		Locator::time::value().StepInput(InputStepTime(frame.deltaTime));
		// The camera reads the mouse of the frame divided across these steps
		Locator::gameActionSystem::value().SplitInput(Locator::time::value().GetInputSteps());
		break;
	case Step::FilmClock:
		// The clock of a full-screen video counts the frames that are due by the real clock
		Locator::videoSystem::value().CountDueFrames(std::chrono::steady_clock::now());
		break;
	case Step::DrawClock:
		// The real time since the last draw, which the steps of the draw go by
		Locator::time::value().StartDraw();
		break;
	case Step::Video:
		// A full-screen video shows the frames that are due, fades, and ends
		Locator::videoSystem::value().Update(std::chrono::steady_clock::now());
		break;
	case Step::PauseText:
		// The words of the pause and the speed, as the draw finishes
		UpdatePauseText();
		break;
	case Step::ScreenFade:
		// The colour over the screen fades by the real time of the draw
		if (Locator::gameInterface::has_value())
		{
			const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
			Locator::gameInterface::value().GetScreenFade().Update(
			    static_cast<uint32_t>(Locator::time::value().GetFrameRealTime().count()), inTemple);
		}
		break;
	case Step::CameraMouse:
		// The camera reads the movement of the mouse and of the hand of this frame as it moves, after the turns
		// The first step of the input reads its part of the mouse. A frame with no step keeps the mouse for the next step.
		if (const auto& steps = Locator::time::value().GetInputSteps();
		    steps.count > 0 && !Locator::cameraPathSystem::value().HoldsCamera())
		{
			auto& actions = Locator::gameActionSystem::value();
			actions.UseInputStep(0);
			Locator::camera::value().HandleActions(std::chrono::duration_cast<std::chrono::microseconds>(steps.Time(0)));
			actions.UseInputStep(std::nullopt);
		}
		break;
	case Step::CameraPaths:
		Locator::cameraPathSystem::value().Update(frame.deltaTime, Locator::time::value().GetFrameGameTime());
		break;
	case Step::CameraControllers:
		UpdateCameraControllers(frame.deltaTime);
		break;
	case Step::Camera:
		UpdateCamera();
		break;
	case Step::CameraShake:
	{
		// The blasts' rubble lies and fades, their dust flies and the camera shakes
		auto explosions = Locator::profiler::value().BeginScoped(Profiler::Stage::ExplosionUpdate);
		Locator::explosionSystem::value().Update(
		    std::chrono::duration<float, std::milli>(Locator::time::value().GetFrameGameTime()).count());
		// The camera keeps this view, with the shake of this frame, for its rays to the land on the next frame. A command
		// of this frame that puts the camera does not change the view of these rays.
		Locator::camera::value().KeepDrawnView();
		break;
	}
	case Step::Temple:
		UpdateTemple(frame.deltaTime);
		break;
	case Step::Bookmarks:
	{
		// The markers of the bookmarks throb on, only on a frame that draws the land
		const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
		const bool landHidden = Locator::videoSystem::has_value() &&
		                        (Locator::videoSystem::value().CoversScreen() || Locator::videoSystem::value().HidesWorld());
		Locator::cameraBookmarkSystem::value().Update(Locator::time::value().GetFrameGameTime(), !inTemple && !landHidden);
		break;
	}
	case Step::HighDetail:
		// The villagers a script draws in high detail are drawn as usual once its cinema bars are gone
		Locator::highDetailSystem::value().Update();
		break;
	case Step::CreaturePoses:
		UpdateCreaturePoses();
		break;
	case Step::FrameEffects:
		UpdateFrameEffects();
		break;
	case Step::Hand:
		UpdateHandFrame(frame.deltaTime);
		break;
	case Step::Entities:
		UpdateEntitiesFrame(frame.deltaTime);
		break;
	case Step::Audio:
	{
		// The frame stats show the time of the audio, with the music, as a stage of its own
		auto audio = Locator::profiler::value().BeginScoped(Profiler::Stage::UpdateAudio);
		Locator::audio::value().Update();
		break;
	}
	case Step::Online:
		UpdateOnline();
		FollowGameEnd();
		TellLeaves();
		break;
	case Step::CameraKeys:
		UpdateCameraKeys(frame.deltaTime);
		break;
	case Step::ProfileAlignment:
		Locator::landSystem::value().KeepAlignmentInProfile();
		break;
	case Step::_Count:
		break;
	}
	return true;
}

bool Game::PlayTurns()
{
	// The game is the view of the turns
	Locator::landSystem::value().PlayTurns(*this);
	return true;
}

bool Game::StartTurnsBesideDraw()
{
	if (!_turnBesideDraw || _frameLoop == nullptr || !Locator::mainThreadQueue::has_value())
	{
		return false;
	}
	// The turns start early only when the end of this frame changes nothing that they read: no scenario waits to be laid
	// out, the player does not go into the temple after this frame, no picture waits for its screen and the game does not
	// close. Else they play in the next frame, as usual.
	const bool templeEntry = Locator::temple::has_value() && Locator::temple::value().GoesInAfterFrame();
	const bool pictureWaits =
	    ecs::challenge_pictures::AnyWaiting(Locator::entitiesRegistry::value()) ||
	    (Locator::creatureWebPageSystem::has_value() && Locator::creatureWebPageSystem::value().AnyPictureWaiting()) ||
	    (Locator::saveGameSystem::has_value() && Locator::saveGameSystem::value().AnyPictureWaiting());
	// A quick load waits for the end of this frame, and the next turns play in the world that it loads
	const bool loadAsked = Locator::saveGameSystem::has_value() && Locator::saveGameSystem::value().LoadAsked();
	if (_scenarioRequest.has_value() || templeEntry || pictureWaits || loadAsked || _closeAsked)
	{
		return false;
	}
	auto& loop = *_frameLoop;
	if (!loop.turnThread)
	{
		loop.turnThread = std::make_unique<SideThread>();
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The turns play on their own thread, beside the end of the draw of the frame "
		                                        "before (deviation row 153)");
	}
	// The turns read the camera of the game, not the camera that the inspector shows
	ShowInspectorCamera(false);
	// The clock of the next frame moves on now, so that the turns that are due in the next frame play now. The statistics
	// of this frame keep the clock of this frame.
	loop.statisticsTicks = machine_clock::Ticks();
	auto& time = Locator::time::value();
	time.Update();
	loop.turnsPlayedBeside = true;
	loop.turnBefore = time.GetTurn();
	loop.turnsStart = std::chrono::steady_clock::now();
	auto& queue = Locator::mainThreadQueue::value();
	loop.turnsElsewhere.emplace(queue);
	loop.turnThread->Start(
	    [this, &loop] {
		    Locator::landSystem::value().PlayTurnSteps(*this);
		    loop.turnsEnd = std::chrono::steady_clock::now();
	    },
	    [&queue] { queue.Wake(); });
	return true;
}

void Game::FinishTurnsBesideDraw()
{
	auto& loop = *_frameLoop;
	auto& queue = Locator::mainThreadQueue::value();
	// Meanwhile the main thread does the calls that the turns wait for, such as the load of the next land
	queue.ServeUntil([&loop] { return loop.turnThread->Done(); });
	loop.turnsElsewhere.reset();
	try
	{
		loop.turnThread->Wait();
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "The turns beside the draw stopped: {}", e.what());
		std::abort();
	}
	Locator::landSystem::value().FinishTurns(loop.turnBefore, loop.turnsStart, loop.turnsEnd);
	// The scopes of the turns go into this frame's entry of the profiler
	Locator::profiler::value().MergeThreads();
}

void Game::WarmUpFirstFrame()
{
	const auto& config = Locator::config::value();
	if (!config.warmUpFirstFrame || !Locator::rendererInterface::has_value() || !Locator::rendereringSystem::has_value() ||
	    !Locator::camera::has_value())
	{
		return;
	}
	auto& renderer = Locator::rendererInterface::value();
	auto& rendering = Locator::rendereringSystem::value();
	const auto start = std::chrono::steady_clock::now();
	// Some renderers divide the warm-up between some frames, each with a part of the draws
	bool shown = false;
	const auto warmUp = [&renderer, &shown, this](const graphics::RendererInterface::DrawSceneDesc& desc) {
		bool partsLeft = true;
		for (uint32_t part = 0; partsLeft; ++part)
		{
			partsLeft = false;
			const bool partShown = RenderLoadingFrame(true, [&renderer, &desc, &partsLeft, part]() {
				partsLeft = renderer.WarmUpScene(desc, part);
				// The interface draws over the scene too, and the cinema bars are its first draw
				gui::Canvas::WarmUp(graphics::RenderPass::CinemaBars);
				// The villagers' labels and the tip bubble test the depth of the scene, sorted among its see-through
				// things outside the temple
				gui::Canvas::WarmUp(graphics::RenderPass::WorldLabels, gui::Canvas::DepthTest::NearerIsGreater);
				gui::Canvas::WarmUp(graphics::RenderPass::Translucent, gui::Canvas::DepthTest::NearerIsGreater, true);
				// The interface that the game draws over the scene, which the loading screen does not blend in each way
				gui::Canvas::WarmUp(graphics::RenderPass::Interface);
				// The debug menu comes up with the game's menu, over all of the game
				if (Locator::debugGui::has_value())
				{
					Locator::debugGui::value().WarmUp();
				}
			});
			shown = shown || partShown;
		}
	};
	const auto warmUpAll = [&config, &rendering, &warmUp, this]() {
		// The draw lists of the land's things, as the first frame makes them
		if (config.drawEntities)
		{
			rendering.PrepareDraw(config.drawBoundingBoxes, config.drawFootpaths, config.drawStreams,
			                      ecs::systems::DrawUse::WarmUp);
		}
		warmUp(SceneDrawDesc(config, 0, IsHandDrawn()));
		// The temple's rooms show only while the player is in the temple. A visit draws them with the temple's own
		// pipelines and targets, thus the warm-up draws a visit too. The temple draws no island and no sea.
		if (Locator::temple::has_value())
		{
			Locator::temple::value().WarmUpVisit([&config, &rendering, &warmUp, this]() {
				if (config.drawEntities)
				{
					rendering.PrepareDraw(config.drawBoundingBoxes, config.drawFootpaths, config.drawStreams,
					                      ecs::systems::DrawUse::WarmUp);
				}
				auto desc = SceneDrawDesc(config, 0, IsHandDrawn());
				desc.drawIsland = false;
				desc.drawWater = false;
				warmUp(desc);
			});
		}
	};
	// Some renderers can make the pipelines of the warm-up in the background, many at the same time. A first pass of
	// the warm-up starts them, and the loading screen shows while the renderer makes them. The second pass then gets
	// the ready pipelines and draws all of the warm-up.
	if (renderer.SetPipelinePrefetch(true))
	{
		warmUpAll();
		renderer.SetPipelinePrefetch(false);
		const auto waitStart = std::chrono::steady_clock::now();
		uint32_t framesAfter = 0;
		const auto waited = [&waitStart]() {
			return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - waitStart);
		};
		while (graphics::warm_up::PrefetchWaits(framesAfter, renderer.GetPipelinePrefetchPending(), waited()) &&
		       !_quitRequested && RenderLoadingFrame(true))
		{
			++framesAfter;
			std::this_thread::sleep_for(graphics::warm_up::k_PrefetchFrameInterval);
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Load: the pipelines of the warm-up made in the background in {} ms, {} not ready", waited().count(),
		                   renderer.GetPipelinePrefetchPending());
	}
	warmUpAll();
	// The first frame makes the draw lists again from what the game's first update changed
	if (config.drawEntities)
	{
		rendering.SetLayoutDirty();
	}
	if (shown)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Load: the first frame drawn behind the loading screen in {:.1f} ms",
		                   std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
	}
}

void Game::StartLandPipelines()
{
	const auto& config = Locator::config::value();
	if (!config.warmUpFirstFrame || !Locator::rendererInterface::has_value() || !Locator::camera::has_value())
	{
		return;
	}
	auto& renderer = Locator::rendererInterface::value();
	if (!renderer.SetPipelinePrefetch(true))
	{
		return;
	}
	// The pipelines include the formats of the targets, which follow the window's size
	SizeSceneToWindow();
	// Only the land, the sky and the sea: the things on the land need resources that are not loaded yet
	auto desc = SceneDrawDesc(config, 0, false);
	desc.drawEntities = false;
	desc.drawSprites = false;
	desc.drawVegetation = false;
	desc.drawBoundingBoxes = false;
	RenderLoadingFrame(true, [&renderer, &desc]() { [[maybe_unused]] const bool partsLeft = renderer.WarmUpScene(desc, 0); });
	renderer.SetPipelinePrefetch(false);
}

game_frame::GraphicsWindow Game::FrameGraphicsWindow() const
{
	game_frame::GraphicsWindow window {
	    .minimised = Locator::windowing::has_value() && Locator::windowing::value().IsMinimised(),
	    .fixedFrameTime = Locator::time::has_value() && Locator::time::value().GetFixedFrameTime().has_value(),
	};
	if (window.minimised)
	{
		// The pictures that the debug inspector, a challenge, the creature's page or a saved game wait for
		window.pictureWanted =
		    (_requestScreenshot.has_value() && _requestScreenshot->first == _frameCount) ||
		    ecs::challenge_pictures::AnyWaiting(Locator::entitiesRegistry::value()) ||
		    (Locator::creatureWebPageSystem::has_value() && Locator::creatureWebPageSystem::value().AnyPictureWaiting()) ||
		    (Locator::saveGameSystem::has_value() && Locator::saveGameSystem::value().AnyPictureWaiting());
	}
	return window;
}

bool Game::DoesFrameGraphics() const
{
	const auto window = FrameGraphicsWindow();
	return game_frame::DoesGraphics(window.minimised, window.pictureWanted, window.fixedFrameTime);
}

void Game::UpdateFloatingNumbers(float seconds)
{
	if (Locator::floatingNumberSystem::has_value())
	{
		Locator::floatingNumberSystem::value().Step(seconds);
	}
}

void Game::UpdateLearningMarks(float milliseconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	ecs::learning_marker::StepAll(registry, milliseconds);
	if (!Locator::particleSystem::has_value())
	{
		return;
	}
	// The bulbs and glows are drawn with the world's effects
	std::vector<particles::draw::LearningMark> marks;
	registry.Each<const ecs::components::LearningMarker>(
	    [&marks](entt::entity /*unused*/, const ecs::components::LearningMarker& marker) {
		    const auto look = ecs::learning_marker::LookOf(marker);
		    marks.push_back({.glow = look.glow,
		                     .glowHalfSize = look.glowHalfSize,
		                     .glowColour = look.glowColour,
		                     .bulb = look.bulb,
		                     .bulbScale = look.bulbScale,
		                     .bulbColour = look.bulbColour,
		                     .bulbMesh = resources::HashIdentifier(MeshId::ObjectLightBulb)});
	    });
	Locator::particleSystem::value().SetLearningMarks(std::move(marks));
}

void Game::PlaceLearningTexts()
{
	if (!Locator::gameInterface::has_value())
	{
		return;
	}
	std::vector<gui::GameInterface::LearningText> texts;
	Locator::entitiesRegistry::value().Each<const ecs::components::LearningMarker>(
	    [this, &texts](entt::entity /*unused*/, const ecs::components::LearningMarker& marker) {
		    const auto look = ecs::learning_marker::LookOf(marker);
		    if (marker.text.empty())
		    {
			    return;
		    }
		    if (const auto screen = OnScreen(look.textPoint); screen.has_value())
		    {
			    texts.push_back({.screen = *screen, .text = gui::ToUtf16(marker.text), .alpha = look.textAlpha});
		    }
	    });
	Locator::gameInterface::value().SetLearningTexts(std::move(texts));
}

void Game::PlaceScreenLabels()
{
	if (!Locator::gameInterface::has_value())
	{
		return;
	}
	// Those on the screen, the furthest first so the nearer are drawn over them
	struct Placed
	{
		float distance;
		gui::GameInterface::FloatingNumber number;
	};
	std::vector<Placed> placed;
	const auto eye = Locator::camera::value().GetOrigin();
	const auto floating = Locator::floatingNumberSystem::has_value() ? Locator::floatingNumberSystem::value().Numbers()
	                                                                 : std::span<const ecs::components::FloatingNumber> {};
	std::ranges::for_each(floating, [this, &placed, eye](const ecs::components::FloatingNumber& number) {
		const auto alpha = ecs::floating_number::Alpha(number.life);
		const auto screen = OnScreen(number.position);
		if (!alpha.has_value() || !screen.has_value())
		{
			return;
		}
		placed.push_back(
		    {.distance = glm::distance(eye, number.position),
		     .number = {.screen = *screen, .text = gui::ToUtf16(number.text), .colour = number.colour, .alpha = *alpha}});
	});
	std::ranges::sort(placed, std::ranges::greater {}, &Placed::distance);
	std::vector<gui::GameInterface::FloatingNumber> numbers;
	numbers.reserve(placed.size());
	for (auto& each : placed)
	{
		numbers.push_back(std::move(each.number));
	}
	Locator::gameInterface::value().SetFloatingNumbers(std::move(numbers));

	if (!Locator::windowing::has_value())
	{
		return;
	}
	// Inside the temple the land's villagers aren't drawn, nor their labels
	if (Locator::temple::has_value() && Locator::temple::value().Active())
	{
		Locator::gameInterface::value().SetVillagerLabels({});
		return;
	}
	Locator::gameInterface::value().SetVillagerLabels(ecs::villager_label_source::Gather({
	    .screen = Locator::windowing::value().GetSize(),
	    .nameFade = _villagerNameFadeDrawn,
	    .texts = &Locator::gameInterface::value().GetTexts(),
	}));
}

void Game::UpdateVillagerLabels(float milliseconds)
{
	// The names display changes size only on a frame that draws the land
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	const bool landHidden = Locator::videoSystem::has_value() &&
	                        (Locator::videoSystem::value().CoversScreen() || Locator::videoSystem::value().HidesWorld());
	const auto frame = gui::villager_labels::StepNameFadeFrame(_villagerNameFade,
	                                                           Locator::entitiesRegistry::value().Context().villagerNamesShown,
	                                                           milliseconds, !inTemple && !landHidden);
	_villagerNameFadeDrawn = frame.drawn;
	_villagerNameFade = frame.next;
}

void Game::ShowBeliefGained()
{
	const auto shown = Locator::reactionSystem::has_value() ? Locator::reactionSystem::value().TakeGainShown(LocalInputPlayer())
	                                                        : std::nullopt;
	if (!shown.has_value())
	{
		return;
	}
	// The hand shows the belief gained at once, over anything else
	if (Locator::gameInterface::has_value())
	{
		Locator::gameInterface::value().GetToolTips().Force(gui::world_tooltip::k_GotBelief, *shown);
	}
	// The number also floats up from the thing under the hand, or else from the point on the land under the hand
	if (!Locator::pickingSystem::has_value() || !Locator::floatingNumberSystem::has_value())
	{
		return;
	}
	const auto pick = Locator::pickingSystem::value().GetPick();
	std::optional<glm::vec3> place = pick.point;
	const auto& registry = Locator::entitiesRegistry::value();
	if (pick.object.has_value() && registry.Valid(*pick.object))
	{
		if (const auto* transform = registry.TryGet<const ecs::components::Transform>(*pick.object))
		{
			place = transform->position;
		}
	}
	if (place.has_value())
	{
		namespace belief = magic::town_belief;
		Locator::floatingNumberSystem::value().AddWide(*place, *shown, belief::k_GainShownColour, belief::k_GainShownWidth);
	}
}

void Game::ProcessTurnCheering()
{
	// While the turning lesson asks for it, the advisors cheer the player's turning of the camera on
	if (!Locator::turnCheerSystem::has_value() || !Locator::gameRandom::has_value() || !Locator::chlapi::has_value())
	{
		return;
	}
	auto& random = Locator::gameRandom::value();
	const help::turn_cheer::Random draws {
	    .upTo = [&random](float x) { return random.LocalFloatRand(x); },
	    .below = [&random](int32_t n) { return random.LocalRand(n); },
	};
	if (const auto line = Locator::turnCheerSystem::value().ProcessTurn(GetTurn(), draws))
	{
		Locator::chlapi::value().PlayBankSoundEffect(static_cast<int32_t>(audio::ScriptSoundBank::HelpSprites), *line, {});
	}
}

void Game::PlayViewStep(game_turn::Step step, glm::vec3 cameraPosition)
{
	using game_turn::Step;
	auto& clock = Locator::time::value();
	switch (step)
	{
	case Step::TurnCheering:
		ProcessTurnCheering();
		break;
	case Step::HandToolTips:
		ProcessHandToolTipTurn();
		break;
	case Step::PlayerSparkles:
		ecs::other_player_sparkles::AddAtCameras();
		break;
	case Step::BeliefGained:
		ShowBeliefGained();
		break;
	case Step::Ambience:
		// The ambience follows the weather at the camera
		if (_atmosAudio)
		{
			ecs::components::WeatherInfo weather {};
			if (Locator::weatherSystem::has_value())
			{
				weather = Locator::weatherSystem::value().GetWeatherSmooth(cameraPosition);
			}
			_atmosAudio->SetAlignment(Locator::alignmentSystem::value().GetCameraAlignment());
			_atmosAudio->EndTurn({
			    .camera = cameraPosition,
			    .weather =
			        {
			            .rain = weather.rain,
			            .snow = weather.snow,
			            .windX = weather.windX,
			            .windZ = weather.windZ,
			        },
			    .paused = false,
			    .turn = clock.GetTurn(),
			    .inCitadel = PlayerInTemple(),
			    .videoPlaying = Locator::videoSystem::value().IsPlaying(),
			});
		}
		break;
	case Step::Music:
		ProcessMusicTurn(cameraPosition, PlayerInTemple());
		break;
	default:
		// The world does the other steps (see simulation::PlayTurnStep)
		break;
	}
}

void Game::UpdatePauseText()
{
	if (!Locator::pauseTextSystem::has_value() || !Locator::gameInterface::has_value())
	{
		return;
	}
	const auto& clock = Locator::time::value();
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	const bool scriptHasSpeed =
	    Locator::scriptControlSystem::has_value() && Locator::scriptControlSystem::value().GetGameSpeedOwner() != 0;
	Locator::pauseTextSystem::value().Update(
	    {
	        .paused = clock.IsPaused(),
	        .speed = clock.GetSpeed(),
	        .started = true,
	        .insideTemple = inTemple,
	        .scriptHasSpeed = scriptHasSpeed,
	        .editing = false,
	        .videoPlaying = Locator::videoSystem::value().IsPlaying(),
	        .boxWithBackground = Locator::gameInterface::value().IsBoxWithBackgroundActive(),
	    },
	    static_cast<uint32_t>(clock.GetFrameRealTime().count()));
}

void Game::ProcessTempleAudioTurn()
{
	// The temple's turns play the citadel's music, while the land's ambience fades out
	ProcessMusicTurn(Locator::camera::value().GetOrigin(), true);
	if (_atmosAudio)
	{
		_atmosAudio->ContinueTurn(
		    {.paused = false, .turn = GetTurn(), .inCitadel = true, .videoPlaying = Locator::videoSystem::value().IsPlaying()});
	}
}

void Game::UpdateTemple(std::chrono::microseconds deltaTime)
{
	// The temple's camera may have taken the player out of the temple
	if (Locator::temple::has_value())
	{
		Locator::temple::value().Update(deltaTime);
		// When the player leaves the save game room after the game is over, the game stops. The program closes. It
		// does not go back to a front end. Because the game is over, it does not write an autosave and does not draw
		// black frames before it closes.
		if (Locator::temple::value().TakeGameEnded())
		{
			RequestQuit();
		}
	}
}

void Game::UpdateInterfaceFrame(std::chrono::microseconds deltaTime)
{
	// A release of a creature that waited for the hold of the world lets go now, before the turns
	if (Locator::creatureHandSystem::has_value())
	{
		Locator::creatureHandSystem::value().InterfaceFrame();
	}
	// The interface reads the places of the pointer that came in, and the help counts each movement of the mouse
	if (Locator::gameActionSystem::has_value())
	{
		const auto moves = Locator::gameActionSystem::value().TakeMouseMoves();
		for (uint32_t i = 0; i < moves && Locator::helpProfileSystem::has_value(); ++i)
		{
			Locator::helpProfileSystem::value().Report(help::profile::k_MouseMoved);
		}
	}
	if (Locator::gameInterface::has_value())
	{
		// The fighters' health and stamina while a fight is on, wanted afresh every frame whether or not the hand is out
		const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
		const bool scriptHasCamera =
		    Locator::scriptControlSystem::has_value() && Locator::scriptControlSystem::value().GetCameraOwner() != 0;
		Locator::gameInterface::value().SetFightPanel(creature_fight_hud::PanelAllowed(inTemple, scriptHasCamera)
		                                                  ? Locator::creatureFightSystem::value().GetPanel()
		                                                  : std::nullopt);
		// The player's own ping, with --show-ping in a network game (deviation 143)
		std::optional<uint64_t> roundTrip = _demoRoundTrip;
		if (!roundTrip.has_value() && Locator::config::value().showPing && Locator::networkGame::has_value() &&
		    Locator::networkGame::value().InGame())
		{
			const auto* session = Locator::networkGame::value().Session();
			roundTrip = session != nullptr ? session->RoundTrip() : std::nullopt;
		}
		Locator::gameInterface::value().SetPingLine(gui::ping_display::Text(roundTrip));
		Locator::gameInterface::value().Update(std::chrono::duration_cast<std::chrono::duration<float>>(deltaTime).count());
		HandleInterfaceAction();
		FollowPlaygroundTrip();
		UpdateTattooMenu(std::chrono::duration<float, std::milli>(deltaTime).count());
#if defined(__EMSCRIPTEN__)
		if (web::TakeJoinOnlineRequest())
		{
			RequestJoinOnline();
		}
#endif
		AnswerJoinOnline();
		UpdateMultiplayerFrontEnd();
		if (const auto end = std::exchange(_gameEndAsked, std::nullopt); end.has_value())
		{
			EndGame(*end);
		}
		FollowLandInfoBox();
		StartNextGameWhenSaved();
		FollowSkirmishBox();
	}
}

void Game::ReportTurnApiCheck() const
{
	if (!Locator::mainThreadQueue::has_value() || _turnApiCheck == TurnApiCheck::Off)
	{
		return;
	}
	const auto calls = Locator::mainThreadQueue::value().GetDirectCalls();
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The check of the turn's calls found {} places that use a service directly",
	                   calls.size());
	for (const auto& call : calls)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Turn check: step {} {} {} {} calls ({})",
		                   call.turnStep.has_value() ? std::to_string(*call.turnStep) : std::string("?"), Name(call.api),
		                   call.what, call.count, call.where);
	}
}

void Game::UpdateCreaturePoses()
{
	simulation::PlayCreaturePoses();
}

void Game::UpdateScreenMarks()
{
	// The numbers and the marks rise and fade only on a frame that draws the land, by the game time of the frame. They
	// stand still while the game pauses.
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	const bool landHidden = Locator::videoSystem::has_value() &&
	                        (Locator::videoSystem::value().CoversScreen() || Locator::videoSystem::value().HidesWorld());
	if (inTemple || landHidden)
	{
		return;
	}
	const auto milliseconds = static_cast<float>(Locator::time::value().GetFrameGameTime().count());
	// The marks over creatures that learn rise, blink and fade
	UpdateLearningMarks(milliseconds);
	// The numbers floating up from things rise and fade
	UpdateFloatingNumbers(milliseconds * 0.001f);
}

void Game::UpdateFrameEffects()
{
	// The game is the view of the effects of the frame
	simulation::PlayFrameEffects(*this);
}

void Game::UpdateAdvisors()
{
	if (!Locator::windowing::has_value())
	{
		return;
	}
	auto& camera = Locator::camera::value();
	auto& clock = Locator::time::value();
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	const auto realMs = static_cast<int32_t>(clock.GetFrameRealTime().count());
	const auto gameMs = static_cast<int32_t>(clock.GetFrameGameTime().count());
	const auto screenSize = glm::max(Locator::windowing::value().GetSize(), glm::ivec2(1));
	Locator::advisorSystem::value().Update({
	    .camera = &camera,
	    .screen = static_cast<glm::u16vec2>(screenSize),
	    .mouse = _mousePosition,
	    .frameMs = static_cast<uint32_t>(std::max(realMs, 1)),
	    .stepMs = std::clamp(inTemple ? realMs : gameMs, 0, 500),
	    .tickMs = machine_clock::Ticks(),
	    .wideScreen = Locator::cinematicDirectorSystem::value().IsWideScreenOn(),
	});
}

void Game::UpdateSingAlong(bool inTemple)
{
	if (!Locator::gameInterface::has_value())
	{
		return;
	}
	const auto& clock = Locator::time::value();
	std::optional<int32_t> word;
	if (Locator::scriptMusicSystem::has_value())
	{
		const auto& script = Locator::scriptMusicSystem::value().GetMusic();
		// Some languages show no glow under the words of a sung verse
		const bool shown = game_language::ShowsSingAlong(Locator::config::value().language);
		if (shown && help::sing_along::IsSungVerse(static_cast<int32_t>(script.GetType())))
		{
			word = script.GetLyrics().word;
		}
	}
	Locator::gameInterface::value().SetSingAlong(
	    word, help::sing_along::StepMilliseconds(inTemple, static_cast<int32_t>(clock.GetFrameRealTime().count()),
	                                             static_cast<int32_t>(clock.GetFrameGameTime().count())));
}

bool Game::DialogueWordsDrawn() const
{
	return Locator::gameInterface::has_value() && Locator::gameInterface::value().WereDialogueWordsDrawn();
}

void Game::CloseMenuForCinema()
{
	if (Locator::gameInterface::has_value() && Locator::gameInterface::value().GetMenu().IsOpen())
	{
		Locator::gameInterface::value().GetMenu().Close();
	}
	// The cinema bars close every box of the game, also the land information box
	if (Locator::gameInterface::has_value() && Locator::gameInterface::value().GetLandInfoBox().IsActive())
	{
		Locator::gameInterface::value().GetLandInfoBox().Close();
	}
}

void Game::UpdateEntitiesFrame(std::chrono::microseconds deltaTime)
{
	auto& profiler = Locator::profiler::value();
	auto& config = Locator::config::value();
	auto& clock = Locator::time::value();
	const auto gameTime = std::chrono::duration<float, std::milli>(clock.GetFrameGameTime());
	{
		// Update Entities
		{
			auto updateEntities = profiler.BeginScoped(Profiler::Stage::UpdateEntities);
			// The scrolls and signs spin, scale and glow towards the camera, and their glints play. The gates and the other
			// scenery that the scripts open and close play on, and the stones of the plinths sit or sink. The town totems
			// that the list draws move to their shares by the real clock, also while the game is paused. All use the draw
			// list of this frame.
			const auto updateDrawnScenery = [&clock, &gameTime]() {
				if (Locator::scriptHighlightSystem::has_value())
				{
					Locator::scriptHighlightSystem::value().UpdateFrame(
					    std::chrono::duration<float, std::milli>(gameTime).count(), clock.GetTurnFraction(),
					    Locator::camera::value().GetOrigin(), clock.GetFrameRealTime(), simulation::LandDrawn());
				}
				Locator::animatedStaticSystem::value().Update(clock.GetFrameGameTime());
				if (Locator::villageTotemSystem::has_value())
				{
					Locator::villageTotemSystem::value().Update(static_cast<uint32_t>(clock.GetElapsedTime().count()));
				}
				// The puzzle totems that the list draws take the steps of their slides
				if (Locator::puzzleTotemSystem::has_value())
				{
					Locator::puzzleTotemSystem::value().UpdateFrame(clock.GetFrameRealTime());
				}
			};
			if (config.drawEntities)
			{
				// The villagers in view are posed for the camera the frame is drawn from
				ShowInspectorCamera(true);
				Locator::livingActionSystem::value().PoseVillagersInView(Locator::camera::value().GetViewProjectionMatrix());
				// The eyes of the villagers in high detail blink and turn by the game time of the frame, once each frame
				Locator::highDetailSystem::value().PlaceEyes(static_cast<uint32_t>(clock.GetFrameGameTime().count()),
				                                             Locator::camera::value().GetViewProjectionMatrix());
				// The villagers that swim in high detail leave rings on the water, on a frame that draws the land
				if (!Locator::temple::has_value() || !Locator::temple::value().Active())
				{
					Locator::highDetailSystem::value().LeaveSwimmingRings(
					    static_cast<uint32_t>(clock.GetFrameGameTime().count()));
				}
				// The trees out of the land are drawn with their roots, as each now is
				ecs::tree_roots::Show(Locator::entitiesRegistry::value());
				// The land's draw list, from the camera the frame is drawn from, is made again when it is due: what stands
				// on the blocks beyond it isn't drawn. A frame that does not draw the land keeps the list.
				if (Locator::objectDrawListSystem::has_value())
				{
					Locator::objectDrawListSystem::value().Update(clock.GetTurn(), simulation::LandDrawn());
					// The things that the list draws show a little more or less of the snow where they stand
					if (Locator::snowSystem::has_value())
					{
						Locator::snowSystem::value().StepObjects(Locator::entitiesRegistry::value(),
						                                         Locator::objectDrawListSystem::value(),
						                                         clock.GetFrameRealTime(), clock.GetFrameGameTime());
					}
					// The towns' buildings that the list draws can make their sounds
					if (Locator::abodeSoundSystem::has_value())
					{
						Locator::abodeSoundSystem::value().Update(
						    std::chrono::duration<float, std::milli>(clock.GetFrameGameTime()));
					}
					// The pools of the teleport stones, the swirls of the dispensers and the effects of the vortices over the
					// land show only while the list draws their things, and move on only in a frame that draws the land
					const bool landDrawn = simulation::LandDrawn();
					auto& drawList = Locator::objectDrawListSystem::value();
					if (Locator::teleportSystem::has_value())
					{
						Locator::teleportSystem::value().UpdateFrame(drawList, clock.GetFrameRealTime(),
						                                             clock.GetFrameGameTime(), landDrawn);
					}
					if (Locator::magicSystem::has_value())
					{
						Locator::magicSystem::value().UpdateDispenserFrame(drawList, clock.GetFrameRealTime(),
						                                                   clock.GetFrameGameTime(), landDrawn);
					}
					if (Locator::vortexSystem::has_value())
					{
						Locator::vortexSystem::value().UpdateDrawnFrame(drawList, clock.GetFrameRealTime(),
						                                                clock.GetFrameGameTime(), landDrawn);
					}
					// The globes that the list draws run their glint on
					if (Locator::miracleFxSystem::has_value())
					{
						Locator::miracleFxSystem::value().UpdateDrawnGlobes(drawList, clock.GetFrameGameTime());
					}
				}
				// The draw of the land makes the waterfall of the third land, slides its water and makes its spray
				if (Locator::designedWaterfallSystem::has_value())
				{
					Locator::designedWaterfallSystem::value().UpdateFrame(
					    Locator::entitiesRegistry::value().Context().mapScriptGlobals.landNumber, clock.GetFrameRealTime(),
					    clock.GetFrameGameTime(), simulation::LandDrawn());
				}
				updateDrawnScenery();
				// The static of each fizzing creature slides only when the frame draws its body on the screen
				Locator::creatureFizzSystem::value().SlideDrawnStatic(
				    std::chrono::duration<float>(clock.GetFrameGameTime()).count());
				// The fast simulation mode makes every frame ready, also the ones it does not draw: the game's things go
				// by what the frames make ready, so a frame left out would change the run
				Locator::rendereringSystem::value().PrepareDraw(config.drawBoundingBoxes, config.drawFootpaths,
				                                                config.drawStreams, ecs::systems::DrawUse::Frame);
				ShowInspectorCamera(false);
				// The interface picks what is under the cursor as the frame is drawn, for the next frame to go by: through
				// an inspector's override while one is shown, as the frame is seen
				ShowInspectorOverride(true);
				PickUnderCursor(std::chrono::duration<float>(deltaTime).count());
				ShowInspectorOverride(false);
			}
			else
			{
				updateDrawnScenery();
			}
		}
	}
}
