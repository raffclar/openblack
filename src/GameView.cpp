/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The game's camera and window: the camera's controllers, the size of the scene, the near clip and the inspector's
// camera

#include <chrono>
#include <filesystem>
#include <optional>
#include <utility>

#include <spdlog/spdlog.h>

#include "3D/DayNightClock.h"
#include "3D/HandAnimation.h"
#include "3D/LandIslandInterface.h"
#include "3D/OceanInterface.h"
#include "3D/SkyDome.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "Camera/FightCameraModel.h"
#include "Camera/FreeCameraModel.h"
#include "Camera/NearClipping.h"
#include "Camera/ScriptCameraModel.h"
#include "Camera/WorldCamera.h"
#include "Common/GameRandom.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/Creature.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CameraPathSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/EditorSystemInterface.h"
#include "ECS/Systems/GameKeysSystemInterface.h"
#include "ECS/Systems/HandDemoSystemInterface.h"
#include "ECS/Systems/HandPressSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerDisciples.h"
#include "ECS/Systems/InspectorSystemInterface.h"
#include "ECS/Systems/IntroSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/ScriptControlSystemInterface.h"
#include "ECS/Systems/TattooEditorSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TipBubbleSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "GameDetail.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/RendererInterface.h"
#include "Gui/Canvas.h"
#include "Gui/GameInterface.h"
#include "Gui/HandToolTips.h"
#include "Gui/MultiplayerFrontEnd.h"
#include "Gui/ScreenFit.h"
#include "Help/DialogueText.h"
#include "Help/HelpProfile.h"
#include "Help/Spirits.h"
#include "Input/GameActionMapInterface.h"
#include "Locator.h"
#include "Online/NetworkSession.h"
#include "Online/OnlineMapStoreInterface.h"
#include "Profiler.h"
#include "Resources/GameAssets.h"
#include "Sound/GameMusic.h"

#if defined(OPENBLACK_INSPECTOR)
#include "Inspector/GameSeam.h"
#endif

using namespace openblack;
using namespace openblack::game_detail;

void Game::UpdateCameraControllers(std::chrono::microseconds deltaTime)
{
	auto& profiler = Locator::profiler::value();
	// The in-game editor keeps its camera on what it has picked
	if (Locator::editorSystem::has_value())
	{
		auto editor = profiler.BeginScoped(Profiler::Stage::EditorUpdate);
		Locator::editorSystem::value().Update(deltaTime);
	}

	// Creature Mode keeps the camera on its creature, and the cave follows the temple's creature room
	if (Locator::creatureModeSystem::has_value())
	{
		auto creatureMode = profiler.BeginScoped(Profiler::Stage::CreatureModeUpdate);
		Locator::creatureModeSystem::value().Update(
		    deltaTime, {.handGripping = Locator::handPressSystem::value().GetHandButtons().gripping});
	}
	// The free camera of a saved game of the original game flies while the hand grips the land
	if (auto* free =
	        Locator::camera::has_value() ? dynamic_cast<FreeCameraModel*>(&Locator::camera::value().GetModel()) : nullptr;
	    free != nullptr)
	{
		free->SetLandGrip(Locator::handPressSystem::value().GetHandButtons().selectGrip);
	}
	if (Locator::creatureCaveSystem::has_value())
	{
		auto creatureCave = profiler.BeginScoped(Profiler::Stage::CreatureCaveUpdate);
		Locator::creatureCaveSystem::value().Update(std::chrono::duration<float, std::milli>(deltaTime));
	}
	if (Locator::tattooEditorSystem::has_value())
	{
		Locator::tattooEditorSystem::value().Update(std::chrono::duration<float, std::milli>(deltaTime).count());
	}
}

void Game::UpdateCamera()
{
	auto& camera = Locator::camera::value();
	// While a miracle's camera path has the camera, the player's camera doesn't move it
	// A hand demonstration has the camera exactly where its recording had it, and the camera's own moves wait
	const auto demoCamera = Locator::handDemoSystem::has_value() ? Locator::handDemoSystem::value().GetCamera() : std::nullopt;
	if (!Locator::cameraPathSystem::value().HoldsCamera() && !demoCamera.has_value())
	{
		// A script's camera track runs on the game's time; the camera itself steps as the hand does, so that a cut scene
		// holds while the game is paused and keeps pace with the game's speed
		if (auto* scriptCamera = Locator::scriptControlSystem::value().GetScriptCamera(camera); scriptCamera != nullptr)
		{
			scriptCamera->PassGameTime(Locator::time::value().GetFrameGameTime());
		}
		camera.Update(Locator::time::value().GetInputSteps());
	}
	// The world's camera continues behind the temple, with its own view. It does not read the keys there.
	if (auto* world = world_camera::BehindTemple(); world != nullptr)
	{
		if (auto* scriptCamera = Locator::scriptControlSystem::value().GetScriptCamera(*world); scriptCamera != nullptr)
		{
			scriptCamera->PassGameTime(Locator::time::value().GetFrameGameTime());
		}
		world->Update(Locator::time::value().GetInputSteps(), false);
		world->KeepDrawnView();
	}
	// The lens glides on the game's time, and holds while the player is in the temple, whose own lens it has
	if (!(Locator::temple::has_value() && Locator::temple::value().Active()))
	{
		camera.UpdateLens(std::chrono::duration<float>(Locator::time::value().GetFrameGameTime()).count());
	}
	if (demoCamera.has_value())
	{
		camera.SetOrigin(demoCamera->origin).SetFocus(demoCamera->focus);
		if (auto* script = dynamic_cast<ScriptCameraModel*>(&camera.GetModel()); script != nullptr)
		{
			script->SetOrigin(demoCamera->origin);
			script->SetFocus(demoCamera->focus);
		}
	}
	// While the opening's camera chases the falling light, it is drawn from behind the light as the last frame left it,
	// whatever the script's camera does underneath
	if (const auto chase = Locator::introSystem::value().GetCameraView(); chase.has_value())
	{
		camera.SetOrigin(chase->origin).SetFocus(chase->focus);
	}
	// The near plane follows the height of the eye that the game drew last, with its lift above the land and its shake
	FitNearClip(camera.GetOrigin(Camera::Interpolation::Drawn));
}

bool Game::ScriptHoldsCamera() const
{
	return Locator::cinematicDirectorSystem::has_value() && Locator::cinematicDirectorSystem::value().IsWideScreenOn() &&
	       Locator::cinematicDirectorSystem::value().GetWideScreenOwner() != 0;
}

std::chrono::milliseconds Game::CameraStepTime() const
{
	const auto& time = Locator::time::value();
	const bool insideTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	return ecs::systems::CameraStep(time.GetFrameRealTime(), time.GetFrameGameTime(), ScriptHoldsCamera(), insideTemple);
}

std::optional<help::profile::WorldCameraView> Game::HelpWorldCameraView()
{
	if (!Locator::camera::has_value())
	{
		return std::nullopt;
	}
	const auto& model = Locator::camera::value().GetModel();
	if (dynamic_cast<const DefaultWorldCameraModel*>(&model) == nullptr &&
	    dynamic_cast<const FightCameraModel*>(&model) == nullptr)
	{
		return std::nullopt;
	}
	const auto camera = gui::hand_tooltip::CameraNow();
	return help::profile::WorldCameraView {.centreDistance = camera.centreDistance, .pitch = camera.pitch};
}

void Game::ResolveScreenShapes()
{
	// The pick's last step, once the interface is drawn: the help's text bubble and the tied leash's rope under the cursor
	// are weighed against what the frame picked, and the bubble and the rope learn whether they have the cursor
	if (!Locator::pickingSystem::has_value() || !Locator::camera::has_value() || !Locator::windowing::has_value())
	{
		return;
	}
	ecs::systems::PickingSystemInterface::ScreenShapes shapes;
	if (Locator::tipBubbleSystem::has_value())
	{
		const auto& bubble = Locator::tipBubbleSystem::value();
		shapes.textBubble = bubble.GetUnderPointer();
		shapes.textBubblePoint = bubble.GetAnchor().value_or(glm::vec3(0.0f));
	}
	if (Locator::leashSystem::has_value())
	{
		shapes.rope = Locator::leashSystem::value().RopeUnderCursor(
		    LocalInputPlayer(), {.viewProjection = Locator::camera::value().GetViewProjectionMatrix(),
		                         .screenSize = static_cast<glm::vec2>(Locator::windowing::value().GetSize()),
		                         .cursor = Locator::gameActionSystem::value().GetPointerPosition()});
	}
	auto& picking = Locator::pickingSystem::value();
	picking.ResolveScreenShapes(shapes);
	const auto& pick = picking.GetPick();
	if (Locator::tipBubbleSystem::has_value())
	{
		Locator::tipBubbleSystem::value().SetUnderHand(pick.textBubble);
	}
	if (Locator::leashSystem::has_value())
	{
		Locator::leashSystem::value().SetRopeHovered(LocalInputPlayer(), pick.rope);
	}
}

std::optional<glm::vec2> Game::OnScreen(glm::vec3 point) const
{
	if (!Locator::windowing::has_value())
	{
		return std::nullopt;
	}
	const auto size = Locator::windowing::value().GetSize();
	glm::vec3 screen;
	if (!Locator::camera::value().ProjectWorldToScreen(point, glm::vec4(0.0f, 0.0f, glm::vec2(size)), screen))
	{
		return std::nullopt;
	}
	// The game takes the whole pixel it falls in, which must be on the screen
	const glm::ivec2 pixel {static_cast<int>(screen.x), static_cast<int>(screen.y)};
	if (pixel.x < 0 || pixel.y < 0 || pixel.x >= size.x || pixel.y >= size.y)
	{
		return std::nullopt;
	}
	return glm::vec2(pixel);
}

void Game::SizeSceneToWindow()
{
	if (!Locator::rendererInterface::has_value() || !Locator::oceanSystem::has_value())
	{
		return;
	}
	if (Locator::windowing::has_value())
	{
		const auto size = static_cast<glm::u16vec2>(Locator::windowing::value().GetSize());
		Locator::rendererInterface::value().ConfigureView(graphics::RenderPass::Main, size, 0x274659ff);
		Locator::oceanSystem::value().ResizeReflectionFramebuffer(size);
	}
	uint16_t width = 0;
	uint16_t height = 0;
	Locator::oceanSystem::value().GetReflectionFramebuffer().GetSize(width, height);
	Locator::rendererInterface::value().ConfigureView(graphics::RenderPass::Reflection, {width, height}, 0x274659ff);
}

void Game::FollowWindowSize()
{
	if (!Locator::windowing::has_value() || !Locator::rendererInterface::has_value())
	{
		return;
	}
	const auto size = _windowSize.Follow(Locator::windowing::value().GetSize());
	if (!size.has_value())
	{
		return;
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The game draws at the new window size {}x{}", size->x, size->y);
	Locator::rendererInterface::value().Reset(*size);
	if (Locator::oceanSystem::has_value())
	{
		Locator::oceanSystem::value().ResizeReflectionFramebuffer(*size);
	}
	// The camera keeps its lens (the world's, the temple's) and its clip planes, only its shape changes
	if (Locator::camera::has_value())
	{
		Locator::camera::value().SetAspect(_windowSize.Aspect());
	}
}

void Game::FitNearClip(const glm::vec3& eye)
{
	// Outside a camera with a lens of its own, the near plane follows the camera's height over the land, but for close
	// shots: a script's, and a miracle's camera path
	auto& camera = Locator::camera::value();
	if (camera.GetModel().GetLens().has_value() || !Locator::terrainSystem::has_value())
	{
		return;
	}
	const float height = eye.y - Locator::terrainSystem::value().GetHeightAt(glm::vec2(eye.x, eye.z));
	const float nearClip = near_clipping::NearPlane(height, Locator::cinematicDirectorSystem::value().IsCloseClipping() ||
	                                                            Locator::cameraPathSystem::value().HoldsCamera());
	if (nearClip != camera.GetNearClip())
	{
		camera.SetNearClip(nearClip);
	}
}

void Game::FitInspectorNearClip(bool shown, const glm::vec3& eyeBefore, const glm::vec3& focusBefore)
{
	auto& camera = Locator::camera::value();
	if (!shown)
	{
		// The game's camera gets back the near plane that it had before the inspector showed a different view
		if (const auto nearClip = std::exchange(_nearClipUnderInspector, std::nullopt); nearClip.has_value())
		{
			camera.SetNearClip(*nearClip);
		}
		return;
	}
	if (camera.GetOrigin() == eyeBefore && camera.GetFocus() == focusBefore)
	{
		return;
	}
	// A different view gets the near plane for the height of its own eye
	if (!_nearClipUnderInspector.has_value())
	{
		_nearClipUnderInspector = camera.GetNearClip();
	}
	FitNearClip(camera.GetOrigin());
}

void Game::ShowInspectorCamera(bool shown)
{
	if (!Locator::inspector::has_value())
	{
		return;
	}
	auto& inspector = Locator::inspector::value();
	const auto& camera = Locator::camera::value();
	const auto eyeBefore = camera.GetOrigin();
	const auto focusBefore = camera.GetFocus();
	if (shown)
	{
		inspector.PlaceCamera();
	}
	else
	{
		inspector.GiveCameraBack();
	}
	FitInspectorNearClip(shown, eyeBefore, focusBefore);
}

void Game::ShowInspectorOverride(bool shown)
{
	if (!Locator::inspector::has_value())
	{
		return;
	}
	auto& inspector = Locator::inspector::value();
	const auto& camera = Locator::camera::value();
	const auto eyeBefore = camera.GetOrigin();
	const auto focusBefore = camera.GetFocus();
	if (shown)
	{
		inspector.ShowOverrideToPointer();
	}
	else
	{
		inspector.GiveCameraBack();
	}
	FitInspectorNearClip(shown, eyeBefore, focusBefore);
}

void Game::RequestScreenshot(const std::filesystem::path& path, bool hideDebugGui) noexcept
{
	_requestScreenshot = std::make_pair(_frameCount, path);
	_screenshotHidesDebugGui = hideDebugGui;
}

void Game::StartCameraAtTemple()
{
	Locator::gameKeysSystem::value().StartShortcutKeysAtTemple(LocalInputPlayer());
}
