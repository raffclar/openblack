/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Game.h"

#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <LNDFile.h>
#include <MorphFile.h>
#include <PackFile.h>
#include <SDL.h>
#include <bgfx/bgfx.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/intersect.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/CreatureCaveTrophies.h"
#include "3D/DayNightClock.h"
#include "3D/FlatLand.h"
#include "3D/GripLandscapeEffect.h"
#include "3D/HandAnimation.h"
#include "3D/HandMorph.h"
#include "3D/HandNavigationPose.h"
#include "3D/HandOrientation.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandLightFrame.h"
#include "3D/LandLightTable.h"
#include "3D/MapCoords.h"
#include "3D/OceanInterface.h"
#include "3D/SkyDome.h"
#include "3D/SnowCover.h"
#include "3D/TempleInteriorInterface.h"
#include "3D/WaterRings.h"
#include "Audio/AtmosAudio.h"
#include "Audio/AudioManagerInterface.h"
#include "Audio/ClipSounds.h"
#include "Audio/GameMusic.h"
#include "Audio/GameSoundEffects.h"
#include "Audio/HelpSpeech.h"
#include "CHLApi.h"
#include "Camera/Camera.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "Camera/NearClipping.h"
#include "Camera/ScriptCameraModel.h"
#include "Common/EventManager.h"
#include "Common/GameRandom.h"
#include "Common/MachineClock.h"
#include "Common/RandomNumberManager.h"
#include "Common/StringUtils.h"
#include "Creature/CreatureHandRules.h"
#include "Debug/DebugGuiInterface.h"
#include "Debug/FrameStatsLog.h"
#include "Debug/TestbedDispenserGrid.h"
#include "ECS/Archetypes/PlayerArchetype.h"
#include "ECS/Archetypes/SkyArchetype.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/CameraBookmark.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/FloatingNumber.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandMorph.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PrayerPower.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Vortex.h"
#include "ECS/FloatingNumber.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AbodeKnockSystemInterface.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/AnimatedStaticSystemInterface.h"
#include "ECS/Systems/BuildingDamageSystemInterface.h"
#include "ECS/Systems/CameraBookmarkSystemInterface.h"
#include "ECS/Systems/CameraHelpSystemInterface.h"
#include "ECS/Systems/CameraPathSystemInterface.h"
#include "ECS/Systems/ChimneySmokeSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CloudSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureAudioSystemInterface.h"
#include "ECS/Systems/CreatureCarryOverSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureFizzSystemInterface.h"
#include "ECS/Systems/CreatureHairSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreaturePenSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/DialogueControlSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/EditorSystemInterface.h"
#include "ECS/Systems/ExplosionSystemInterface.h"
#include "ECS/Systems/FieldSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/FireflySystemInterface.h"
#include "ECS/Systems/FishFarmSystemInterface.h"
#include "ECS/Systems/FootprintSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/GestureEventsInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/HandDemoSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/HelpSpeechSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/HighDetailSystemInterface.h"
#include "ECS/Systems/Implementations/ObjectMeasures.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/InspectorLoading.h"
#include "ECS/Systems/InspectorSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/MagicShieldSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "ECS/Systems/MistSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PathfindingSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/RainSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "ECS/Systems/RewardSystemInterface.h"
#include "ECS/Systems/ScriptControlSystemInterface.h"
#include "ECS/Systems/ScriptHighlightSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/SharkSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/SnowSystemInterface.h"
#include "ECS/Systems/SnowfallSystemInterface.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "ECS/Systems/TattooEditorSystemInterface.h"
#include "ECS/Systems/TeleportSystemInterface.h"
#include "ECS/Systems/TempleDestructionSystemInterface.h"
#include "ECS/Systems/TempleExteriorSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TipBubbleSystemInterface.h"
#include "ECS/Systems/TornadoSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/Systems/TutorialSkipSystemInterface.h"
#include "ECS/Systems/VegetationInterface.h"
#include "ECS/Systems/VideoSystemInterface.h"
#include "ECS/Systems/VillageLightSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/Systems/VortexSystemInterface.h"
#include "ECS/Systems/WalkPathSystemInterface.h"
#include "ECS/Systems/WaterRingSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/VillageTotem.h"
#include "ECS/WorldObjects.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Gestures/GestureTrailBuilder.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/RendererInterface.h"
#include "Gui/GameInterface.h"
#include "Gui/LoadingScreen.h"
#include "Hand/HandFeel.h"
#include "Hand/HandVisibility.h"
#include "Help/Spirits.h"
#include "Input/GameActionMapInterface.h"
#include "LHScriptX/Script.h"
#include "Locator.h"
#include "Parsers/InfoFile.h"
#include "Physics/Materials.h"
#include "Profiler.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"
#include "Serializer/FotFile.h"

#ifdef __ANDROID__
#include <spdlog/sinks/android_sink.h>
#endif

using namespace openblack;
using namespace openblack::lhscriptx;
using namespace std::chrono_literals;

namespace
{
/// The hand's splash, its sound and the scare it gives the fish are this high over the sea
constexpr float k_HandSplashHeight = 0.2f;
// Where the camera starts on the testbed: above and behind the middle of the map
constexpr float k_TestbedCameraHeight = 60.0f;
/// How far the testbed's player's influence reaches from its middle, and the prayer power their worship has stored
constexpr float k_TestbedInfluenceRadius = 400.0f;
constexpr float k_TestbedPrayer = 1.0e6f;
constexpr float k_TestbedCameraBack = 120.0f;

/// The meshes the hand is pulled towards as its player turns evil or good, the first two of its morph file's variants,
/// and the files of the base and those two for their skins. Black & White would also blend the hand's bones and
/// animations towards theirs, but its own meshes share the base's bones and the morph file has no animations for
/// them, so that changes nothing; a warning says when other data would.
void LoadHandLooks(const morph::MorphFile& morphFile)
{
	using ecs::components::HandMorph;
	auto& fileSystem = Locator::filesystem::value();
	auto& resources = Locator::resources::value();
	auto& meshes = resources.GetMeshes();
	auto& files = resources.GetL3DFiles();
	const auto& header = morphFile.GetHeader();
	const auto pathOf = [&fileSystem](const std::array<char, 0x20>& name) {
		return fileSystem.GetPath<filesystem::Path::CreatureMesh>() / (std::string(name.data()) + ".l3d");
	};
	const auto load = [&files](entt::id_type id, const std::filesystem::path& path) {
		try
		{
			files.Load(id, resources::L3DFileLoader::FromDiskTag {}, path);
		}
		catch (std::exception& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Can't read the hand's skins from {}: {}", path.string(), err.what());
		}
	};
	load(HandMorph::k_SkinFileIds[0], pathOf(header.baseMeshName));
	const auto base = meshes.Handle(ecs::components::Hand::k_MeshId);
	for (size_t look = 0; look < HandMorph::k_LookMeshIds.size(); ++look)
	{
		const auto& name = header.variantMeshNames.at(look);
		if (name[0] == '\0')
		{
			continue;
		}
		const auto path = pathOf(name);
		try
		{
			meshes.Load(HandMorph::k_LookMeshIds.at(look), resources::L3DLoader::FromDiskTag {}, path);
		}
		catch (std::exception& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Can't load the hand mesh {}: {}", path.string(), err.what());
			continue;
		}
		load(HandMorph::k_SkinFileIds.at(look + 1), path);
		const auto variant = meshes.Handle(HandMorph::k_LookMeshIds.at(look));
		if (base && variant->GetBoneMatrices() != base->GetBoneMatrices())
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "The bones of {} differ from the hand's: they are not blended",
			                   path.string());
		}
		if (!morphFile.GetVariantAnimationSet(static_cast<uint32_t>(look)).empty())
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "The animations of {} in the hand's morph file are not blended",
			                   path.string());
		}
	}
}

/// Whether the hand turns to the face of a model it rests on: not a tree's, a dead tree's, a totem statue's, a seed's or
/// a shield's, whose faces leave the hand turned to the land
bool FeelsModel(entt::entity object)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (registry.AnyOf<ecs::components::Tree, ecs::components::DeadTree, ecs::components::SpellSeed,
	                   ecs::components::MagicShield>(object))
	{
		return false;
	}
	const auto* info = ecs::world_objects::InfoOf(object);
	return info == nullptr || info->type != ObjectType::TotemStatue;
}

/// How many bytes a game file has, for the loading budget, or none when it isn't there
std::optional<size_t> GameFileBytes(const std::filesystem::path& path)
{
	try
	{
		std::error_code error;
		const auto size = std::filesystem::file_size(Locator::filesystem::value().FindPath(path), error);
		if (!error)
		{
			return static_cast<size_t>(size);
		}
	}
	catch (const std::exception&)
	{
		// Not found: there is nothing to load
	}
	return std::nullopt;
}

/// Registers a resource read from a game file, to be loaded by its cache's loader with these arguments when it is first
/// needed or prefetched. One whose file isn't there is left out, as loading it would fail.
template <typename Manager, typename Id, typename... Args>
void RegisterFile(Manager& manager, Id id, const std::filesystem::path& path, Args... args)
{
	if (const auto bytes = GameFileBytes(path))
	{
		manager.RegisterLoad(id, *bytes, std::move(args)...);
	}
	else
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Can't find {}", path.generic_string());
	}
}

/// The sound banks whose samples are decoded as soon as they are read, as they are wanted at once and often: the hand's,
/// the miracles' and the interface's
constexpr std::array<std::string_view, 1> k_DecodedAheadBanks = {"InGame.sad"};
/// Both advisors are sent home, the good one first; a help script's vanish rather than fly home
void SendAdvisorsHome(bool helpScript)
{
	if (!Locator::advisorSystem::has_value() || !Locator::advisorSystem::value().IsLoaded())
	{
		return;
	}
	auto& advisors = Locator::advisorSystem::value().GetController();
	advisors.SpiritHome(1, helpScript);
	advisors.SpiritHome(2, helpScript);
}

} // namespace

const std::string k_WindowTitle = "openblack";

Game* Game::sInstance = nullptr;

Game::Game(Arguments&& args) noexcept
    : _gamePath(args.gamePath)
    , _startMap(args.startLevel)
    , _playVideo(args.playVideo)
    , _preIntro(args.preIntro)
    , _skipLogos(args.skipLogos)
    , _startTestbed(args.startTestbed || args.scenario.has_value())
    , _scenarioRequest(args.scenario)
    , _inspectPort(args.inspectPort)
    , _screenshotRoot(args.screenshotRoot)
    , _seed(args.seed)
    , _inspectInputLock(args.inspectInputLock)
    , _testbedWindow(!args.scenario.has_value() || !args.scenario->hideWindow)
    , _requestScreenshot(args.requestScreenshot)
{
	Locator::camera::emplace(glm::zero<glm::vec3>());
	std::function<std::shared_ptr<spdlog::logger>(const std::string&)> createLogger;
#ifdef __ANDROID__
	if (!args.logFile.empty() && args.logFile == "logcat")
	{
		createLogger = [](const std::string& name) { return spdlog::android_logger_mt(name, "spdlog-android"); };
	}
	else
#endif // __ANDROID__
	{
		if (!args.logFile.empty() && args.logFile != "stdout")
		{
			// Every subsystem's logger writes through the one file: each opening the file for itself, their writes
			// overwrote each other's and lines went missing
			auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(args.logFile);
			createLogger = [fileSink](const std::string& name) {
				auto logger = std::make_shared<spdlog::logger>(name, fileSink);
				spdlog::register_logger(logger);
				return logger;
			};
		}
		else
		{
			createLogger = [](const std::string& name) { return spdlog::stdout_color_mt(name); };
		}
	}
	// TODO (#749) use std::views::enumerate
	for (size_t i = 0; const auto& subsystem : k_LoggingSubsystemStrs)
	{
		auto logger = createLogger(subsystem.data());
		logger->set_level(args.logLevels.at(i));
		// An error is written out at once, so it is in the file even if the process is then ended from outside, where no
		// crash report can be written
		logger->flush_on(spdlog::level::err);
		++i;
	}
	// Everything else is written out within a second
	spdlog::flush_every(std::chrono::seconds(1));
	sInstance = this;

	auto& config = Locator::config::emplace();
	config.numFramesToSimulate = args.numFramesToSimulate;
	config.frameStatsInterval = args.frameStatsInterval;
	config.frameStatsViews = args.frameStatsViews;
	config.resolution = {args.windowWidth, args.windowHeight};
	config.displayMode = args.displayMode;
	config.graphicsBackend = args.graphicsBackend;
	config.vsync = args.vsync;
	config.detailLevel = args.detailLevel;
	config.guiScale = args.guiScale;
}

Game::~Game() noexcept
{
	// Its textures go before the renderer, and the temple's scrolls are written with its text
	if (Locator::temple::has_value())
	{
		Locator::temple::value().SetInterface(nullptr);
	}
	if (Locator::creatureCaveSystem::has_value())
	{
		Locator::creatureCaveSystem::value().SetInterface(nullptr);
	}
	if (Locator::miracleFxSystem::has_value())
	{
		Locator::miracleFxSystem::value().SetInterface(nullptr);
	}
	_interface.reset();
	_loadingScreen.reset();
	// What the scripts asked of openblack that it can't do yet, for the natives to write next
	if (Locator::chlapi::has_value())
	{
		Locator::chlapi::value().LogStubCalls();
	}
	ShutDownServices();
	SDL_Quit(); // todo: move to GameWindow
	spdlog::shutdown();
}

bool Game::ProcessEvents(const SDL_Event& event) noexcept
{
	static bool leftMouseButton = false;
	static bool middleMouseButton = false;
	static bool rightMouseButton = false;

	// Pressed and let go, or as the mouse moving finds them: a press or a let go the menu or the debug windows took would
	// otherwise leave the hand gripping, as on leaving the temple
	if ((event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) && event.button.button == SDL_BUTTON_LEFT)
	{
		leftMouseButton = event.type == SDL_MOUSEBUTTONDOWN;
	}
	// A press of the left button may click the dialogue on, whatever else it does
	if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT &&
	    !Locator::debugGui::value().IsMouseOverWindow())
	{
		_dialogueClick = true;
	}
	if ((event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) && event.button.button == SDL_BUTTON_MIDDLE)
	{
		middleMouseButton = event.type == SDL_MOUSEBUTTONDOWN;
	}
	const bool rightLetGo =
	    rightMouseButton && ((event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT) ||
	                         (event.type == SDL_MOUSEMOTION && (event.motion.state & SDL_BUTTON_RMASK) == 0));
	if ((event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) && event.button.button == SDL_BUTTON_RIGHT)
	{
		rightMouseButton = event.type == SDL_MOUSEBUTTONDOWN;
	}
	if (event.type == SDL_MOUSEMOTION)
	{
		leftMouseButton = (event.motion.state & SDL_BUTTON_LMASK) != 0;
		middleMouseButton = (event.motion.state & SDL_BUTTON_MMASK) != 0;
		rightMouseButton = (event.motion.state & SDL_BUTTON_RMASK) != 0;
	}

	// The hand grips the land, which the temple has none of: its camera takes the clicks
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	// Holding the right button on a creature holds the hand to it to stroke or slap it; clicking it puts the leash on.
	// While the player's creature fights, clicks on the creatures and the arena's ground direct the fight instead.
	auto& creatureHand = Locator::creatureHandSystem::value();
	auto& fights = Locator::creatureFightSystem::value();
	auto& magic = Locator::magicSystem::value();
	// The left button taps a one-shot bubble under the hand into it, before the creatures and the land
	bool magicTookPress = false;
	// The hand holds one thing at a time: a thing it picked up, or a miracle
	auto* handGrab = Locator::handGrabSystem::has_value() ? &Locator::handGrabSystem::value() : nullptr;
	const bool handHoldsThing = handGrab != nullptr && handGrab->IsBusy();
	if (!inTemple && !handHoldsThing && event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT &&
	    !middleMouseButton && !Locator::debugGui::value().IsMouseOverWindow())
	{
		magicTookPress = magic.TapAction();
	}
	// Letting go of the Action button lets go of a town's totem, leaving it where it was slid
	if (Locator::villageTotemSystem::has_value() && Locator::villageTotemSystem::value().GetGripped().has_value() &&
	    (rightLetGo || (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT)))
	{
		Locator::villageTotemSystem::value().LetGo();
		_totemPointer.reset();
	}
	// Letting go of the Action button lets go of what the hand was taking, or puts down or throws what it holds
	if (handGrab != nullptr && (rightLetGo || (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT)))
	{
		// TODO(hand): a press too short to take the thing taps it
		[[maybe_unused]] const auto tapped = handGrab->Release(machine_clock::Ticks(), Locator::time::value().GetTurn());
		// Let go with an empty hand that did nothing with the press, it clicks the thing or the place under it, for the
		// scripts; not while the game is paused
		if (!_actionPressTaken && !handHoldsThing && !magic.IsHandBusy() && !inTemple && !IsPaused())
		{
			handGrab->ClickReleased(Locator::time::value().GetTurn());
		}
	}
	// The action button (the right) casts the miracle in the hand, which comes before the creatures: pressed, it arms,
	// locks on or casts it, and let go it throws an armed one or lets a locked one go. Without a miracle it takes hold
	// of the creature under the hand; with the leash on, pressed on another creature it ties the leash to that one
	// instead (see the leash's input).
	if (rightLetGo || (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT))
	{
		magic.ReleaseAction();
	}
	if (!inTemple && event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT &&
	    !Locator::debugGui::value().IsMouseOverWindow())
	{
		_actionPressTaken = magic.IsHandBusy() && magic.PressAction();
		// Holding a thing, the hand makes ready to throw it
		if (!_actionPressTaken && handHoldsThing)
		{
			_actionPressTaken = handGrab->Press(machine_clock::Ticks(), Locator::time::value().GetTurn());
		}
		if (!_actionPressTaken)
		{
			const auto screenSize = Locator::windowing::value().GetSize();
			glm::vec3 rayOrigin;
			glm::vec3 rayDirection;
			Locator::camera::value().DeprojectScreenToWorld(glm::vec2(event.button.x, event.button.y) /
			                                                    static_cast<glm::vec2>(glm::max(screenSize, glm::ivec2(1))),
			                                                rayOrigin, rayDirection);
			// While the player's creature duels, the Action button adds a move to its queue
			if (fights.Press(rayOrigin, rayDirection, creature_fight::Button::Action, machine_clock::Ticks(),
			                 Locator::time::value().GetTurn()))
			{
				_actionPressTaken = true;
				_fightButton = creature_fight::Button::Action;
			}
			const auto& leashes = Locator::leashSystem::value();
			const auto own = leashes.PlayersCreature(PlayerNames::PLAYER_ONE);
			const auto under = creatureHand.CreatureUnderCursor();
			const bool tying = under.has_value() && own.has_value() && *under != *own && leashes.IsLeashed(*own);
			if (!_actionPressTaken && under.has_value() && !tying)
			{
				_actionPressTaken = true;
				if (handGrab != nullptr)
				{
					handGrab->ClickThing(*under, Locator::time::value().GetTurn());
				}
				if (!creatureHand.Grab())
				{
					// A creature the hand may not hold: a click on it still asks the leash, which says why not
					Locator::leashSystem::value().TapCreature(PlayerNames::PLAYER_ONE, *under);
				}
			}
		}
		// A town's totem under the hand is taken hold of, to slide it up and down
		if (!_actionPressTaken && !magic.IsHandBusy() && !handHoldsThing && Locator::villageTotemSystem::has_value() &&
		    Locator::pickingSystem::has_value())
		{
			auto& totems = Locator::villageTotemSystem::value();
			if (const auto picked = Locator::pickingSystem::value().GetPick().object; picked.has_value())
			{
				if (const auto totem = totems.TotemOf(*picked);
				    totem.has_value() && totems.Grip(*totem, PlayerNames::PLAYER_ONE))
				{
					_actionPressTaken = true;
					_totemPointer = _mousePosition;
				}
			}
		}
		// Otherwise the hand takes hold of a thing under it
		if (!_actionPressTaken && !magic.IsHandBusy() && handGrab != nullptr)
		{
			_actionPressTaken = handGrab->Press(machine_clock::Ticks(), Locator::time::value().GetTurn());
		}
	}
	if (!magicTookPress && !magic.IsHandBusy() && !inTemple && event.type == SDL_MOUSEBUTTONDOWN &&
	    event.button.button == SDL_BUTTON_LEFT && !middleMouseButton)
	{
		const auto screenSize = Locator::windowing::value().GetSize();
		glm::vec3 rayOrigin;
		glm::vec3 rayDirection;
		Locator::camera::value().DeprojectScreenToWorld(glm::vec2(event.button.x, event.button.y) /
		                                                    static_cast<glm::vec2>(glm::max(screenSize, glm::ivec2(1))),
		                                                rayOrigin, rayDirection);
		// The second press of a double click on a creature, anyone's, locks the camera onto it
		const bool doubleClicked =
		    Locator::creatureModeSystem::has_value() &&
		    Locator::creatureModeSystem::value().Press({.milliseconds = event.button.timestamp,
		                                                .screen = glm::vec2(event.button.x, event.button.y),
		                                                .creature = creatureHand.CreatureUnderCursor()});
		// While the player's creature duels, the Move button makes a move at once in place of those queued
		if (!doubleClicked && fights.Press(rayOrigin, rayDirection, creature_fight::Button::Move, machine_clock::Ticks(),
		                                   Locator::time::value().GetTurn()))
		{
			_fightButton = creature_fight::Button::Move;
		}
		else if (!doubleClicked)
		{
			PlayHandGrabSound();
		}
	}
	// Letting go of the button that pressed charges the blow it queued
	if (_fightButton.has_value() && fights.IsPressed() &&
	    ((*_fightButton == creature_fight::Button::Move && !leftMouseButton) ||
	     (*_fightButton == creature_fight::Button::Action && !rightMouseButton)))
	{
		fights.Release(machine_clock::Ticks(), Locator::time::value().GetTurn());
		_fightButton.reset();
	}
	// Letting go of the right button lets go of the creature. Let go quickly, having neither stroked nor slapped it, the
	// press was a click, which puts the leash on the player's creature.
	if (rightLetGo && creatureHand.GetCreature().has_value() && !creatureHand.IsHeldByCommand())
	{
		const auto creature = *creatureHand.GetCreature();
		const bool click = creatureHand.IsClick();
		creatureHand.Release();
		if (click)
		{
			Locator::leashSystem::value().TapCreature(PlayerNames::PLAYER_ONE, creature);
		}
	}
	const bool onCreature = creatureHand.GetCreature().has_value();

	// A miracle in the hand doesn't stop the hand moving the land; the middle button, or both buttons, turn the camera
	_handGripping = !inTemple && (middleMouseButton || (leftMouseButton && !onCreature && !fights.IsPressed()));
	_handRotating = !inTemple && (middleMouseButton || (leftMouseButton && rightMouseButton));

	auto& window = Locator::windowing::value();
	auto& camera = Locator::camera::value();

	switch (event.type)
	{
	case SDL_QUIT:
		return false;
	case SDL_WINDOWEVENT:
		if (event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == window.GetID())
		{
			return false;
		}
		else if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
		{
			const auto resolution = glm::u16vec2(event.window.data1, event.window.data2);
			Locator::rendererInterface::value().Reset(resolution);
			Locator::rendererInterface::value().ConfigureView(graphics::RenderPass::Main, resolution, 0x274659ff);
			Locator::oceanSystem::value().ResizeReflectionFramebuffer(resolution);
			Locator::rendererInterface::value().ConfigureView(graphics::RenderPass::Reflection, resolution, 0x274659ff);

			auto aspect = window.GetAspectRatio();
			const auto& config = Locator::config::value();
			// A camera model, as the temple's, can look through its own lens
			const auto lens = camera.GetModel().GetLens();
			camera.SetProjectionMatrixPerspective(lens.has_value() ? lens->horizontalFieldOfView : config.cameraXFov, aspect,
			                                      lens.has_value() ? lens->nearClip : config.cameraNearClip,
			                                      config.cameraFarClip);
		}
		break;
	case SDL_KEYDOWN:
		switch (event.key.keysym.sym)
		{
		case SDLK_ESCAPE:
			// Not while a script has the cinema bars in for its scene
			if (!Locator::cinematicDirectorSystem::value().IsInterfaceActive())
			{
				break;
			}
			return false;
		case SDLK_f:
			window.SetDisplayMode(windowing::DisplayMode::Fullscreen);
			break;
		case SDLK_p:
			Locator::time::value().SetPaused(!IsPaused());
			break;
		// F1 is the game's Help key, so the renderer's statistics are on F11
		case SDLK_F11:
			Locator::rendererInterface::value().SetDebug(!Locator::rendererInterface::value().GetDebug());
			break;
		case SDLK_1:
		case SDLK_2:
		case SDLK_3:
		case SDLK_4:
		case SDLK_5:
		case SDLK_6:
		case SDLK_7:
		case SDLK_8:
			// The camera's bookmarks aren't for the player while a script has the cinema bars in
			if (!Locator::cinematicDirectorSystem::value().IsInterfaceActive() ||
			    !Locator::cameraBookmarkSystem::value().IsEnabled())
			{
				break;
			}
			if ((event.key.keysym.mod & KMOD_CTRL) != 0)
			{
				const auto index = static_cast<uint8_t>(event.key.keysym.sym - SDLK_1);
				const auto positions = Locator::handSystem::value().GetPlayerHandPositions();
				if (positions[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)] ||
				    positions[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Right)])
				{
					const auto handPosition =
					    positions[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)].value_or(
					        positions[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Right)].value_or(
					            glm::zero<glm::vec3>()));
					Locator::cameraBookmarkSystem::value().SetBookmark(index, handPosition, camera.GetOrigin());
				}
			}
			else
			{
				const auto& entitiesRegistry = Locator::entitiesRegistry::value();
				const size_t index = event.key.keysym.sym - SDLK_1;
				const auto& bookmarkEntities = Locator::cameraBookmarkSystem::value().GetBookmarks();
				const auto entity = bookmarkEntities.at(index);
				const auto [transform, bookmark] =
				    entitiesRegistry.TryGet<ecs::components::Transform, ecs::components::CameraBookmark>(entity);
				if (transform != nullptr && bookmark != nullptr)
				{
					camera.GetModel().SetFlight(bookmark->savedOrigin, transform->position);
				}
			}
			break;
		}
		break;
	case SDL_MOUSEMOTION:
	{
		// While the mouse turns the camera the cursor is held where it was, and the pointer isn't followed
		if (!Locator::gameActionSystem::value().IsCursorFrozen())
		{
			_mousePosition = {event.motion.x, event.motion.y};
		}
		break;
	}
	}

	return true;
}

void Game::SetGameSpeed(float multiplier)
{
	Locator::time::value().SetSpeed(1.0f / multiplier);
}

float Game::GetGameSpeed() const
{
	return 1.0f / Locator::time::value().GetSpeed();
}

uint32_t Game::GetTurn() const
{
	return Locator::time::value().GetTurn();
}

bool Game::IsPaused() const
{
	return Locator::time::value().IsPaused();
}

bool Game::IsHandDrawn() const
{
	return hand_visibility::IsShown(_interface && _interface->IsDialogOpen(),
	                                Locator::cinematicDirectorSystem::value().IsInterfaceActive());
}

void Game::UpdateGestures(const Camera& camera, glm::ivec2 screenSize, float deltaSeconds)
{
	if (!Locator::gestureSystem::has_value() || screenSize.x <= 0 || screenSize.y <= 0)
	{
		return;
	}
	const auto size = static_cast<glm::vec2>(screenSize);
	const auto rayAt = [&camera, size](glm::vec2 pixel) {
		glm::vec3 origin;
		glm::vec3 direction;
		camera.DeprojectScreenToWorld(pixel / size, origin, direction);
		return std::pair {origin, direction};
	};
	ecs::systems::GestureSystemInterface::Frame frame {
	    .seconds = deltaSeconds,
	    .cursor = static_cast<glm::vec2>(_mousePosition),
	    // Not over a debug window, nor while a cut scene has the screen
	    .overWorld = !Locator::debugGui::value().StealsFocus() && !Locator::debugGui::value().IsMouseOverWindow() &&
	                 Locator::cinematicDirectorSystem::value().IsInterfaceActive(),
	    .actionHeld = Locator::gameActionSystem::value().Get(input::BindableActionMap::ACTION),
	    .player = PlayerNames::PLAYER_ONE,
	    .view = {.screenSize = size,
	             .cameraRight = camera.GetRight(),
	             .cameraEye = camera.GetOrigin(),
	             .cameraForward = camera.GetForward()},
	    .cameraShaking = Locator::explosionSystem::has_value() && Locator::explosionSystem::value().IsShaking(),
	};
	// The land or the sea under a point of the screen, as the interface takes it to be under the cursor
	frame.view.landAt = [&camera, size](glm::vec2 pixel) -> std::optional<glm::vec3> {
		if (!Locator::pickingSystem::has_value())
		{
			return std::nullopt;
		}
		const auto [eye, nearPoint] = camera.OnNearPlane(pixel / size);
		return Locator::pickingSystem::value().LandUnderPixel(eye, nearPoint, true);
	};
	// A point of the screen as far in front of the camera as another point
	frame.view.atDepthOf = [rayAt, forward = camera.GetForward()](glm::vec2 pixel, glm::vec3 sameDepthAs) {
		const auto [origin, direction] = rayAt(pixel);
		float distance = 0.0f;
		if (glm::intersectRayPlane(origin, direction, sameDepthAs, -forward, distance))
		{
			return origin + (direction * distance);
		}
		return sameDepthAs;
	};
	// Where a recognised gesture's trail is laid under a pixel: where the line from the camera through it meets the land,
	// or the sea's level no further than 7500 units from the camera across the land, at the land's height there;
	// otherwise 400 units from the camera towards the pixel
	frame.view.trailPointUnder = [&camera, rayAt, size, eye = camera.GetOrigin()](glm::ivec2 pixel) {
		const auto direction = rayAt(glm::vec2(pixel)).second;
		const auto onNear = camera.OnNearPlane(glm::vec2(pixel) / size);
		if (Locator::pickingSystem::has_value())
		{
			if (const auto hit = Locator::pickingSystem::value().LandOrSeaAlong(onNear.eye, onNear.point, onNear.eye))
			{
				return *hit;
			}
		}
		return eye + (direction * gesture::k_TrailSkyDistance);
	};
	Locator::gestureSystem::value().Update(frame);
	// A path drawn for the testbed is drawn by the hand, which follows it
	if (const auto drawing = Locator::gestureSystem::value().GetDrawingPoint())
	{
		_mousePosition = glm::ivec2(*drawing * (size.y / gesture::k_ReferenceHeight));
	}
}

void Game::PickUnderCursor(float seconds)
{
	if (!Locator::pickingSystem::has_value() || !Locator::windowing::has_value())
	{
		return;
	}
	const auto screenSize = Locator::windowing::value().GetSize();
	if (screenSize.x <= 0 || screenSize.y <= 0)
	{
		return;
	}
	const auto& camera = Locator::camera::value();
	const auto resolution = static_cast<glm::vec2>(screenSize);
	const auto cursor = static_cast<glm::vec2>(_mousePosition);
	const auto [eye, nearPoint] = camera.OnNearPlane(cursor / resolution);
	Locator::pickingSystem::value().PickUnderCursor({
	    .view =
	        {
	            .worldToClip = camera.GetViewProjectionMatrix(),
	            .resolution = resolution,
	            .near = camera.GetNearClip(),
	            .xScale = camera.GetProjectionMatrix()[0][0],
	            .camera = eye,
	            .cursor = cursor,
	        },
	    .nearPoint = nearPoint,
	    .seconds = seconds,
	    // Gripping the land, the interface holds what it picked
	    .locked = _handCameraState && _handPose == hand_navigation_pose::Pose::Grip,
	});
}

void Game::UpdateHandInterface()
{
	if (!_interface)
	{
		return;
	}
	_interface->SetCreaturePanel(std::nullopt);
	// The temple places the hand's tooltip itself
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	// The fighters' health and stamina, while a fight is on
	_interface->SetFightPanel(inTemple ? std::nullopt : Locator::creatureFightSystem::value().GetPanel());
	if (inTemple)
	{
		return;
	}
	// Outside it, the tooltip is drawn by the hand, which is at the cursor
	_interface->SetHandOnScreen(Locator::debugGui::value().IsMouseOverWindow() ? std::nullopt
	                                                                           : std::optional(glm::vec2(_mousePosition)));
	// Nor is the panel shown while a script has the cinema bars in
	if (!Locator::cinematicDirectorSystem::value().IsInterfaceActive())
	{
		return;
	}
	// The creature the hand is held to, or else the one it is over, any player's
	const auto& creatureHand = Locator::creatureHandSystem::value();
	auto creature = creatureHand.GetCreature();
	const bool rightButtonHeld = (Locator::gameActionSystem::value().GetPointerButtons() & SDL_BUTTON_RMASK) != 0;
	if (!creature.has_value() && creature_panel::Triggered(rightButtonHeld))
	{
		creature = _creatureUnderHand;
	}
	// Without one, the creature Creature Mode follows shows near the top of the screen, without the reward
	std::optional<float> reward = creatureHand.GetLastFeedbackSum();
	if (!creature.has_value() && Locator::creatureModeSystem::has_value())
	{
		creature = Locator::creatureModeSystem::value().GetCreature();
		reward.reset();
	}
	if (!creature.has_value())
	{
		return;
	}
	const auto* needs = Locator::entitiesRegistry::value().TryGet<const ecs::components::CreatureNeeds>(*creature);
	if (needs != nullptr)
	{
		// The reward is the hand's, which it keeps showing after letting go until it takes hold again
		_interface->SetCreaturePanel(creature_panel::FromNeeds(needs->needs, reward));
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

void Game::UpdateFloatingNumbers(float seconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	ecs::floating_number::StepAll(registry, seconds);
	if (!_interface)
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
	registry.Each<const ecs::components::FloatingNumber>(
	    [this, &placed, eye](entt::entity /*unused*/, const ecs::components::FloatingNumber& number) {
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
	_interface->SetFloatingNumbers(std::move(numbers));
}

void Game::ProcessHandToolTipTurn()
{
	if (!_interface || (Locator::temple::has_value() && Locator::temple::value().Active()))
	{
		return;
	}
	auto& toolTips = _interface->GetToolTips();
	// As the player's own totem moves on the screen, the hand shows the share it is held at, over anything else
	if (const auto tip = Locator::villageTotemSystem::has_value()
	                         ? Locator::villageTotemSystem::value().TakeShareToolTip(PlayerNames::PLAYER_ONE)
	                         : std::nullopt;
	    tip.has_value() && Locator::entitiesRegistry::value().Valid(tip->totem) &&
	    OnScreen(Locator::entitiesRegistry::value().Get<const ecs::components::Transform>(tip->totem).position).has_value())
	{
		toolTips.Force(ecs::village_totem::k_ShareToolTip, tip->percent);
	}
	// With the leash held, the hand says what a tap of the Action button does with it, before anything else
	if (!_interface->IsDialogOpen() && Locator::cinematicDirectorSystem::value().IsInterfaceActive())
	{
		const auto hovered = Locator::pickingSystem::value().GetPick().object;
		if (const auto tip = Locator::leashSystem::value().ToolTip(PlayerNames::PLAYER_ONE, hovered))
		{
			toolTips.Submit(*tip, gui::ToolTipAction::Apply, gui::ToolTipArrows::k_None);
			toolTips.ProcessTurn();
			return;
		}
	}
	// Over the player's own creature, the hand shows that it can take hold of it to stroke or slap it. It can hold other
	// players' creatures too, but the game only offers it for the player's own.
	const auto over = _creatureUnderHand.has_value() ? _creatureUnderHand : Locator::creatureHandSystem::value().GetCreature();
	const bool shown = !_interface->IsDialogOpen() && Locator::cinematicDirectorSystem::value().IsInterfaceActive();
	// While the player's creature duels, the hand offers to block over it, to attack over its opponent, and to manoeuvre
	// anywhere else, each by the Action button
	if (const auto tip = Locator::creatureFightSystem::value().HandTip(_creatureUnderHand))
	{
		if (shown)
		{
			toolTips.Submit(creature_fight::ToolTipIndex(*tip), gui::ToolTipAction::Apply, gui::ToolTipArrows::k_None);
		}
	}
	else if (over.has_value() && shown)
	{
		const auto* creature = Locator::entitiesRegistry::value().TryGet<const ecs::components::Creature>(*over);
		const auto* mind = Locator::entitiesRegistry::value().TryGet<const ecs::components::CreatureMindState>(*over);
		const bool asleep = mind != nullptr && creature_mind::IsAsleep(mind->idle);
		if (creature != nullptr && creature_hand::ShowsInteractTip(PlayerNames::PLAYER_ONE, creature->owner, asleep))
		{
			toolTips.Submit(creature_panel::k_InteractToolTip, gui::ToolTipAction::Select, gui::ToolTipArrows::k_None);
		}
	}
	toolTips.ProcessTurn();
}

bool Game::GameLogicLoop() noexcept
{
	using namespace ecs::components;
	using namespace ecs::systems;

	const auto currentTime = machine_clock::Ticks();
	const auto delta = std::chrono::milliseconds(currentTime - _lastGameLoopTime);
	auto& clock = Locator::time::value();

	// The game pauses the world while the player is in the temple, whose own turns keep the audio going
	if (Locator::temple::has_value() && Locator::temple::value().Active())
	{
		// NOLINTNEXTLINE(modernize-use-nullptr): clang-tidy bug
		if (delta >= k_TurnDuration * GetGameSpeed())
		{
			ProcessTempleAudioTurn();
			_lastGameLoopTime = currentTime;
		}
		return false;
	}

	if (clock.IsPaused())
	{
		// The ambience is silent while the game is paused
		Locator::audio::value().AtmosProcess(false);
		return false;
	}

	if (!clock.IsTurnDue())
	{
		return false;
	}
	clock.StartTurn();
	// The influence asked during the turn is measured from where the hands were at it
	Locator::influenceSystem::value().SetInGameTurn(true);
	ProcessHandToolTipTurn();

	// What moved since the last turn goes into its new map cell
	Locator::entitiesMap::value().Sync();

	// The players' temples take their turn first: one whose heart lost all its life moves on through its destruction
	if (Locator::templeDestructionSystem::has_value())
	{
		Locator::templeDestructionSystem::value().ProcessTurn();
	}
	// The sharks' turns start where they are, then the things the scripts walk along tracks go on, before the living
	Locator::sharkSystem::value().ProcessTurn();
	// The dances go on after the players and before the things walking tracks and the living
	Locator::danceSystem::value().ProcessTurn();
	Locator::walkPathSystem::value().ProcessTurn();

	auto& profiler = Locator::profiler::value();

	{
		auto pathfinding = profiler.BeginScoped(Profiler::Stage::PathfindingUpdate);
		Locator::pathfindingSystem::value().Update();
	}
	// The towns work out what they want, then their villagers act on it
	Locator::townDesireSystem::value().ProcessTurn();
	// A town's storage pit or village centre on fire calls its people together
	Locator::townSystem::value().ProcessTurn();
	Locator::chimneySmokeSystem::value().ProcessTurn();
	// How far the players' influence reaches, and its border
	Locator::influenceSystem::value().ProcessTurn(Locator::time::value().GetTurn());
	// The crops in the fields grow
	Locator::fieldSystem::value().ProcessTurn(Locator::time::value().GetTurn());
	// The fish come back to the fish farms
	Locator::fishFarmSystem::value().ProcessTurn(Locator::time::value().GetTurn());
	// The forests' growing trees grow, faster in the rain, and the forests spread
	Locator::forestSystem::value().GrowForests();
	{
		// The creatures age, grow, get hungry, tired and thirsty, and heal while they sleep
		auto creaturePhysiology = profiler.BeginScoped(Profiler::Stage::CreaturePhysiologyUpdate);
		Locator::creaturePhysiologySystem::value().ProcessTurn();
	}
	// A creature's home is its temple's pen, and in the pen it is shown smaller so that it fits
	Locator::creaturePenSystem::value().ProcessTurn();
	// The creatures' bodies follow their fatness, and their marks heal
	Locator::creatureAnimationSystem::value().ProcessTurn();
	Locator::creatureSkinSystem::value().ProcessTurn();
	{
		// A taut leash pulls its creature to the hand before the creature's mind thinks
		auto creatureLeash = profiler.BeginScoped(Profiler::Stage::CreatureLeashUpdate);
		Locator::leashSystem::value().ProcessTurn();
	}
	{
		// The creatures want things, and decide what to do while idle
		auto creatureMind = profiler.BeginScoped(Profiler::Stage::CreatureMindUpdate);
		Locator::creatureMindSystem::value().ProcessTurn();
	}
	{
		// They weigh their desires against what they could do about them, and change what they do for a pressing plan
		auto creaturePlanner = profiler.BeginScoped(Profiler::Stage::CreaturePlannerUpdate);
		Locator::creatureMindSystem::value().PlanTurn();
	}
	{
		// They learn from what the leash shows them, and copy the player
		auto creatureLearning = profiler.BeginScoped(Profiler::Stage::CreatureLearningUpdate);
		Locator::creatureMindSystem::value().LearnTurn();
	}
	{
		// They plan their routes and walk, run and turn
		auto creatureLocomotion = profiler.BeginScoped(Profiler::Stage::CreatureLocomotionUpdate);
		Locator::creatureLocomotionSystem::value().ProcessTurn();
	}
	{
		// They walk up to the things they act on, and what they carry makes them stronger
		auto creatureObjectActions = profiler.BeginScoped(Profiler::Stage::CreatureObjectActionUpdate);
		Locator::creatureObjectActionSystem::value().ProcessTurn();
	}
	{
		// Fights start and end, the fighters choose their moves, and creatures knocked out come round
		auto creatureCombat = profiler.BeginScoped(Profiler::Stage::CreatureCombatUpdate);
		Locator::creatureFightSystem::value().ProcessTurn();
		// Creatures fizz on out of sight or back in, after they have acted
		Locator::creatureFizzSystem::value().ProcessTurn();
	}
	{
		auto actions = profiler.BeginScoped(Profiler::Stage::LivingActionUpdate);
		Locator::livingActionSystem::value().Update();
	}
	// The living that walked this turn go into their new map cells before anything searches them
	Locator::entitiesMap::value().Sync();
	// The game burns its fires here, after the living and before the scripts and the miracles: a fire either of those
	// lights this turn waits for the next before it burns
	if (Locator::fireSystem::has_value())
	{
		Locator::fireSystem::value().MarkBurnPoint();
	}

	auto& lhvm = Locator::vm::value();
	lhvm.LookIn(lhvm::ScriptType::All);
	// Every object the scripts no longer hold in a variable lets go of its place in their table, after their turn
	Locator::scriptObjects::value().ReleaseUnreferenced();
	// The scripts' fade moves on with their turn
	Locator::cinematicDirectorSystem::value().ProcessTurn();
	// The advisors follow what they point at and look at
	Locator::advisorSystem::value().ProcessTurn();

	// The fireflies come out at nightfall and go home at dawn, by the time of day the turn began at
	if (Locator::fireflySystem::has_value())
	{
		Locator::fireflySystem::value().ProcessTurn();
	}

	// The time of day moves on
	Locator::skySystem::value().ProcessTurn();

	// The weather moves on, then the ambience follows the weather at the camera
	const auto cameraPosition = Locator::camera::value().GetOrigin();
	ecs::components::WeatherInfo weather {};
	if (Locator::weatherSystem::has_value())
	{
		auto& weatherSystem = Locator::weatherSystem::value();
		weatherSystem.Update(clock.GetTurn());
		// The storms that snow lay it on the land, and it melts
		Locator::snowSystem::value().ProcessTurn(weatherSystem.GetActiveStorms());
		weather = weatherSystem.GetWeatherSmooth(cameraPosition);
	}

	// The objects' looping sounds start again where they have stopped
	Locator::soundTagSystem::value().ProcessTurn(cameraPosition);
	// The worship sites charge their icons and store what their dancers chant, before the miracles draw their upkeep
	if (Locator::worshipSiteSystem::has_value())
	{
		Locator::worshipSiteSystem::value().ProcessChants();
	}
	{
		// The dispensers, then each miracle's upkeep, its own particle effect and what that effect tells it
		auto magic = profiler.BeginScoped(Profiler::Stage::MagicUpdate);
		Locator::magicSystem::value().ProcessTurn();
	}
	{
		// The flocks fly and the wolves run and hunt
		auto animals = profiler.BeginScoped(Profiler::Stage::AnimalsUpdate);
		Locator::animalSystem::value().ProcessTurn();
	}
	{
		// The shields' objects: the spiritual shields' marks and the physical shields' domes, and the people sheltering
		auto shields = profiler.BeginScoped(Profiler::Stage::MagicShieldsUpdate);
		Locator::magicShieldSystem::value().ProcessTurn();
	}
	{
		// The forest miracles' forests grow or wither
		auto forests = profiler.BeginScoped(Profiler::Stage::ForestsUpdate);
		Locator::forestSystem::value().ProcessTurn();
	}
	{
		// What is hot burns, cools and spreads, and the villagers near it react
		auto fire = profiler.BeginScoped(Profiler::Stage::FireUpdate);
		Locator::fireSystem::value().ProcessTurn();
	}
	{
		// What the miracles and other happenings made the living react to: belief for the villagers' towns, and the
		// creatures impressed
		auto reactions = profiler.BeginScoped(Profiler::Stage::ReactionsUpdate);
		Locator::reactionSystem::value().ProcessTurn();
	}
	{
		// The teleport stones: the passers-by turn aside into them and jump between them
		auto teleport = profiler.BeginScoped(Profiler::Stage::TeleportUpdate);
		Locator::teleportSystem::value().ProcessTurn();
	}
	// A creature loaded from what a last land kept sparkles into sight
	Locator::creatureCarryOverSystem::value().ProcessTurn();
	{
		// The particle effects not owned by a miracle step, and the spot visuals count down
		auto particles = profiler.BeginScoped(Profiler::Stage::ParticlesUpdate);
		Locator::particleSystem::value().ProcessTurn();
	}
	{
		// What a tornado carried and flung is let go once its particle has gone
		auto tornado = profiler.BeginScoped(Profiler::Stage::TornadoUpdate);
		Locator::tornadoSystem::value().ProcessTurn();
	}
	// A reward chest that thumped down goes into the map's cells
	if (Locator::rewardSystem::has_value())
	{
		Locator::rewardSystem::value().ProcessTurn();
	}
	// The scrolls and signs the scripts put up pulse, find what they stand on, and the signs whose tips were read start
	if (Locator::scriptHighlightSystem::has_value())
	{
		Locator::scriptHighlightSystem::value().ProcessTurn();
	}
	// The help's count of what the player has done moves on a turn, and its tip bubble stays while its sign shows
	if (Locator::helpProfileSystem::has_value())
	{
		Locator::helpProfileSystem::value().ProcessTurn();
	}
	if (Locator::tipBubbleSystem::has_value())
	{
		Locator::tipBubbleSystem::value().ProcessTurn();
	}
	// Then the physics, after the living, the fires, the reactions, the miracles and the particles have had their turn,
	// so a body any of them sets moving this turn flies this turn: what was thrown, dropped, knocked or pushed flies,
	// collides and comes to rest
	if (Locator::dynamicsSystem::has_value())
	{
		auto physics = profiler.BeginScoped(Profiler::Stage::PhysicsUpdate);
		Locator::dynamicsSystem::value().ProcessTurn();
	}
	// The pieces broken off buildings count down their time, and go when it runs out
	if (Locator::buildingDamageSystem::has_value())
	{
		Locator::buildingDamageSystem::value().ProcessTurn();
	}
	// What the hand holds stays where the hand is for the game, and is let go once it is gone
	if (Locator::handGrabSystem::has_value())
	{
		Locator::handGrabSystem::value().ProcessTurn();
	}
	// At the end of the turn the vortices between the lands open, close and level the ground under them
	if (Locator::vortexSystem::has_value())
	{
		Locator::vortexSystem::value().ProcessTurn();
	}
	// Once the whole turn is over, the local player whose temple is being destroyed has lost
	if (Locator::templeDestructionSystem::has_value())
	{
		Locator::templeDestructionSystem::value().EndTurn();
	}

	// Each turn ends with the camera taking the alignment of the player of most influence at its eye
	Locator::alignmentSystem::value().UpdateTurn(cameraPosition);
	// The temples' outsides follow their players' alignments
	Locator::templeExteriorSystem::value().UpdateTurn();
	// And their worship sites wear their looks
	if (Locator::worshipSiteSystem::has_value())
	{
		Locator::worshipSiteSystem::value().UpdateTurn();
	}

	if (_atmosAudio)
	{
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
		    .inCitadel = false,
		    .videoPlaying = Locator::videoSystem::value().IsPlaying(),
		});
	}

	ProcessMusicTurn(cameraPosition, false);
	Locator::influenceSystem::value().SetInGameTurn(false);

	_lastGameLoopTime = currentTime;
	_turnDeltaTime = delta;

	return false;
}

void Game::ProcessMusicTurn(glm::vec3 cameraPosition, bool inCitadel)
{
	// The game picks the music for the turn
	if (!_gameMusic)
	{
		return;
	}
	const auto& alignment = Locator::alignmentSystem::value();
	// The land's music waits while a script holds the cinema bars, and while they slide in or out
	bool cinema = false;
	if (Locator::cinematicDirectorSystem::has_value())
	{
		const auto& director = Locator::cinematicDirectorSystem::value();
		cinema =
		    (director.IsWideScreenOn() && director.GetWideScreenOwner() != 0) || !director.IsWideScreenTransitionFinished();
	}
	audio::GameMusic::TurnInputs music {
	    .turn = GetTurn(),
	    .camera = cameraPosition,
	    .groundHeight = Locator::terrainSystem::value().GetHeightAt(glm::xz(cameraPosition)),
	    .inCitadel = inCitadel,
	    .alignment = alignment.GetCameraAlignment(),
	    .playerAlignment = alignment.GetPlayerAlignment(Locator::playerSystem::value().GetLocalPlayer()),
	    .cinema = cinema,
	    .towns = {},
	};
	Locator::entitiesRegistry::value().Each<const ecs::components::Town, const Tribe, const ecs::components::Transform>(
	    [&music](const ecs::components::Town& town, const Tribe tribe, const ecs::components::Transform& transform) {
		    music.towns.push_back({
		        .position = transform.position,
		        .tribe = static_cast<int32_t>(tribe),
		        .id = town.id,
		    });
	    });
	_gameMusic->ProcessTurn(music);
}

void Game::ProcessTempleRoomKeys()
{
	if (!Locator::temple::has_value())
	{
		return;
	}
	// The keys for the temple's rooms take the player into the temple at that room's path in, and inside the temple
	// they cut to the room
	constexpr std::array<std::pair<input::BindableActionMap, TempleRoom>, 6> k_RoomKeys {{
	    {input::BindableActionMap::ZOOM_TO_INSIDE_TEMPLE, TempleRoom::Main},
	    {input::BindableActionMap::ZOOM_TO_CREATURE_ROOM, TempleRoom::CreatureCave},
	    {input::BindableActionMap::ZOOM_TO_CHALLENGE_ROOM, TempleRoom::Challenge},
	    {input::BindableActionMap::ZOOM_TO_SAVE_GAME_ROOM, TempleRoom::SaveGame},
	    {input::BindableActionMap::ZOOM_TO_OPTIONS_ROOM, TempleRoom::Options},
	    {input::BindableActionMap::ZOOM_TO_LIBRARY, TempleRoom::Credits},
	}};
	const auto& actions = Locator::gameActionSystem::value();
	auto& temple = Locator::temple::value();
	for (const auto& [action, room] : k_RoomKeys)
	{
		if (!actions.Get(action) || !actions.GetChanged(action))
		{
			continue;
		}
		if (temple.Active())
		{
			temple.GoToRoom(room);
		}
		else
		{
			temple.Activate(room);
		}
		return;
	}
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

bool Game::Update() noexcept
{
	auto& profiler = Locator::profiler::value();

	profiler.Frame();

	auto& camera = Locator::camera::value();
	auto& config = Locator::config::value();

	auto previous = profiler.GetEntries().at(profiler.GetEntryIndex(-1)).frameStart;
	auto current = profiler.GetEntries().at(profiler.GetEntryIndex(0)).frameStart;
	// Prevent spike at first frame
	if (previous.time_since_epoch().count() == 0)
	{
		current = previous;
	}
	auto deltaTime = std::chrono::duration_cast<std::chrono::microseconds>(current - previous);

	Locator::debugGui::value().SetScale(config.guiScale);
	Locator::time::value().Update();
	// The debug inspector answers what was asked since the last frame, and holds or releases the game for stepping,
	// before the frame's input and turn
	if (Locator::inspector::has_value())
	{
		Locator::inspector::value().Service();
	}
	// Stepped deterministically, a frame takes the fixed time the clock gives it, whatever the wall clock says
	if (const auto fixed = Locator::time::value().GetFixedFrameTime(); fixed.has_value())
	{
		deltaTime = std::chrono::duration_cast<std::chrono::microseconds>(*fixed);
	}

	// The physics world isn't stepped: the game's objects don't move as rigid bodies, and the world only answers the
	// rays cast for the hand, the camera and the like. Stepping it let the features fall and lose their turn.

	// Input events
	{
		auto sdlInput = profiler.BeginScoped(Profiler::Stage::SdlInput);
		// The debug windows' presses of the options screen's actions are made by the action map's frame
		auto& actions = Locator::gameActionSystem::value();
		// The world's camera is the one turned with the mouse, which holds the cursor and the hand still meanwhile
		actions.AllowCursorFreeze(
		    !(Locator::temple::has_value() && Locator::temple::value().Active()) &&
		    dynamic_cast<DefaultWorldCameraModel*>(&camera.GetModel()) != nullptr &&
		    (Locator::cameraHelpSystem::value().Get().features & camera_help::feature::k_RotateAroundMouse) != 0);
		// The land's scripts may take the camera's keys away, as the tutorials do
		actions.SetBlockedActions(Locator::cameraHelpSystem::value().Get().BlockedActions());
		if (!Locator::debugGui::value().StealsFocus() || actions.HasQueuedPresses())
		{
			actions.Frame();
		}
		// A hand demonstration plays its records due now through the hand's own input
		if (Locator::handDemoSystem::has_value())
		{
			Locator::handDemoSystem::value().Update();
		}
		SDL_Event e;
		while (SDL_PollEvent(&e) != 0)
		{
			// While an agent drives the game, the player's mouse and keyboard are kept out of it
			if (!actions.AdmitEvent(e))
			{
				continue;
			}
			Locator::events::value().Create<SDL_Event>(e);
		}
		if (actions.IsCursorFrozen())
		{
			_mousePosition = glm::ivec2(actions.GetMousePosition());
		}
		// A miracle's camera path keeps the camera until the player moves it with the movement keys or grips the land,
		// which hand it straight back; meanwhile the player's other camera controls do nothing
		auto& cameraPaths = Locator::cameraPathSystem::value();
		cameraPaths.HandlePlayerControl(
		    {.movementKey = actions.GetAny(input::BindableActionMap::MOVE_LEFT, input::BindableActionMap::MOVE_RIGHT,
		                                   input::BindableActionMap::MOVE_FORWARDS, input::BindableActionMap::MOVE_BACKWARDS),
		     .frameMilliseconds =
		         static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(deltaTime).count()),
		     .grippingLand = _handGripping && !_handRotating});
		if (!cameraPaths.HoldsCamera())
		{
			camera.HandleActions(deltaTime);
		}
		if (const auto warp = actions.GetCursorWarp(); warp.has_value())
		{
			_mousePosition = *warp;
		}
		ProcessTempleRoomKeys();
		_shortcutKeys.Update();
	}
	Locator::cameraPathSystem::value().Update(deltaTime);
	// The numbers floating up from things rise and fade, and are shown where they are on the screen
	UpdateFloatingNumbers(std::chrono::duration<float>(deltaTime).count());

	if (!config.running || _quitRequested)
	{
		return false;
	}

	// ImGui events + prepare
	{
		auto guiLoop = profiler.BeginScoped(Profiler::Stage::GuiLoop);
		// The debug windows keep off the mouse while the player's input is locked out, and show a notice
		Locator::debugGui::value().SetInputLock(Locator::gameActionSystem::value().IsPlayerInputBlocked(),
		                                        Locator::gameActionSystem::value().GetScriptedPointer().has_value());
		// The debug menu bar comes up with the game's menu, or always without it
		Locator::debugGui::value().SetMenuBarVisible(!_interface || _interface->GetMenu().IsOpen());
		if (Locator::debugGui::value().Loop())
		{
			return false; // Quit event
		}
	}
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
		Locator::creatureModeSystem::value().Update(deltaTime, {.handGripping = _handGripping});
	}
	if (Locator::creatureCaveSystem::has_value())
	{
		auto creatureCave = profiler.BeginScoped(Profiler::Stage::CreatureCaveUpdate);
		Locator::creatureCaveSystem::value().Update();
	}
	if (Locator::tattooEditorSystem::has_value())
	{
		Locator::tattooEditorSystem::value().Update(std::chrono::duration<float, std::milli>(deltaTime).count());
	}

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
		camera.Update(std::chrono::duration_cast<std::chrono::microseconds>(CameraStepTime()));
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
	FitNearClip();
	// The temple's camera may have taken the player out of the temple
	if (Locator::temple::has_value())
	{
		Locator::temple::value().Update(deltaTime);
	}
	Locator::cameraBookmarkSystem::value().Update(deltaTime);
	// The villagers a script draws in high detail are drawn as usual once its cinema bars are gone
	Locator::highDetailSystem::value().Update();
	if (_interface)
	{
		_interface->Update(std::chrono::duration_cast<std::chrono::duration<float>>(deltaTime).count());
		HandleInterfaceAction();
	}
	GripLandscapeEffect::Update(std::chrono::duration_cast<std::chrono::duration<float>>(deltaTime).count());

	// Update Game Logic in Registry
	{
		auto gameLogic = profiler.BeginScoped(Profiler::Stage::GameLogic);
		if (GameLogicLoop())
		{
			return false; // Quit event
		}
	}

	// The frame's game time: none while paused, quicker or slower with the game speed
	auto& clock = Locator::time::value();
	clock.UpdateFrame();
	// A full-screen video decodes the frames due by the real clock, fades, and ends
	Locator::videoSystem::value().Update(std::chrono::steady_clock::now());
	const auto gameTime = std::chrono::duration<float, std::milli>(clock.GetFrameGameTime());
	Locator::alignmentSystem::value().Update(gameTime);
	{
		auto actions = profiler.BeginScoped(Profiler::Stage::VegetationUpdate);
		Locator::vegetation::value().Update(gameTime);
	}
	// The clouds drift with the wind, then every mist and cloud animates
	Locator::cloudSystem::value().Update(gameTime);
	// The rain falls as the storm nearest the camera has it
	Locator::rainSystem::value().Update(std::chrono::duration<float>(gameTime).count(), camera.GetOrigin());
	// The static the fizzing creatures are drawn through slides across them
	Locator::creatureFizzSystem::value().UpdateFrame(std::chrono::duration<float>(gameTime).count());
	// The rings on the water grow and fade
	Locator::waterRingSystem::value().Update(gameTime);
	{
		// The flames, steam and smoke of what burns move on
		auto fire = profiler.BeginScoped(Profiler::Stage::FireUpdate);
		Locator::fireSystem::value().Update(std::chrono::duration<float>(gameTime).count());
	}
	// The vortices' swirls and effects step with the frame, and their marks on the ground follow their openness
	if (Locator::vortexSystem::has_value())
	{
		Locator::vortexSystem::value().UpdateFrame(std::chrono::duration<float>(gameTime).count());
	}
	// The reward chests from the sky fall and thump down, and their dust fades
	if (Locator::rewardSystem::has_value())
	{
		Locator::rewardSystem::value().Update(std::chrono::duration<float, std::milli>(gameTime).count());
	}
	{
		// The blasts' rubble lies and fades, their dust flies and the camera shakes
		auto explosions = profiler.BeginScoped(Profiler::Stage::ExplosionUpdate);
		Locator::explosionSystem::value().Update(std::chrono::duration<float, std::milli>(gameTime).count());
	}
	// The fireflies out drift about where they are between their last two turns
	if (Locator::fireflySystem::has_value())
	{
		Locator::fireflySystem::value().Update(std::chrono::duration<float, std::milli>(gameTime).count(),
		                                       clock.GetTurnFraction());
	}
	// The scrolls and signs spin, scale and glow towards the camera, and their glints play
	if (Locator::scriptHighlightSystem::has_value())
	{
		Locator::scriptHighlightSystem::value().UpdateFrame(std::chrono::duration<float, std::milli>(gameTime).count(),
		                                                    clock.GetTurnFraction(), camera.GetOrigin());
	}
	// The moving bodies are drawn between their last two turns, and the dust their landings threw up flies and fades
	if (Locator::dynamicsSystem::has_value())
	{
		Locator::dynamicsSystem::value().UpdateFrame(clock.GetTurnFraction(), std::chrono::duration<float>(gameTime).count());
	}
	{
		// The fight animations play, blows land and the fighters move as their animations carry them
		auto creatureCombat = profiler.BeginScoped(Profiler::Stage::CreatureCombatUpdate);
		Locator::creatureFightSystem::value().Update(gameTime);
	}
	{
		// The animals are drawn between their last two turns, their models posed by their clips
		auto animals = profiler.BeginScoped(Profiler::Stage::AnimalsUpdate);
		Locator::animalSystem::value().Update(clock.GetTurn(), clock.GetTurnFraction());
		// The clips the villagers' states play go on, and the sounds of their frames play
		Locator::livingActionSystem::value().UpdatePoses(clock.GetTurn(), clock.GetTurnFraction());
		// The sharks swim between their last two turns and leave their wakes
		Locator::sharkSystem::value().Update(gameTime, clock.GetTurnFraction());
		// The gates and the other scenery the scripts open and close play on, and the plinths' stones sit or sink
		Locator::animatedStaticSystem::value().Update(clock.GetTurn(), clock.GetTurnFraction());
	}
	{
		// The creatures are drawn moving between the last two turns
		auto creatureLocomotion = profiler.BeginScoped(Profiler::Stage::CreatureLocomotionUpdate);
		Locator::creatureLocomotionSystem::value().Update(clock.GetTurnFraction());
	}
	{
		// Drops of creatures' sick fly and fall
		auto creaturePhysiology = profiler.BeginScoped(Profiler::Stage::CreaturePhysiologyUpdate);
		Locator::creaturePhysiologySystem::value().Update(std::chrono::duration<float>(gameTime).count());
	}
	{
		// What they do with things plays on their bodies
		auto creatureObjectActions = profiler.BeginScoped(Profiler::Stage::CreatureObjectActionUpdate);
		Locator::creatureObjectActionSystem::value().Update(gameTime);
	}
	{
		// The creatures breathe, act, pull faces and look about
		auto creatureAnimation = profiler.BeginScoped(Profiler::Stage::CreatureAnimationUpdate);
		Locator::creatureAnimationSystem::value().Update(gameTime);
	}
	{
		// They take hold of and let go of things with their hands as posed, and what they let go of flies
		auto creatureObjectActions = profiler.BeginScoped(Profiler::Stage::CreatureObjectActionUpdate);
		Locator::creatureObjectActionSystem::value().LateUpdate(gameTime);
	}
	{
		// Their hair swings from the posed bodies
		auto creatureHair = profiler.BeginScoped(Profiler::Stage::CreatureHairUpdate);
		Locator::creatureHairSystem::value().Update(gameTime);
	}
	{
		// They make the sounds of the moments their animations have played past
		auto creatureAudio = profiler.BeginScoped(Profiler::Stage::CreatureAudioUpdate);
		Locator::creatureAudioSystem::value().Update(gameTime);
	}
	{
		// Their footprints fade away
		auto creatureFootprints = profiler.BeginScoped(Profiler::Stage::CreatureFootprintsUpdate);
		Locator::footprintSystem::value().Update(gameTime);
	}
	{
		// Their skins are painted again where their alignment, tattoos or marks have changed
		auto creatureSkin = profiler.BeginScoped(Profiler::Stage::CreatureSkinUpdate);
		Locator::creatureSkinSystem::value().Update();
	}
	{
		// What a tornado carries is drawn where it whirls, between the last two turns
		auto tornado = profiler.BeginScoped(Profiler::Stage::TornadoUpdate);
		Locator::tornadoSystem::value().Update(clock.GetTurnFraction());
	}
	// The snow falls as the rain does
	Locator::snowfallSystem::value().Update(std::chrono::duration<float>(gameTime).count(),
	                                        Locator::rainSystem::value().GetFall());
	// The homes' smoke rises while someone is in
	Locator::chimneySmokeSystem::value().Update(gameTime);
	Locator::influenceSystem::value().Update(gameTime);
	// What the hand shows past the border is shown with the hand
	if (IsHandDrawn())
	{
		Locator::influenceSystem::value().ShowHandInfluence(gameTime);
	}
	Locator::mistSystem::value().Update(gameTime);
	Locator::villageLightSystem::value().Update(gameTime);
	// The town totems ease to their shares
	Locator::villageTotemSystem::value().Update(gameTime.count());
	Locator::fieldSystem::value().Update(gameTime);
	// The shoals near the camera swim, and dart from what scared them
	Locator::fishFarmSystem::value().Update(std::chrono::duration<float>(gameTime).count(),
	                                        Locator::camera::value().GetOrigin());
	Locator::cinematicDirectorSystem::value().Update(gameTime);
	// The advisors move and act once a frame, by the real time inside the temple and the game's otherwise
	{
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
	// The scripts' dialogue: the voices, the player's click and the newest text sliding in
	{
		const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
		const auto* keys = SDL_GetKeyboardState(nullptr);
		const bool demoPlaying = Locator::handDemoSystem::has_value() && Locator::handDemoSystem::value().IsPlaying(0);
		Locator::helpTextSystem::value().Update({
		    .gameMs = static_cast<uint32_t>(clock.GetFrameGameTime().count()),
		    .realMs = static_cast<uint32_t>(clock.GetFrameRealTime().count()),
		    .inTemple = inTemple,
		    // Nor can the player click or key through the advisors' lines while a hand demonstration plays
		    .click = _dialogueClick && !demoPlaying,
		    .skipKey = !demoPlaying && keys != nullptr && keys[SDL_SCANCODE_KP_ENTER] != 0,
		});
		_dialogueClick = false;
	}
	// The tip bubble's time to show runs down by the game's time while it is up
	Locator::tipBubbleSystem::value().UpdateFrame(static_cast<float>(clock.GetFrameGameTime().count()));
	// The cinema bars coming in hide the game's dialogs
	if (Locator::cinematicDirectorSystem::value().TakeHideDialogs() && _interface && _interface->GetMenu().IsOpen())
	{
		_interface->GetMenu().Close();
	}

	// Update Uniforms
	{
		auto profilerScopedUpdateUniforms = profiler.BeginScoped(Profiler::Stage::UpdateUniforms);

		// Update Hand and intersection point
		// Upright where nothing is under the cursor
		ecs::components::Transform intersectionTransform {
		    .position = glm::vec3(0.0f), .rotation = glm::mat3(1.0f), .scale = glm::vec3(1.0f)};
		bool enterTemple = false;
		// The point the interface picks under the cursor, which the hand's influence is tested at
		std::optional<map_coords::MapCoords> handPick;
		{
			const auto screenSize =
			    Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::zero<glm::ivec2>();
			const auto scale = glm::vec3(50.0f, 50.0f, 50.0f);
			if (screenSize.x > 0 && screenSize.y > 0)
			{
				glm::vec3 rayOrigin;
				glm::vec3 rayDirection;
				camera.DeprojectScreenToWorld(static_cast<glm::vec2>(_mousePosition) / static_cast<glm::vec2>(screenSize),
				                              rayOrigin, rayDirection);

				_cursorWorldPosition.reset();
				if (Locator::temple::has_value() && Locator::temple::value().Active())
				{
					// In the temple, the hand goes where the cursor meets the room, turning to its surface
					if (const auto hit = Locator::temple::value().GetCursorHit())
					{
						const auto seconds = std::chrono::duration_cast<std::chrono::duration<float>>(deltaTime).count();
						_handTempleNormal.SetDestination(hit->normal, k_HandTempleTurnTime);
						_handTempleNormal.Update(seconds);
						const auto normal = _handTempleNormal.GetValue();
						_cursorWorldPosition = hit->point;
						intersectionTransform.position = hit->point;
						intersectionTransform.rotation =
						    glm::length(normal) > 0.0f
						        ? glm::mat3_cast(glm::rotation(glm::vec3(0.0f, 1.0f, 0.0f), glm::normalize(normal)))
						        : glm::mat3(1.0f);
					}
				}
				else if (!glm::any(glm::isnan(rayOrigin) || glm::isnan(rayDirection)))
				{
					// The Action button on the player's own temple's entrance takes them inside, when the scripts let it
					const auto& actions = Locator::gameActionSystem::value();
					if (Locator::entitiesRegistry::value().Context().scriptLetsTempleBeEntered &&
					    Locator::cinematicDirectorSystem::value().IsInterfaceActive() &&
					    !Locator::magicSystem::value().IsHandBusy() && actions.GetChanged(input::BindableActionMap::ACTION) &&
					    actions.Get(input::BindableActionMap::ACTION))
					{
						enterTemple = Locator::templeExteriorSystem::value().EntranceAt(rayOrigin, rayDirection) ==
						              PlayerNames::PLAYER_ONE;
					}
					// The leash keys, and the Action button tapping leash posts, and with the leash held giving orders and
					// tying the leash to things.
					// Not while the debug windows have the keyboard or mouse, as when typing in a text field: the controls
					// aren't updated then, so a key just pressed would read as pressed again every frame.
					if (!Locator::debugGui::value().StealsFocus())
					{
						auto& leashes = Locator::leashSystem::value();
						leashes.HandleInput(rayOrigin, rayDirection, static_cast<glm::vec2>(_mousePosition),
						                    machine_clock::Ticks(), _actionPressTaken);
						_actionPressTaken = false;
					}
					// The gestures drawn with the hand: circles and power-ups for the miracles, the leash's gestures, and
					// the scribble that shakes off what the hand holds
					{
						auto gestures = profiler.BeginScoped(Profiler::Stage::GestureUpdate);
						UpdateGestures(camera, screenSize,
						               std::chrono::duration_cast<std::chrono::duration<float>>(deltaTime).count());
					}
					// What the interface picked under the cursor as the last frame was drawn: the object nearest the camera, or
					// else the land or the sea. The hand's influence is tested at that point.
					const auto& picking = Locator::pickingSystem::value();
					const auto& pick = picking.GetPick();
					_cursorOnObject = pick.object.has_value();
					if (pick.point.has_value())
					{
						handPick = map_coords::FromMetres({pick.point->x, pick.point->z});
					}
					// The hand rests over what was picked: before a villager or an animal, on a model it feels the triangles
					// of, or else on the land, turned to its slope
					const auto [eye, nearPoint] =
					    camera.OnNearPlane(static_cast<glm::vec2>(_mousePosition) / static_cast<glm::vec2>(screenSize));
					const auto cursorDirection = glm::normalize(nearPoint - eye);
					_handSurfaceUp.reset();
					_handOverObject = false;
					std::optional<glm::vec3> rest;
					if (pick.object.has_value())
					{
						const auto& registry = Locator::entitiesRegistry::value();
						const auto object = *pick.object;
						const auto& grab = Locator::handGrabSystem::value();
						const auto held = grab.GetHeld();
						const auto* mesh = registry.TryGet<const ecs::components::Mesh>(object);
						auto& meshes = Locator::resources::value().GetMeshes();
						const bool posed = mesh != nullptr && meshes.Contains(mesh->id) && meshes.Handle(mesh->id)->IsBoned();
						const auto feel = hand_feel::FeelOf({
						    .living =
						        registry.AnyOf<ecs::components::Villager, ecs::components::Animal, ecs::components::Creature>(
						            object),
						    .creature = registry.AllOf<ecs::components::Creature>(object),
						    .posedModel = posed,
						    .animatedStatic = registry.AllOf<ecs::components::AnimatedStatic>(object),
						    .ignored = registry.AllOf<ecs::components::CarriedByTornado>(object) || held == object,
						});
						// Holding something, the hand feels along the line to the cursor's point on the land, raised by
						// its lift for what it holds
						std::optional<glm::vec3> raisedGround;
						if (held.has_value())
						{
							raisedGround = pick.land.value_or(_handPosition) + glm::vec3(0.0f, grab.GetCursorRaise(), 0.0f);
						}
						const auto feltDirection = hand_feel::FeelDirection(eye, cursorDirection, raisedGround);
						if (feel == hand_feel::Feel::AtPosition)
						{
							rest = hand_feel::RestAtPosition(eye, cursorDirection, feltDirection,
							                                 registry.Get<const ecs::components::Transform>(object).position,
							                                 ecs::systems::object_measures::TwoDRadius(registry, object));
							_handOverObject = true;
						}
						else if (feel == hand_feel::Feel::OnModel)
						{
							_handOverObject = true;
							if (const auto felt = picking.FeelModel(object, eye, feltDirection))
							{
								std::optional<hand_feel::Holding> holding;
								if (held.has_value())
								{
									holding = hand_feel::Holding {
									    .radius = ecs::systems::object_measures::TwoDRadius(registry, *held),
									    .creature = std::nullopt,
									};
									if (registry.AllOf<ecs::components::Creature>(object))
									{
										const auto& at = registry.Get<const ecs::components::Transform>(object).position;
										holding->creature = hand_feel::CreatureReach {
										    .centre = {at.x, at.z},
										    .radius = ecs::systems::object_measures::TwoDRadius(registry, object),
										};
									}
								}
								const auto onModel = hand_feel::RestOnModel(eye, cursorDirection, feltDirection, felt->point,
								                                            felt->normal, FeelsModel(object), holding);
								rest = onModel.point;
								_handSurfaceUp = onModel.up;
							}
						}
					}
					const auto restPoint = rest.has_value() ? rest : pick.point;
					if (restPoint.has_value())
					{
						const auto up = _handSurfaceUp.has_value()
						                    ? glm::normalize(*_handSurfaceUp)
						                    : Locator::terrainSystem::value().GetNormalAt({restPoint->x, restPoint->z});
						intersectionTransform.position = *restPoint;
						intersectionTransform.rotation = glm::mat3_cast(glm::rotation(glm::vec3(0.0f, 1.0f, 0.0f), up));
						_cursorWorldPosition = *restPoint;
					}
				}
				intersectionTransform.scale = scale;
			}
		}

		if (enterTemple && Locator::temple::has_value())
		{
			Locator::temple::value().Activate();
		}

		// Update Hand
		{
			// Put away, it stays where it was and does nothing until it comes back
			if (IsHandDrawn())
			{
				const glm::mat4 modelRotationCorrection = glm::eulerAngleX(glm::radians(90.0f));

				const auto handEntity =
				    Locator::handSystem::value()
				        .GetPlayerHands()[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)];
				auto& handTransform = Locator::entitiesRegistry::value().Get<ecs::components::Transform>(handEntity);
				UpdateHandNavigation(handTransform);
				UpdateHandKnock(handTransform);
				if (Locator::temple::has_value() && Locator::temple::value().Active())
				{
					if (!_handGripping)
					{
						handTransform.rotation = glm::eulerAngleY(camera.GetRotation().y) * modelRotationCorrection;
						handTransform.rotation = intersectionTransform.rotation * handTransform.rotation;
					}
				}
				else
				{
					OrientHand(handTransform, glm::mat3(glm::eulerAngleY(camera.GetRotation().y) * modelRotationCorrection),
					           intersectionTransform.rotation * glm::vec3(0.0f, 1.0f, 0.0f),
					           std::chrono::duration_cast<std::chrono::duration<float>>(deltaTime).count());
				}
				PlaceHand(handTransform, std::chrono::duration_cast<std::chrono::duration<float>>(deltaTime).count());
				// Held to a creature, the hand rests on its body under the cursor, stroking and slapping it
				{
					auto creatureHand = profiler.BeginScoped(Profiler::Stage::CreatureHandUpdate);
					const auto screenSize =
					    Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::zero<glm::ivec2>();
					_handOnCreature.reset();
					_creatureUnderHand.reset();
					if (screenSize.x > 0 && screenSize.y > 0)
					{
						auto& hands = Locator::creatureHandSystem::value();
						glm::vec3 rayOrigin;
						glm::vec3 rayDirection;
						camera.DeprojectScreenToWorld(static_cast<glm::vec2>(_mousePosition) /
						                                  static_cast<glm::vec2>(screenSize),
						                              rayOrigin, rayDirection);
						// The hand isn't over the world while it is over a debug window, or in the temple
						const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
						if (!inTemple && !Locator::debugGui::value().IsMouseOverWindow())
						{
							_creatureUnderHand = hands.CreatureUnderCursor();
						}
						if (hands.GetCreature().has_value())
						{
							_handOnCreature = hands.Update(rayOrigin, rayDirection, static_cast<glm::vec2>(_mousePosition),
							                               HandStepSeconds());
						}
						if (_handOnCreature.has_value())
						{
							handTransform.position = _handOnCreature->position;
						}
					}
					UpdateHandInterface();
				}
				{
					auto magic = profiler.BeginScoped(Profiler::Stage::MagicUpdate);
					// Food and wood pouring from the hand lift it and tip it forward, and what it pours comes from there
					const auto pour = Locator::magicSystem::value().GetHandPour(Locator::time::value().GetTurnFraction());
					// The hand is drawn where a scenario puts it, and stays where a pour that holds it began
					const auto driven = Locator::magicSystem::value().GetDrivenHand();
					if (driven.has_value())
					{
						handTransform.position = driven->handPosition;
					}
					handTransform.position = pour.pinned.value_or(handTransform.position);
					handTransform.position.y += pour.raise;
					// A seed in the hand lifts it, by how it is held; the pour tips it as the hand is posed
					// (magic::HandHoldPoser)
					if (const auto held = magic::HandHoldPoser::Find())
					{
						// Measured to the land under the cursor, or under the hand a scenario puts
						const auto land = driven.has_value() ? driven->point : _cursorWorldPosition;
						handTransform.position.y += magic::HandHoldPoser::Lift(
						    *held, glm::distance(camera.GetOrigin(), land.value_or(handTransform.position)));
					}
					else if (const auto screenSize = Locator::windowing::has_value() ? Locator::windowing::value().GetSize()
					                                                                 : glm::zero<glm::ivec2>();
					         Locator::handGrabSystem::has_value() && screenSize.x > 0 && screenSize.y > 0)
					{
						// A thing the hand takes or holds lifts it, and makes ready to throw, its spring drags it
						glm::vec3 rayOrigin;
						glm::vec3 rayDirection;
						camera.DeprojectScreenToWorld(static_cast<glm::vec2>(_mousePosition) /
						                                  static_cast<glm::vec2>(screenSize),
						                              rayOrigin, rayDirection);
						const auto land = _cursorWorldPosition.value_or(handTransform.position);
						handTransform.position = Locator::handGrabSystem::value().UpdateFrame({
						    .target = handTransform.position,
						    .rayOrigin = rayOrigin,
						    .rayDirection = rayDirection,
						    .camera = camera.GetOrigin(),
						    .cursorGround = _cursorWorldPosition,
						    .handSize = HandAnimation::SizeAtDistance(glm::distance(camera.GetOrigin(), land)),
						    .seconds = std::chrono::duration_cast<std::chrono::duration<float>>(deltaTime).count(),
						    .gameMs = static_cast<uint32_t>(std::lround(gameTime.count())),
						    .nowMs = machine_clock::Ticks(),
						    .turn = Locator::time::value().GetTurn(),
						});
					}
					UpdateMagicHand(handTransform.position,
					                std::chrono::duration_cast<std::chrono::duration<float>>(deltaTime).count());
				}
				// Holding a town's totem, the mouse slides it up and down and the hand stays on its icon
				if (Locator::villageTotemSystem::has_value() && Locator::villageTotemSystem::value().GetGripped().has_value())
				{
					auto& totems = Locator::villageTotemSystem::value();
					const auto screenSize =
					    Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::ivec2(0);
					// The pointer is put back on the hand every frame, so how far it went from there is how far the mouse
					// moved
					const int up = _totemPointer.has_value() ? _totemPointer->y - _mousePosition.y : 0;
					totems.Slide(static_cast<float>(up), static_cast<float>(screenSize.y));
					if (const auto hold = totems.GetHandHold())
					{
						handTransform.position = hold->position;
						// It faces the way it always does, tipped forwards over the icon
						using namespace hand_orientation;
						const auto facingCamera = glm::mat3(glm::eulerAngleY(camera.GetRotation().y) * modelRotationCorrection);
						const auto cameraHeading = HeadingAlongRay(camera.GetForward(), _handHeading);
						handTransform.rotation =
						    TipForwards(TurnToHeading(facingCamera, cameraHeading, _handHeading), _handHeading, hold->tilt);
						// The pointer goes where the hand is on the screen, while it is on it
						glm::vec3 screen;
						if (camera.ProjectWorldToScreen(hold->position, glm::vec4(0.0f, 0.0f, glm::vec2(screenSize)), screen))
						{
							const glm::ivec2 at {std::lrint(screen.x), std::lrint(screen.y)};
							if (at.x >= 0 && at.y >= 0 && at.x < screenSize.x && at.y < screenSize.y)
							{
								// Only put when it isn't there already
								if (at != _mousePosition)
								{
									Locator::gameActionSystem::value().WarpCursor(at);
									_mousePosition = at;
								}
								_totemPointer = at;
							}
						}
					}
				}
				UpdateMagicHand(handTransform.position,
				                std::chrono::duration_cast<std::chrono::duration<float>>(deltaTime).count());
			}
			{
				// The globes and the hand show their miracles
				auto miracleFx = profiler.BeginScoped(Profiler::Stage::MiracleFxUpdate);
				Locator::miracleFxSystem::value().Update(
				    std::chrono::duration_cast<std::chrono::duration<float>>(deltaTime).count(),
				    std::chrono::duration<float>(gameTime).count());
			}
			Locator::entitiesRegistry::value().SetDirty();
		}

		// The leashes' ropes swing from where the hand now is
		{
			auto creatureLeash = profiler.BeginScoped(Profiler::Stage::CreatureLeashUpdate);
			Locator::leashSystem::value().Update(std::chrono::duration<float>(gameTime).count());
		}

		// Holding a miracle's seed, the hand takes the still pose of its hold, sways with the seed and tips with a pour. A
		// thing it picked up it holds the same way.
		const auto heldSeed = magic::HandHoldPoser::Find();
		auto heldThing = heldSeed;
		if (!heldThing.has_value() && Locator::handGrabSystem::has_value())
		{
			if (const auto held = Locator::handGrabSystem::value().GetHeldPose())
			{
				heldThing = magic::HandHoldPoser::HeldSeed {
				    .entity = held->object,
				    .hold = held->hold,
				    .hang = held->hang,
				    .reach = held->reach,
				    .yRotate = 0.0f,
				    .effectInFingers = false,
				};
			}
		}
		bool holdingSeed = false;
		{
			const auto handEntity = Locator::handSystem::value()
			                            .GetPlayerHands()[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)];
			if (auto* handTransform = Locator::entitiesRegistry::value().TryGet<ecs::components::Transform>(handEntity))
			{
				const magic::HandHoldPoser::Frame holdFrame {
				    .dt = deltaTime,
				    .cursor = _mousePosition,
				    .camera = camera.GetOrigin(),
				    .levelTurn = glm::mat3(glm::eulerAngleY(camera.GetRotation().y)),
				    .modelCorrection = glm::mat3(glm::eulerAngleX(glm::radians(90.0f))),
				    .tilt = Locator::magicSystem::value().GetHandPour(Locator::time::value().GetTurnFraction()).tilt,
				    // A thing the hand picked up isn't turned about for a right hand as a seed is
				    .rightHanded = heldSeed.has_value() && config.rightHandedHand,
				};
				holdingSeed = _handHold.Pose(heldThing, holdFrame, _handAnimation.get(), *handTransform);
			}
		}

		// Animate the hand: gripping while it drags the land, otherwise its normal pose. Turning the camera with the
		// middle button leaves the hand idle.
		if (_handAnimation)
		{
			using HandState = HandAnimation::State;
			using HandCycle = HandAnimation::Cycle;
			using hand_navigation_pose::Pose;
			const auto state = _handCameraState ? HandState::Camera : HandState::Normal;
			auto cycle = HandCycle::Wiggle;
			switch (_handPose)
			{
			case Pose::Idle:
				break;
			case Pose::Grip:
				cycle = HandCycle::Grip;
				break;
			case Pose::Rotate:
				cycle = HandCycle::Rotate;
				break;
			case Pose::Pitch:
				cycle = HandCycle::Pitch;
				break;
			case Pose::Zoom:
				cycle = HandCycle::Zoom;
				break;
			}
			// On a creature, it strokes it or shows its slap
			if (_handOnCreature.has_value())
			{
				cycle = _handOnCreature->slapping ? HandCycle::Slap
				        : _handOnCreature->onBody ? HandCycle::Stroke
				                                  : HandCycle::Wiggle;
			}
			// Pulling a thing free, the hand takes the pose the thing is held with
			std::optional<ecs::systems::HandGrabSystemInterface::PullPose> pull;
			if (!holdingSeed && Locator::handGrabSystem::has_value())
			{
				pull = Locator::handGrabSystem::value().GetPullPose();
			}
			const auto pullCycle = pull.has_value() ? magic::hand_hold::HoldCycle(pull->hold) : std::nullopt;
			const auto* pullClip =
			    pullCycle.has_value() ? _handAnimation->GetAnimation(static_cast<size_t>(*pullCycle)) : nullptr;
			// Holding a town's totem, the hand takes its side hold on the icon, closed by how wide the icon is
			const auto totemHold =
			    Locator::villageTotemSystem::has_value() ? Locator::villageTotemSystem::value().GetHandHold() : std::nullopt;
			const auto* sideHold =
			    totemHold.has_value() ? _handAnimation->GetAnimation(static_cast<size_t>(HandCycle::HoldSide)) : nullptr;
			if (sideHold != nullptr)
			{
				_handAnimation->UpdateHeld(deltaTime, HandCycle::HoldSide,
				                           ecs::village_totem::GripTimeMs(totemHold->closure, sideHold->duration),
				                           _mousePosition);
			}
			else if (pullClip != nullptr)
			{
				const float handSize = HandAnimation::SizeAtDistance(glm::distance(camera.GetOrigin(), _handPosition));
				_handAnimation->UpdateHeld(deltaTime, *pullCycle,
				                           magic::hand_hold::TugTimeMs(pull->hold, pullClip->duration, pull->reach, handSize),
				                           _mousePosition);
			}
			else if (_handKnocking)
			{
				// Knocking on a house, the hand plays its tap once through, without leaning
				const auto* tap = _handAnimation->GetAnimation(static_cast<size_t>(HandCycle::TapHouse));
				const auto timeMs =
				    static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(_handKnock->time).count());
				_handAnimation->UpdateHeld(deltaTime, HandCycle::TapHouse, timeMs, _mousePosition);
				_handAnimation->SettleCursor(_mousePosition);
				_handKnock->time += deltaTime;
				if (tap == nullptr || _handKnock->time >= std::chrono::milliseconds(tap->duration))
				{
					_handKnock.reset();
				}
			}
			else if (!holdingSeed)
			{
				_handAnimation->Update(deltaTime, state, cycle, _mousePosition);
			}

			const auto handEntity = Locator::handSystem::value()
			                            .GetPlayerHands()[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)];
			auto& registry = Locator::entitiesRegistry::value();
			auto* hand = registry.TryGet<ecs::components::Hand>(handEntity);
			if (hand != nullptr)
			{
				hand->boneMatrices = _handAnimation->GetBoneMatrices();
			}
			// The hand keeps about the same size on screen however far away it is
			if (auto* handTransform = registry.TryGet<ecs::components::Transform>(handEntity))
			{
				const auto distance = glm::distance(camera.GetOrigin(), handTransform->position);
				// The game mirrors the mesh's left hand along its x axis to make a right hand
				const auto scale = _handAnimation->ScaleAtDistance(distance);
				handTransform->scale = glm::vec3(config.rightHandedHand ? -scale : scale, scale, scale);
				// Showing it turns the camera as it drags the land, the hand is drawn a third of its height higher, the
				// place it holds staying where it is
				if (hand != nullptr && _handCameraState && _handPose == Pose::Rotate &&
				    glm::determinant(handTransform->rotation) != 0.0f && scale != 0.0f)
				{
					const auto lift =
					    hand_navigation_pose::k_RotateLiftShare * k_HandHeight * HandAnimation::SizeAtDistance(_handDistance);
					const auto toModel =
					    glm::inverse(handTransform->rotation * glm::mat3(glm::scale(glm::mat4(1.0f), handTransform->scale)));
					const auto raise = glm::translate(glm::mat4(1.0f), toModel * glm::vec3(0.0f, lift, 0.0f));
					for (auto& bone : hand->boneMatrices)
					{
						bone = raise * bone;
					}
				}
			}
		}
		// Taking or letting go of a seed cross-fades the drawn hand, and the seed's in-hand effect sits in its fingers
		{
			const auto handEntity = Locator::handSystem::value()
			                            .GetPlayerHands()[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)];
			if (auto* handTransform = Locator::entitiesRegistry::value().TryGet<ecs::components::Transform>(handEntity))
			{
				const float handSize =
				    HandAnimation::SizeAtDistance(glm::distance(camera.GetOrigin(), handTransform->position));
				_handHold.Fade(heldThing, deltaTime, *handTransform);
				_handHold.PlaceHandEffect(heldSeed, _handAnimation.get(), *handTransform, handSize);
				// The trails' sheets of light and the chain behind the gesturing hand move on every frame
				Locator::particleSystem::value().UpdateFrame(
				    std::chrono::duration<float>(gameTime).count(),
				    {.position = handTransform->position,
				     .size = handSize,
				     .cameraPosition = camera.GetOrigin(),
				     .gesturing = Locator::gestureSystem::has_value() && Locator::gestureSystem::value().IsGesturing()});
			}
			// A tribe's power behind a miracle spins its name round the hand, or up from where it was cast
			Locator::miracleFxSystem::value().UpdateTribalPower(std::chrono::duration<float>(gameTime).count());
		}

		// The hand shows its player's alignment, catching up with it each frame once it has moved far enough. Its skin is
		// blended again as it goes into or out of the player's influence, tested where the cursor picks the land. The
		// point is held while the hand grips the land, and in the temple.
		// TODO(raffclar): what the game picks in the temple, which needs it run under a debugger
		{
			const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
			Locator::handSystem::value().UpdateAlignmentMorph(handPick, _handGripping || inTemple);
		}

		// The trees bend away from where the hand now is, and rustle
		{
			auto actions = profiler.BeginScoped(Profiler::Stage::VegetationUpdate);
			auto& vegetation = Locator::vegetation::value();
			vegetation.UpdateBendPoints(IsHandDrawn());
			vegetation.Rustle(gameTime);
		}

		// Update Entities
		{
			auto updateEntities = profiler.BeginScoped(Profiler::Stage::UpdateEntities);
			if (config.drawEntities)
			{
				// The villagers in view are posed for the camera the frame is drawn from
				ShowInspectorCamera(true);
				Locator::livingActionSystem::value().PoseVillagersInView(Locator::camera::value().GetViewProjectionMatrix());
				Locator::rendereringSystem::value().PrepareDraw(config.drawBoundingBoxes, config.drawFootpaths,
				                                                config.drawStreams);
				ShowInspectorCamera(false);
				// The interface picks what is under the cursor as the frame is drawn, for the next frame to go by
				PickUnderCursor(std::chrono::duration<float>(deltaTime).count());
			}
		}
	} // Update Uniforms

	// Update Audio
	{
		auto updateAudio = profiler.BeginScoped(Profiler::Stage::UpdateAudio);
		Locator::audio::value().Update();
	} // Update Audio

	return config.numFramesToSimulate == 0 || _frameCount < config.numFramesToSimulate;
}

bool Game::Initialize() noexcept
{
	auto& config = Locator::config::value();
	_startupTimer.emplace("startup", _launchTime);
	_startupTimer->Step("launch");

	if (config.graphicsBackend != GraphicsBackend::Noop)
	{
		uint32_t extraFlags = 0;
		if (config.graphicsBackend == GraphicsBackend::Metal)
		{
			extraFlags |= SDL_WINDOW_METAL;
		}
		openblack::InitializeWindow(k_WindowTitle, config.resolution.x, config.resolution.y, config.displayMode, extraFlags);
	}

	using filesystem::Path;
	if (!InitializeEngine(config.graphicsBackend, config.vsync))
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "Failed to initialize engine services.");
		return false;
	}
	_startupTimer->Step("window, renderer and audio device");
	auto& fileSystem = Locator::filesystem::value();
	auto& events = Locator::events::value();

	events.AddHandler(std::function([this, &config](const SDL_Event& event) {
		// If gui captures this input, do not propagate
		if (!Locator::debugGui::value().ProcessEvents(event))
		{
			// A full-screen video takes Escape: it fades out, unless Shift or Ctrl is held
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE && event.key.repeat == 0 &&
			    Locator::videoSystem::value().Escape((event.key.keysym.mod & KMOD_SHIFT) != 0,
			                                         (event.key.keysym.mod & KMOD_CTRL) != 0))
			{
				return;
			}
			// The tattoo editor's dialog takes the keyboard and mouse while it is open, Escape and Enter included
			if (_interface && _interface->GetTattooEditor().IsOpen() && Locator::windowing::has_value() &&
			    _interface->ProcessEvent(event, static_cast<glm::u16vec2>(Locator::windowing::value().GetSize())))
			{
				return;
			}
			// Inside the temple, Escape goes back to its main room and out, as the temple's keys do
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE && event.key.repeat == 0 &&
			    Locator::temple::has_value() && Locator::temple::value().Active())
			{
				Locator::temple::value().Escape();
				return;
			}
			// The Creature Cave shown on its own closes on Escape
			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE && event.key.repeat == 0 &&
			    Locator::creatureCaveSystem::has_value() && Locator::creatureCaveSystem::value().Escape())
			{
				return;
			}
			// The game's menu takes Escape, and the keyboard and mouse while it is open
			if (_interface && Locator::windowing::has_value() &&
			    _interface->ProcessEvent(event, static_cast<glm::u16vec2>(Locator::windowing::value().GetSize())))
			{
				HandleInterfaceAction();
				return;
			}
			config.running = this->ProcessEvents(event);
			Locator::gameActionSystem::value().ProcessEvent(event);
		}
	}));

	if (!fileSystem.IsPathValid(_gamePath))
	{
		// no key, don't guess, let the user know to set the command param
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Game Path missing",
		                         "Game path was not supplied, use the -g "
		                         "command parameter to set it.",
		                         nullptr);
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Failed to find the GameDir.");
		return false;
	}

	fileSystem.SetGamePath(_gamePath);

	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "The GamePath is \"{}\".", fileSystem.GetGamePath().generic_string());

	if (std::filesystem::path(_startMap).is_absolute())
	{
		if (std::find(_startMap.begin(), _startMap.end(), "Scripts") != _startMap.end())
		{
			auto p = _startMap;
			while (p.filename() != "Scripts" && p != p.parent_path())
			{
				p = p.parent_path();
			}
			fileSystem.AddAdditionalPath(p.parent_path());
		}
		else
		{
			fileSystem.AddAdditionalPath(_startMap.parent_path());
		}
	}
	else
	{
		_startMap = fileSystem.GetPath<Path::Scripts>() / _startMap;
	}

	if (!InitializeGame())
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "Failed to initialize game services.");
		return false;
	}
	_startupTimer->Step("game services and shaders");
	// A deterministic run: every random number from the seed, the date pinned
	if (_seed.has_value())
	{
		Locator::rng::value().SetRunSeed(*_seed);
		Locator::time::value().RestartClock(ecs::systems::TimeSystemInterface::k_DeterministicDate);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Seeded run: seed {}, date pinned", *_seed);
	}
	// The debug inspector answers from the first frame; the game carries on without it if it can't listen. Until then,
	// while the game's data loads, it answers that the game is loading
	std::optional<ecs::systems::InspectorLoading> loadingData;
	if (_inspectPort.has_value())
	{
		// An agent drives it: the player's mouse and keyboard are kept out while a client is connected, so that a knock
		// of the mouse doesn't spoil its tests; from the start for a headless run, or never when asked
		if (StartInspector(*_inspectPort))
		{
			loadingData.emplace("game data");
			Locator::gameActionSystem::value().SetInputLockMode(_inspectInputLock);
		}
	}

	// The logos, the first run's pre-intro and the tips screen, which stays up while the rest loads. Closing the window
	// meanwhile ends the game at its first frame
	PlayStartupScreens();

	auto& resources = Locator::resources::value();
	auto& meshManager = resources.GetMeshes();
	auto& textureManager = resources.GetTextures();
	auto& animationManager = resources.GetAnimations();
	auto& levelManager = resources.GetLevels();
	auto& soundManager = resources.GetSounds();
	auto& glowManager = resources.GetGlows();
	auto& camPathManager = resources.GetCameraPaths();

	// The main room's markers of the temples, creatures and challenges on the map, and the creature's room's belts and
	// medals
	std::vector<std::string> icons {"I_citadel_on_map", "I_creature_on_map", "I_challenge_on_map"};
	for (uint32_t i = 0; i < CreatureCaveTrophies::k_IconCount; ++i)
	{
		icons.push_back(CreatureCaveTrophies::IconName(i));
	}
	for (const auto& icon : icons)
	{
		const auto path = fileSystem.GetPath<Path::Citadel>() / "icons" / fmt::format("{}.l3d", icon);
		try
		{
			RegisterFile(meshManager, fmt::format("temple/icons/{}", icon), path, resources::L3DLoader::FromDiskTag {}, path);
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	}

	fileSystem.Iterate(fileSystem.GetPath<Path::Citadel>() / "OutsideMeshes", false,
	                   [&meshManager, &resources](const std::filesystem::path& f) {
		                   const auto extension = string_utils::LowerCase(f.extension().string());
		                   const auto name = fmt::format("temple/{}", string_utils::LowerCase(f.stem().string()));
		                   try
		                   {
			                   if (extension == ".zzz")
			                   {
				                   SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading temple mesh: {}", f.stem().string());
				                   RegisterFile(meshManager, name, f, resources::L3DLoader::FromDiskTag {}, f);
				                   // The temple's outside is blended from the temple meshes, into the first temple's,
				                   // its entrance is picked under the cursor, and its worship sites wear its skin
				                   if (name.starts_with("temple/b_temple") || name.starts_with("temple/b_first_temple") ||
				                       name == "temple/entrance_l3d" || name == "temple/b_worship_l3d")
				                   {
					                   RegisterFile(resources.GetL3DFiles(), name, f, resources::L3DFileLoader::FromDiskTag {},
					                                f);
				                   }
			                   }
			                   else if (extension == ".16b")
			                   {
				                   // And its texture from these, from evil to neutral to good
				                   RegisterFile(resources.GetBitmaps(), name, f, resources::Bitmap16BLoader::FromDiskTag {}, f);
			                   }
		                   }
		                   catch (std::runtime_error& err)
		                   {
			                   SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		                   }
	                   });

	_startupTimer->Step("temple outside meshes");

	// What things are made of, for the physics; without the file every material is zero, as in the game
	if (const auto constants = fileSystem.GetPath<filesystem::Path::Data>() / "PhysicsConstants.txt";
	    fileSystem.Exists(constants))
	{
		resources.GetPhysicsMaterials().Load(physics::k_MaterialsId.value(), resources::PhysicsMaterialsLoader::FromDiskTag {},
		                                     constants);
	}
	else
	{
		resources.GetPhysicsMaterials().Load(physics::k_MaterialsId.value(), resources::PhysicsMaterialsLoader::EmptyTag {});
	}
	// The sheet the physics' dust puffs are drawn from
	for (const auto* name : {"blobs", "blobsa"})
	{
		if (const auto sheet = fileSystem.GetPath<filesystem::Path::Data>() / fmt::format("{}.raw", name);
		    fileSystem.Exists(sheet))
		{
			textureManager.Load(fmt::format("raw/{}", name), resources::Texture2DLoader::FromDiskTag {}, sheet);
		}
	}

	// The land's light is built from this every frame
	if (const auto palette = fileSystem.GetPath<filesystem::Path::WeatherSystem>() / "palette.raw"; fileSystem.Exists(palette))
	{
		resources.GetLandLightPalettes().Load(LandLightPalette::k_Id.value(), resources::LandLightPaletteLoader::FromDiskTag {},
		                                      palette);
	}

	_startupTimer->Step("physics materials, dust sheets and land palette");
	fileSystem.Iterate( //
	    fileSystem.GetPath<filesystem::Path::Citadel>() / "engine", false,
	    [&meshManager, &glowManager](const std::filesystem::path& f) {
		    if (f.extension() == ".zzz")
		    {
			    if (f.stem().string().ends_with("lo_l3d"))
			    {
				    SPDLOG_LOGGER_WARN(
				        spdlog::get("game"),
				        "Skipping lo duplicate lo meshes. See https://github.com/openblack/openblack/issues/727");
				    return;
			    }
			    SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading interior temple mesh: {}", f.stem().string());
			    try
			    {
				    RegisterFile(meshManager, fmt::format("temple/interior/{}", f.stem().string()), f,
				                 resources::L3DLoader::FromDiskTag {}, f);
			    }
			    catch (std::runtime_error& err)
			    {
				    SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
			    }
		    }
		    else if (f.extension() == ".glw")
		    {
			    SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading interior temple glows: {}", f.stem().string());
			    try
			    {
				    RegisterFile(glowManager, fmt::format("temple/interior/glow/{}", f.stem().string()), f,
				                 resources::LightLoader::FromDiskTag {}, f);
			    }
			    catch (std::runtime_error& err)
			    {
				    SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
			    }
		    }
	    });

	_startupTimer->Step("temple interior meshes and glows");
	// The pack is kept by the loads of its meshes and textures until the last of them has run
	const auto pack = std::make_shared<pack::PackFile>();

	auto packResult = pack->ReadFile(*fileSystem.GetData(fileSystem.GetPath<Path::Data>() / "AllMeshes.g3d"));
	if (packResult != pack::PackResult::Success)
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "Unable to load AllMeshes.g3d: {}", pack::ResultToStr(packResult));
		return false;
	}

	_startupTimer->Step("mesh pack read");
	const auto& meshes = pack->GetMeshes();
	for (uint32_t i = 0; i < meshes.size(); ++i)
	{
		meshManager.Register(
		    static_cast<MeshId>(i),
		    [pack, i] {
			    return resources::L3DLoader {}(resources::L3DLoader::FromBufferTag {}, std::string(k_MeshNames.at(i)),
			                                   pack->GetMesh(i));
		    },
		    meshes[i].size());
	}

	_startupTimer->Step("mesh pack meshes");
	for (const auto& [name, g3dTexture] : pack->GetTextures())
	{
		textureManager.Register(
		    g3dTexture.header.id,
		    [pack, name] {
			    return resources::Texture2DLoader {}(resources::Texture2DLoader::FromPackTag {}, name, pack->GetTexture(name));
		    },
		    g3dTexture.ddsData.size());
	}

	_startupTimer->Step("mesh pack textures");
	pack::PackFile animationPack;
	packResult = animationPack.ReadFile(*fileSystem.GetData(fileSystem.GetPath<Path::Data>() / "AllAnims.anm"));
	if (packResult != pack::PackResult::Success)
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "Unable to load AllAnims.anm: {}", pack::ResultToStr(packResult));
		return false;
	}

	const auto& animations = animationPack.GetAnimations();
	// TODO (#749) use std::views::enumerate
	for (size_t i = 0; i < animations.size(); i++)
	{
		animationManager.Load(i, resources::L3DAnimLoader::FromBufferTag {}, animations[i]);
	}
	_startupTimer->Step("animation pack");
	// The game stops one of the packed clips playing round and round as it loads them
	if (constexpr uint32_t k_HeldOnceClip = 323; animationManager.Contains(resources::HashIdentifier(k_HeldOnceClip)))
	{
		animationManager.Handle(resources::HashIdentifier(k_HeldOnceClip))->StopLooping();
	}
	// The sounds placed on the frames of the people's, animals' and birds' clips
	if (const auto sounds = fileSystem.GetPath<Path::Data>() / "SmallSounds.SAS"; fileSystem.Exists(sounds))
	{
		resources.GetClipSounds().Load(audio::clip_sounds::k_TableId.value(), resources::ClipSoundsLoader::FromDiskTag {},
		                               sounds);
	}
	else
	{
		resources.GetClipSounds().Load(audio::clip_sounds::k_TableId.value(), resources::ClipSoundsLoader::EmptyTag {});
	}
	{
		// Each name's sounds go to the first packed clip bearing it
		std::vector<std::string_view> packNames;
		packNames.reserve(animations.size());
		for (size_t i = 0; i < animations.size(); ++i)
		{
			const auto clip = animationManager.Handle(resources::HashIdentifier(i));
			packNames.emplace_back(clip ? std::string_view(clip->GetName().c_str()) : std::string_view {});
		}
		resources.GetClipSounds().Handle(audio::clip_sounds::k_TableId.value())->Attach(packNames);
	}

	_startupTimer->Step("clip sounds");
	fileSystem.Iterate(fileSystem.GetPath<Path::CreatureMesh>(), false, [&meshManager](const std::filesystem::path& f) {
		const auto& fileName = f.stem().string();
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading creature mesh: {}", fileName);
		try
		{
			if (string_utils::BeginsWith(fileName, "Hand"))
			{
				return;
			}

			const auto meshId = creature::GetIdFromMeshName(fileName);
			RegisterFile(meshManager, meshId, f, resources::L3DLoader::FromDiskTag {}, f);
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	});

	_startupTimer->Step("creature meshes");
	fileSystem.Iterate(fileSystem.GetPath<Path::Symbols>(), false, [&camPathManager](const std::filesystem::path& f) {
		if (f.extension() != ".cam")
		{
			return;
		}
		const auto& fileName = f.stem().string();
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading symbol cam: {}", fileName);
		const auto pathId = fmt::format("symbol/{}", fileName);
		try
		{
			camPathManager.Load(pathId, resources::CameraPathLoader::FromDiskTag {}, f);
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	});

	fileSystem.Iterate(fileSystem.GetPath<Path::CitadelEngine>(), false, [&camPathManager](const std::filesystem::path& f) {
		if (f.extension() != ".cam")
		{
			return;
		}
		const auto& fileName = f.stem().string();
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading interior temple cam: {}", fileName);
		const auto pathId = fmt::format("temple/{}", fileName);
		try
		{
			camPathManager.Load(pathId, resources::CameraPathLoader::FromDiskTag {}, f);
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	});

	_startupTimer->Step("camera paths");
	// Load loose one-off assets
	{
		using AFromDiskTag = resources::L3DAnimLoader::FromDiskTag;
		animationManager.Load("coffre", AFromDiskTag {}, fileSystem.GetPath<Path::Misc>() / "coffre.anm");

		using LFromDiskTag = resources::L3DLoader::FromDiskTag;
		meshManager.Load("hand", LFromDiskTag {}, fileSystem.GetPath<Path::CreatureMesh>() / "Hand_Boned_Base2.l3d");
		_startupTimer->Step("hand mesh");
		LoadHandAnimation();
		_startupTimer->Step("hand animations");
		LoadCreatureRigs();
		_startupTimer->Step("creature rigs");
		Locator::advisorSystem::value().Load();
		const auto registerMesh = [&meshManager](auto id, const std::filesystem::path& path) {
			RegisterFile(meshManager, id, path, LFromDiskTag {}, path);
		};
		registerMesh("coffre", fileSystem.GetPath<Path::Misc>() / "coffre.l3d");
		// The closed reward chest
		if (const auto path = fileSystem.GetPath<Path::Misc>() / "chest0.l3d"; fileSystem.Exists(path))
		{
			registerMesh("misc/chest0", path);
		}
		// The collar the citadel's leash posts are drawn with
		if (const auto path = fileSystem.GetPath<Path::Misc>() / "leash.l3d"; fileSystem.Exists(path))
		{
			registerMesh("misc/leash", path);
			// Each player's temple hangs one of each leash, each drawn with its own copy of the collar
			for (uint8_t player = 0; player < static_cast<uint8_t>(PlayerNames::_COUNT); ++player)
			{
				for (const auto type : creature_leash::k_Types)
				{
					registerMesh(temple_leashes::CollarMeshName(static_cast<PlayerNames>(player), type), path);
				}
			}
		}
		// The eyes every creature is drawn with
		for (const auto& [id, file] : {std::pair {ecs::components::CreatureEyes::k_EyeballMeshId, "Eyeball.l3d"},
		                               std::pair {ecs::components::CreatureEyes::k_EyelidMeshId, "Eyelid.l3d"}})
		{
			if (const auto path = fileSystem.GetPath<Path::Data>() / file; fileSystem.Exists(path))
			{
				registerMesh(id, path);
			}
		}
		registerMesh("cone", fileSystem.GetPath<Path::Data>() / "cone.l3d");
		registerMesh("marker", fileSystem.GetPath<Path::Data>() / "marker.l3d");
		registerMesh("river", fileSystem.GetPath<Path::Data>() / "river.l3d");
		registerMesh("river2", fileSystem.GetPath<Path::Data>() / "river2.l3d");
		registerMesh("metre_sphere", fileSystem.GetPath<Path::Data>() / "metre_sphere.l3d");
		// The sky: its dome, with the dome's pictures, the sun and the moon
		using SkyArchetype = ecs::archetypes::SkyArchetype;
		registerMesh(SkyArchetype::k_DomeMeshId.value(), fileSystem.GetPath<Path::WeatherSystem>() / "sky.l3d");
		{
			std::vector<std::filesystem::path> pictures;
			for (const auto& file : sky_dome::PictureFiles())
			{
				pictures.push_back(fileSystem.GetPath<Path::WeatherSystem>() / file);
			}
			textureManager.Load(SkyArchetype::k_DomeTextureId.value(), resources::Texture2DLoader::FromBitmapLayersTag {},
			                    "Sky", pictures, sky_dome::k_Rows);
		}
		registerMesh(SkyArchetype::k_SunMeshId.value(), fileSystem.GetPath<Path::WeatherSystem>() / "sun.l3d");
		registerMesh(SkyArchetype::k_MoonMeshId.value(), fileSystem.GetPath<Path::WeatherSystem>() / "moon.l3d");
		registerMesh(ecs::components::Mist::k_MeshId, fileSystem.GetPath<Path::Landscape>() / "mist.l3d");

		using CFromDiskTag = resources::CameraPathLoader::FromDiskTag;
		camPathManager.Load("cam", CFromDiskTag {}, fileSystem.GetPath<Path::Data>() / "cam.cam");
		camPathManager.Load("flying", CFromDiskTag {}, fileSystem.GetPath<Path::Data>() / "flying.cam");
	}

	_startupTimer->Step("one-off meshes");
	// The game's menu, which greets the player by their profile's name: openblack has no profiles, so by the name
	// they log in with
	{
		const auto* user = std::getenv("USERNAME");
		user = user != nullptr ? user : std::getenv("USER");
		// The settings the menu starts with are the game's
		gui::MenuSettings settings;
		auto& audio = Locator::audio::value();
		settings.sfxVolume = audio.GetSfxVolume();
		settings.musicVolume = audio.GetMusicVolume();
		settings.leftHandedHand = !Locator::config::value().rightHandedHand;
		_interface = gui::GameInterface::Create(gui::ToUtf16(user != nullptr ? user : "Player"), std::move(settings));
		if (!_interface)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "The game's menu is not available, Escape quits");
		}
		else
		{
			if (Locator::temple::has_value())
			{
				Locator::temple::value().SetInterface(_interface.get());
			}
			if (Locator::creatureCaveSystem::has_value())
			{
				Locator::creatureCaveSystem::value().SetInterface(_interface.get());
			}
			if (Locator::miracleFxSystem::has_value())
			{
				Locator::miracleFxSystem::value().SetInterface(_interface.get());
			}
		}
	}

	_startupTimer->Step("game interface");
	// TODO(raffclar): #400: Parse level files within the resource loader
	// TODO(raffclar): #405: Determine campaign levels from the challenge script file
	// Load the campaign levels
	fileSystem.Iterate(fileSystem.GetPath<Path::Scripts>(), false, [&levelManager](const std::filesystem::path& f) {
		const auto& name = f.stem().string();
		if (f.extension() != ".txt" || name.rfind("InfoScript", 0) != std::string::npos)
		{
			return;
		}
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading campaign level: {}", f.stem().string());
		try
		{
			if (Level::IsLevelFile(f))
			{
				levelManager.Load(fmt::format("campaign/{}", name), resources::LevelLoader::FromDiskTag {}, f,
				                  Level::LandType::Campaign);
			}
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	});
	// Load Playgrounds
	// Attempt to load additional levels as playgrounds
	fileSystem.Iterate(fileSystem.GetPath<Path::Playgrounds>(), false, [&levelManager](const std::filesystem::path& f) {
		if (f.extension() != ".txt")
		{
			return;
		}
		const auto& name = f.stem().string();
		if (levelManager.Contains(fmt::format("playgrounds/{}", name)))
		{
			// Already added
			return;
		}

		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading custom level: {}", f.stem().string());
		try
		{
			if (Level::IsLevelFile(f))
			{
				levelManager.Load(fmt::format("playgrounds/{}", name), resources::LevelLoader::FromDiskTag {}, f,
				                  Level::LandType::Skirmish);
			}
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	});

	_startupTimer->Step("level list");
	// Load all sound packs in the Audio directory
	auto& audioManager = Locator::audio::value();
	fileSystem.Iterate(
	    fileSystem.GetPath<Path::Audio>(), true, [&audioManager, &soundManager, &fileSystem](const std::filesystem::path& f) {
		    if (f.extension() != ".sad")
		    {
			    return;
		    }

		    // Only the samples' headers are read now: each sample is read from the file when it is wanted
		    pack::PackFile soundPack;
		    SPDLOG_LOGGER_DEBUG(spdlog::get("audio"), "Opening sound pack {}", f.filename().string());
		    auto result = soundPack.ReadFile(*fileSystem.GetData(f), {"LHAudioWaveData"});
		    const auto waveData = soundPack.GetUnreadBlock("LHAudioWaveData");
		    if (result == pack::PackResult::Success && soundPack.HasBlock("LHAudioBankSampleTable") && !waveData.has_value())
		    {
			    result = pack::PackResult::ErrMissingAudioWaveDataBlock;
		    }
		    const auto& audioHeaders = soundPack.GetAudioSampleHeaders();
		    // As when the whole pack is read, a sample beyond the wave data spoils the pack
		    if (result == pack::PackResult::Success && waveData.has_value() &&
		        std::ranges::any_of(audioHeaders, [&waveData](const pack::AudioBankSampleHeader& header) {
			        return static_cast<uint64_t>(header.offset) + header.size > waveData->size;
		        }))
		    {
			    result = pack::PackResult::ErrFileTooSmall;
		    }
		    if (result != pack::PackResult::Success)
		    {
			    SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Unable to load sound pack {}: {}", f.filename().string(),
			                        pack::ResultToStr(result));
			    return;
		    }

		    if (audioHeaders.empty())
		    {
			    SPDLOG_LOGGER_WARN(spdlog::get("audio"), "Empty sound pack found for {}. Skipping", f.filename().string());
			    return;
		    }
		    auto soundName = std::filesystem::path(audioHeaders[0].name.data());

		    auto groupName = f.filename().string();

		    // A hacky way of detecting if the sound is music as all music sounds end with "mpg"
		    if (soundName.extension() == ".mpg")
		    {
			    auto buffers = std::queue<std::vector<uint8_t>>();
			    auto packName = f.string();
			    audioManager.AddMusicEntry(packName);
		    }
		    else
		    {
			    audioManager.CreateSoundGroup(groupName);
			    const bool decodeAhead = std::ranges::find(k_DecodedAheadBanks, groupName) != k_DecodedAheadBanks.end();
			    for (size_t i = 0; i < audioHeaders.size(); i++)
			    {
				    soundName = std::filesystem::path(audioHeaders[i].name.data());
				    // Banks have gaps between their samples, which are skipped without skipping the samples after them
				    if (audioHeaders[i].size == 0)
				    {
					    SPDLOG_LOGGER_DEBUG(spdlog::get("audio"), "Empty sound buffer found for {}/{}. Skipping", groupName,
					                        audioHeaders[i].id);
					    continue;
				    }

				    const auto stringId = fmt::format("{}/{}", groupName, audioHeaders[i].id);
				    const entt::id_type id = entt::hashed_string(stringId.c_str());
				    SPDLOG_LOGGER_DEBUG(spdlog::get("audio"), "Registering sound {}: {}", stringId,
				                        audioHeaders[i].name.data());
				    soundManager.RegisterLoad(id, audioHeaders[i].size, resources::SoundLoader::FromBankFileTag {}, f,
				                              waveData->offset, audioHeaders[i], decodeAhead);
				    audioManager.AddToSoundGroup(groupName, id);
			    }

			    // What the bank plays for things happening in the game, such as trees rustling
			    if (soundPack.HasBlock("LHAudioAnimArrayTable") && soundPack.HasBlock("LHAudioWaveNumTable"))
			    {
				    if (auto effects = audio::AnimEffectTable::Parse(soundPack.GetBlock("LHAudioAnimArrayTable"),
				                                                     soundPack.GetBlock("LHAudioWaveNumTable")))
				    {
					    audioManager.AddAnimEffects(groupName, std::move(*effects));
				    }
				    else
				    {
					    SPDLOG_LOGGER_WARN(spdlog::get("audio"), "Malformed animation effects in sound pack {}",
					                       f.filename().string());
				    }
			    }
		    }
	    });

	// Which sample of the speech banks says each help text, for the lines the scripts have spoken
	if (_interface)
	{
		auto& sounds = Locator::resources::value().GetSounds();
		std::array<std::vector<audio::SpeechBankSample>, audio::k_SpeechBankFiles.size()> banks;
		for (const auto& [groupName, group] : audioManager.GetSoundGroups())
		{
			const auto bank = std::ranges::find_if(audio::k_SpeechBankFiles, [&groupName](std::string_view file) {
				return string_utils::LowerCase(std::string(file)) == string_utils::LowerCase(groupName);
			});
			if (bank == audio::k_SpeechBankFiles.end())
			{
				continue;
			}
			auto& samples = banks.at(static_cast<size_t>(std::distance(audio::k_SpeechBankFiles.begin(), bank)));
			for (const auto id : group.sounds)
			{
				const auto& sound = *sounds.Handle(id);
				samples.push_back({.sample = static_cast<uint32_t>(sound.id), .file = sound.name, .sound = id});
			}
		}
		Locator::helpSpeechSystem::value().SetTable(audio::HelpSpeechTable(_interface->GetTexts().GetHelpNames(), banks));
		// The scripts' dialogue, its text sized for the screen it starts on
		const int screenHeight = Locator::windowing::has_value() ? Locator::windowing::value().GetSize().y : 0;
		Locator::helpTextSystem::value().Start(_interface->GetTexts(), screenHeight);
		// What the dialogue changing hands does to the advisors and the texts
		Locator::dialogueControlSystem::value().SetHooks({
		    .sendSpiritsHome = [](bool helpScript) { SendAdvisorsHome(helpScript); },
		    .taken = []() { Locator::helpTextSystem::value().ClearAllText(); },
		    .released =
		        [](bool helpScript) {
			        // They fly home, are cut short in what they say, are sent home as the script would, and the texts go
			        SendAdvisorsHome(false);
			        Locator::helpTextSystem::value().InterruptAdvisors();
			        SendAdvisorsHome(helpScript);
			        Locator::helpTextSystem::value().ClearAllText();
		        },
		});
	}

	_startupTimer->Step("sound and music banks");
	{
		InfoFile infoFile;
		auto result = infoFile.LoadFromFile(Locator::filesystem::value().GetPath<filesystem::Path::Scripts>() / "info.dat");
		if (!result)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Failed to load game info data.");
			return false;
		}
		Locator::infoConstants::reset(result.release());
	}

	_startupTimer->Step("info tables");
	// The temple's leashes are drawn with the leash texture and its alpha
	if (const auto leash = fileSystem.GetPath<Path::Textures>() / "leash.raw",
	    alpha = fileSystem.GetPath<Path::Textures>() / "leasha.raw";
	    fileSystem.Exists(leash) && fileSystem.Exists(alpha))
	{
		constexpr uint16_t k_LeashTextureSide = 256;
		try
		{
			textureManager.Load(ecs::components::LeashPost::k_TextureId, resources::Texture2DLoader::FromDiskWithAlphaTag {},
			                    leash, alpha, k_LeashTextureSide);
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	}
	// The marks the vortices leave on the ground, with their alphas
	for (const auto& textures : ecs::components::Vortex::k_GroundTextures)
	{
		for (const auto& [id, name] :
		     {std::pair {textures.hole, textures.holeFile}, std::pair {textures.ring, textures.ringFile}})
		{
			const auto colours = fileSystem.GetPath<Path::Textures>() / fmt::format("{}.raw", name);
			const auto alpha = fileSystem.GetPath<Path::Textures>() / fmt::format("{}a.raw", name);
			if (!fileSystem.Exists(colours) || !fileSystem.Exists(alpha))
			{
				continue;
			}
			constexpr uint16_t k_VortexTextureSide = 256;
			try
			{
				textureManager.Load(id, resources::Texture2DLoader::FromDiskWithAlphaTag {}, colours, alpha,
				                    k_VortexTextureSide);
			}
			catch (std::runtime_error& err)
			{
				SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
			}
		}
	}
	fileSystem.Iterate(fileSystem.GetPath<Path::Textures>(), false, [&textureManager](const std::filesystem::path& f) {
		// The game ships a grey ice map it never loads, cut short of a whole texture
		constexpr std::string_view k_UnusedTexture = "s_iceenvmapgrey.raw";
		if (string_utils::LowerCase(f.extension().string()) == ".raw" &&
		    string_utils::LowerCase(f.filename().string()) != k_UnusedTexture)
		{
			SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading raw texture: {}", f.stem().string());
			try
			{
				RegisterFile(textureManager, fmt::format("raw/{}", f.stem().string()), f,
				             resources::Texture2DLoader::FromDiskTag {}, f);
			}
			catch (std::runtime_error& err)
			{
				SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
			}
		}
	});

	_startupTimer->Step("raw textures");
	// The texture every creature's hair is drawn with, and its alpha beside it
	for (const auto& [id, name] : {std::pair {ecs::components::CreatureHair::k_TextureId, "C_Ape_Hair.raw"},
	                               std::pair {ecs::components::CreatureHair::k_AlphaTextureId, "C_Ape_Haira.raw"}})
	{
		const auto path = fileSystem.GetPath<Path::Data>() / name;
		if (!fileSystem.Exists(path))
		{
			continue;
		}
		try
		{
			textureManager.Load(id, resources::Texture2DLoader::FromDiskTag {}, path);
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	}

	// What creatures' tattoos and marks are painted with
	try
	{
		const auto data = fileSystem.GetPath<Path::Data>();
		Locator::resources::value().GetCreatureSkinArt().Load(
		    creature_skin::k_ArtId, resources::CreatureSkinArtLoader::FromDiskTag {},
		    resources::CreatureSkinArtLoader::Paths {
		        .symbols = fileSystem.GetPath<Path::Textures>() / "OriginalChooseSymbol.raw",
		        .freshDamage = data / "damage_new256.raw",
		        .freshDamageAlpha = data / "damage_new256A.raw",
		        .oldDamage = data / "damage_old256.raw",
		        .oldDamageAlpha = data / "damage_old256A.raw",
		        .palette = data / "tattoocols.raw",
		    });
	}
	catch (std::runtime_error& err)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
	}

	// The noise that makes the snow's edges on the land ragged
	try
	{
		textureManager.Load(snow_cover::k_NoiseTextureId.value(), resources::Texture2DLoader::FromDiskTag {},
		                    fileSystem.GetPath<Path::WeatherSystem>() / "snowmap.raw");
	}
	catch (std::runtime_error& err)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
	}
	_startupTimer->Step("hair, skin art and snow textures");

	// Everything registered is loaded on the loading threads from now on, while the first land is made: what the land
	// needs before its turn comes is loaded there and then
	resources.PrefetchAll();
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "{} resources registered to be loaded", resources.PendingCount());
	_prefetchTimer.emplace("of every resource");

	return true;
}

bool Game::Run() noexcept
{
	auto& config = Locator::config::value();

	if (_startTestbed)
	{
		LoadTestbed();
	}
	else if (!LoadMap(_startMap))
	{
		return false;
	}
	if (_startupTimer.has_value())
	{
		_startupTimer->Step("first land");
	}

	auto& fileSystem = Locator::filesystem::value();

	auto challengePath = fileSystem.GetPath<filesystem::Path::Quests>() / "challenge.chl";
	if (fileSystem.Exists(challengePath))
	{
		auto& chlapi = Locator::chlapi::value();
		auto& lhvm = Locator::vm::value();
		// The virtual machine's errors go to the scripting log, where the editor's Scripts panel shows them
		// The land's scripts start with every place of the scripts' object table free; each native tells the table
		// whether it takes control of what it is given, and the scripts' variables keep their objects' references
		Locator::scriptObjects::value().Reset();
		chlapi.ResetSwitches();
		lhvm.Initialise(
		    &chlapi.GetFunctionsTable(),
		    [](uint32_t func) {
			    Locator::scriptObjects::value().EnterNative(func);
			    Locator::chlapi::value().EnterNative(func);
		    },
		    nullptr, [](uint32_t task) { chlapi::CHLApi::TaskStopped(task); },
		    [](lhvm::ErrorCode code, const std::string& text, uint32_t number) {
			    SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Script error: {} ({} {})",
			                        lhvm::k_ErrorMsg.at(static_cast<size_t>(code)), text, number);
		    },
		    [](uint32_t object) { Locator::scriptObjects::value().AddReference(static_cast<entt::entity>(object)); },
		    [](uint32_t object) { Locator::scriptObjects::value().RemoveReference(static_cast<entt::entity>(object)); });
		try
		{
			lhvm.LoadBinary(fileSystem.ReadAll(challengePath));
			// The story's scripts run the first land; on the testbed they would set its time of day and stop its clock
			// The story opens on a black screen: its first land comes up at noon, and its scripts set the dawn of the
			// opening scene and fade the picture in from black some turns later
			if (!_startTestbed && lhvm.StartScript("LandControlAll", lhvm::ScriptType::All) != 0)
			{
				Locator::cinematicDirectorSystem::value().StartStory();
			}
		}
		catch (const std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Failed to read challenge file at {}: {}",
			                    (fileSystem.GetGamePath() / challengePath).generic_string(), err.what());
		}
	}
	else
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Challenge file not found at {}",
		                    (fileSystem.GetGamePath() / challengePath).generic_string());
		return false;
	}
	if (!_startTestbed)
	{
		AskNewGameChoice();
	}

	if (_startupTimer.has_value())
	{
		_startupTimer->Step("story scripts");
	}
	// Everything the map made goes into the map's cells, in the order it was made
	Locator::entitiesMap::value().Sync();

	if (Locator::windowing::has_value())
	{
		const auto size = static_cast<glm::u16vec2>(Locator::windowing::value().GetSize());
		Locator::rendererInterface::value().ConfigureView(graphics::RenderPass::Main, size, 0x274659ff);
		Locator::oceanSystem::value().ResizeReflectionFramebuffer(size);
	}

	{
		uint16_t width;
		uint16_t height;
		Locator::oceanSystem::value().GetReflectionFramebuffer().GetSize(width, height);
		Locator::rendererInterface::value().ConfigureView(graphics::RenderPass::Reflection, {width, height}, 0x274659ff);
	}

	Game::SetTime(config.timeOfDay);
	Locator::time::value().Start();

	if (_playVideo == "intro")
	{
		Locator::videoSystem::value().Play("Data/INTRO.bik");
		Locator::videoSystem::value().ScheduleIntro();
	}
	else if (_playVideo == "fall")
	{
		Locator::videoSystem::value().StartFallingSpell();
	}
	else if (!_playVideo.empty())
	{
		Locator::videoSystem::value().Play(_playVideo);
	}

	_frameCount = 0;
	auto lastTime = std::chrono::high_resolution_clock::now();
	auto& profiler = Locator::profiler::value();
	std::optional<debug::FrameStatsLog> frameStats;
	if (config.frameStatsInterval > 0)
	{
		frameStats.emplace(config.frameStatsInterval, config.frameStatsViews);
		if (config.frameStatsViews)
		{
			Locator::rendererInterface::value().SetProfile(true);
		}
	}
	auto frameStart = std::chrono::steady_clock::now();
	while (Update())
	{
		// What the loading threads finished goes into the caches, and they're given a little more
		auto& resources = Locator::resources::value();
		resources.UpdateLoading(k_FrameLoadBudget, k_FrameUploads);
		if (_prefetchTimer.has_value() && resources.PendingCount() == 0)
		{
			_prefetchTimer.reset();
		}

		auto duration = std::chrono::high_resolution_clock::now() - lastTime;
		auto milliseconds = std::chrono::duration_cast<std::chrono::duration<uint32_t, std::milli>>(duration);
		// The frame is drawn from where the inspector shows the camera (an override, a picture's), while everything the
		// game did this frame went by its own camera, which it gets back once the frame is drawn
		ShowInspectorCamera(true);
		{
			auto section = profiler.BeginScoped(Profiler::Stage::SceneDraw);
			const graphics::RendererInterface::DrawSceneDesc drawDesc {
			    .camera = &Locator::camera::value(),
			    .frameBuffer = nullptr,
			    .entities = Locator::entitiesRegistry::value(),
			    .time = milliseconds.count(), // TODO(#481): get actual time
			    .timeOfDay = config.timeOfDay,
			    .smallBumpMapStrength = config.smallBumpMapStrength,
			    .viewId = graphics::RenderPass::Main,
			    .drawSky = config.drawSky,
			    .drawWater = config.drawWater,
			    .drawIsland = config.drawIsland,
			    .drawEntities = config.drawEntities,
			    .drawSprites = config.drawSprites,
			    .drawVegetation = config.drawVegetation,
			    .drawBoundingBoxes = config.drawBoundingBoxes,
			    .cullBack = false,
			    .wireframe = config.wireframe,
			    .drawHand = IsHandDrawn(),
			};
			// The sun, the moon and the dome's blend of this frame, for drawing it
			Locator::skySystem::value().UpdateFrame(config.drawSky);
			// Nothing of the world shows under a video that covers the screen, nor behind the falling spell's film
			const auto& videos = Locator::videoSystem::value();
			if (videos.CoversScreen() || videos.HidesWorld())
			{
				bgfx::touch(static_cast<bgfx::ViewId>(graphics::RenderPass::Main));
			}
			else
			{
				Locator::rendererInterface::value().DrawScene(drawDesc);
			}
		}

		// The game's interface over the scene
		if (_interface && Locator::windowing::has_value())
		{
			const auto mouse = Locator::gameActionSystem::value().GetPointerPosition();
			_interface->Draw(static_cast<glm::u16vec2>(Locator::windowing::value().GetSize()), mouse, machine_clock::Ticks(),
			                 Locator::debugGui::value().IsMouseOverWindow());
		}

		{
			auto section = profiler.BeginScoped(Profiler::Stage::GuiDraw);
			const bool screenshotThisFrame = _requestScreenshot.has_value() && _requestScreenshot->first == _frameCount;
			if (screenshotThisFrame)
			{
				Locator::rendererInterface::value().RequestScreenshot(_requestScreenshot->second);
			}
			// A picture without the debug windows: the frame's windows are made as ever but not drawn
			const bool hiddenThisFrame = _debugGuiHiddenFrame == _frameCount;
			if ((!screenshotThisFrame || !_screenshotHidesDebugGui) && !hiddenThisFrame)
			{
				Locator::debugGui::value().Draw();
			}
		}

		{
			auto section = profiler.BeginScoped(Profiler::Stage::RendererFrame);
			Locator::rendererInterface::value().Frame();
		}
		ShowInspectorCamera(false);

		// Clear the stale screenshot request
		if (_requestScreenshot.has_value())
		{
			if (_requestScreenshot->first <= _frameCount)
			{
				_requestScreenshot = std::nullopt;
			}
		}

		_frameCount++;
		// The game is playable from its first frame drawn
		if (_startupTimer.has_value())
		{
			_startupTimer->Step("first frame");
			_startupTimer.reset();
		}

		const auto frameEnd = std::chrono::steady_clock::now();
		if (frameStats.has_value())
		{
			frameStats->Frame(std::chrono::duration<float, std::milli>(frameEnd - frameStart).count(), profiler);
		}
		frameStart = frameEnd;
	}

	// The last line of an orderly end: a log that stops without it, and without a crash report, was ended from outside
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The game ends after {} frames, at turn {}", _frameCount,
	                   Locator::time::value().GetTurn());
	return true;
}

bool Game::LoadMap(const std::filesystem::path& path, loading::LoadingClock::Mode look) noexcept
{
	const ecs::systems::InspectorLoading loading(path.filename().generic_string());
	auto& fileSystem = Locator::filesystem::value();

	if (!fileSystem.Exists(path))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Could not find script {}", path.generic_string());
		return false;
	}

	LoadTimer timer(fmt::format("land {}", path.stem().string()));
	BeginLoadingScreen(look);
	const auto data = fileSystem.ReadAll(path);
	const auto source = std::string(reinterpret_cast<const char*>(data.data()), data.size());

	PrepareNewLand();
	timer.Step("reset");
	RenderLoadingFrame();

	// A playground land is played as a skirmish, in which losing a temple doesn't end the game
	bool skirmish = false;
	Locator::resources::value().GetLevels().Each([&path, &skirmish](entt::id_type /*id*/, const Level& level) {
		skirmish = skirmish || (level.GetType() == Level::LandType::Skirmish &&
		                        level.GetScriptPath().lexically_normal() == path.lexically_normal());
	});
	Locator::entitiesRegistry::value().Context().skirmish = skirmish;
	_landPath = path;

	Script script;
	try
	{
		// The tips screen is drawn again as the map's commands are carried out, when it is due
		script.Load(source, [this]() { RenderLoadingFrame(); });
	}
	catch (const std::exception& e)
	{
		// A script that can't be read leaves the land as far as it got, rather than ending the game
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Error in the map script {}: {}", path.generic_string(), e.what());
	}

	timer.Step("map script");
	// Each released map comes with an optional .fot file which contains the footpath information for the map
	const auto stem = string_utils::LowerCase(path.stem().generic_string());
	const auto fotPath = fileSystem.GetPath<filesystem::Path::Landscape>() / fmt::format("{}.fot", stem);

	if (fileSystem.Exists(fotPath))
	{
		FotFile fotFile(*this);
		fotFile.Load(fotPath);
	}
	else
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The map at {} does not come with a footpath file. Expected {}",
		                   path.generic_string(), fotPath.generic_string());
	}

	timer.Step("footpaths");
	// With the land laid out, each town of a player with a temple is given its worship site if it has none
	if (Locator::worshipSiteSystem::has_value())
	{
		Locator::worshipSiteSystem::value().LandLaidOut();
	}

	// With the land laid out, each town gathers the lone trees about it into its scenic forest
	if (Locator::forestSystem::has_value())
	{
		Locator::forestSystem::value().MakeScenicForests();
		// Each town finds the forests near it with wood in them
		Locator::forestSystem::value().AssignForestsToTowns();
	}

	timer.Step("forests");
	RenderLoadingFrame();
	StartNewLand();
	timer.Step("start");
	EndLoadingScreen();
	return true;
}

bool Game::LoadMapWithFreshScripts(const std::filesystem::path& path) noexcept
{
	// Outside the story, a land is loaded with the challenge's scripts started again from scratch: every task of the
	// last land stops, the scripts' variables are cleared and the challenge's scripts that start by themselves start
	// again. None of the last land's scripts go on running on the new land.
	if (Locator::vm::has_value())
	{
		// The program starting again frees every place of the scripts' objects, while the last land's objects are still
		// there to be let go back into the game
		if (Locator::scriptObjects::has_value())
		{
			Locator::scriptObjects::value().Reset();
		}
		if (Locator::chlapi::has_value())
		{
			Locator::chlapi::value().ResetSwitches();
		}
		auto& fileSystem = Locator::filesystem::value();
		const auto challengePath = fileSystem.GetPath<filesystem::Path::Quests>() / "challenge.chl";
		try
		{
			Locator::vm::value().LoadBinary(fileSystem.ReadAll(challengePath));
		}
		catch (const std::exception& err)
		{
			Locator::vm::value().StopAllTasks();
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Failed to read challenge file at {}: {}", challengePath.generic_string(),
			                    err.what());
		}
	}
	return LoadMap(path);
}

void Game::LoadTestbed() noexcept
{
	const ecs::systems::InspectorLoading loading("testbed");
	_landPath = "testbed";
	// No script runs on the testbed: the story's would set its time of day and stop its clock a few turns in
	if (Locator::vm::has_value())
	{
		Locator::vm::value().StopAllTasks();
	}
	BeginLoadingScreen(loading::LoadingClock::Mode::Tips);
	PrepareNewLand();
	RenderLoadingFrame();
	InitializeLevel(flat_land::Build());
	SetUpLandscape();
	RenderLoadingFrame();
	EndLoadingScreen();

	// Looking down over the middle of the map, from the south
	const auto& land = Locator::terrainSystem::value();
	const auto middle = (land.GetExtent().minimum + land.GetExtent().maximum) * 0.5f;
	const auto ground = land.GetHeightAt(middle);
	Locator::camera::value()
	    .SetOrigin({middle.x, ground + k_TestbedCameraHeight, middle.y - k_TestbedCameraBack})
	    .SetFocus({middle.x, ground, middle.y});

	StartNewLand();

	// The testbed has no temple to give its player influence, so a source of influence over its middle gives them the
	// reach a citadel would: the miracles cast only in influence may be cast there, and nowhere beyond. Their worship
	// stands behind them with prayer power to spare, which the miracles still draw from. A dispenser of every miracle
	// stands in a grid in front of the camera.
	auto& registry = Locator::entitiesRegistry::value();
	const auto influence = registry.Create();
	registry.Assign<ecs::components::Transform>(influence, glm::vec3(middle.x, ground, middle.y), glm::mat3(1.0f),
	                                            glm::vec3(1.0f));
	registry.Assign<ecs::components::InfluenceSource>(influence, PlayerNames::PLAYER_ONE, k_TestbedInfluenceRadius);
	registry.Each<const ecs::components::Player>([&registry](entt::entity entity, const ecs::components::Player& player) {
		if (player.name == PlayerNames::PLAYER_ONE)
		{
			registry.AssignOrReplace<ecs::components::PrayerPower>(entity, k_TestbedPrayer, false);
		}
	});
	testbed_dispensers::PlaceGrid(middle);

	// The testbed comes with its window of scenarios to try out on it
	if (Locator::debugGui::has_value() && _testbedWindow)
	{
		Locator::debugGui::value().OpenWindow(debug::gui::k_TestbedScenariosWindow);
	}
}

void Game::PrepareNewLand()
{
	// The player's creature is kept with its mind and body before anything of the land goes, for a later land's script
	// to load it again; and the players keep what is theirs rather than the land's, such as their alignment
	if (Locator::creatureCarryOverSystem::has_value())
	{
		Locator::creatureCarryOverSystem::value().KeepPlayersCreature();
		Locator::creatureCarryOverSystem::value().Reset();
	}
	if (Locator::playerSystem::has_value())
	{
		Locator::playerSystem::value().KeepForNextLand();
	}
	// The last land's scripts forget what they held: the land's objects go with it before anything could be let go back
	// into the game
	if (Locator::scriptObjects::has_value())
	{
		Locator::scriptObjects::value().ClearForNewLand();
	}
	// A new land has no weather of the last one, and none of its script's fades, cinema bars or clipping
	if (Locator::weatherSystem::has_value())
	{
		Locator::weatherSystem::value().Reset();
		Locator::snowSystem::value().Reset();
		Locator::waterRingSystem::value().Reset();
	}
	Locator::cinematicDirectorSystem::value().Reset();
	// Nor does any script keep the camera, the game's speed or the dialogue
	if (Locator::camera::has_value())
	{
		auto& scriptControl = Locator::scriptControlSystem::value();
		if (const auto owner = scriptControl.GetCameraOwner(); owner != 0)
		{
			scriptControl.EndCameraControl(Locator::camera::value(), owner);
		}
		scriptControl.Reset();
	}
	Locator::dialogueControlSystem::value().Reset();
	// The camera's bookmarks a script put away come back (a land's first load makes them anew)
	if (Locator::cameraBookmarkSystem::has_value())
	{
		Locator::cameraBookmarkSystem::value().SetEnabled(true);
	}
	// The scripts start again, and the help's texts and voices with them
	Locator::helpTextSystem::value().Reset();
	Locator::cameraHelpSystem::value().Get().ResetForNewLand();
	Locator::influenceSystem::value().Reset();
	// Nor its creatures' footprints, nor a scare of its fish
	Locator::footprintSystem::value().Reset();
	Locator::fishFarmSystem::value().Reset();
	// Nor its miracles, nor their particle effects, nor its fires
	Locator::magicSystem::value().Reset();
	Locator::miracleFxSystem::value().Reset();
	Locator::fireSystem::value().Reset();
	// Nor the beat of its scrolls
	Locator::scriptHighlightSystem::value().Reset();
	Locator::tipBubbleSystem::value().Reset();
	Locator::handDemoSystem::value().Reset();
	Locator::creatureFightSystem::value().Reset();
	Locator::explosionSystem::value().Reset();
	Locator::magicSystem::value().SetIgnoreInfluence(false);
	Locator::animalSystem::value().Reset();
	if (Locator::animatedStaticSystem::has_value())
	{
		Locator::animatedStaticSystem::value().Reset();
	}
	Locator::magicShieldSystem::value().Reset();
	Locator::forestSystem::value().Reset();
	// Nor its fireflies, nor what they give
	Locator::fireflySystem::value().Reset();
	Locator::reactionSystem::value().Reset();
	Locator::teleportSystem::value().Reset();
	Locator::gestureEvents::value().Reset();
	Locator::particleSystem::value().Reset();
	// Nor its bodies in the physics
	if (Locator::dynamicsSystem::has_value())
	{
		Locator::dynamicsSystem::value().ResetSimulation();
	}
	// Nor its broken buildings and their pieces
	if (Locator::buildingDamageSystem::has_value())
	{
		Locator::buildingDamageSystem::value().Reset();
	}
	// Nor anything in the hand
	if (Locator::handGrabSystem::has_value())
	{
		Locator::handGrabSystem::value().Reset();
	}

	// The sky goes on as it was: it is kept while every entity goes, and made again after
	Locator::skySystem::value().KeepForNextLand();
	// Reset everything. Deletes all entities and their components
	Locator::entitiesRegistry::value().Reset();
	Locator::skySystem::value().Initialize();
	// TODO(#661): split entities that are permanent from map entities and move hand and camera to init
	// We need a hand for the player
	Locator::handSystem::value().Initialize();

	// create our camera
	auto& config = Locator::config::value();
	const auto aspect = Locator::windowing::has_value() ? Locator::windowing::value().GetAspectRatio() : 1.0f;
	Locator::camera::value().SetProjectionMatrixPerspective(config.cameraXFov, aspect, config.cameraNearClip,
	                                                        config.cameraFarClip);
}

void Game::StartNewLand()
{
	_lastGameLoopTime = machine_clock::Ticks();
	_turnDeltaTime = 0ns;
	// The game starts running, as Black & White does
	Locator::time::value().StartGameClock(false);
	SetGameSpeed(Game::k_TurnDurationMultiplierNormal);

	if (!_atmosAudio)
	{
		_atmosAudio = std::make_unique<audio::AtmosAudio>();
	}
	_atmosAudio->Init();
	if (!_gameMusic)
	{
		_gameMusic = std::make_unique<audio::GameMusic>();
		audio::GameMusic::RegisterBanks();
	}
	_gameMusic->Reset();
}

void Game::AskNewGameChoice()
{
	// Every new game starts with nothing skipped, and only a returning player is asked
	Locator::tutorialSkipSystem::value().Set({});
	const auto& profiles = Locator::playerProfileSystem::value();
	if (!new_game_choice::AsksAtNewGame(profiles.GetProfileCount(), profiles.CurrentProfileHasCreature()) || !_interface)
	{
		return;
	}
	// The game waits, paused, for the answer
	Locator::time::value().SetPaused(true);
	_interface->ShowSkipBox();
}

void Game::HandleInterfaceAction()
{
	using Action = gui::GameMenu::Action;
	const auto action = _interface->TakeAction();

	// The answer to the start-of-game question tells the story what to skip, and the game goes on
	if (const auto answer = _interface->TakeSkipBoxAnswer(); answer.has_value())
	{
		Locator::tutorialSkipSystem::value().Set(new_game_choice::SkipFor(*answer));
		Locator::time::value().SetPaused(false);
	}

	// The settings the player changes take effect at once
	if (_interface->TakeSettingsChanged())
	{
		const auto& settings = _interface->GetMenu().GetSettings();
		auto& audio = Locator::audio::value();
		audio.SetSfxVolume(settings.sfxVolume);
		audio.SetMusicVolume(settings.musicVolume);
		Locator::config::value().rightHandedHand = !settings.leftHandedHand;
	}

	// The menu pauses the game while it is open
	const auto open = _interface->GetMenu().IsOpen();
	if (open && !_menuWasOpen)
	{
		_pausedBeforeMenu = IsPaused();
		Locator::time::value().SetPaused(true);
	}
	else if (!open && _menuWasOpen)
	{
		Locator::time::value().SetPaused(_pausedBeforeMenu);
	}
	_menuWasOpen = open;

	switch (action)
	{
	case Action::Quit:
		RequestQuit();
		break;
	case Action::StartSkirmish:
	case Action::JoinOnline:
	case Action::Statistics:
	case Action::CreatePlayer:
	case Action::DeletePlayer:
	case Action::EditTattoo:
	case Action::StartNewGame:
	case Action::RedefineControl:
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "This part of the menu is not available yet");
		break;
	case Action::Continue:
	case Action::None:
		break;
	}
}

void Game::LoadLandscape(const std::filesystem::path& path)
{
	auto& fileSystem = Locator::filesystem::value();

	auto fixedName = fileSystem.FindPath(filesystem::FileSystemInterface::FixPath(path));

	if (!fileSystem.Exists(fixedName))
	{
		throw std::runtime_error("Could not find landscape " + path.generic_string());
	}
	LoadTimer timer(fmt::format("landscape {}", path.stem().string()));
	InitializeLevel(fixedName);
	SetUpLandscape();
}

void Game::SetUpLandscape()
{
	// A land starts at noon on the game's cycle of day and night, which its script may change, under new clouds
	auto& sky = Locator::skySystem::value();
	sky.GetClock().Reset();
	sky.SetTime(sky.GetClock().GetScriptTime());
	Locator::cloudSystem::value().Reset();

	// There is always a player active
	Locator::playerSystem::value().AddPlayer(ecs::archetypes::PlayerArchetype::Create(PlayerNames::PLAYER_ONE));

	Locator::cameraBookmarkSystem::value().Initialize();
	Locator::playerSystem::value().RegisterPlayers();
	if (Locator::cameraPathSystem::value().IsPathing())
	{
		Locator::cameraPathSystem::value().Stop();
	}
}

void Game::SetTime(float time) noexcept
{
	Locator::skySystem::value().SetTime(time);
}

void Game::FitNearClip()
{
	// Outside a camera with a lens of its own, the near plane follows the camera's height over the land, but for close
	// shots: a script's, and a miracle's camera path
	auto& camera = Locator::camera::value();
	if (camera.GetModel().GetLens().has_value() || !Locator::terrainSystem::has_value())
	{
		return;
	}
	const auto origin = camera.GetOrigin();
	const float height = origin.y - Locator::terrainSystem::value().GetHeightAt(glm::vec2(origin.x, origin.z));
	const float nearClip = near_clipping::NearPlane(height, Locator::cinematicDirectorSystem::value().IsCloseClipping() ||
	                                                            Locator::cameraPathSystem::value().HoldsCamera());
	if (nearClip != camera.GetNearClip())
	{
		camera.SetNearClip(nearClip);
	}
}

void Game::ShowInspectorCamera(bool shown)
{
	if (!Locator::inspector::has_value())
	{
		return;
	}
	auto& inspector = Locator::inspector::value();
	if (shown)
	{
		inspector.PlaceCamera();
	}
	else
	{
		inspector.GiveCameraBack();
	}
	FitNearClip();
}

void Game::RequestScreenshot(const std::filesystem::path& path, bool hideDebugGui) noexcept
{
	_requestScreenshot = std::make_pair(_frameCount, path);
	_screenshotHidesDebugGui = hideDebugGui;
}

void Game::LoadHandAnimation()
{
	auto& fileSystem = Locator::filesystem::value();
	const auto path = fileSystem.GetPath<filesystem::Path::Data>() / "CTR" / "hh.hbn";
	const auto specPath = fileSystem.GetPath<filesystem::Path::Data>() / "hndspec5.txt";
	if (!fileSystem.Exists(path) || !fileSystem.Exists(specPath))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The hand is not animated: {} or {} is missing", path.string(),
		                   specPath.string());
		return;
	}

	pack::PackFile pack;
	const auto packResult = pack.ReadFile(*fileSystem.GetData(path));
	if (packResult != pack::PackResult::Success || !pack.HasBlock("Hand"))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Unable to read the Hand block of {}: {}", path.string(),
		                    pack::ResultToStr(packResult));
		return;
	}
	morph::MorphFile morphFile;
	const auto morphResult = morphFile.Open(pack.GetBlock("Hand"), fileSystem.FindPath(specPath).parent_path());
	if (morphResult != morph::MorphResult::Success)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Unable to read the hand animations of {}: {}", path.string(),
		                    morph::ResultToStr(morphResult));
		return;
	}

	LoadHandLooks(morphFile);

	const auto mesh = Locator::resources::value().GetMeshes().Handle(entt::hashed_string("hand"));
	auto animation = std::make_unique<HandAnimation>();
	if (!mesh || !animation->Load(morphFile, mesh->GetBoneParents(), mesh->GetBoneMatrices()))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The hand animations of {} do not fit the hand mesh", path.string());
		return;
	}
	_handAnimation = std::move(animation);
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loaded the hand animations of {}", path.string());
}

void Game::LoadCreatureRigs()
{
	auto& fileSystem = Locator::filesystem::value();
	const auto directory = fileSystem.GetPath<filesystem::Path::Data>() / "CTR";
	const auto specPath = fileSystem.GetPath<filesystem::Path::Data>() / "ctrspec27.txt";
	if (!fileSystem.Exists(specPath))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The creatures are not animated: {} is missing", specPath.string());
		return;
	}
	const auto specDirectory = fileSystem.FindPath(specPath).parent_path();
	const auto meshDirectory = fileSystem.GetPath<filesystem::Path::CreatureMesh>();
	auto& rigs = Locator::resources::value().GetCreatureRigs();
	fileSystem.Iterate(directory, false, [&](const std::filesystem::path& path) {
		if (string_utils::LowerCase(path.extension().string()) != ".cbn")
		{
			return;
		}
		// Only the header of the species' animations is read now, for which species they are; the rest when the species
		// is first wanted
		pack::PackFile pack;
		const auto stream = fileSystem.GetData(path);
		const auto block =
		    pack.ReadFile(*stream, {"Creature"}) == pack::PackResult::Success ? pack.GetUnreadBlock("Creature") : std::nullopt;
		if (!block.has_value())
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Unable to read the Creature block of {}", path.string());
			return;
		}
		if (block->size < sizeof(morph::MorphHeader))
		{
			return;
		}
		// The species is the one the base mesh named in the header is of
		morph::MorphHeader header {};
		stream->clear();
		stream->seekg(static_cast<std::streamoff>(block->offset));
		stream->read(reinterpret_cast<char*>(&header), sizeof(header));
		const auto species = creature::GetSpeciesFromMeshName(header.baseMeshName.data());
		if (!*stream || species == CreatureType::Unknown)
		{
			return;
		}
		rigs.Register(
		    creature::GetRigId(species),
		    [path, block = *block, specDirectory, meshDirectory] {
			    const auto file = Locator::filesystem::value().GetData(path);
			    std::vector<uint8_t> bytes(block.size);
			    file->seekg(static_cast<std::streamoff>(block.offset));
			    file->read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
			    if (!*file)
			    {
				    throw std::runtime_error("Unable to read the Creature block of " + path.string());
			    }
			    return resources::CreatureRigLoader {}(resources::CreatureRigLoader::FromBufferTag {}, bytes, specDirectory,
			                                           meshDirectory);
		    },
		    block->size);
	});
}

void Game::PlaceHand(ecs::components::Transform& handTransform, float deltaSeconds)
{
	const auto& camera = Locator::camera::value();
	const auto eye = camera.GetOrigin();
	// How far the hand reaches from the camera, as the land's scripts allow
	const auto handReach = Locator::cameraHelpSystem::value().Get().handReach;

	// In the temple the hand hangs on the line of sight through the cursor, a little short of where it
	// meets the room, slowly away from the camera and quickly towards it
	if (Locator::temple::has_value() && Locator::temple::value().Active())
	{
		if (_cursorWorldPosition)
		{
			const auto toRoom = *_cursorWorldPosition - eye;
			const auto roomDistance = glm::length(toRoom);
			if (roomDistance > 0.0f)
			{
				_handRayDirection = toRoom / roomDistance;
			}
			const auto target =
			    glm::clamp(glm::max(roomDistance - k_HandTempleGap, 1.0f), k_HandTempleMinDistance, k_HandTempleMaxDistance);
			const auto easeTime = _handHoverZoomer.GetDestination() <= target ? 0.4f : 0.2f;
			_handHoverZoomer.SetDestination(target, easeTime);
			_handHoverZoomer.Update(deltaSeconds);
			// The hand's distance from the camera
			_handDistance = _handHoverZoomer.GetValue();
		}
		handTransform.position = eye + _handRayDirection * _handDistance;
		return;
	}

	// Tapping a house it knocked on, the hand stays where it knocked
	if (_handKnocking)
	{
		_handPosition = _handKnock->point;
		_handCrossFade.Update(deltaSeconds);
		handTransform.position = _handCrossFade.Apply(_handPosition);
		return;
	}

	// Gripping the land, the camera keeps the land the hand gripped under the cursor, and the hand stays on the land it
	// gripped, so it moves with it. Its hover carries on from how far the gripped land was from the camera.
	const bool gripsLand = _handCameraState && _handPose == hand_navigation_pose::Pose::Grip;
	if (gripsLand)
	{
		if (!_handWasGripping)
		{
			_handGripPoint = _cursorWorldPosition.value_or(_handPosition);
		}
		_handPosition = _handGripPoint;
		_handDistance = glm::clamp(glm::distance(eye, _handGripPoint), k_HandMinDistance, handReach);
		_handHoverZoomer.Reset(_handDistance);
		_handHoldZoomer.Reset(_handDistance);
		_handWasGripping = true;
		_handCrossFade.Update(deltaSeconds);
		handTransform.position = _handCrossFade.Apply(_handPosition);
		return;
	}
	_handWasGripping = false;

	// Dragging by the edge of the screen, before the drag is decided or as it turns or tilts the camera, the hand keeps
	// on the line of sight through the cursor, holding how far it is from the camera over 0.4 seconds but never past
	// the land
	if (_handCameraState)
	{
		const auto screenSize =
		    Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::zero<glm::ivec2>();
		if (screenSize.x > 0 && screenSize.y > 0)
		{
			glm::vec3 rayOrigin;
			glm::vec3 rayDirection;
			camera.DeprojectScreenToWorld(static_cast<glm::vec2>(_mousePosition) / static_cast<glm::vec2>(screenSize),
			                              rayOrigin, rayDirection);
			if (!glm::any(glm::isnan(rayDirection)))
			{
				_handRayDirection = glm::normalize(rayDirection);
			}
		}
		_handHoldZoomer.SetDestination(glm::distance(eye, _handPosition), k_HandHoldTime);
		_handHoldZoomer.Update(deltaSeconds);
		const auto landDistance = _cursorWorldPosition.has_value() ? glm::distance(eye, *_cursorWorldPosition) : handReach;
		_handDistance = glm::clamp(glm::min(_handHoldZoomer.GetValue(), landDistance), k_HandMinDistance, handReach);
		_handHoverZoomer.Reset(_handDistance);
		_handPosition = eye + _handRayDirection * _handDistance;
		_handCrossFade.Update(deltaSeconds);
		handTransform.position = _handCrossFade.Apply(_handPosition);
		return;
	}
	_handHoldZoomer.Reset(_handDistance);

	// The game puts the origin of the hand, by its fingertips, on the line of sight through the cursor, so the hand is
	// always under the cursor on screen. How far along it depends on the land the cursor is over. Turning the camera
	// with the mouse holds the cursor still, so the hand stays where it is on screen and eases to the land coming under
	// it.
	if (_cursorWorldPosition)
	{
		const auto toLand = *_cursorWorldPosition - eye;
		const auto landDistance = glm::length(toLand);
		if (landDistance > 0.0f)
		{
			_handRayDirection = toLand / landDistance;
		}

		// The hand is pulled back from the land towards the camera by its height, so its fingers hang
		// down to the land. Over the sea it rests on the water.
		const auto overSea = Locator::terrainSystem::value().GetHeightAt(glm::xz(*_cursorWorldPosition)) < k_HandSeaAltitude;
		const auto handHeight = k_HandHeight * HandAnimation::SizeAtDistance(_handDistance);
		const auto nearest = glm::clamp(overSea ? landDistance : landDistance - handHeight, k_HandMinDistance, handReach);

		// The hand eases out to the land, slowly away from the camera and quickly towards it
		const auto target = glm::max(landDistance, 1.0f);
		const auto easeTime = _handHoverZoomer.GetValue() <= target ? k_HandEaseOutTime : k_HandEaseInTime;
		_handHoverZoomer.SetDestination(target, easeTime);
		_handHoverZoomer.Update(deltaSeconds);
		if (_handHoverZoomer.GetValue() < 1.0f)
		{
			_handHoverZoomer.Reset(1.0f);
		}
		// The hand's distance from the camera, no further out than the land less the hand's height
		_handDistance = glm::clamp(glm::min(_handHoverZoomer.GetValue(), nearest), k_HandMinDistance, handReach);
	}
	_handPosition = eye + _handRayDirection * _handDistance;
	_handCrossFade.Update(deltaSeconds);
	handTransform.position = _handCrossFade.Apply(_handPosition);
}

void Game::UpdateHandKnock(const ecs::components::Transform& handTransform)
{
	if (!Locator::abodeKnockSystem::has_value())
	{
		return;
	}
	auto& knocks = Locator::abodeKnockSystem::value();
	// The houses' read-out of their people runs by the frame
	knocks.Update(Locator::time::value().GetFrameRealTime());
	// A knock while the tap plays doesn't start it over, the hand already being where it knocked
	if (knocks.TakeHandKnock())
	{
		if (_handKnock.has_value())
		{
			_handKnock->point = _handPosition;
		}
		else
		{
			_handKnock = HandKnock {.point = _handPosition};
		}
	}
	// Holding something comes first; the tap starts over once the hand lets go
	bool holds = magic::HandHoldPoser::Find().has_value();
	if (!holds && Locator::handGrabSystem::has_value())
	{
		holds = Locator::handGrabSystem::value().GetHeldPose().has_value();
	}
	if (holds && _handKnock.has_value())
	{
		_handKnock->time = std::chrono::microseconds::zero();
	}
	const bool knocking = _handKnock.has_value() && !holds;
	// Starting and ending the tap, the hand fades from where it was drawn
	if (knocking != _handKnocking)
	{
		_handCrossFade.Start(handTransform.position);
	}
	_handKnocking = knocking;
}

void Game::UpdateHandNavigation(const ecs::components::Transform& handTransform)
{
	using namespace hand_navigation_pose;
	auto cues = Locator::camera::value().GetModel().GetHandCues();
	// A hand demonstration shows the camera hints its recording had
	if (Locator::handDemoSystem::has_value() && Locator::handDemoSystem::value().IsPlaying(0))
	{
		cues.tricons = Locator::handDemoSystem::value().GetHints();
	}
	// Dragging the land is the hand's camera state; turning the camera with the middle button or both buttons isn't
	const bool cameraState = _handGripping && !_handRotating;
	auto pose = Pose::Idle;
	if (cameraState)
	{
		// Other cameras than the world's don't sort their drags: the hand grips the land at once
		const bool gripsLand = !cues.dragging || cues.dragMode == camera_drag::DragMode::Pan || cues.clearViewGrip;
		pose = WhileDragging(gripsLand, cues.tricons);
	}
	else if (!_cursorOnObject && !_creatureUnderHand.has_value() && !_handOnCreature.has_value())
	{
		pose = WhileHovering(cues.tricons).value_or(Pose::Idle);
	}
	// Changing state or pose, the hand fades from where it was drawn to its new place
	if (cameraState != _handCameraState || pose != _handPose)
	{
		_handCrossFade.Start(handTransform.position);
	}
	_handCameraState = cameraState;
	_handPose = pose;
}

void Game::OrientHand(ecs::components::Transform& handTransform, const glm::mat3& facingCamera, glm::vec3 surfaceUp,
                      float deltaSeconds)
{
	using namespace hand_orientation;
	const auto& camera = Locator::camera::value();
	const auto screenSize = Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::zero<glm::ivec2>();

	// The hand faces along the line of sight through the cursor, laid flat
	if (screenSize.x > 0 && screenSize.y > 0)
	{
		glm::vec3 rayOrigin;
		glm::vec3 rayDirection;
		camera.DeprojectScreenToWorld(static_cast<glm::vec2>(_mousePosition) / static_cast<glm::vec2>(screenSize), rayOrigin,
		                              rayDirection);
		if (!glm::any(glm::isnan(rayDirection)))
		{
			_handHeading = HeadingAlongRay(rayDirection, _handHeading);
		}
	}
	const auto cameraHeading = HeadingAlongRay(camera.GetForward(), _handHeading);

	// Its up eases over 0.4 seconds to the slope of the land under it, or to the face of what the cursor is on, given
	// afresh only as the cursor moves across the screen. Offering to turn the camera at the edge of the screen it
	// stands up towards where the camera looks instead. Dragging the land it holds its up, or stands straight towards
	// the camera's focus showing it turns the camera; letting go it stands up straight and eases from there.
	using hand_navigation_pose::Pose;
	const bool cursorMovedAcross = _mousePosition.x != _handLastCursorX;
	_handLastCursorX = _mousePosition.x;
	if (_handCameraState)
	{
		if (_handPose == Pose::Rotate)
		{
			_handUp.Reset(hand_navigation_pose::UpTowardsFocus(_handPosition, camera.GetFocus()));
		}
	}
	else
	{
		if (_handUpWasHeld)
		{
			_handUp.Reset(glm::vec3(0.0f, 1.0f, 0.0f));
		}
		if (cursorMovedAcross)
		{
			// The face of a model the hand rests on, unless it is lower over the thing than half its own height; else the
			// land where the hand is
			const float handHeight = k_HandHeight * HandAnimation::SizeAtDistance(_handDistance);
			const auto handPoint = map_coords::ToMetres(map_coords::FromMetres(glm::xz(handTransform.position)));
			const float aboveLand = Locator::terrainSystem::has_value()
			                            ? handTransform.position.y - Locator::terrainSystem::value().GetHeightAt(handPoint)
			                            : 0.0f;
			auto up = !hand_feel::TurnsToLand(_handSurfaceUp.has_value(), _handOverObject, aboveLand, handHeight)
			              ? surfaceUp
			              : (Locator::terrainSystem::has_value()
			                     ? Locator::terrainSystem::value().GetNormalAt(glm::xz(handTransform.position))
			                     : glm::vec3(0.0f, 1.0f, 0.0f));
			if (_handPose == Pose::Rotate)
			{
				up = hand_navigation_pose::UpTowardsFocus(_cursorWorldPosition.value_or(_handPosition), camera.GetFocus());
			}
			_handUp.SetDestination(up, k_UpEaseSeconds);
		}
		_handUp.Update(deltaSeconds);
	}
	_handUpWasHeld = _handCameraState;

	// Tapping a house, the hand stands straight up
	if (_handKnocking)
	{
		_handUp.Reset(glm::vec3(0.0f, 1.0f, 0.0f));
	}
	const auto up = _handUp.GetValue();
	const auto onLevelLand = TurnToHeading(facingCamera, cameraHeading, _handHeading);
	handTransform.rotation = glm::length(up) > 0.0f ? StandOnSlope(onLevelLand, _handHeading, up) : onLevelLand;
	// Scooping, the hand faces along its heading on level land and is tipped down towards what it scoops from
	if (Locator::handGrabSystem::has_value())
	{
		if (const auto tip = Locator::handGrabSystem::value().GetScoopTip())
		{
			handTransform.rotation = TipForwards(onLevelLand, _handHeading, *tip);
		}
	}
}

std::chrono::milliseconds Game::CameraStepTime() const
{
	const auto& time = Locator::time::value();
	const bool scripted = Locator::cinematicDirectorSystem::has_value() &&
	                      Locator::cinematicDirectorSystem::value().IsWideScreenOn() &&
	                      Locator::cinematicDirectorSystem::value().GetWideScreenOwner() != 0;
	return ecs::systems::CameraStep(time.GetFrameRealTime(), time.GetFrameGameTime(), scripted);
}

float Game::HandStepSeconds() const
{
	return static_cast<float>(static_cast<int32_t>(CameraStepTime().count())) * 0.001f;
}

void Game::UpdateMagicHand(const glm::vec3& handPosition, float deltaSeconds)
{
	const auto screenSize = Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::zero<glm::ivec2>();
	ecs::systems::MagicSystemInterface::HandFrame frame {.handPosition = handPosition, .point = _cursorWorldPosition};
	if (screenSize.x > 0 && screenSize.y > 0)
	{
		Locator::camera::value().DeprojectScreenToWorld(
		    static_cast<glm::vec2>(_mousePosition) / static_cast<glm::vec2>(screenSize), frame.rayOrigin, frame.rayDirection);
	}
	frame.cameraForward = Locator::camera::value().GetForward();
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	frame.overWorld = !inTemple && !Locator::debugGui::value().IsMouseOverWindow();
	auto& magic = Locator::magicSystem::value();
	// The hand's movement, and the spin it gives a miracle, are measured by what the hand steps by
	magic.UpdateHand(frame, HandStepSeconds());
	magic.Update(deltaSeconds);
	Locator::magicShieldSystem::value().Update(deltaSeconds);
}

void Game::PlayHandGrabSound()
{
	if (!_cursorWorldPosition || !Locator::terrainSystem::has_value())
	{
		return;
	}
	const auto position = *_cursorWorldPosition;

	// Land is a cell of the landscape without water
	bool isLand = false;
	const auto cell = glm::floor(glm::vec2(position.x, position.z) / 10.0f);
	if (cell.x >= 0.0f && cell.y >= 0.0f && cell.x < 512.0f && cell.y < 512.0f)
	{
		const auto* landCell = Locator::terrainSystem::value().FindCell(glm::u16vec2(cell));
		isLand = landCell != nullptr && landCell->properties.hasWater == 0;
	}

	if (isLand)
	{
		// The game throws up a spot visual where the hand grips the land, as it plays the sound
		GripLandscapeEffect::Spawn(
		    glm::vec3(position.x, Locator::terrainSystem::value().GetHeightAt(glm::xz(position)), position.z));
	}
	if (isLand)
	{
		// One of G_HandGrabLand_01 to _06, centred on the listener
		const auto sample = 4 + Locator::rng::value().NextValue(0, 5);
		const auto id = fmt::format("InGame.sad/{}", sample);
		audio::PlayGameSoundEffect(entt::hashed_string(id.c_str()), std::nullopt);
	}
	else
	{
		// While the game runs, the hand splashes where it goes in: a ring on the water, in the land's brightest light
		if (!Locator::time::value().IsPaused())
		{
			const auto angle = Locator::gameRandom::value().CrtRandom(0.0f, glm::two_pi<float>());
			Locator::waterRingSystem::value().Add(
			    water_rings::HandSplash(glm::vec2(position.x, position.z), angle, FrameLandLight(255)));
			// The fish near where the hand went in dart away
			Locator::fishFarmSystem::value().Scare({position.x, k_HandSplashHeight, position.z});
		}
		// G_HandInWater_01 to _10 in turn, on the water's surface where the hand went in
		const auto id = fmt::format("InGame.sad/{}", 99 + _handInWaterSample);
		_handInWaterSample = (_handInWaterSample + 1) % 10;
		audio::PlayGameSoundEffect(entt::hashed_string(id.c_str()), glm::vec3(position.x, k_HandSplashHeight, position.z));
	}
}
