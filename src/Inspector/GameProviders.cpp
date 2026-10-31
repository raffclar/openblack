/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameProviders.h"

#include <cctype>
#include <cmath>

#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <Inspector.h>
#include <InspectorQuery.h>
#include <L3DFile.h>
#include <LHVM.h>
#include <LNDFile.h>
#include <SaveSlots.h>
#include <fmt/format.h>
#include <glm/matrix.hpp>
#include <glm/trigonometric.hpp>

#include "3D/CreatureBody.h"
#include "3D/DayNightClock.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "3D/OceanInterface.h"
#include "3D/TempleInteriorInterface.h"
#include "CHLApi.h"
#include "Camera/Camera.h"
#include "Camera/CameraZones.h"
#include "Common/EventManager.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameHeading.h"
#include "Common/GameRandom.h"
#include "Common/MainThreadQueue.h"
#include "Common/RandomNumberManager.h"
#include "Creature/BonusCreatures.h"
#include "Creature/CreatureCastMoves.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureFriendAgendas.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureRoute.h"
#include "Creature/CreatureScriptAgendas.h"
#include "Creature/LeashOrders.h"
#include "Creature/LeashRules.h"
#include "Debug/DebugGuiInterface.h"
#include "Debug/TempleVillagersSurvey.h"
#include "Debug/TownMiracles.h"
#include "ECS/ArtefactRules.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/AppleBarrel.h"
#include "ECS/Components/AudioEmitter.h"
#include "ECS/Components/BoatFilm.h"
#include "ECS/Components/CameraBookmark.h"
#include "ECS/Components/ChimneySmoke.h"
#include "ECS/Components/Cloud.h"
#include "ECS/Components/ComputerPlayer.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCopy.h"
#include "ECS/Components/CreatureHelp.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLeashRope.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Firefly.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/GameStanding.h"
#include "ECS/Components/GuidanceHeard.h"
#include "ECS/Components/HiddenByState.h"
#include "ECS/Components/HighDetail.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerRelations.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Shark.h"
#include "ECS/Components/Sky.h"
#include "ECS/Components/SoundTag.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TempleExterior.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownArtefact.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageLight.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Vortex.h"
#include "ECS/Components/WalkPath.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/Workshop.h"
#include "ECS/CreatureReactionNotice.h"
#include "ECS/DrawnRegistry.h"
#include "ECS/Map.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/PlayerRelations.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AbodeKnockSystemInterface.h"
#include "ECS/Systems/AbodeSoundSystemInterface.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/AdvisorVoiceSystemInterface.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/AnimatedStaticSystemInterface.h"
#include "ECS/Systems/AppleBarrelSystemInterface.h"
#include "ECS/Systems/ArtefactSystemInterface.h"
#include "ECS/Systems/BoatLaunchSystemInterface.h"
#include "ECS/Systems/BuildingDamageSystemInterface.h"
#include "ECS/Systems/CameraBookmarkSystemInterface.h"
#include "ECS/Systems/CameraHelpSystemInterface.h"
#include "ECS/Systems/CameraPathSystemInterface.h"
#include "ECS/Systems/CameraZoneSystemInterface.h"
#include "ECS/Systems/ChatSystemInterface.h"
#include "ECS/Systems/ChimneySmokeSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CloudSystemInterface.h"
#include "ECS/Systems/ComputerSettingsSystemInterface.h"
#include "ECS/Systems/CreatureAnimationLogSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureAudioSystemInterface.h"
#include "ECS/Systems/CreatureCarryOverSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureFizzSystemInterface.h"
#include "ECS/Systems/CreatureHairSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreaturePenSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/CreatureWebPageSystemInterface.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/DialogueControlSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/EditorSystemInterface.h"
#include "ECS/Systems/EffectRequestSystemInterface.h"
#include "ECS/Systems/ExplosionSystemInterface.h"
#include "ECS/Systems/FieldSystemInterface.h"
#include "ECS/Systems/FilmCreatureSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/FireflySystemInterface.h"
#include "ECS/Systems/FishFarmSystemInterface.h"
#include "ECS/Systems/FloatingNumberSystemInterface.h"
#include "ECS/Systems/FootpathSystemInterface.h"
#include "ECS/Systems/FootprintSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/GameEndSystemInterface.h"
#include "ECS/Systems/GestureEventsInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/GuidanceSystemInterface.h"
#include "ECS/Systems/HandDemoSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HeartbeatSystemInterface.h"
#include "ECS/Systems/HelpMessageSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/HelpSpeechSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/HighDetailSystemInterface.h"
#include "ECS/Systems/IncomingMessageSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/InspectorSystemInterface.h"
#include "ECS/Systems/IntroSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/LiquidParticleSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/LobbyClientSystemInterface.h"
#include "ECS/Systems/MagicShieldSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "ECS/Systems/MistSystemInterface.h"
#include "ECS/Systems/MusicMoodSystemInterface.h"
#include "ECS/Systems/NetworkGameSystemInterface.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PathfindingSystemInterface.h"
#include "ECS/Systems/PauseTextSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/PlaygroundTripSystemInterface.h"
#include "ECS/Systems/ProfileFolderSystemInterface.h"
#include "ECS/Systems/RainSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/RealWeatherSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/Systems/RewardSystemInterface.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "ECS/Systems/ScriptControlSystemInterface.h"
#include "ECS/Systems/ScriptHighlightSystemInterface.h"
#include "ECS/Systems/ScriptMusicSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/SharkSystemInterface.h"
#include "ECS/Systems/SkyClockSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/SnowSystemInterface.h"
#include "ECS/Systems/SnowfallSystemInterface.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "ECS/Systems/SyncChecksumSystemInterface.h"
#include "ECS/Systems/TattooEditorSystemInterface.h"
#include "ECS/Systems/TeleportSystemInterface.h"
#include "ECS/Systems/TempleDestructionSystemInterface.h"
#include "ECS/Systems/TempleExteriorSystemInterface.h"
#include "ECS/Systems/ThingsMadeSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TipBubbleSystemInterface.h"
#include "ECS/Systems/TornadoSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/Systems/TurnCheerSystemInterface.h"
#include "ECS/Systems/TutorialSkipSystemInterface.h"
#include "ECS/Systems/VegetationInterface.h"
#include "ECS/Systems/VideoSystemInterface.h"
#include "ECS/Systems/VillageLightSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/Systems/VortexSystemInterface.h"
#include "ECS/Systems/WalkPathSystemInterface.h"
#include "ECS/Systems/WaterRingSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/Systems/WorkshopSystemInterface.h"
#include "EditProviders.h"
#include "Editor/EditorSelection.h"
#include "EngineConfig.h"
#include "FightProviders.h"
#include "FileSystem/FileSystemInterface.h"
#include "GameControls.h"
#include "GameSeamInterface.h"
#include "Gui/ScreenFade.h"
#include "Gui/ScriptTexts.h"
#include "Gui/TextDatabase.h"
#include "Hand/HandTrace.h"
#include "Help/AdvisorModel.h"
#include "Help/AdvisorVoices.h"
#include "Help/DialogueText.h"
#include "Help/HelpProfile.h"
#include "Help/Spirits.h"
#include "InfoConstants.h"
#include "Input/GameActionMapInterface.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Nature/Fireflies.h"
#include "Online/HttpClientInterface.h"
#include "Online/NetworkSession.h"
#include "Online/WebSocketInterface.h"
#include "Particles/LightSheet.h"
#include "Profiler.h"
#include "RegistryProviders.h"
#include "Resources/ResourcesInterface.h"
#include "RunControl.h"
#include "Sound/CustomMusicPlayerInterface.h"
#include "Sound/GameMusic.h"
#include "Sound/MusicMood.h"
#include "Sound/SoundRequestInterface.h"
#include "SystemProviders.h"
#include "Temple/TemplePeople.h"
#include "Visual/RendererRequestsInterface.h"
#include "Windowing/WindowingInterface.h"
#include "WorldProviders.h"

namespace openblack
{
/// A heading is written to the inspector as its radians
void to_json(inspector::Json& json, const GameHeading& heading)
{
	json = heading.Radians();
}
} // namespace openblack

using namespace openblack;
using namespace openblack::inspector;
using namespace openblack::ecs::components;

namespace
{

/// Every locator service and the query that inspects it, in the locator's order
constexpr std::array k_Coverage {
    LocatorCoverage {"config", "engine.config"},
    LocatorCoverage {"computerSettingsSystem", "engine.computer_settings"},
    LocatorCoverage {"infoConstants", "engine.services"},
    LocatorCoverage {"profiler", "engine.frame"},
    LocatorCoverage {"mainThreadQueue", "engine.services"},
    LocatorCoverage {"events", "engine.services"},
    LocatorCoverage {"windowing", "engine.frame"},
    LocatorCoverage {"debugGui", "engine.services"},
    LocatorCoverage {"filesystem", "engine.services"},
    LocatorCoverage {"resources", "engine.resources"},
    LocatorCoverage {"rng", "engine.services"},
    LocatorCoverage {"gameRandom", "engine.services"},
    LocatorCoverage {"terrainSystem", "land.terrain"},
    LocatorCoverage {"oceanSystem", "land.ocean"},
    LocatorCoverage {"skyClockSystem", "sky.state"},
    LocatorCoverage {"skySystem", "sky.moon"},
    LocatorCoverage {"audio", "audio.state"},
    // The sound requests keep no state of their own. They go to the audio at once, and audio.state shows the audio.
    LocatorCoverage {"sounds", "audio.state"},
    LocatorCoverage {"gameMusic", "audio.state"},
    LocatorCoverage {"camera", "camera.state"},
    LocatorCoverage {"gameActionSystem", "players.input"},
    // The input device keeps no state of the game. The keys and the buttons it gives show in the player input.
    LocatorCoverage {"inputDevice", "players.input"},
    LocatorCoverage {"rendereringSystem", "engine.frame"},
    LocatorCoverage {"rendererInterface", "engine.frame"},
    LocatorCoverage {"rendererRequests", "engine.frame"},
    // The device makes what the renderer draws, which engine.frame shows
    LocatorCoverage {"graphicsDevice", "engine.frame"},
    LocatorCoverage {"dynamicsSystem", "physics.state"},
    LocatorCoverage {"pickingSystem", "players.pick"},
    LocatorCoverage {"cameraBookmarkSystem", "view.bookmarks"},
    LocatorCoverage {"cameraZoneSystem", "view.zones"},
    LocatorCoverage {"cameraPathSystem", "view.state"},
    LocatorCoverage {"livingActionSystem", "living.action"},
    LocatorCoverage {"townSystem", "town.list"},
    LocatorCoverage {"resourceStoreSystem", "living.resource"},
    LocatorCoverage {"weatherSystem", "land.weather"},
    LocatorCoverage {"pathfindingSystem", "living.pathfinding"},
    LocatorCoverage {"entitiesRegistry", "ecs.components"},
    LocatorCoverage {"drawnRegistry", "drawn.components"},
    LocatorCoverage {"entitiesMap", "map.cell"},
    LocatorCoverage {"playerSystem", "players.list"},
    LocatorCoverage {"alignmentSystem", "players.list"},
    LocatorCoverage {"cameraHelpSystem", "view.state"},
    LocatorCoverage {"turnCheerSystem", "view.state"},
    LocatorCoverage {"templeExteriorSystem", "temple.state"},
    LocatorCoverage {"templeDestructionSystem", "temple.state"},
    LocatorCoverage {"computerPlayerSystem", "players.list"},
    LocatorCoverage {"worshipSiteSystem", "worship.sites"},
    LocatorCoverage {"handSystem", "players.hand"},
    LocatorCoverage {"handGrabSystem", "players.hand"},
    LocatorCoverage {"temple", "temple.interior"},
    LocatorCoverage {"time", "game.state"},
    LocatorCoverage {"simulation", "sim.state"},
    LocatorCoverage {"vegetation", "land.vegetation"},
    LocatorCoverage {"mistSystem", "land.atmosphere"},
    LocatorCoverage {"cloudSystem", "land.atmosphere"},
    LocatorCoverage {"villageLightSystem", "living.villages"},
    LocatorCoverage {"abodeSoundSystem", "living.abode_sounds"},
    LocatorCoverage {"fieldSystem", "living.field"},
    LocatorCoverage {"footpathSystem", "living.footpaths"},
    LocatorCoverage {"animalSystem", "living.animal"},
    LocatorCoverage {"snowSystem", "land.snow"},
    LocatorCoverage {"snowfallSystem", "land.precipitation"},
    LocatorCoverage {"waterRingSystem", "particles.splash"},
    LocatorCoverage {"creatureAnimationSystem", "creatures.status"},
    LocatorCoverage {"creatureMindSystem", "creature.desires"},
    LocatorCoverage {"creatureLocomotionSystem", "creatures.status"},
    LocatorCoverage {"creatureHairSystem", "creatures.systems"},
    LocatorCoverage {"creatureAudioSystem", "creatures.systems"},
    LocatorCoverage {"creatureObjectActionSystem", "creatures.status"},
    LocatorCoverage {"creatureHandSystem", "creatures.systems"},
    LocatorCoverage {"footprintSystem", "creatures.systems"},
    LocatorCoverage {"creatureSkinSystem", "creatures.status"},
    LocatorCoverage {"editorSystem", "view.editor"},
    LocatorCoverage {"creaturePhysiologySystem", "creatures.systems"},
    LocatorCoverage {"leashSystem", "creatures.status"},
    LocatorCoverage {"creatureFightSystem", "creatures.status"},
    LocatorCoverage {"fightTranscript", "fights.transcripts"},
    LocatorCoverage {"creatureAnimationLog", "creature.animation"},
    LocatorCoverage {"creatureModeSystem", "creatures.systems"},
    LocatorCoverage {"creatureCaveSystem", "creatures.systems"},
    LocatorCoverage {"cinematicDirectorSystem", "view.cinematic"},
    LocatorCoverage {"scriptControlSystem", "view.script_control"},
    LocatorCoverage {"dialogueControlSystem", "view.script_control"},
    LocatorCoverage {"helpSpeechSystem", "view.script_control"},
    LocatorCoverage {"highDetailSystem", "view.script_control"},
    LocatorCoverage {"walkPathSystem", "living.walk_paths"},
    LocatorCoverage {"puzzleGameSystem", "puzzle.list"},
    LocatorCoverage {"wonderSystem", "town.wonders"},
    LocatorCoverage {"danceSystem", "living.dances"},
    LocatorCoverage {"sharkSystem", "living.sharks"},
    LocatorCoverage {"introSystem", "living.intro"},
    LocatorCoverage {"boatLaunchSystem", "living.boat_launch"},
    LocatorCoverage {"soundTagSystem", "living.sound_tags"},
    LocatorCoverage {"rainSystem", "land.precipitation"},
    LocatorCoverage {"chimneySmokeSystem", "living.chimneys"},
    LocatorCoverage {"thingsMadeSystem", "living.things_made"},
    LocatorCoverage {"abodeKnockSystem", "view.state"},
    LocatorCoverage {"influenceSystem", "influence.hand"},
    LocatorCoverage {"townDesireSystem", "living.town_desires"},
    LocatorCoverage {"particleRuleSystem", "particles.effects"},
    LocatorCoverage {"particleSystem", "particles.drawn"},
    LocatorCoverage {"effectRequestSystem", "particles.requests"},
    LocatorCoverage {"magicSystem", "magic.state"},
    LocatorCoverage {"gestureEvents", "players.gestures"},
    LocatorCoverage {"reactionSystem", "magic.reactions"},
    LocatorCoverage {"teleportSystem", "magic.teleport"},
    LocatorCoverage {"creatureCarryOverSystem", "creatures.systems"},
    LocatorCoverage {"saveGameSystem", "saves.slots"},
    LocatorCoverage {"creaturePenSystem", "creatures.systems"},
    LocatorCoverage {"creatureFizzSystem", "creatures.systems"},
    LocatorCoverage {"liquidParticleSystem", "particles.liquid"},
    LocatorCoverage {"landShadeSystem", "map.shade"},
    LocatorCoverage {"fixedOutlineSystem", "map.free"},
    LocatorCoverage {"creatureWebPageSystem", "creatures.systems"},
    LocatorCoverage {"httpClient", "engine.online"},
    LocatorCoverage {"webSocketClient", "engine.online"},
    LocatorCoverage {"lobbyClient", "engine.online"},
    LocatorCoverage {"onlineMapStore", "engine.online"},
    LocatorCoverage {"networkGame", "engine.online"},
    LocatorCoverage {"realWeatherSystem", "land.real_weather"},
    LocatorCoverage {"tattooEditorSystem", "creatures.systems"},
    LocatorCoverage {"tattooMenuSystem", "creatures.systems"},
    LocatorCoverage {"filmCreatureSystem", "view.video"},
    LocatorCoverage {"tornadoSystem", "magic.state"},
    LocatorCoverage {"vortexSystem", "magic.vortices"},
    LocatorCoverage {"magicShieldSystem", "magic.state"},
    LocatorCoverage {"forestSystem", "living.forests"},
    LocatorCoverage {"fireflySystem", "living.fireflies"},
    LocatorCoverage {"fishFarmSystem", "living.fish_farms"},
    LocatorCoverage {"workshopSystem", "living.workshops"},
    LocatorCoverage {"animatedStaticSystem", "living.animated_statics"},
    LocatorCoverage {"objectDrawListSystem", "view.draw_list"},
    LocatorCoverage {"appleBarrelSystem", "living.apple_barrel"},
    LocatorCoverage {"gestureSystem", "players.gestures"},
    LocatorCoverage {"miracleFxSystem", "magic.state"},
    LocatorCoverage {"pauseTextSystem", "view.pause_text"},
    LocatorCoverage {"incomingMessageSystem", "view.incoming_messages"},
    LocatorCoverage {"chatSystem", "view.chat"},
    LocatorCoverage {"fireSystem", "magic.fires"},
    LocatorCoverage {"explosionSystem", "magic.state"},
    LocatorCoverage {"floatingNumberSystem", "magic.state"},
    LocatorCoverage {"rewardSystem", "living.rewards"},
    LocatorCoverage {"scriptObjects", "script.objects"},
    LocatorCoverage {"scriptHighlightSystem", "script.highlights"},
    LocatorCoverage {"buildingDamageSystem", "living.building"},
    LocatorCoverage {"inspector", "engine.services"},
    LocatorCoverage {"vm", "script.vm"},
    LocatorCoverage {"chlapi", "script.natives"},
    LocatorCoverage {"villageTotemSystem", "living.totems"},
    LocatorCoverage {"spellIconSystem", "town.spells"},
    LocatorCoverage {"artefactSystem", "living.artefacts"},
    LocatorCoverage {"heartbeatSystem", "players.heartbeat"},
    LocatorCoverage {"gameEndSystem", "players.game_end"},
    LocatorCoverage {"playerCommandSystem", "players.commands"},
    LocatorCoverage {"syncChecksumSystem", "players.checksum"},
    LocatorCoverage {"landSystem", "level.current"},
    LocatorCoverage {"videoSystem", "view.video"},
    LocatorCoverage {"screenFade", "view.video"},
    LocatorCoverage {"playerProfileSystem", "players.new_game"},
    LocatorCoverage {"profileFolderSystem", "players.profile_folder"},
    LocatorCoverage {"musicMoodSystem", "players.custom_music"},
    LocatorCoverage {"scriptMusicSystem", "audio.state"},
    LocatorCoverage {"tutorialSkipSystem", "players.new_game"},
    LocatorCoverage {"playgroundTripSystem", "players.playground_trip"},
    LocatorCoverage {"advisorSystem", "help.advisors"},
    LocatorCoverage {"advisorVoiceSystem", "help.advisors"},
    LocatorCoverage {"helpTextSystem", "help.dialogue"},
    LocatorCoverage {"helpProfileSystem", "help.profile"},
    LocatorCoverage {"creatureHelpSystem", "help.creature"},
    LocatorCoverage {"helpMessageSystem", "help.messages"},
    LocatorCoverage {"guidanceSystem", "help.guidance"},
    LocatorCoverage {"tipBubbleSystem", "help.bubble"},
    LocatorCoverage {"handDemoSystem", "help.hand_demo"},
    LocatorCoverage {"helpSpeechSystem", "help.dialogue"},
    LocatorCoverage {"dialogueControlSystem", "help.dialogue"},
};

constexpr std::string_view k_NoRegistry = "there is no registry: no land is loaded";

Json Point(const glm::vec3& point)
{
	return {point.x, point.y, point.z};
}

Json Point(const glm::vec2& point)
{
	return {point.x, point.y};
}

Json Id(entt::entity entity)
{
	return entity == entt::null ? Json(nullptr) : Json(ToId(entity));
}

Json Id(const std::optional<entt::entity>& entity)
{
	return entity.has_value() ? Id(*entity) : Json(nullptr);
}

template <typename Value>
Json Optional(const std::optional<Value>& value)
{
	if (!value.has_value())
	{
		return nullptr;
	}
	if constexpr (std::is_same_v<Value, glm::vec3> || std::is_same_v<Value, glm::vec2>)
	{
		return Point(*value);
	}
	else
	{
		return *value;
	}
}

const ecs::Registry* Registry()
{
	return Locator::entitiesRegistry::has_value() ? &Locator::entitiesRegistry::value() : nullptr;
}

/// The things that only the frames make, out of the synced world
const ecs::Registry* DrawnRegistry()
{
	return Locator::drawnRegistry::has_value() ? &Locator::drawnRegistry::value() : nullptr;
}

const InfoConstants* Info()
{
	return Locator::infoConstants::has_value() ? &Locator::infoConstants::value() : nullptr;
}

WorldSources World()
{
	return {.registry = &Registry, .info = &Info};
}

/// How many entities have a component
template <typename Component>
size_t CountOf(const ecs::Registry& registry)
{
	const auto* storage = registry.Underlying().storage<Component>();
	return storage != nullptr ? storage->size() : 0;
}

QueryDescription Query(std::string name, std::string description, std::vector<ParameterDescription> parameters = {},
                       ResultKind kind = ResultKind::Object, bool needsNear = false)
{
	return {.name = std::move(name),
	        .description = std::move(description),
	        .parameters = std::move(parameters),
	        .kind = kind,
	        .needsNear = needsNear,
	        .writes = false};
}

ParameterDescription IdParameter(std::string description)
{
	return {.name = "id", .type = "integer", .description = std::move(description), .required = true};
}

ParameterDescription PositionParameter()
{
	return {.name = "position", .type = "point", .description = "[x, z] or [x, y, z]", .required = true};
}

/// The entity a request names, if it exists
std::optional<entt::entity> EntityParam(const Json& params)
{
	const auto* registry = Registry();
	const auto it = params.find("id");
	const auto entity = it == params.end() ? std::nullopt : FromId(*it);
	if (registry == nullptr || !entity.has_value() || !registry->Valid(*entity))
	{
		return std::nullopt;
	}
	return entity;
}

std::optional<glm::vec3> PositionParam(const Json& params)
{
	const auto it = params.find("position");
	if (it == params.end())
	{
		return std::nullopt;
	}
	const auto point = ReadPoint(*it);
	if (!point.has_value())
	{
		return std::nullopt;
	}
	return glm::vec3(static_cast<float>((*point)[0]), static_cast<float>((*point)[1]), static_cast<float>((*point)[2]));
}

PlayerNames PlayerParam(const Json& params)
{
	const auto number = static_cast<int>(NumberMember(params, "player").value_or(0.0));
	return static_cast<PlayerNames>(std::clamp(number, 0, static_cast<int>(PlayerNames::NEUTRAL)));
}

/// The player that a request names, by number (0 to 7) or by name (PLAYER_TWO): none when it names no player
std::optional<PlayerNames> NamedPlayerParam(const Json& params)
{
	const auto it = params.find("player");
	if (it == params.end())
	{
		return std::nullopt;
	}
	if (it->is_string())
	{
		const auto& text = it->get_ref<const std::string&>();
		const auto found = std::ranges::find(k_PlayerNamesStrs, std::string_view(text));
		if (found == k_PlayerNamesStrs.end())
		{
			return std::nullopt;
		}
		return static_cast<PlayerNames>(found - k_PlayerNamesStrs.begin());
	}
	const auto number = NumberMember(params, "player");
	if (!number.has_value() || *number < 0.0 || *number >= static_cast<double>(PlayerNames::_COUNT))
	{
		return std::nullopt;
	}
	return static_cast<PlayerNames>(static_cast<int>(*number));
}

/// The entity of the player, if the land has it
std::optional<entt::entity> PlayerEntityOf(const ecs::Registry& registry, PlayerNames name)
{
	std::optional<entt::entity> found;
	registry.Each<const Player>([&](entt::entity entity, const Player& player) {
		if (player.name == name)
		{
			found = entity;
		}
	});
	return found;
}

/// The names that the nodes of every computer mind have in this game: a personality load can change them
const computer_mind::NodeNames& SharedNodeNames(const ecs::Registry& registry)
{
	const computer_mind::NodeNames* names = &computer_mind::FirstNodeNames();
	registry.Each<const ecs::components::ComputerNodeNames>(
	    [&names](entt::entity /*entity*/, const ecs::components::ComputerNodeNames& shared) { names = &shared.names; });
	return *names;
}

/// A query of one locator service, answered only while the service is there
template <typename Service, typename Read>
FunctionProvider::Function Serve(std::string_view name, Read read)
{
	return [name, read](const QueryContext& context) -> QueryResult {
		if (!Service::has_value())
		{
			return QueryResult::Error(std::string(name) + " isn't there: no land is loaded, or this mode doesn't make it");
		}
		if constexpr (std::is_same_v<std::invoke_result_t<Read, decltype(Service::value()), const QueryContext&>, QueryResult>)
		{
			return read(Service::value(), context);
		}
		else
		{
			return QueryResult::Value(read(Service::value(), context));
		}
	};
}

/// A query of the registry, answered only while there is one
template <typename Read>
FunctionProvider::Function ServeRegistry(Read read)
{
	return [read](const QueryContext& context) -> QueryResult {
		const auto* registry = Registry();
		if (registry == nullptr)
		{
			return QueryResult::Error(std::string(k_NoRegistry));
		}
		if constexpr (std::is_same_v<std::invoke_result_t<Read, const ecs::Registry&, const QueryContext&>, QueryResult>)
		{
			return read(*registry, context);
		}
		else
		{
			return QueryResult::Value(read(*registry, context));
		}
	};
}

Json Listed(const ecs::Registry& registry, entt::entity entity)
{
	return ToListItem(entity, Describe(registry, entity, Info()));
}

std::string_view KindName(ecs::PhysicsEntry::Kind kind)
{
	switch (kind)
	{
	case ecs::PhysicsEntry::Kind::Other:
		return "other";
	case ecs::PhysicsEntry::Kind::Villager:
		return "villager";
	case ecs::PhysicsEntry::Kind::FelledTree:
		return "felled_tree";
	case ecs::PhysicsEntry::Kind::FelledTreeToppled:
		return "felled_tree_toppled";
	}
	return "unknown";
}

// The engine's services

/// The names of the breeder's bonus creatures the debug option unlocks
Json UnlockedBonusCreatureNames(const creature_swap::Unlocked& unlocked)
{
	auto names = Json::array();
	for (size_t i = 0; i < creature_swap::k_BonusCount; ++i)
	{
		if (unlocked.test(i))
		{
			names.push_back(std::string(creature_swap::NameOf(static_cast<creature_swap::Bonus>(i))));
		}
	}
	return names;
}

std::unique_ptr<ProviderInterface> EngineProvider()
{
	auto provider = std::make_unique<FunctionProvider>("engine");
	provider->Add(
	    Query("online", "The web client, the WSS client, the lobby client and the network game: the requests that wait, "
	                    "the open sockets, the session, the push channel, and the turns and checksums of the game server"),
	    [](const QueryContext& /*context*/) {
		    Json result {
		        {"http_pending", Locator::httpClient::has_value() ? Locator::httpClient::value().Pending() : 0},
		        {"websockets", Locator::webSocketClient::has_value() ? Locator::webSocketClient::value().Count() : 0},
		    };
		    const auto* session = Locator::networkGame::has_value() ? Locator::networkGame::value().Session() : nullptr;
		    if (session == nullptr)
		    {
			    result["network_game"] = nullptr;
		    }
		    else
		    {
			    const auto& compares = session->GetCompares();
			    const auto& joined = session->GetJoined();
			    const auto roundTrip = session->RoundTrip();
			    result["network_game"] = Json {
			        {"state", online::StateName(session->GetState())},
			        {"end", online::EndName(session->GetEnd())},
			        {"role", joined.has_value() ? Json(game_socket::RoleName(joined->role)) : Json(nullptr)},
			        {"slot", joined.has_value() && joined->slot.has_value() ? Json(*joined->slot) : Json(nullptr)},
			        {"turns_done", session->TurnsDone()},
			        {"checksums_compared", compares.compared},
			        {"checksums_same", compares.same},
			        {"first_different", compares.firstDifferent.has_value() ? Json(*compares.firstDifferent) : Json(nullptr)},
			        {"resyncing", session->ShowsResync()},
			        {"resync_tries", session->Resync().Tries()},
			        {"resumes", session->Resumes()},
			        {"buffer_ms", static_cast<double>(session->Playout().BufferMicroseconds()) / 1000.0},
			        {"round_trip_ms", roundTrip.has_value() ? Json(static_cast<double>(*roundTrip) / 1000.0) : Json(nullptr)},
			    };
		    }
		    // The store of the lands of network games that the lobby gives (deviation 169)
		    result["online_map_store"] = Locator::onlineMapStore::has_value();
		    if (!Locator::lobbyClient::has_value())
		    {
			    result["lobby"] = nullptr;
			    return QueryResult::Value(std::move(result));
		    }
		    const auto& lobby = Locator::lobbyClient::value();
		    const auto state = lobby.GetState();
		    const auto account = lobby.GetAccount();
		    result["lobby"] = Json {
		        {"configured", lobby.IsConfigured()},
		        {"state", state == ecs::systems::LobbySessionState::LoggedIn    ? "logged_in"
		                  : state == ecs::systems::LobbySessionState::LoggingIn ? "logging_in"
		                                                                        : "logged_out"},
		        {"account", account.has_value() ? Json(account->name) : Json(nullptr)},
		        {"push_open", lobby.IsPushOpen()},
		        {"known_servers", lobby.GetKnownServers().size()},
		    };
		    return QueryResult::Value(std::move(result));
	    });
	provider->Add(
	    Query("config", "The engine's settings: what is drawn, the display and the camera's clip"),
	    Serve<Locator::config>("the config", [](const EngineConfig& config, const QueryContext& /*context*/) {
		    return Json {
		        {"draw",
		         {{"sky", config.drawSky},
		          {"water", config.drawWater},
		          {"island", config.drawIsland},
		          {"entities", config.drawEntities},
		          {"vegetation", config.drawVegetation},
		          {"sprites", config.drawSprites},
		          {"bounding_boxes", config.drawBoundingBoxes}}},
		        {"shadows", {{"creatures", config.drawCreatureShadows}, {"dynamic_trees", config.dynamicTreeShadows}}},
		        {"wireframe", config.wireframe},
		        {"unlocked_bonus_creatures", UnlockedBonusCreatureNames(config.unlockedBonusCreatures)},
		        {"time_of_day", config.timeOfDay},
		        {"resolution", {config.resolution.x, config.resolution.y}},
		        {"vsync", config.vsync},
		        {"camera_fov", config.cameraXFov},
		        {"camera_near_clip", config.cameraNearClip},
		        {"gui_scale", config.guiScale},
		        {"running", config.running},
		    };
	    }));
	provider->Add(
	    Query("computer_settings", "The settings of this computer (settings.json) as the game read them at the start, with the "
	                               "changes kept since; a value the file does not keep is null"),
	    Serve<Locator::computerSettingsSystem>(
	        "the computer settings",
	        [](const ecs::systems::ComputerSettingsSystemInterface& system, const QueryContext& /*context*/) {
		        const auto& settings = system.GetSettings();
		        const auto value = [](const auto& kept) -> Json { return kept.has_value() ? Json(*kept) : Json(nullptr); };
		        return Json {
		            {"detail_level", value(settings.detailLevel)},
		            {"max_detail_level", value(settings.maxDetailLevel)},
		            {"effects_volume", value(settings.effectsVolume)},
		            {"music_volume", value(settings.musicVolume)},
		            {"width", value(settings.width)},
		            {"height", value(settings.height)},
		            {"window_mode", value(settings.windowMode)},
		            {"vsync", value(settings.vsync)},
		        };
	        }));
	provider->Add(Query("frame", "The last frame: its time, the window's size, the renderer's flags and how much it draws"),
	              [](const QueryContext& /*context*/) {
		              Json result = Json::object();
		              if (Locator::profiler::has_value())
		              {
			              const auto& profiler = Locator::profiler::value();
			              const auto& entry = profiler.GetEntries()[profiler.GetEntryIndex(-1)];
			              result["frame_ms"] =
			                  std::chrono::duration<double, std::milli>(entry.frameEnd - entry.frameStart).count();
		              }
		              if (Locator::windowing::has_value())
		              {
			              const auto size = Locator::windowing::value().GetSize();
			              result["window"] = {size.x, size.y};
		              }
		              if (Locator::rendererRequests::has_value())
		              {
			              result["renderer_debug"] = Locator::rendererRequests::value().GetDebug();
			              result["renderer_profile"] = Locator::rendererRequests::value().GetProfile();
		              }
		              if (Locator::rendereringSystem::has_value())
		              {
			              const auto& context = Locator::rendereringSystem::value().GetContext();
			              result["drawn_entities"] = context.entityDraws.size();
			              result["instances"] = context.instanceUniforms.size();
			              result["trees"] = context.treeInstanceCount;
		              }
		              return QueryResult::Value(std::move(result));
	              });
	provider->Add(Query("resources", "How many of each kind of resource the caches hold"),
	              Serve<Locator::resources>("the resources",
	                                        [](resources::ResourcesInterface& resources, const QueryContext& /*context*/) {
		                                        return Json {
		                                            {"meshes", resources.GetMeshes().Size()},
		                                            {"textures", resources.GetTextures().Size()},
		                                            {"animations", resources.GetAnimations().Size()},
		                                            {"sounds", resources.GetSounds().Size()},
		                                            {"levels", resources.GetLevels().Size()},
		                                            {"creature_minds", resources.GetCreatureMinds().Size()},
		                                            {"particle_files", resources.GetParticleFiles().Size()},
		                                            {"camera_paths", resources.GetCameraPaths().Size()},
		                                        };
	                                        }));
	provider->Add(Query("services", "Which of the engine's services are there, the game's random seeds, the game's path, the "
	                                "debug windows' focus and the inspector's port"),
	              [](const QueryContext& /*context*/) {
		              Json result = {
		                  {"info_tables", Locator::infoConstants::has_value()},
		                  {"events", Locator::events::has_value()},
		                  {"rng", Locator::rng::has_value()},
		              };
		              if (Locator::mainThreadQueue::has_value())
		              {
			              // The work that the turn gave to the main thread and that still waits
			              result["main_thread_work_waits"] = Locator::mainThreadQueue::value().GetPendingCount();
		              }
		              if (Locator::gameRandom::has_value())
		              {
			              const auto seeds = Locator::gameRandom::value().GetSeeds();
			              result["seeds"] = {{"synced", seeds.synced}, {"local", seeds.local}};
		              }
		              if (Locator::filesystem::has_value())
		              {
			              result["game_path"] = Locator::filesystem::value().GetGamePath().generic_string();
		              }
		              if (Locator::debugGui::has_value())
		              {
			              result["debug_gui_focus"] = Locator::debugGui::value().StealsFocus();
			              result["mouse_over_debug_window"] = Locator::debugGui::value().IsMouseOverWindow();
		              }
		              if (Locator::inspector::has_value())
		              {
			              result["inspector_port"] = Locator::inspector::value().GetPort();
		              }
		              return QueryResult::Value(std::move(result));
	              });
	return provider;
}

// The land, the sea and the weather

std::unique_ptr<ProviderInterface> LandProvider()
{
	auto provider = std::make_unique<FunctionProvider>("land");
	provider->Add(Query("terrain", "The land: its extent, blocks and countries"),
	              Serve<Locator::terrainSystem>("the land", [](const LandIslandInterface& land, const QueryContext& /*c*/) {
		              const auto extent = land.GetExtent();
		              return Json {{"minimum", Point(extent.minimum)},
		                           {"maximum", Point(extent.maximum)},
		                           {"blocks", land.GetBlocks().size()},
		                           {"countries", land.GetCountries().size()}};
	              }));
	provider->Add(Query("height", "The land's height and normal under a point", {PositionParameter()}),
	              Serve<Locator::terrainSystem>("the land", [](const LandIslandInterface& land, const QueryContext& context) {
		              const auto position = PositionParam(context.params);
		              if (!position.has_value())
		              {
			              return QueryResult::Error("position must be [x, z] or [x, y, z]");
		              }
		              const glm::vec2 point(position->x, position->z);
		              return QueryResult::Value(
		                  {{"height", land.GetHeightAt(point)}, {"normal", Point(land.GetNormalAt(point))}});
	              }));
	provider->Add(Query("ocean", "The sea's textures"),
	              Serve<Locator::oceanSystem>("the sea", [](const OceanInterface& ocean, const QueryContext& /*c*/) {
		              return Json {{"texture", ocean.GetTexture()}};
	              }));
	provider->Add(Query("weather", "The climate: whether it runs, its storms and the season"),
	              Serve<Locator::weatherSystem>(
	                  "the weather", [](const ecs::systems::WeatherSystemInterface& weather, const QueryContext& /*c*/) {
		                  const auto turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
		                  Json storms = Json::array();
		                  for (const auto& storm : weather.GetActiveStorms())
		                  {
			                  if (storms.size() == 8)
			                  {
				                  break;
			                  }
			                  storms.push_back({{"position", Point(storm.currentPosition)},
			                                    {"radius", storm.currentInnerRadius},
			                                    {"strength", storm.currentStrength},
			                                    {"age", storm.age},
			                                    {"dead", storm.dead}});
		                  }
		                  return Json {{"climate", weather.IsClimateSystemEnabled()},
		                               {"storm_creation", weather.IsStormCreationEnabled()},
		                               {"storm_count", weather.GetActiveStorms().size()},
		                               {"storms", std::move(storms)},
		                               {"season", weather.GetSeason(turn)},
		                               {"days_from_start", weather.GetDaysFromStart(turn)}};
	                  }));
	provider->Add(Query("storm_clouds", "The clouds of the storms that the frames make and draw: for each, its storm, its "
	                                    "place in the order that its storm gathered them, where it is drawn, its size and "
	                                    "its alpha"),
	              Serve<Locator::cloudSystem>(
	                  "the clouds", [](const ecs::systems::CloudSystemInterface& clouds, const QueryContext& /*c*/) {
		                  Json list = Json::array();
		                  for (const auto& cloud : clouds.GetStormClouds())
		                  {
			                  list.push_back({{"storm", entt::to_integral(cloud.storm)},
			                                  {"index", cloud.index},
			                                  {"position", Point(cloud.position)},
			                                  {"size", cloud.mist.size},
			                                  {"alpha", cloud.mist.colour >> 24u}});
		                  }
		                  return Json {{"count", clouds.GetStormClouds().size()}, {"clouds", std::move(list)}};
	                  }));
	provider->Add(Query("real_weather", "The real-world weather: the weather that the lobby gave, whether it changes the land, "
	                                    "its storm, the place of its message and the background weather"),
	              Serve<Locator::realWeatherSystem>(
	                  "the real weather", [](const ecs::systems::RealWeatherSystemInterface& real, const QueryContext& /*c*/) {
		                  Json result {{"applied", real.IsApplied()}, {"storm", real.GetStorm() != entt::null}};
		                  if (const auto weather = real.GetWeather(); weather.has_value())
		                  {
			                  result["weather"] = Json {{"type", weather->type},
			                                            {"temperature", weather->temperature},
			                                            {"visibility", weather->visibility},
			                                            {"city", weather->city},
			                                            {"area", weather->area},
			                                            {"country", weather->country}};
			                  result["place"] = real.GetPlace().value_or("");
		                  }
		                  else
		                  {
			                  result["weather"] = nullptr;
		                  }
		                  if (Locator::weatherSystem::has_value())
		                  {
			                  const auto background = Locator::weatherSystem::value().GetBackgroundWeather();
			                  result["background"] = Json {{"temperature", background.temperature},
			                                               {"rain", background.rain},
			                                               {"snow", background.snow},
			                                               {"overcast", background.overcast},
			                                               {"wind", {background.windX, background.windZ}},
			                                               {"snow_cover", background.snowCover}};
		                  }
		                  return result;
	                  }));
	provider->Add(Query("weather_at", "The weather at a point: temperature, rain, snow, overcast, wind", {PositionParameter()}),
	              Serve<Locator::weatherSystem>("the weather",
	                                            [](ecs::systems::WeatherSystemInterface& weather, const QueryContext& context) {
		                                            const auto position = PositionParam(context.params);
		                                            if (!position.has_value())
		                                            {
			                                            return QueryResult::Error("position must be [x, z] or [x, y, z]");
		                                            }
		                                            const auto info = weather.GetWeather(*position);
		                                            return QueryResult::Value({{"temperature", info.temperature},
		                                                                       {"rain", info.rain},
		                                                                       {"snow", info.snow},
		                                                                       {"overcast", info.overcast},
		                                                                       {"wind", {info.windX, info.windZ}},
		                                                                       {"snow_cover", info.snowCover}});
	                                            }));
	provider->Add(
	    Query("snow", "The snow lying on the land: its cells, deepest and revision"),
	    Serve<Locator::snowSystem>("the snow", [](const ecs::systems::SnowSystemInterface& snow, const QueryContext& /*c*/) {
		    const auto depths = snow.GetDepths();
		    const auto deepest = depths.empty() ? 0.0f : *std::ranges::max_element(depths);
		    return Json {{"cells", depths.size()}, {"deepest", deepest}, {"revision", snow.GetRevision()}};
	    }));
	provider->Add(Query("precipitation", "The rain's streaks, fall and splashes, and the falling snow's flakes"),
	              [](const QueryContext& /*c*/) {
		              Json result = Json::object();
		              if (Locator::rainSystem::has_value())
		              {
			              const auto& rain = Locator::rainSystem::value();
			              result["rain"] = {{"streaks", rain.GetStreaks().size()},
			                                {"height", rain.GetHeight()},
			                                {"speed", rain.GetFall().speed},
			                                {"splashes", rain.GetSplashCount()}};
		              }
		              if (Locator::snowfallSystem::has_value())
		              {
			              result["snowflakes"] = Locator::snowfallSystem::value().GetFlakes().size();
		              }
		              return QueryResult::Value(std::move(result));
	              });
	provider->Add(Query("vegetation", "How many trees and fields sway with the wind"),
	              ServeRegistry([](const ecs::Registry& registry, const QueryContext& /*c*/) {
		              return Json {{"swaying", Locator::vegetation::has_value()},
		                           {"trees", CountOf<Tree>(registry)},
		                           {"fields", CountOf<Field>(registry)}};
	              }));
	provider->Add(Query("atmosphere", "The mists and clouds over the land"),
	              ServeRegistry([](const ecs::Registry& registry, const QueryContext& /*c*/) {
		              // The sky's clouds are things of the drawn registry
		              const auto* drawn = DrawnRegistry();
		              return Json {{"mists", CountOf<Mist>(registry)},
		                           {"mist_system", Locator::mistSystem::has_value()},
		                           {"clouds", drawn != nullptr ? CountOf<Cloud>(*drawn) : size_t {0}},
		                           {"cloud_system", Locator::cloudSystem::has_value()}};
	              }));
	return provider;
}

std::unique_ptr<ProviderInterface> SkyStateProvider(std::unique_ptr<ProviderInterface> sky)
{
	// The moon's provider, with the sky's own state beside it
	auto provider = std::make_unique<FunctionProvider>("sky");
	auto* moon = sky.release();
	std::shared_ptr<ProviderInterface> shared(moon);
	for (const auto& description : shared->Describe())
	{
		provider->Add(description,
		              [shared, name = description.name](const QueryContext& context) { return shared->Run(name, context); });
	}
	provider->Add(Query("state", "The sky: its type, its time, the script clock's hour and the day and night's times"),
	              Serve<Locator::skyClockSystem>(
	                  "the sky", [](const ecs::systems::SkyClockSystemInterface& sky, const QueryContext& /*c*/) {
		                  const auto times = sky.GetDayNightTimes();
		                  return Json {{"sky_type", sky.GetCurrentSkyType()},
		                               {"time", sky.GetTime()},
		                               {"script_hour", sky.GetClock().GetScriptTime()},
		                               {"visual_hour", sky.GetClock().GetVisualTime()},
		                               {"night_full", times.nightFull},
		                               {"dusk_start", times.duskStart},
		                               {"dusk_end", times.duskEnd},
		                               {"day_full", times.dayFull}};
	                  }));
	return provider;
}

// The things living on the land

std::unique_ptr<ProviderInterface> LivingProvider()
{
	auto provider = std::make_unique<FunctionProvider>("living");
	provider->Add(
	    Query("animated_statics", "The scenery the scripts open and close: its open word and its gate stones' value", {},
	          ResultKind::List),
	    ServeRegistry([](const ecs::Registry& registry, const QueryContext& /*c*/) {
		    Json items = Json::array();
		    registry.Each<const AnimatedStatic>([&items, &registry](entt::entity entity, const AnimatedStatic& scenery) {
			    auto item = Listed(registry, entity);
			    item["open_state"] = scenery.openState;
			    if (Locator::animatedStaticSystem::has_value())
			    {
				    item["gate_stones_value"] = Optional(Locator::animatedStaticSystem::value().GateStoneValue(entity));
			    }
			    items.push_back(std::move(item));
		    });
		    return items;
	    }));
	provider->Add(
	    Query("apple_barrel",
	          "The apple barrel beside the storage pit of the player at this computer nearest their temple: the pit (an "
	          "entity of the land), the barrel and its worm (entities of the drawn registry), where they stand and the "
	          "worm's place in its clip; none while no pit carries it",
	          {}),
	    ServeRegistry([](const ecs::Registry& /*registry*/, const QueryContext& /*c*/) {
		    const auto* drawn = DrawnRegistry();
		    const auto barrel =
		        Locator::appleBarrelSystem::has_value() ? Locator::appleBarrelSystem::value().GetBarrel() : entt::null;
		    if (drawn == nullptr || barrel == entt::null || !drawn->Valid(barrel) ||
		        !drawn->AllOf<AppleBarrel, Transform>(barrel))
		    {
			    return Json(nullptr);
		    }
		    const auto& found = drawn->Get<const AppleBarrel>(barrel);
		    Json result = {{"barrel", Id(barrel)},
		                   {"store", Id(found.store)},
		                   {"worm", Id(found.worm)},
		                   {"position", Point(drawn->Get<const Transform>(barrel).position)}};
		    if (drawn->Valid(found.worm) && drawn->AllOf<AppleBarrelWorm, Transform>(found.worm))
		    {
			    result["worm_position"] = Point(drawn->Get<const Transform>(found.worm).position);
			    result["worm_place"] = drawn->Get<const AppleBarrelWorm>(found.worm).place;
		    }
		    return result;
	    }));
	provider->Add(
	    Query("totems",
	          "The village centres' totems: their town centre and icon, the share they stand at, ease to and are "
	          "held at, how fast they move, whether the hand grips one, and their see-through second totem and "
	          "whether it shows",
	          {}, ResultKind::List),
	    ServeRegistry([](const ecs::Registry& registry, const QueryContext& /*c*/) {
		    Json items = Json::array();
		    const auto gripped =
		        Locator::villageTotemSystem::has_value() ? Locator::villageTotemSystem::value().GetGripped() : std::nullopt;
		    registry.Each<const VillageTotem>([&items, &registry, gripped](entt::entity entity, const VillageTotem& totem) {
			    auto item = Listed(registry, entity);
			    item["town_centre"] = Id(totem.townCentre);
			    item["icon"] = Id(totem.icon);
			    item["rest_y"] = totem.restY;
			    item["share"] = totem.ease.share;
			    item["speed"] = totem.ease.speed;
			    item["target"] = totem.ease.target;
			    item["moving"] = totem.ease.moving;
			    item["held"] = totem.held;
			    item["gripped"] = gripped.has_value() && *gripped == entity;
			    item["ghost"] = Id(totem.ghost);
			    item["ghost_shown"] = registry.Valid(totem.ghost) && !registry.AllOf<VillageTotemGhostHidden>(totem.ghost);
			    items.push_back(std::move(item));
		    });
		    return items;
	    }));
	provider->Add(
	    Query("artefacts",
	          "The artefacts: the town and worship site each last joined and whether it is among their artefacts now, its "
	          "owner and worth, its dance (started or not, its state, dancers and turns since it started), whether it "
	          "glows and shows its owner's symbol, and how many each player owns",
	          {}, ResultKind::Object),
	    ServeRegistry([](const ecs::Registry& registry, const QueryContext& /*c*/) {
		    Json items = Json::array();
		    registry.Each<const TownArtefact>([&items, &registry](entt::entity entity, const TownArtefact& artefact) {
			    auto item = Listed(registry, entity);
			    item["town"] = Id(artefact.town);
			    item["site"] = Id(artefact.site);
			    item["in_town"] = artefact.inTown;
			    item["in_site"] = artefact.inSite;
			    item["player"] = artefact.player.has_value() ? Json(static_cast<int>(*artefact.player)) : Json(nullptr);
			    item["value"] = artefact.value;
			    item["dance"] = Id(artefact.dance);
			    item["dancing"] = artefact.dancing;
			    item["dance_started"] = artefact.danceStarted;
			    if (const auto* dance = registry.Valid(artefact.dance) ? registry.TryGet<const Dance>(artefact.dance) : nullptr)
			    {
				    item["dance_type"] = static_cast<int>(dance->type);
				    item["dance_state"] = dance->state == Dance::State::Dancing ? "dancing" : "stopped";
				    item["dancers"] = dance->dancers;
				    item["dance_duration"] = dance->duration;
				    item["dance_start_turn"] = dance->startTurn;
			    }
			    item["effect"] = artefact.effect;
			    item["glows"] = ecs::artefact_rules::Glows(artefact.value);
			    item["symbol"] = artefact.symbol && ecs::artefact_rules::ShowsSymbol(artefact.value);
			    items.push_back(std::move(item));
		    });
		    Json counts = Json::object();
		    if (Locator::artefactSystem::has_value())
		    {
			    for (int p = 0; p < static_cast<int>(PlayerNames::_COUNT); ++p)
			    {
				    counts[std::to_string(p)] = Locator::artefactSystem::value().CountFor(static_cast<PlayerNames>(p));
			    }
		    }
		    return QueryResult::Value(Json {{"artefacts", std::move(items)}, {"count_by_player", std::move(counts)}});
	    }));
	provider->Add({.name = "artefact_worth",
	               .description = "Sets an artefact's worth as its dances would raise it: it starts glowing once worth more "
	                              "than 1, and shows its owner's symbol once worth more than 2",
	               .parameters = {IdParameter("The artefact's entity id"),
	                              {.name = "worth", .type = "number", .description = "Its new worth", .required = true},
	                              {.name = "town",
	                               .type = "integer",
	                               .description = "A town to make it an artefact of first, as if put there by the player",
	                               .required = false},
	                              {.name = "player",
	                               .type = "integer",
	                               .description = "The player who puts it in the town (0 by default)",
	                               .required = false}},
	               .kind = ResultKind::Object,
	               .needsNear = false,
	               .writes = true},
	              [](const QueryContext& context) {
		              const auto entity = EntityParam(context.params);
		              const auto worth = context.params.find("worth");
		              if (!entity.has_value() || worth == context.params.end() || !worth->is_number() ||
		                  !Locator::artefactSystem::has_value())
		              {
			              return QueryResult::Error("give an artefact's id and a worth");
		              }
		              // Made an artefact of a town first, when one is named, as if its player had put it there
		              if (const auto town = context.params.find("town"); town != context.params.end())
		              {
			              const auto townEntity = FromId(*town);
			              const auto player = context.params.value("player", 0);
			              if (!townEntity.has_value() || player < 0 || player >= static_cast<int>(PlayerNames::_COUNT))
			              {
				              return QueryResult::Error("give a town's id and a player number");
			              }
			              Locator::artefactSystem::value().AddToTown(*entity, *townEntity, static_cast<PlayerNames>(player));
		              }
		              if (!Locator::artefactSystem::value().SetWorth(*entity, worth->get<float>()))
		              {
			              return QueryResult::Error("it isn't an artefact");
		              }
		              return QueryResult::Value(Json {{"id", Id(*entity)}, {"worth", worth->get<float>()}});
	              });
	provider->Add(Query("action", "A villager's or animal's action states: top, final, previous, and turns in them",
	                    {IdParameter("The living thing's entity id")}),
	              ServeRegistry([](const ecs::Registry& registry, const QueryContext& context) {
		              const auto entity = EntityParam(context.params);
		              const auto* action = entity.has_value() ? registry.TryGet<const LivingAction>(*entity) : nullptr;
		              if (action == nullptr)
		              {
			              return QueryResult::Error("it has no living action");
		              }
		              Json result = {{"id", ToId(*entity)},
		                             {"top", action->states[0]},
		                             {"final", action->states[1]},
		                             {"previous", action->states[2]},
		                             {"turns_until_change", action->turnsUntilStateChange},
		                             {"turns_since_change", action->turnsSinceStateChange}};
		              if (Locator::livingActionSystem::has_value())
		              {
			              result["final_state"] =
			                  static_cast<int>(Locator::livingActionSystem::value().VillagerGetFinalState(*action));
		              }
		              return QueryResult::Value(std::move(result));
	              }));
	provider->Add(
	    Query("animal",
	          "An animal: its kind, state (and the game's number of the state it is in for itself), count of turns, "
	          "whether a kill made it start to die, clip, flock and town, its kind's radius, where it moves to, whether "
	          "a creature fears it, the hunger of a kind that is not a grazer, a grazer's needs, and a spell wolf's prey "
	          "and target",
	          {IdParameter("The animal's entity id")}),
	    Serve<Locator::animalSystem>(
	        "the animals", [](const ecs::systems::AnimalSystemInterface& animals, const QueryContext& context) {
		        const auto entity = EntityParam(context.params);
		        const auto* registry = Registry();
		        if (!entity.has_value() || registry == nullptr || !registry->AllOf<Animal>(*entity))
		        {
			        return QueryResult::Error("no animal with that id");
		        }
		        const auto& animal = registry->Get<const Animal>(*entity);
		        Json result = {{"id", ToId(*entity)},
		                       {"kind", static_cast<int>(animal.type)},
		                       {"state", static_cast<int>(animal.state)},
		                       {"final_game_state", animals.FinalGameStateOf(*entity).value_or(0)},
		                       {"turns_in_state", animal.turnsInState},
		                       {"turns_until_state_change", animal.turnsUntilStateChange},
		                       {"dying", animal.dying},
		                       {"hunger", animal.hunger},
		                       {"birth_turn", animal.birthTurn},
		                       {"clip", static_cast<int>(animal.animation)},
		                       {"speed", animal.move.speed},
		                       {"flock", Id(animal.flock)},
		                       {"town", Id(animal.town)},
		                       {"radius", animals.RadiusOf(*entity)},
		                       {"movement", Point(animals.MovementOf(*entity))},
		                       {"goal", Point(animals.GoalOf(*entity))},
		                       {"frightening_to_creature", animals.IsFrighteningToCreature(*entity)},
		                       {"player_can_pick_up", animals.CanPlayerPickUp(*entity)}};
		        // A grazer's needs; the grazing clips left of its meal are its count of turns
		        if (const auto* grazer = registry->TryGet<const Grazer>(*entity))
		        {
			        result["grazer"] = {
			            {"hunger", grazer->needs.hunger}, {"sleep", grazer->needs.sleep}, {"breed", grazer->needs.breed}};
		        }
		        // A spell wolf's prey that it hunts, and its target: the prey that it brought down and eats
		        if (const auto* run = registry->TryGet<const SpellWolf>(*entity))
		        {
			        result["spell_wolf"] = {{"prey", Id(run->hunt.prey)}, {"food", Id(run->hunt.food)}};
		        }
		        // A wild hunter's needs, the prey that it hunts, its target and the cell where its flock sleeps
		        if (const auto* hunter = registry->TryGet<const Hunter>(*entity))
		        {
			        result["hunter"] = {{"hunger", hunter->needs.hunger},
			                            {"sleep", hunter->needs.sleep},
			                            {"breed", hunter->needs.breed},
			                            {"prey", Id(hunter->hunt.prey)},
			                            {"food", Id(hunter->hunt.food)},
			                            {"sleeping_cell", hunter->sleepingCell.has_value()
			                                                  ? Json::array({hunter->sleepingCell->x, hunter->sleepingCell->y})
			                                                  : Json()}};
		        }
		        return QueryResult::Value(std::move(result));
	        }));
	provider->Add(
	    Query("villages", "Whether it is dark enough for the village lights, and how many there are"),
	    Serve<Locator::villageLightSystem>(
	        "the village lights", [](const ecs::systems::VillageLightSystemInterface& lights, const QueryContext& /*c*/) {
		        const auto* registry = Registry();
		        return Json {{"dark", lights.IsDark()}, {"lights", registry != nullptr ? CountOf<VillageLight>(*registry) : 0}};
	        }));
	provider->Add(
	    Query("abode_sounds", "How many sounds the buildings of the towns made by themselves"),
	    Serve<Locator::abodeSoundSystem>("the sounds of the buildings",
	                                     [](const ecs::systems::AbodeSoundSystemInterface& sounds, const QueryContext& /*c*/) {
		                                     return Json {{"sounds", sounds.GetSoundCount()}};
	                                     }));
	provider->Add(
	    Query("footpaths",
	          "The footpaths: how many there are and how many are being found; with an id, the paths that lead from that "
	          "thing and the villagers walking them; with from and to (metres), the nodes of a path the game would lay at once "
	          "between them",
	          {{.name = "id", .type = "integer", .description = "A thing's entity id", .required = false},
	           {.name = "from", .type = "point", .description = "[x, z] in metres", .required = false},
	           {.name = "to", .type = "point", .description = "[x, z] in metres", .required = false}}),
	    Serve<Locator::footpathSystem>("the footpaths", [](const ecs::systems::FootpathSystemInterface& footpaths,
	                                                       const QueryContext& context) {
		    const auto* registry = Registry();
		    Json result {{"footpaths", CountOf<Footpath>(*registry)}, {"finders", CountOf<FootpathFinder>(*registry)}};
		    const auto from = context.params.find("from");
		    const auto to = context.params.find("to");
		    if (from != context.params.end() && to != context.params.end())
		    {
			    const auto a = ReadPoint(*from);
			    const auto b = ReadPoint(*to);
			    if (!a.has_value() || !b.has_value())
			    {
				    return QueryResult::Error("from and to must be points");
			    }
			    const auto whole = [](const auto& point) {
				    return glm::ivec2(map_coords::ToFixed(static_cast<float>(point[0])),
				                      map_coords::ToFixed(static_cast<float>(point[2])));
			    };
			    const auto planned = footpaths.PlanBetween(whole(*a), whole(*b));
			    Json nodes = Json::array();
			    for (const auto& place : planned.value_or(std::vector<glm::ivec2> {}))
			    {
				    nodes.push_back({map_coords::ToMetres(place.x), map_coords::ToMetres(place.y)});
			    }
			    result["planned"] = planned.has_value() ? nodes : Json(nullptr);
		    }
		    if (const auto entity = EntityParam(context.params))
		    {
			    Json paths = Json::array();
			    if (const auto* link = registry->TryGet<const FootpathLink>(*entity))
			    {
				    for (const auto id : link->footpaths)
				    {
					    const auto path = static_cast<entt::entity>(id);
					    const auto* footpath = registry->Valid(path) ? registry->TryGet<const Footpath>(path) : nullptr;
					    if (footpath == nullptr)
					    {
						    continue;
					    }
					    Json nodes = Json::array();
					    for (const auto& node : footpath->nodes)
					    {
						    nodes.push_back({{"place", {node.place.x, node.place.y}},
						                     {"planned", node.planned},
						                     {"hidden", node.hidden},
						                     {"marker", node.marker}});
					    }
					    // The villagers that walk the path, each with the node it walks to
					    Json walkers = Json::array();
					    registry->Each<const FootpathWalk>([&walkers, path](entt::entity villager, const FootpathWalk& walk) {
						    if (walk.footpath == path)
						    {
							    walkers.push_back({{"id", ToId(villager)},
							                       {"node", walk.node.has_value() ? Json(*walk.node) : Json(nullptr)},
							                       {"towardsStart", walk.towardsStart}});
						    }
					    });
					    paths.push_back({{"id", ToId(path)}, {"nodes", std::move(nodes)}, {"walkers", std::move(walkers)}});
				    }
			    }
			    result["paths"] = std::move(paths);
		    }
		    return QueryResult::Value(std::move(result));
	    }));
	provider->Add(
	    Query("field", "A field's crop as it is drawn: its tint, height and sway", {IdParameter("The field's entity id")}),
	    Serve<Locator::fieldSystem>(
	        "the fields", [](const ecs::systems::FieldSystemInterface& fields, const QueryContext& context) {
		        const auto entity = EntityParam(context.params);
		        const auto* field = entity.has_value() ? Registry()->TryGet<const Field>(*entity) : nullptr;
		        if (field == nullptr)
		        {
			        return QueryResult::Error("no field with that id");
		        }
		        const auto look = fields.GetLook(*field);
		        if (!look.has_value())
		        {
			        return QueryResult::Value({{"id", ToId(*entity)}, {"crop", nullptr}});
		        }
		        return QueryResult::Value(
		            {{"id", ToId(*entity)}, {"tint", look->tint}, {"height", look->height}, {"sways", look->sways}});
	        }));
	provider->Add(Query("forests", "The land's forests: their wood and trees", {}, ResultKind::List),
	              Serve<Locator::forestSystem>(
	                  "the forests", [](const ecs::systems::ForestSystemInterface& forests, const QueryContext& /*c*/) {
		                  const auto* registry = Registry();
		                  Json items = Json::array();
		                  for (const auto forest : forests.LandForests())
		                  {
			                  auto item = registry != nullptr ? Listed(*registry, forest) : Json {{"id", ToId(forest)}};
			                  item["wood"] = forests.WoodOf(forest);
			                  item["trees"] = forests.TreesOf(forest, false).size();
			                  item["growing"] = forests.TreesOf(forest, true).size();
			                  items.push_back(std::move(item));
		                  }
		                  return items;
	                  }));
	provider->Add(Query("sharks", "The sharks: where each is this turn and was at its start, the way it faces, its clip "
	                              "and the wake's timer"),
	              Serve<Locator::sharkSystem>(
	                  "the sharks", [](const ecs::systems::SharkSystemInterface& sharks, const QueryContext& /*c*/) {
		                  Json items = Json::array();
		                  if (const auto* registry = Registry(); registry != nullptr)
		                  {
			                  registry->Each<const Shark>([&items, registry](entt::entity entity, const Shark& shark) {
				                  auto item = Listed(*registry, entity);
				                  item["at"] = Point(shark.position);
				                  item["turn_start"] = Point(shark.turnStart);
				                  item["heading"] = shark.heading;
				                  item["clip_place"] = shark.clipPlace;
				                  items.push_back(std::move(item));
			                  });
		                  }
		                  return Json {{"wake_timer", sharks.GetWakeTimer()}, {"sharks", std::move(items)}};
	                  }));
	provider->Add(Query("intro", "The opening's light falling onto the boy and the god's hand lifting him and setting him "
	                             "down: the stage, the light's state and head, the chasing camera, the hand's clip time and "
	                             "where it holds the boy"),
	              Serve<Locator::introSystem>(
	                  "the opening's light and hand", [](const ecs::systems::IntroSystemInterface& intro, const QueryContext&) {
		                  const auto state = intro.GetState();
		                  Json json {
		                      {"stage", state.stage},
		                      {"light", state.light.has_value() ? Json(static_cast<int32_t>(*state.light)) : Json(nullptr)},
		                      {"light_head", state.lightHead.has_value() ? Point(*state.lightHead) : Json(nullptr)},
		                      {"light_elapsed", state.lightElapsed},
		                      {"camera_chasing", state.cameraChasing},
		                      {"hand", state.hand == entt::null ? Json(nullptr) : Json(ToId(state.hand))},
		                      {"hand_clip_time", state.handClipTime},
		                      {"hand_playing", state.handPlaying},
		                      {"holding_boy", state.holdingBoy},
		                      {"grip", state.grip.has_value() ? Point(*state.grip) : Json(nullptr)},
		                      {"lift_finished", state.liftFinished}};
		                  if (const auto view = intro.GetCameraView(); view.has_value())
		                  {
			                  json["camera"] = {{"origin", Point(view->origin)}, {"focus", Point(view->focus)}};
		                  }
		                  return json;
	                  }));
	provider->Add(
	    Query("boat_launch", "The explorers' boat film: the stage, its clocks, where it plays, where the boat is drawn and "
	                         "the things drawn in it with their clips' places"),
	    Serve<Locator::boatLaunchSystem>(
	        "the explorers' boat", [](const ecs::systems::BoatLaunchSystemInterface& film, const QueryContext&) {
		        const auto state = film.GetState();
		        Json wakes = Json::array();
		        for (const auto phase : state.clocks.wakes)
		        {
			        wakes.push_back(phase);
		        }
		        Json parts = Json::array();
		        if (const auto* registry = DrawnRegistry(); registry != nullptr)
		        {
			        // Only the things of the film that the frames show now, in the drawn registry
			        registry->Each<const BoatFilmPart, const Transform>(
			            [&parts, registry](entt::entity entity, const BoatFilmPart& part, const Transform& transform) {
				            if (registry->AllOf<HiddenByState>(entity))
				            {
					            return;
				            }
				            auto item = Listed(*registry, entity);
				            item["role"] = static_cast<int32_t>(part.role);
				            item["index"] = part.index;
				            item["at"] = Point(transform.position);
				            item["clip_time"] = part.clipTime;
				            parts.push_back(std::move(item));
			            });
		        }
		        Json json {{"playing", state.playing},
		                   {"stage", state.clocks.stage == ecs::boat_launch::Stage::Launch ? "launch" : "sailing"},
		                   {"elapsed", state.clocks.elapsed},
		                   {"boat_time", state.clocks.boatTime},
		                   {"pushing", state.clocks.pushing},
		                   {"wakes", std::move(wakes)},
		                   {"dock", Point(state.places.dock)},
		                   {"sailing_start", Point(state.places.sailingStart)},
		                   {"parts", std::move(parts)}};
		        if (state.boat.has_value())
		        {
			        json["boat"] = Point(glm::vec3((*state.boat)[3]));
		        }
		        return json;
	        }));
	provider->Add(Query("walk_paths", "The things walking the camera editor's tracks: the track, how far and up to where", {},
	                    ResultKind::List),
	              Serve<Locator::walkPathSystem>(
	                  "the walk paths", [](const ecs::systems::WalkPathSystemInterface& /*paths*/, const QueryContext&) {
		                  Json items = Json::array();
		                  if (const auto* registry = Registry(); registry != nullptr)
		                  {
			                  registry->Each<const WalkPath>([&items, registry](entt::entity entity, const WalkPath& path) {
				                  auto item = Listed(*registry, entity);
				                  item["track"] = path.number;
				                  item["forward"] = path.walk.forward;
				                  item["current"] = path.walk.current;
				                  item["to"] = path.walk.to;
				                  item["percentage"] = camera_track::Percentage(path.walk, *path.track);
				                  item["segment"] = path.walk.runner.GetSegment();
				                  items.push_back(std::move(item));
			                  });
		                  }
		                  return items;
	                  }));
	provider->Add(
	    Query("dances",
	          "The dances: what they are danced for, whether dancing, their speed and clock, and each group's shape, move "
	          "and dancers",
	          {}, ResultKind::List),
	    Serve<Locator::danceSystem>("the dances", [](const ecs::systems::DanceSystemInterface& /*dances*/,
	                                                 const QueryContext&) {
		    Json items = Json::array();
		    if (const auto* registry = Registry(); registry != nullptr)
		    {
			    registry->Each<const Dance>([&items, registry](entt::entity entity, const Dance& dance) {
				    auto item = Listed(*registry, entity);
				    item["type"] = static_cast<int>(dance.type);
				    item["owner"] = ToId(dance.owner);
				    item["dancing"] = dance.state == Dance::State::Dancing;
				    item["speed"] = dance.speed;
				    item["rate"] = dance.rate;
				    item["clock"] = dance.clock;
				    item["start_turn"] = dance.startTurn;
				    item["duration"] = dance.duration;
				    item["dancers"] = dance.dancers;
				    item["round"] = dance.groups.round;
				    Json groups = Json::array();
				    for (const auto& group : dance.groups.all)
				    {
					    Json dancers = Json::array();
					    for (const auto dancer : group.dancers)
					    {
						    dancers.push_back(ToId(dancer));
					    }
					    groups.push_back({{"name", group.name},
					                      {"limited", group.limited},
					                      {"quota", group.quota},
					                      {"weight", group.weight},
					                      {"sexes", group.sexes},
					                      {"shape", group.shape},
					                      {"radius", group.radius},
					                      {"offset", {group.offset.x, group.offset.y}},
					                      {"rotation", group.rotation},
					                      {"spin", group.spin},
					                      {"move", {group.move.action, group.move.first, group.move.second, group.move.end}},
					                      {"moved", group.moved},
					                      {"dancers", std::move(dancers)}});
				    }
				    item["groups"] = std::move(groups);
				    items.push_back(std::move(item));
			    });
		    }
		    return items;
	    }));
	provider->Add(Query("flocks",
	                    "The flocks, of animals and of the scripts' villagers: their place, their domain and flock distances, "
	                    "and their members from the first to the leader",
	                    {}, ResultKind::List),
	              ServeRegistry([](const ecs::Registry& registry, const QueryContext& /*c*/) {
		              Json items = Json::array();
		              registry.Each<const Flock>([&items](entt::entity entity, const Flock& flock) {
			              Json members = Json::array();
			              for (const auto member : flock.members)
			              {
				              members.push_back(ToId(member));
			              }
			              const auto place = map_coords::ToMetres(flock.place);
			              items.push_back({{"id", ToId(entity)},
			                               {"place", {place.x, place.y}},
			                               {"domain_radius", flock.domainRadius},
			                               {"flock_distance", flock.flockDistance},
			                               {"calm", flock.calm},
			                               {"members", std::move(members)}});
		              });
		              return items;
	              }));
	provider->Add(
	    Query("fireflies",
	          "How many fireflies there are and how many hide, fly out, hover and fly home; whether the next nightfall "
	          "tops them up; the trees and the things standing over several cells they can hide in; and what the last "
	          "frame did with those out: in view, near enough to be seen, drawn, and whether their picture was found; and the "
	          "land's reward weights by magic type number, with their total"),
	    Serve<Locator::fireflySystem>(
	        "the fireflies", [](const ecs::systems::FireflySystemInterface& fireflies, const QueryContext& /*c*/) {
		        std::array<size_t, 4> states {};
		        size_t hidden = 0;
		        if (const auto* registry = Registry(); registry != nullptr)
		        {
			        for (const auto entity : fireflies.GetFireflies())
			        {
				        const auto& firefly = registry->Get<const Firefly>(entity);
				        ++states.at(static_cast<size_t>(firefly.state));
				        hidden += firefly.hidden ? 1 : 0;
			        }
		        }
		        const auto frame = fireflies.GetLastFrame();
		        // The land's reward weights that are not nothing, by the number of the magic type
		        Json weights = Json::object();
		        const auto& rewards = fireflies.GetRewards();
		        for (size_t kind = 0; kind < fireflies::k_RewardKinds; ++kind)
		        {
			        if (rewards.Weight(kind) != 0.0f)
			        {
				        weights[std::to_string(kind)] = rewards.Weight(kind);
			        }
		        }
		        return Json {{"fireflies", fireflies.GetFireflies().size()},
		                     {"reward_weights", std::move(weights)},
		                     {"reward_total", rewards.Total()},
		                     {"resting", states.at(static_cast<size_t>(Firefly::State::Resting))},
		                     {"flying_out", states.at(static_cast<size_t>(Firefly::State::FlyingOut))},
		                     {"hovering", states.at(static_cast<size_t>(Firefly::State::Hovering))},
		                     {"flying_home", states.at(static_cast<size_t>(Firefly::State::FlyingHome))},
		                     {"hidden", hidden},
		                     {"top_up_due", fireflies.IsTopUpDue()},
		                     {"trees", fireflies.GetTreeCount()},
		                     {"fixed", fireflies.GetFixedCount()},
		                     {"last_frame",
		                      {{"out", frame.out},
		                       {"in_view", frame.inView},
		                       {"in_reach", frame.inReach},
		                       {"drawn", frame.drawn},
		                       {"picture_ready", frame.pictureReady}}}};
	        }));
	provider->Add(Query("fish_farms", "The fish farms: their town, fish left, fishermen and shoal", {}, ResultKind::List),
	              Serve<Locator::fishFarmSystem>(
	                  "the fish farms", [](const ecs::systems::FishFarmSystemInterface& farms, const QueryContext& /*c*/) {
		                  Json items = Json::array();
		                  const auto* registry = Registry();
		                  if (registry == nullptr)
		                  {
			                  return items;
		                  }
		                  registry->Each<const FishFarm>([&](entt::entity entity, const FishFarm& farm) {
			                  auto item = Listed(*registry, entity);
			                  item["town"] = farm.town == entt::null ? Json(nullptr) : Json(ToId(farm.town));
			                  item["fish_left"] = farms.FishLeft(entity);
			                  item["fishermen"] = farm.fishermen.size();
			                  item["shoal"] = farm.shoal.has_value() ? Point(farm.shoal->centre) : Json(nullptr);
			                  items.push_back(std::move(item));
		                  });
		                  return items;
	                  }));
	provider->Add(Query("workshops",
	                    "The workshops: their room for scaffolds, the turns left on a scaffold, and the scaffolds they count",
	                    {}, ResultKind::List),
	              Serve<Locator::workshopSystem>(
	                  "the workshops", [](const ecs::systems::WorkshopSystemInterface& workshops, const QueryContext& /*c*/) {
		                  Json items = Json::array();
		                  const auto* registry = Registry();
		                  if (registry == nullptr)
		                  {
			                  return items;
		                  }
		                  registry->Each<const Workshop>([&](entt::entity entity, const Workshop& workshop) {
			                  auto item = Listed(*registry, entity);
			                  item["room"] = workshops.RoomOf(entity);
			                  item["making_turns"] = workshop.makingTurns;
			                  item["counted"] = workshop.counted;
			                  Json places = Json::array();
			                  for (const auto place : workshop.places)
			                  {
				                  places.push_back(static_cast<int>(place));
			                  }
			                  item["places"] = std::move(places);
			                  Json scaffolds = Json::array();
			                  for (const auto scaffold : workshop.scaffolds)
			                  {
				                  scaffolds.push_back(ToId(scaffold));
			                  }
			                  item["scaffolds"] = std::move(scaffolds);
			                  items.push_back(std::move(item));
		                  });
		                  return items;
	                  }));
	provider->Add(
	    Query("animated_statics",
	          "The scenery the scripts open and close (gates, the gate stone plinth, the piper's cave, the phone box): "
	          "its open word, place in its clip and resting place, whether it is in the land's draw list and was on "
	          "screen, its gate stones' value and the circles a creature's route goes round",
	          {}, ResultKind::List),
	    Serve<Locator::animatedStaticSystem>(
	        "the animated scenery", [](const ecs::systems::AnimatedStaticSystemInterface& scenery, const QueryContext& /*c*/) {
		        Json items = Json::array();
		        const auto* registry = Registry();
		        if (registry == nullptr)
		        {
			        return items;
		        }
		        registry->Each<const AnimatedStatic>([&](entt::entity entity, const AnimatedStatic& still) {
			        auto item = Listed(*registry, entity);
			        item["type"] = static_cast<int>(still.type);
			        item["open"] = still.openState;
			        if (const auto* pose = registry->TryGet<const AnimatedStaticPose>(entity); pose != nullptr)
			        {
				        item["place"] = pose->place;
				        item["resting_place"] = pose->restingPlace;
				        item["in_draw_list"] = pose->inDrawList;
				        item["on_screen"] = pose->onScreen;
				        item["stones_drawn"] = pose->stones.size();
			        }
			        item["stone_value"] = scenery.GateStoneValue(entity).value_or(0);
			        const auto circles = scenery.RouteCircles(entity);
			        item["route_circles"] = circles.has_value() ? Json(circles->size()) : Json(nullptr);
			        items.push_back(std::move(item));
		        });
		        return items;
	        }));
	provider->Add(Query("chimneys", "The chimneys smoking", {}, ResultKind::List),
	              ServeRegistry([](const ecs::Registry& registry, const QueryContext& /*c*/) {
		              Json items = Json::array();
		              registry.Each<const ChimneySmoke>([&items](entt::entity entity, const ChimneySmoke& smoke) {
			              items.push_back({{"id", ToId(entity)},
			                               {"position", Point(smoke.chimney)},
			                               {"state", static_cast<int>(smoke.state)},
			                               {"system", Locator::chimneySmokeSystem::has_value()}});
		              });
		              return items;
	              }));
	provider->Add(Query("things_made",
	                    "The count of the things of the world made since the land was loaded: the place in the order of "
	                    "the next thing made",
	                    {}, ResultKind::Object),
	              Serve<Locator::thingsMadeSystem>("thingsMadeSystem",
	                                               [](const ecs::systems::ThingsMadeSystemInterface& count,
	                                                  const QueryContext& /*c*/) { return Json {{"made", count.Made()}}; }));
	provider->Add(
	    Query("sound_tags",
	          "The sounds tagged to things: sound, whether active, its emitter, the object it is attached to, whether it loops",
	          {}, ResultKind::List),
	    ServeRegistry([](const ecs::Registry& registry, const QueryContext& /*c*/) {
		    Json items = Json::array();
		    registry.Each<const SoundTag>([&items, &registry](entt::entity entity, const SoundTag& tag) {
			    auto item = Listed(registry, entity);
			    item["sound"] = tag.sound;
			    item["active"] = tag.active;
			    item["emitter"] = Id(tag.emitter);
			    item["owner"] = Id(tag.owner);
			    item["loops"] = tag.loops;
			    items.push_back(std::move(item));
		    });
		    return items;
	    }));
	provider->Add(Query("rewards", "The rewards on their way to the land, lying on it, or opened", {}, ResultKind::List),
	              ServeRegistry([](const ecs::Registry& registry, const QueryContext& /*c*/) {
		              Json items = Json::array();
		              registry.Each<const Reward>([&items, &registry](entt::entity entity, const Reward& reward) {
			              auto item = Listed(registry, entity);
			              item["type"] = static_cast<int>(reward.type);
			              item["state"] = static_cast<int>(reward.state);
			              item["opened"] = reward.opened;
			              item["turns_left"] = reward.turnsLeft;
			              item["clip_place"] = reward.clipPlace;
			              item["landing_point"] = Point(reward.landingPoint);
			              items.push_back(std::move(item));
		              });
		              return items;
	              }));
	provider->Add(Query("pathfinding", "The walkers, and the circles of the things in their way they head for or go round"),
	              ServeRegistry([](const ecs::Registry& registry, const QueryContext& /*c*/) {
		              size_t walkers = 0;
		              registry.Each<const WallHug>([&walkers](entt::entity, const WallHug&) { ++walkers; });
		              Json heading = Json::array();
		              registry.Each<const WallHugObjectReference>(
		                  [&heading, &registry](entt::entity entity, const WallHugObjectReference& reference) {
			                  auto item = Listed(registry, entity);
			                  item["obstacle"] = Id(reference.entity);
			                  item["centre"] = Point(reference.centre);
			                  item["radius"] = reference.radius;
			                  item["steps_away"] = reference.stepsAway;
			                  heading.push_back(std::move(item));
		                  });
		              return Json {{"walkers", walkers},
		                           {"heading_for", std::move(heading)},
		                           {"system", Locator::pathfindingSystem::has_value()}};
	              }));
	provider->Add(Query("resource", "What a thing holds as a resource, and the store a pile belongs to",
	                    {IdParameter("The thing's entity id")}),
	              Serve<Locator::resourceStoreSystem>(
	                  "the stores", [](const ecs::systems::ResourceStoreSystemInterface& stores, const QueryContext& context) {
		                  const auto entity = EntityParam(context.params);
		                  if (!entity.has_value())
		                  {
			                  return QueryResult::Error("no entity with that id");
		                  }
		                  const auto resource = stores.ResourceOf(*entity);
		                  return QueryResult::Value({{"id", ToId(*entity)},
		                                             {"type", static_cast<int>(resource.type)},
		                                             {"amount", resource.amount},
		                                             {"poisoned", resource.poisoned},
		                                             {"store", Id(stores.StoreOf(*entity))}});
	                  }));
	provider->Add(
	    Query("building", "How much of a building is drawn while it is built or broken",
	          {IdParameter("The building's entity id")}),
	    Serve<Locator::buildingDamageSystem>(
	        "the building damage", [](const ecs::systems::BuildingDamageSystemInterface& damage, const QueryContext& context) {
		        const auto entity = EntityParam(context.params);
		        if (!entity.has_value())
		        {
			        return QueryResult::Error("no entity with that id");
		        }
		        return QueryResult::Value({{"id", ToId(*entity)},
		                                   {"partial_share", Optional(damage.PartialShare(*entity))},
		                                   {"draws_whole", damage.DrawsWhole(*entity)}});
	        }));
	provider->Add(
	    Query("town_desires", "What a town wants most, and how much it wants each thing",
	          {IdParameter("The town's entity id")}),
	    Serve<Locator::townDesireSystem>("the town desires", [](const ecs::systems::TownDesireSystemInterface& desires,
	                                                            const QueryContext& context) {
		    const auto entity = EntityParam(context.params);
		    if (!entity.has_value())
		    {
			    return QueryResult::Error("no entity with that id");
		    }
		    Json each = Json::object();
		    for (int i = 0; i <= static_cast<int>(TownDesireInfo::ForSleep); ++i)
		    {
			    const auto desire = static_cast<TownDesireInfo>(i);
			    each[std::to_string(i)] = desires.GetDesire(*entity, desire);
		    }
		    Json raw = Json::object();
		    Json doing = Json::object();
		    const auto* townDesire = Locator::entitiesRegistry::value().TryGet<const ecs::components::TownDesire>(*entity);
		    for (int i = 0; i <= static_cast<int>(TownDesireInfo::ForSleep); ++i)
		    {
			    raw[std::to_string(i)] = desires.GetRawDesire(*entity, static_cast<TownDesireInfo>(i));
			    if (townDesire != nullptr)
			    {
				    doing[std::to_string(i)] = townDesire->doingNow.at(static_cast<size_t>(i));
			    }
		    }
		    // The wood the town wants before it sends villagers for more: the targets grow past a number of abodes
		    Json wood = Json::object();
		    if (Locator::infoConstants::has_value())
		    {
			    const auto& town = Locator::infoConstants::value().town;
			    const auto& multipliers =
			        Locator::infoConstants::value().townDesire.at(static_cast<size_t>(TownDesireInfo::ForWood)).tribeMultiplier;
			    wood = {{"minimum_for_desire", town.minimumWoodForDesire},
			            {"maximum_for_desire", town.maximumWoodForDesire},
			            {"buildings_for_desired_wood", town.numOfBuildingsForDesiredWood},
			            {"tribe_multipliers", std::vector<float>(multipliers.begin(), multipliers.end())}};
		    }
		    return QueryResult::Value({{"id", ToId(*entity)},
		                               {"most_wanted", static_cast<int>(desires.GetMostWanted(*entity))},
		                               {"desires", std::move(each)},
		                               {"raw", std::move(raw)},
		                               {"doing_now", std::move(doing)},
		                               {"wood_targets", std::move(wood)}});
	    }));
	return provider;
}

// The players, their hands and their input

std::unique_ptr<ProviderInterface> PlayersProvider()
{
	auto provider = std::make_unique<FunctionProvider>("players");
	provider->Add(Query("new_game", "How the new game started: whether the tutorial and the creature training are "
	                                "skipped and the old creature kept, and the player profiles"),
	              [](const QueryContext& /*c*/) {
		              Json result = Json::object();
		              if (Locator::tutorialSkipSystem::has_value())
		              {
			              const auto& skip = Locator::tutorialSkipSystem::value().Get();
			              result["skip_tutorial"] = skip.skipTutorial;
			              result["skip_creature_training"] = skip.skipCreatureTraining;
			              result["keep_old_creature"] = skip.keepOldCreature;
			              result["start_with_cow"] = skip.startWithCow;
		              }
		              if (Locator::playerProfileSystem::has_value())
		              {
			              const auto& profiles = Locator::playerProfileSystem::value();
			              result["profiles"] = profiles.GetProfileCount();
			              result["profile_has_creature"] = profiles.CurrentProfileHasCreature();
		              }
		              return QueryResult::Value(std::move(result));
	              });
	provider->Add(Query("playground_trip", "The trip to the Gods' Playground: whether every player's influence covers "
	                                       "the whole land, and whether a lesson runs"),
	              [](const QueryContext& /*c*/) {
		              Json result = Json::object();
		              if (Locator::playgroundTripSystem::has_value())
		              {
			              const auto& trip = Locator::playgroundTripSystem::value();
			              result["influence_everywhere"] = trip.InfluenceEverywhere();
			              result["lesson_running"] = trip.IsLessonRunning();
		              }
		              return QueryResult::Value(std::move(result));
	              });
	provider->Add(Query("profile_folder", "The profile folder, the folder of the player played in it, and whether that "
	                                      "folder holds the help record (helpstats.dat)"),
	              [](const QueryContext& /*c*/) {
		              Json result = Json::object();
		              if (!Locator::profileFolderSystem::has_value())
		              {
			              return QueryResult::Value(std::move(result));
		              }
		              const auto& folder = Locator::profileFolderSystem::value();
		              const auto root = folder.GetFolder();
		              result["folder"] = root.has_value() ? Json(root->generic_string()) : Json(nullptr);
		              const auto profile = Locator::playerProfileSystem::has_value()
		                                       ? Locator::playerProfileSystem::value().GetCurrentProfile()
		                                       : std::nullopt;
		              const auto player = profile.has_value() ? gui::ToUtf16(profile->name) : std::u16string {};
		              const auto playerFolder = folder.GetPlayerFolder(player);
		              result["player_folder"] = playerFolder.has_value() ? Json(playerFolder->generic_string()) : Json(nullptr);
		              const auto record = folder.ReadPlayerFile(player, "helpstats.dat");
		              result["help_record_bytes"] = record.has_value() ? Json(record->size()) : Json(nullptr);
		              return QueryResult::Value(std::move(result));
	              });
	provider->Add(Query("custom_music", "The custom music player (its tracks, the track that plays, its state and "
	                                    "position) and the music moods: the record of each track and the current feelings"),
	              [](const QueryContext& /*c*/) {
		              Json result = Json::object();
		              auto* player = Locator::sounds::has_value() ? Locator::sounds::value().GetCustomMusic() : nullptr;
		              if (player != nullptr)
		              {
			              Json tracks = Json::array();
			              for (const auto& track : player->GetTracks())
			              {
				              tracks.push_back(track.fileName);
			              }
			              result["tracks"] = std::move(tracks);
			              const auto current = player->GetCurrentTrack();
			              result["current"] = current.has_value() ? Json(*current) : Json(nullptr);
			              result["song"] = player->GetSongName();
			              result["state"] = static_cast<int>(player->GetPlayState());
			              result["position_ms"] = player->GetPosition();
			              result["length_ms"] = player->GetLength();
			              result["volume"] = player->GetVolume();
			              result["track_id"] = music_mood::TrackId(player->GetSongName());
		              }
		              if (Locator::musicMoodSystem::has_value())
		              {
			              const auto& moods = Locator::musicMoodSystem::value();
			              Json records = Json::array();
			              for (const auto& record : moods.GetRecords())
			              {
				              Json item = Json::object();
				              item["id"] = record.id;
				              item["time"] = record.time;
				              item["mood"] = record.emotions.empty() ? 0.0f : record.emotions.front().mood;
				              item["fear"] = record.emotions.empty() ? 0.0f : record.emotions.front().fear;
				              records.push_back(std::move(item));
			              }
			              result["records"] = std::move(records);
			              const auto emotion = moods.GetCurrentEmotion();
			              result["current_mood"] = emotion.has_value() ? Json(emotion->mood) : Json(nullptr);
		              }
		              return QueryResult::Value(std::move(result));
	              });
	provider->Add(Query("list",
	                    "The players: their entities, primary creatures, computer minds (the nodes wanted now; "
	                    "players.computer_nodes has every node) and alignment",
	                    {}, ResultKind::List),
	              Serve<Locator::playerSystem>("the players", [](const ecs::systems::PlayerSystemInterface& players,
	                                                             const QueryContext& /*c*/) {
		              Json items = Json::array();
		              const auto* registry = Registry();
		              if (registry == nullptr)
		              {
			              return items;
		              }
		              // The players the land has, by their own components: the system is asked only of those it knows
		              registry->Each<const Player>([&](entt::entity entity, const Player& player) {
			              const auto name = player.name;
			              Json item = {{"player", static_cast<int>(name)},
			                           {"id", Id(entity)},
			                           {"local", (Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer()
			                                                                       : PlayerNames::PLAYER_ONE) == name},
			                           {"computer", players.IsComputerPlayer(name)},
			                           {"primary_creature", Id(players.GetPrimaryCreature(name))}};
			              // The computer mind of the player, when a land script switched it on or named a player for it
			              if (const auto* mind = registry->TryGet<const ecs::components::ComputerPlayer>(entity);
			                  mind != nullptr)
			              {
				              item["computer_mind"] = {{"switched_on", mind->switchedOn},
				                                       {"marked", mind->marked},
				                                       {"started", mind->started},
				                                       {"creature_like", mind->creatureLike.has_value()
				                                                             ? Json(static_cast<int>(*mind->creatureLike))
				                                                             : Json(nullptr)}};
				              // What the god wants and what its hand does: the nodes that were wanted when last looked at
				              auto& computer = item["computer_mind"];
				              computer["paused"] = mind->mind.paused;
				              computer["last_empty_turn"] = mind->mind.lastEmptyTurn;
				              computer["doing"] = mind->mind.doing;
				              computer["hand"] = {mind->hand.hand.x, mind->hand.hand.y, mind->hand.hand.z};
				              Json steps = Json::array();
				              for (const auto& step : mind->hand.steps)
				              {
					              steps.push_back(static_cast<int>(step.kind));
				              }
				              computer["steps"] = std::move(steps);
				              computer["step_part"] = mind->hand.part;
				              Json wanted = Json::array();
				              for (const auto& node : mind->mind.nodes)
				              {
					              if (node.value != 0.0f)
					              {
						              wanted.push_back({{"name", computer_mind::InfoOf(node.kind).name},
						                                {"value", node.value},
						                                {"suppression", node.suppression},
						                                {"object", Id(node.object)}});
					              }
				              }
				              computer["wanted"] = std::move(wanted);
				              Json requests = Json::object();
				              for (size_t type = 0; type < mind->mind.castRequested.size(); ++type)
				              {
					              if (mind->mind.castRequested.at(type) != 0)
					              {
						              requests[std::to_string(type)] = mind->mind.castRequested.at(type);
					              }
				              }
				              computer["cast_requests"] = std::move(requests);
				              computer["casts"] = mind->casts.size();
			              }
			              if (Locator::alignmentSystem::has_value())
			              {
				              item["alignment"] = Locator::alignmentSystem::value().GetPlayerAlignment(name);
				              item["pending_alignment"] = Locator::alignmentSystem::value().GetPendingAlignment(name);
			              }
			              items.push_back(std::move(item));
		              });
		              return items;
	              }));
	// The whole tree of a computer mind, where the list of the players shows only the nodes that are wanted now
	provider->Add(
	    Query("computer_nodes",
	          "Every node of the tree of one computer god's mind: its name, personality weight, suppression, how much it "
	          "was wanted when last looked at, its place in the tree and the node above. Pages with limit and cursor. "
	          "name keeps only the nodes that the script name finds",
	          {{.name = "player",
	            .type = "integer|string",
	            .description = "The god: its number (0 to 7) or its name, such as PLAYER_TWO",
	            .required = true},
	           {.name = "name",
	            .type = "string",
	            .description = "Only the nodes that a script finds with this name, such as ExpandInfluence",
	            .required = false}},
	          ResultKind::List),
	    ServeRegistry([](const ecs::Registry& registry, const QueryContext& context) -> QueryResult {
		    const auto player = NamedPlayerParam(context.params);
		    if (!player.has_value())
		    {
			    return QueryResult::Error("player must be a number from 0 to 7 or a name such as PLAYER_TWO");
		    }
		    const auto entity = PlayerEntityOf(registry, *player);
		    const auto* computer =
		        entity.has_value() ? registry.TryGet<const ecs::components::ComputerPlayer>(*entity) : nullptr;
		    if (computer == nullptr)
		    {
			    return QueryResult::Error("that player has no computer mind");
		    }
		    const auto& mind = computer->mind;
		    const auto& names = SharedNodeNames(registry);
		    std::vector<uint16_t> shown;
		    if (const auto filter = StringMember(context.params, "name"); filter.has_value())
		    {
			    shown = computer_mind::FindNodes(mind, *filter, names);
		    }
		    else
		    {
			    shown.resize(mind.nodes.size());
			    std::ranges::generate(shown, [next = uint16_t {0}]() mutable { return next++; });
		    }
		    Json items = Json::array();
		    for (const auto index : shown)
		    {
			    const auto& node = mind.nodes.at(index);
			    items.push_back({{"index", index},
			                     {"name", computer_mind::NameOf(names, node.kind)},
			                     {"layer", static_cast<int>(computer_mind::InfoOf(node.kind).layer)},
			                     {"weight", node.weight},
			                     {"suppression", node.suppression},
			                     {"value", node.value},
			                     {"parent", node.parent.has_value() ? Json(*node.parent) : Json(nullptr)},
			                     {"children", node.childCount},
			                     {"object", Id(node.object)}});
		    }
		    return QueryResult::Value(std::move(items));
	    }));
	provider->Add(
	    Query("computer_attitudes",
	          "What one computer god's mind thinks of each player: the value that a script reads back, and the value the "
	          "mind keeps (twice as much)",
	          {{.name = "player",
	            .type = "integer|string",
	            .description = "The god: its number (0 to 7) or its name, such as PLAYER_TWO",
	            .required = true}}),
	    ServeRegistry([](const ecs::Registry& registry, const QueryContext& context) -> QueryResult {
		    const auto player = NamedPlayerParam(context.params);
		    if (!player.has_value())
		    {
			    return QueryResult::Error("player must be a number from 0 to 7 or a name such as PLAYER_TWO");
		    }
		    const auto entity = PlayerEntityOf(registry, *player);
		    if (!entity.has_value() || !registry.AllOf<ecs::components::ComputerPlayer>(*entity))
		    {
			    return QueryResult::Error("that player has no computer mind");
		    }
		    // A mind that no script gave an attitude keeps the first value for every player
		    const auto* attitudes = registry.TryGet<const ecs::components::ComputerAttitudes>(*entity);
		    Json about = Json::array();
		    for (size_t other = 0; other < static_cast<size_t>(PlayerNames::_COUNT); ++other)
		    {
			    const auto kept =
			        attitudes != nullptr ? attitudes->kept.at(other) : ecs::components::ComputerAttitudes::k_StartValue;
			    about.push_back({{"player", other},
			                     {"name", k_PlayerNamesStrs.at(other)},
			                     {"attitude", player_relations::ReadAttitude(kept)},
			                     {"kept", kept}});
		    }
		    return QueryResult::Value({{"player", static_cast<int>(*player)}, {"attitudes", std::move(about)}});
	    }));
	provider->Add(
	    Query("commands", "The queue of the players' commands: the player of this computer, whose number the commands carry, "
	                      "and the commands that wait for the start of a turn"),
	    Serve<Locator::playerCommandSystem>(
	        "the players' commands", [](const ecs::systems::PlayerCommandSystemInterface& commands, const QueryContext& /*c*/) {
		        return Json {{"sender", static_cast<int>(commands.GetSender())}, {"pending", commands.PendingCount()}};
	        }));
	provider->Add({.name = "checksum",
	               .description = "The checksum of the synced world that a network game sends after each turn: whether it "
	                              "is on, and the turn and checksum of the last turn. With on, it turns it on or off",
	               .parameters = {{.name = "on",
	                               .type = "boolean",
	                               .description = "Turn the checksum on or off from the next turn",
	                               .required = false}},
	               .kind = ResultKind::Object,
	               .needsNear = false,
	               .writes = true},
	              [](const QueryContext& context) {
		              if (!Locator::syncChecksumSystem::has_value())
		              {
			              return QueryResult::Error("no checksum of the synced world");
		              }
		              auto& checksum = Locator::syncChecksumSystem::value();
		              if (const auto on = context.params.find("on"); on != context.params.end() && on->is_boolean())
		              {
			              checksum.SetEnabled(on->get<bool>());
		              }
		              const auto last = checksum.GetLast();
		              return QueryResult::Value(
		                  Json {{"on", checksum.IsEnabled()},
		                        {"turn", last.has_value() ? Json(last->turn) : Json(nullptr)},
		                        {"checksum", last.has_value() ? Json(fmt::format("{:016x}", last->checksum)) : Json(nullptr)}});
	              });
	provider->Add(Query("heartbeat", "Each player's heartbeat: danger, rate, beat and pulse", {}, ResultKind::List),
	              Serve<Locator::heartbeatSystem>(
	                  "the heartbeats", [](const ecs::systems::HeartbeatSystemInterface& hearts, const QueryContext& /*c*/) {
		                  Json items = Json::array();
		                  const auto* registry = Registry();
		                  if (registry == nullptr)
		                  {
			                  return items;
		                  }
		                  registry->Each<const Player>([&](const Player& player) {
			                  const auto state = hearts.StateOf(player.name);
			                  if (!state.has_value())
			                  {
				                  return;
			                  }
			                  items.push_back({{"player", static_cast<int>(player.name)},
			                                   {"stress", state->stress},
			                                   {"rate", state->rate},
			                                   {"beats_per_second", state->rate * ecs::heartbeat::k_BeatsPerRate},
			                                   {"phase", state->phase},
			                                   {"pulse", state->pulse},
			                                   {"believer_share", state->believerShare},
			                                   {"influence_share", state->influenceShare},
			                                   {"destruction_beats", state->destructionBeats}});
		                  });
		                  return items;
	                  }));
	provider->Add(
	    Query("game_end", "The end of a skirmish or a network game: whether it is decided, and where each god stands"),
	    Serve<Locator::gameEndSystem>(
	        "the end of the game", [](const ecs::systems::GameEndSystemInterface& end, const QueryContext& /*c*/) {
		        Json gods = Json::array();
		        if (const auto* registry = Registry(); registry != nullptr)
		        {
			        registry->Each<const Player, const ecs::components::GameStanding>(
			            [&gods](const Player& player, const ecs::components::GameStanding& standing) {
				            gods.push_back({{"player", static_cast<int>(player.name)},
				                            {"out", standing.out},
				                            {"finished", standing.finished},
				                            {"share", standing.share},
				                            {"place", standing.place.has_value() ? Json(*standing.place) : Json(nullptr)},
				                            {"left", standing.reason == ecs::components::OutReason::Left}});
			            });
		        }
		        Json events = Json::array();
		        for (const auto& event : end.LastEvents())
		        {
			        events.push_back({{"god", static_cast<int>(event.god)}, {"kind", static_cast<int>(event.kind)}});
		        }
		        return Json {{"decided", end.IsDecided()}, {"gods", gods}, {"last_events", events}};
	        }));
	provider->Add(
	    Query("hand", "This computer's hands, the hands of its own god that it moves and draws: the player, where the hands "
	                  "are, and what the hand holds or takes"),
	    Serve<Locator::handSystem>("the hands", [](const ecs::systems::HandSystemInterface& hands, const QueryContext& /*c*/) {
		    Json result = OwnHandsJson(hands);
		    if (Locator::handGrabSystem::has_value())
		    {
			    const auto& grab = Locator::handGrabSystem::value();
			    result["held"] = Id(grab.GetHeld());
			    result["busy"] = grab.IsBusy();
			    result["in_influence"] = grab.IsInInfluence();
			    result["cursor_raise"] = grab.GetCursorRaise();
		    }
		    return result;
	    }));
	provider->Add(Query("pick", "What the cursor picks: the object, the point and the land under it, whether the help's text "
	                            "bubble or the leash's rope took the cursor, and the land behind the hand's "
	                            "middle, about which a press with nothing under the cursor reaches"),
	              Serve<Locator::pickingSystem>("the picking", [](const ecs::systems::PickingSystemInterface& picking,
	                                                              const QueryContext& /*c*/) {
		              const auto& pick = picking.GetPick();
		              return Json {{"object", Id(pick.object)},          {"point", Optional(pick.point)},
		                           {"land", Optional(pick.land)},        {"land_distance", pick.landDistance},
		                           {"text_bubble", pick.textBubble},     {"rope", pick.rope},
		                           {"distance", pick.distance},          {"hover_object", Id(pick.hoverObject)},
		                           {"hover_seconds", pick.hoverSeconds}, {"behind_hand", Optional(picking.GetBehindHand())}};
	              }));
	provider->Add(
	    Query("hand_trace",
	          "The player's hand frame by frame, for hand bugs: start keeps the last frames (frames, 600 by default) of every "
	          "part of the frame that put the hand somewhere, where it was placed, drawn and showed, on the screen too, and "
	          "what it did; read returns them (the last few with last) and the jumps among them: frames where the hand "
	          "moved on the screen more than threshold pixels (12) further than the cursor, came_back when it returned "
	          "within return_within frames (4); stop forgets them",
	          {{.name = "action", .type = "string", .description = "start, read (the default) or stop", .required = false},
	           {.name = "frames", .type = "integer", .description = "With start: how many frames to keep", .required = false},
	           {.name = "last", .type = "integer", .description = "With read: only the last frames", .required = false},
	           {.name = "threshold",
	            .type = "number",
	            .description = "With read: the pixels beyond the cursor's move that make a jump",
	            .required = false},
	           {.name = "return_within",
	            .type = "integer",
	            .description = "With read: the frames a jump may take to come back",
	            .required = false},
	           {.name = "jumps_only",
	            .type = "boolean",
	            .description = "With read: only the jumps, each with the frames around it",
	            .required = false}}),
	    [](const QueryContext& context) {
		    if (!Locator::handSystem::has_value() || Locator::handSystem::value().GetTrace() == nullptr)
		    {
			    return QueryResult::Error("the hands keep no trace");
		    }
		    auto& trace = *Locator::handSystem::value().GetTrace();
		    const auto action = context.params.value("action", std::string("read"));
		    if (action == "start")
		    {
			    const auto frames = context.params.value("frames", 600);
			    if (frames < 1 || frames > 100000)
			    {
				    return QueryResult::Error("frames is 1 to 100000");
			    }
			    trace.Enable(static_cast<size_t>(frames));
			    return QueryResult::Value(Json {{"tracing", true}, {"frames", frames}});
		    }
		    if (action == "stop")
		    {
			    trace.Disable();
			    return QueryResult::Value(Json {{"tracing", false}});
		    }
		    if (action != "read")
		    {
			    return QueryResult::Error("action is start, read or stop");
		    }
		    auto frames = trace.Frames();
		    const auto threshold = context.params.value("threshold", 12.0f);
		    const auto returnWithin = static_cast<size_t>(std::max(context.params.value("return_within", 4), 0));
		    const auto jumps = hand_trace::FindJumps(frames, threshold, returnWithin);
		    const auto vec = [](glm::vec3 v) { return Json {v.x, v.y, v.z}; };
		    const auto frameJson = [&vec](const hand_trace::Frame& frame) {
			    Json writes = Json::array();
			    for (const auto& write : frame.Writes())
			    {
				    writes.push_back({{"by", std::string(write.writer)}, {"at", vec(write.position)}});
			    }
			    return Json {{"frame", frame.frame},
			                 {"seconds", frame.seconds},
			                 {"cursor", {frame.cursor.x, frame.cursor.y}},
			                 {"placed", vec(frame.placed)},
			                 {"drawn", vec(frame.drawn)},
			                 {"visible", vec(frame.visible)},
			                 {"screen", frame.screen.has_value() ? Json {frame.screen->x, frame.screen->y} : Json(nullptr)},
			                 {"wrist", vec(frame.wrist)},
			                 {"wrist_screen", frame.wristScreen.has_value() ? Json {frame.wristScreen->x, frame.wristScreen->y}
			                                                                : Json(nullptr)},
			                 {"stage", frame.stage},
			                 {"busy", frame.busy},
			                 {"held", frame.held.has_value() ? Json(*frame.held) : Json(nullptr)},
			                 {"picked", frame.picked.has_value() ? Json(*frame.picked) : Json(nullptr)},
			                 {"cursor_land", frame.cursorLand.has_value() ? vec(*frame.cursorLand) : Json(nullptr)},
			                 {"rest_on_thing", frame.restOnThing.has_value() ? vec(*frame.restOnThing) : Json(nullptr)},
			                 {"held_at", frame.heldAt.has_value() ? vec(*frame.heldAt) : Json(nullptr)},
			                 {"writes", std::move(writes)},
			                 {"writes_lost", frame.writesLost}};
		    };
		    Json jumpItems = Json::array();
		    for (const auto& jump : jumps)
		    {
			    jumpItems.push_back({{"frame", jump.frame},
			                         {"pixels", jump.pixels},
			                         {"beyond_cursor", jump.beyondCursor},
			                         {"came_back", jump.cameBack},
			                         {"last_writer", std::string(jump.writer)}});
		    }
		    Json items = Json::array();
		    if (context.params.value("jumps_only", false))
		    {
			    // Each jump with the two frames before it and the few it may take to come back
			    std::vector<bool> keep(frames.size(), false);
			    for (const auto& jump : jumps)
			    {
				    const auto from = jump.index >= 2 ? jump.index - 2 : 0;
				    for (size_t i = from; i < frames.size() && i <= jump.index + returnWithin; ++i)
				    {
					    keep[i] = true;
				    }
			    }
			    for (size_t i = 0; i < frames.size(); ++i)
			    {
				    if (keep[i])
				    {
					    items.push_back(frameJson(frames[i]));
				    }
			    }
		    }
		    else
		    {
			    const auto last = static_cast<size_t>(std::max(context.params.value("last", 0), 0));
			    const size_t first = last > 0 && last < frames.size() ? frames.size() - last : 0;
			    for (size_t i = first; i < frames.size(); ++i)
			    {
				    items.push_back(frameJson(frames[i]));
			    }
		    }
		    return QueryResult::Value(Json {{"tracing", trace.Enabled()},
		                                    {"kept", frames.size()},
		                                    {"jumps", std::move(jumpItems)},
		                                    {"frames", std::move(items)}});
	    });
	provider->Add(Query("input", "The mouse and the hands as the game's actions see them"),
	              Serve<Locator::gameActionSystem>(
	                  "the game's actions", [](const input::GameActionInterface& actions, const QueryContext& /*c*/) {
		                  const auto mouse = actions.GetMousePosition();
		                  const auto hands = actions.GetHandPositions();
		                  return Json {{"mouse", {mouse.x, mouse.y}},
		                               {"wheel", actions.GetMouseWheelDelta()},
		                               {"hands", {Optional(hands[0]), Optional(hands[1])}},
		                               {"queued_presses", actions.HasQueuedPresses()},
		                               {"scripted_pointer", actions.GetScriptedPointer().has_value()},
		                               {"cursor_frozen", actions.IsCursorFrozen()}};
	                  }));
	provider->Add(Query("gestures", "The gesture being drawn and the last recognised"),
	              Serve<Locator::gestureSystem>(
	                  "the gestures", [](const ecs::systems::GestureSystemInterface& gestures, const QueryContext& /*c*/) {
		                  Json result = {{"drawing", gestures.IsDrawingPath()},
		                                 {"gesturing", gestures.IsGesturing()},
		                                 {"templates", gestures.GetTemplates().size()},
		                                 {"leash_picker_open", gestures.IsLeashPickerOpen()},
		                                 {"circle_seconds_left", gestures.GetCircleSecondsLeft()},
		                                 {"events_queue", Locator::gestureEvents::has_value()}};
		                  // The row of hints at the bottom right: each hinted gesture and its leash, and the row's fade
		                  Json hints = Json::array();
		                  for (const auto& hint : gestures.GetHints().hints)
		                  {
			                  hints.push_back({{"gesture", static_cast<uint32_t>(hint.gesture)},
			                                   {"leash", static_cast<int>(hint.leash)},
			                                   {"dot", hint.dot}});
		                  }
		                  result["hints"] = {{"alpha", gestures.GetHints().alpha}, {"shown", hints}};
		                  if (const auto last = gestures.GetLastRecognised(); last.has_value())
		                  {
			                  result["last_recognised"] = {{"number", last->number}, {"age", last->age}};
		                  }
		                  return result;
	                  }));
	return provider;
}

// Miracles, fire and what reacts to them

std::unique_ptr<ProviderInterface> MagicProvider()
{
	auto provider = std::make_unique<FunctionProvider>("magic");
	auto townMiracle =
	    Query("town_miracle",
	          "Debug only: gives a town a miracle or takes it away, through the same steps as the land's scripts. A town given "
	          "one without a worship site can have its site, which goes up for its builders.",
	          {IdParameter("The town (town.list)"),
	           {.name = "magic",
	            .type = "integer|string",
	            .description = "The magic type, by number or by its info name",
	            .required = true},
	           {.name = "enable", .type = "boolean", .description = "Give it (default) or take it away", .required = false},
	           {.name = "with_plain",
	            .type = "boolean",
	            .description = "Give a power-up level's plain miracle too, as a land file line does (default true)",
	            .required = false},
	           {.name = "all_levels",
	            .type = "boolean",
	            .description = "Take the plain miracle and every level, as a creature that steals it does (default false)",
	            .required = false},
	           {.name = "open_site",
	            .type = "boolean",
	            .description = "Let a town without a worship site have one, as the scripts do (default true)",
	            .required = false},
	           {.name = "any_land",
	            .type = "boolean",
	            .description = "Also on the first land, which never has worship sites (default true)",
	            .required = false}});
	townMiracle.writes = true;
	provider->Add(std::move(townMiracle), [](const QueryContext& context) {
		const auto* registry = Registry();
		if (registry == nullptr)
		{
			return QueryResult::Error("there are no towns without a land");
		}
		const auto town = EntityParam(context.params);
		if (!town.has_value() || !registry->AllOf<Town>(*town))
		{
			return QueryResult::Error("give a town's id (town.list)");
		}
		const auto& params = context.params;
		std::optional<MagicType> type;
		if (const auto magic = params.find("magic"); magic != params.end())
		{
			if (magic->is_number_integer())
			{
				type = static_cast<MagicType>(magic->get<int>());
			}
			else if (magic->is_string() && Locator::infoConstants::has_value())
			{
				type = magic::FindMagicTypeByName(Locator::infoConstants::value(), magic->get<std::string>());
			}
		}
		if (!type.has_value())
		{
			return QueryResult::Error("give magic as a magic type number or an info name");
		}
		const auto flag = [&params](const char* name, bool fallback) {
			const auto found = params.find(name);
			return found != params.end() && found->is_boolean() ? found->get<bool>() : fallback;
		};
		const auto outcome = debug::town_miracles::Apply({.town = *town,
		                                                  .type = *type,
		                                                  .give = flag("enable", true),
		                                                  .withPlain = flag("with_plain", true),
		                                                  .allLevels = flag("all_levels", false),
		                                                  .openSite = flag("open_site", true),
		                                                  .anyLand = flag("any_land", true)});
		if (!outcome.error.empty())
		{
			return QueryResult::Error(outcome.error);
		}
		Json changed = Json::array();
		for (const auto changedType : outcome.changed)
		{
			changed.push_back(static_cast<int>(changedType));
		}
		Json held = Json::array();
		for (const auto heldType : debug::town_miracles::HeldBy(*town))
		{
			held.push_back(static_cast<int>(heldType));
		}
		return QueryResult::Value(
		    {{"changed", std::move(changed)},
		     {"magic_held", std::move(held)},
		     {"worship_site", Id(outcome.site)},
		     {"site_is_new", outcome.siteIsNew},
		     {"no_site",
		      outcome.noSite.has_value() ? Json(std::string(town_miracle_tool::Describe(*outcome.noSite))) : Json(nullptr)}});
	});
	provider->Add(
	    Query("state", "The hand's miracle, the spells and dispensers there are, and the other miracles' "
	                   "systems: shields, tornadoes, explosions and the piles of roots"),
	    Serve<Locator::magicSystem>("magic", [](const ecs::systems::MagicSystemInterface& magic, const QueryContext& /*c*/) {
		    Json result = {{"spells", magic.GetSpells().size()},
		                   {"dispensers", magic.GetDispensers().size()},
		                   {"hand_busy", magic.IsHandBusy()},
		                   {"held_seed", Id(magic.GetHeldSeed())},
		                   {"last_hand_result", static_cast<int>(magic.GetLastHandResult())},
		                   {"ignoring_influence", magic.IsIgnoringInfluence()}};
		    if (Locator::magicShieldSystem::has_value())
		    {
			    result["shields"] = Locator::magicShieldSystem::value().GetDomes(0.0f).size();
		    }
		    if (Locator::tornadoSystem::has_value())
		    {
			    result["carried_by_tornadoes"] = Locator::tornadoSystem::value().CarriedCount();
		    }
		    if (Locator::explosionSystem::has_value())
		    {
			    result["shaking"] = Locator::explosionSystem::value().IsShaking();
			    result["ghosts"] = Locator::explosionSystem::value().Ghosts().size();
			    // The piles of roots that this computer draws, with the milliseconds that stay for each
			    auto piles = Json::array();
			    for (const auto& pile : Locator::explosionSystem::value().RootsPiles())
			    {
				    piles.push_back({{"position", {pile.position.x, pile.position.y, pile.position.z}},
				                     {"scale", pile.scale},
				                     {"ms_left", pile.millisecondsLeft},
				                     {"alpha", pile.alpha.has_value() ? Json(*pile.alpha) : Json(nullptr)}});
			    }
			    result["roots_piles"] = std::move(piles);
		    }
		    if (Locator::floatingNumberSystem::has_value())
		    {
			    result["floating_numbers"] = Locator::floatingNumberSystem::value().Numbers().size();
		    }
		    result["miracle_fx"] = Locator::miracleFxSystem::has_value();
		    return result;
	    }));
	provider->Add(
	    Query("spells", "The spells cast, nearest first when searched about a point", {}, ResultKind::List),
	    Serve<Locator::magicSystem>("magic", [](const ecs::systems::MagicSystemInterface& magic, const QueryContext& /*c*/) {
		    Json items = Json::array();
		    for (const auto& spell : magic.GetSpells())
		    {
			    items.push_back({{"id", Id(spell.entity)},
			                     {"type", static_cast<int>(spell.magicType)},
			                     {"player", static_cast<int>(spell.player)},
			                     {"position", Point(spell.position)},
			                     {"age", spell.age},
			                     {"duration", spell.duration},
			                     {"strength", spell.strength},
			                     {"closing", spell.closing}});
		    }
		    return items;
	    }));
	provider->Add(
	    Query("dispensers", "The miracle dispensers", {}, ResultKind::List),
	    Serve<Locator::magicSystem>("magic", [](const ecs::systems::MagicSystemInterface& magic, const QueryContext& /*c*/) {
		    Json items = Json::array();
		    for (const auto& dispenser : magic.GetDispensers())
		    {
			    items.push_back({{"id", Id(dispenser.entity)},
			                     {"type", static_cast<int>(dispenser.magicType)},
			                     {"position", Point(dispenser.position)},
			                     {"has_orb", dispenser.hasOrb},
			                     {"active", dispenser.active},
			                     {"tick", dispenser.tick},
			                     {"period", dispenser.period}});
		    }
		    return items;
	    }));
	provider->Add(
	    Query("teleport", "A player's teleport stones",
	          {{.name = "player", .type = "integer", .description = "The player's number, 0 by default", .required = false}},
	          ResultKind::List),
	    Serve<Locator::teleportSystem>(
	        "the teleports", [](const ecs::systems::TeleportSystemInterface& teleports, const QueryContext& context) {
		        const auto* registry = Registry();
		        Json items = Json::array();
		        for (const auto stone : teleports.GetStones(PlayerParam(context.params)))
		        {
			        items.push_back(registry != nullptr ? Listed(*registry, stone) : Json {{"id", ToId(stone)}});
		        }
		        return items;
	        }));
	provider->Add(Query("vortices",
	                    "The vortices between the lands: their kind, state, openness and how far they levelled the ground", {},
	                    ResultKind::List),
	              Serve<Locator::vortexSystem>(
	                  "the vortices", [](const ecs::systems::VortexSystemInterface& vortices, const QueryContext& /*c*/) {
		                  Json items = Json::array();
		                  if (auto* registry = Registry(); registry != nullptr)
		                  {
			                  registry->Each<const ecs::components::Vortex>(
			                      [&items, &vortices](entt::entity entity, const ecs::components::Vortex& vortex) {
				                      items.push_back({{"id", Id(entity)},
				                                       {"type", static_cast<int>(vortex.type)},
				                                       {"state", static_cast<int>(vortex.state)},
				                                       {"state_start_turn", vortex.stateStartTurn},
				                                       {"centre", Point(vortex.centre)},
				                                       {"openness", vortices.GetOpenness(entity)},
				                                       {"level_applied", vortex.levelApplied},
				                                       {"before_land_effect", vortex.beforeLandEffect},
				                                       {"after_land_effect", vortex.afterLandEffect},
				                                       {"object_mover_effect", vortex.objectMoverEffect},
				                                       {"light_map_effect", vortex.lightMapEffect}});
			                      });
		                  }
		                  return items;
	                  }));
	provider->Add(Query("reactions", "The reactions going on, nearest first when searched about a point", {}, ResultKind::List),
	              Serve<Locator::reactionSystem>(
	                  "the reactions", [](const ecs::systems::ReactionSystemInterface& reactions, const QueryContext& /*c*/) {
		                  Json items = Json::array();
		                  for (const auto& reaction : reactions.GetReactions())
		                  {
			                  items.push_back({{"id", reaction.id},
			                                   {"type", static_cast<int>(reaction.source.type)},
			                                   {"initiator", Id(reaction.source.initiator)},
			                                   {"player", static_cast<int>(reaction.source.player)},
			                                   {"position", Point(reaction.source.position)},
			                                   {"age", reaction.age},
			                                   {"reach", reaction.reach}});
		                  }
		                  return items;
	                  }));
	provider->Add(
	    Query("fires", "The fires burning, nearest first when searched about a point", {}, ResultKind::List),
	    Serve<Locator::fireSystem>("the fires", [](const ecs::systems::FireSystemInterface& fires, const QueryContext& /*c*/) {
		    Json items = Json::array();
		    for (const auto& fire : fires.GetFires())
		    {
			    items.push_back({{"id", Id(fire.object)},
			                     {"position", Point(fire.position)},
			                     {"temperature", fire.temperature},
			                     {"burning", fire.burning},
			                     {"charring", fire.charring},
			                     {"life", fire.life},
			                     {"root", Id(fire.root)},
			                     {"blaze_size", fire.blazeSize},
			                     {"firemen", fire.firemen}});
		    }
		    return items;
	    }));
	return provider;
}

// The temple

// The name of a submesh of a temple room's mesh, or null when the room or the submesh is not known
Json TempleSubMeshName(TempleRoom room, std::optional<uint32_t> subMesh)
{
	constexpr std::array<std::string_view, 7> k_RoomMeshes {"challenge", "creature", "credits", "main",
	                                                        "multi",     "options",  "savegame"};
	const auto index = static_cast<size_t>(room);
	if (!subMesh.has_value() || index >= k_RoomMeshes.size() || !Locator::resources::has_value())
	{
		return nullptr;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto id = entt::hashed_string(("temple/interior/" + std::string(k_RoomMeshes[index]) + "_l3d").c_str()).value();
	if (!meshes.Contains(id))
	{
		return nullptr;
	}
	const auto& subMeshes = meshes.Handle(id)->GetSubMeshes();
	return *subMesh < subMeshes.size() ? Json(subMeshes[*subMesh]->GetName()) : Json(nullptr);
}

std::unique_ptr<ProviderInterface> TempleProvider()
{
	auto provider = std::make_unique<FunctionProvider>("temple");
	provider->Add(
	    Query("state", "The temples on the land: their owners, and whether they are being destroyed", {}, ResultKind::List),
	    ServeRegistry([](const ecs::Registry& registry, const QueryContext& /*c*/) {
		    Json items = Json::array();
		    registry.Each<const Temple>([&items, &registry](entt::entity entity, const Temple& temple) {
			    auto item = Listed(registry, entity);
			    item["owner"] = static_cast<int>(temple.owner);
			    item["destroying"] = temple.destroying;
			    item["last_hit_turn"] = temple.lastHitTurn;
			    item["has_exterior"] = registry.AllOf<TempleExterior>(entity);
			    items.push_back(std::move(item));
		    });
		    return items;
	    }));
	provider->Add(Query("interior", "The temple's inside: whether the player is in it, the room, the visits, the "
	                                "worshippers walking about it (how many came in, and where those drawn are), the "
	                                "seeds of the miracles over the map, how the map shows the influence and what the cursor "
	                                "is over (the room, its submesh and the submesh's name)"),
	              Serve<Locator::temple>("the temple", [](const TempleInteriorInterface& temple, const QueryContext& /*c*/) {
		              const auto transition = temple.GetTransitionRoom();
		              Json drawn = Json::array();
		              for (const auto& person : temple.GetPeople())
		              {
			              drawn.push_back(Point(glm::vec3(person.model[3])));
		              }
		              Json seeds = Json::array();
		              for (const auto& seed : temple.GetMapSpellSeeds())
		              {
			              seeds.push_back({{"seed", static_cast<int>(seed.seed)},
			                               {"middle", Point(seed.middle)},
			                               {"spin", seed.spin},
			                               {"phial_frame", seed.phialFrame}});
		              }
		              const auto influence = temple.GetMapInfluence();
		              // What the cursor is over: the room, its submesh and the submesh's name, so that a tool can find a
		              // room's pictures, buttons and doors when it moves the pointer
		              Json cursor = nullptr;
		              if (const auto hit = temple.GetCursorHit(); hit.has_value())
		              {
			              cursor = Json {{"point", Point(hit->point)},
			                             {"room", static_cast<int>(hit->room)},
			                             {"sub_mesh", hit->subMesh.has_value() ? Json(*hit->subMesh) : Json(nullptr)},
			                             {"name", TempleSubMeshName(hit->room, hit->subMesh)}};
		              }
		              return Json {
		                  {"cursor", std::move(cursor)},
		                  {"map_spell_seeds", std::move(seeds)},
		                  {"map_influence", influence.has_value()
		                                        ? Json {{"offset", Point(influence->offset)}, {"scale", influence->scale}}
		                                        : Json(nullptr)},
		                  {"people_inside", temple.GetPeopleInside()},
		                  {"people_drawn", std::move(drawn)},
		                  {"active", temple.Active()},
		                  {"position", Point(temple.GetPosition())},
		                  {"room", static_cast<int>(temple.GetCurrentRoom())},
		                  {"transition_room", transition.has_value() ? Json(static_cast<int>(*transition)) : Json(nullptr)},
		                  {"visits", temple.GetVisits()}};
	              }));
	provider->Add(
	    Query("villagers",
	          "The villagers around the player's temple, as the Temple Villagers debug window shows them: its worship "
	          "sites and the places at them, the towns worshipping there, every villager worshipping, on its way or "
	          "within the radius with its state and walk, and those walking about inside the temple",
	          {{.name = "radius",
	            .type = "number",
	            .description = "Villagers this near the temple's middle, in metres, are listed too (60 if not given)",
	            .required = false},
	           {.name = "player", .type = "integer", .description = "The player, 0 if not given", .required = false}}),
	    [](const QueryContext& context) {
		    const auto player = static_cast<PlayerNames>(context.params.value("player", 0));
		    const auto survey = temple_villagers::Take(player, context.params.value("radius", 60.0f), false);
		    if (survey.temple == entt::null)
		    {
			    return QueryResult::Error("the player has no temple");
		    }
		    Json sites = Json::array();
		    for (const auto& site : survey.sites)
		    {
			    Json places = Json::object();
			    for (size_t i = 0; i < site.places.size(); ++i)
			    {
				    if (const auto& place = site.places.at(i); place.has_value())
				    {
					    places[std::string(temple_villagers::Name(static_cast<temple_villagers::SitePlace>(i)))] = {place->x,
					                                                                                                place->y};
				    }
			    }
			    Json worshippers = Json::array();
			    for (const auto villager : site.worshippers)
			    {
				    worshippers.push_back(Id(villager));
			    }
			    sites.push_back({{"id", Id(site.site)},
			                     {"tribe", static_cast<int>(site.tribe)},
			                     {"place", site.place},
			                     {"built", site.built},
			                     {"dancers", site.dancers},
			                     {"food_in_pot", site.foodInPot},
			                     {"worshippers", std::move(worshippers)},
			                     {"places", std::move(places)}});
		    }
		    Json towns = Json::array();
		    for (const auto& town : survey.towns)
		    {
			    towns.push_back({{"id", Id(town.town)},
			                     {"town_id", town.id},
			                     {"site", Id(town.site)},
			                     {"share", town.share},
			                     {"population", town.population},
			                     {"worshipping", town.worshipping},
			                     {"on_the_way", town.onTheWay},
			                     {"needed", town.needed}});
		    }
		    Json villagers = Json::array();
		    for (const auto& villager : survey.villagers)
		    {
			    Json item {{"id", Id(villager.villager)},
			               {"standing", std::string(temple_villagers::Name(villager.standing))},
			               {"tribe", static_cast<int>(villager.tribe)},
			               {"job", static_cast<int>(villager.number)},
			               {"child", villager.child},
			               {"age", villager.age},
			               {"life", villager.life},
			               {"town", Id(villager.town)},
			               {"site", Id(villager.site)},
			               {"state", static_cast<int>(villager.final)},
			               {"top_state", static_cast<int>(villager.top)},
			               {"turns_in_state", villager.turnsInState},
			               {"position", Point(villager.position)},
			               {"distance", villager.distance}};
			    if (villager.walk.has_value())
			    {
				    const auto& walk = *villager.walk;
				    Json walking {{"goal", {walk.goal.x, walk.goal.y}},
				                  {"move_state", std::string(walk.moveState)},
				                  {"speed", walk.speed}};
				    if (walk.stepGoal.has_value())
				    {
					    walking["step_goal"] = {walk.stepGoal->x, walk.stepGoal->y};
				    }
				    if (walk.circleCentre.has_value())
				    {
					    walking["circle"] = {{"centre", {walk.circleCentre->x, walk.circleCentre->y}},
					                         {"radius", walk.circleRadius}};
				    }
				    item["walk"] = std::move(walking);
			    }
			    villagers.push_back(std::move(item));
		    }
		    Json inside = Json::array();
		    for (const auto& walker : survey.inside.walkers)
		    {
			    inside.push_back({{"villager", Id(walker.villager)},
			                      {"position", Point(walker.position)},
			                      {"target", Point(walker.target)},
			                      {"walking", walker.walking},
			                      {"timer", walker.timer},
			                      {"pause", walker.pause}});
		    }
		    return QueryResult::Value({{"temple", Id(survey.temple)},
		                               {"temple_position", Point(survey.templePosition)},
		                               {"sites", std::move(sites)},
		                               {"towns", std::move(towns)},
		                               {"villagers", std::move(villagers)},
		                               {"in_temple", survey.inside.active},
		                               {"inside", std::move(inside)}});
	    });
	return provider;
}

// The camera's helpers, the editor and the cinematics

/// The seam is the game on the player's computer, or null with none
std::unique_ptr<ProviderInterface> ViewProvider(const GameSeamInterface* seam)
{
	auto provider = std::make_unique<FunctionProvider>("view");
	provider->Add(Query("pause_text", "The words of the pause and the speed: which words show (paused, slower, faster or "
	                                  "none), their fade made smooth, and the turn of their letters"),
	              [](const QueryContext& /*c*/) {
		              Json result = {{"shown", false}};
		              if (!Locator::pauseTextSystem::has_value())
		              {
			              return QueryResult::Value(std::move(result));
		              }
		              if (const auto shown = Locator::pauseTextSystem::value().GetShown())
		              {
			              constexpr std::array<std::string_view, 3> k_Kinds {"paused", "slower", "faster"};
			              result["shown"] = true;
			              result["kind"] = shown->kind == gui::pause_text::Kind::None
			                                   ? std::string_view("none")
			                                   : k_Kinds.at(static_cast<size_t>(shown->kind));
			              result["eased"] = shown->eased;
			              result["turn"] = shown->turn;
		              }
		              return QueryResult::Value(std::move(result));
	              });
	provider->Add(
	    Query("incoming_messages",
	          "The incoming messages at the bottom left of the screen: the list in order (kind, text, weather picture, the "
	          "real time it started to show), the real time now, and whether the button shows with its look"),
	    [seam](const QueryContext& /*c*/) {
		    Json result = {{"count", 0}, {"messages", Json::array()}, {"shown", false}};
		    if (!Locator::incomingMessageSystem::has_value())
		    {
			    return QueryResult::Value(std::move(result));
		    }
		    const auto& system = Locator::incomingMessageSystem::value();
		    constexpr std::array<std::string_view, 3> k_Kinds {"chat", "mail", "weather"};
		    result["now"] = system.GetMilliseconds();
		    result["count"] = system.GetQueue().Count();
		    result["shown_text"] = gui::ToUtf8(system.GetQueue().ShownText());
		    // The list is read from the top message down, by dismissing copies
		    auto copy = system.GetQueue();
		    while (const auto* top = copy.Top())
		    {
			    result["messages"].push_back({{"kind", k_Kinds.at(static_cast<size_t>(top->kind))},
			                                  {"text", gui::ToUtf8(top->text)},
			                                  {"picture", top->picture},
			                                  {"since", top->since}});
			    copy.Dismiss(top->since);
		    }
		    if (seam != nullptr)
		    {
			    seam->AddIncomingMessages(result);
		    }
		    return QueryResult::Value(std::move(result));
	    });
	provider->Add(Query("chat",
	                    "The chat of a network game: the names of the gods, the lines that came in (turn, sender, the "
	                    "players that get them, text or taunt, shown in the incoming messages), the speech bubbles over "
	                    "the creatures (lines, time to show, scroll), and the chat controls (shown, open, quick chat, "
	                    "the typed line, the list of players and who gets a line)"),
	              [seam](const QueryContext& /*c*/) {
		              Json result = {{"names", Json::object()}, {"received", Json::array()}, {"bubbles", Json::array()}};
		              if (!Locator::chatSystem::has_value())
		              {
			              return QueryResult::Value(std::move(result));
		              }
		              auto& chat = Locator::chatSystem::value();
		              for (const auto& [slot, name] : chat.GetNames())
		              {
			              result["names"][std::to_string(static_cast<int>(slot))] = gui::ToUtf8(name);
		              }
		              for (const auto& line : chat.GetReceived())
		              {
			              Json recipients = Json::array();
			              for (const auto player : line.recipients)
			              {
				              recipients.push_back(static_cast<int>(player));
			              }
			              result["received"].push_back({{"turn", line.turn},
			                                            {"sender", static_cast<int>(line.sender)},
			                                            {"recipients", std::move(recipients)},
			                                            {"text", gui::ToUtf8(line.text)},
			                                            {"taunt", line.taunt.has_value() ? Json(*line.taunt) : Json(nullptr)},
			                                            {"shown", line.shown}});
		              }
		              for (const auto& [creature, bubble] : chat.GetBubbles())
		              {
			              Json lines = Json::array();
			              for (const auto& line : bubble.lines)
			              {
				              lines.push_back({{"text", gui::ToUtf8(line.text)}, {"sender", static_cast<int>(line.sender)}});
			              }
			              result["bubbles"].push_back({{"creature", entt::to_integral(creature)},
			                                           {"lines", std::move(lines)},
			                                           {"display_time", bubble.displayTime},
			                                           {"scroll", bubble.scroll}});
		              }
		              if (seam != nullptr)
		              {
			              seam->AddChatControls(result);
		              }
		              return QueryResult::Value(std::move(result));
	              });
	provider->Add(Query("tooltip", "The tooltip that the hand shows: its index and text name, its mouse button and arrows, "
	                               "its fade, and the numbers or other words that it shows"),
	              [seam](const QueryContext& /*c*/) {
		              Json result = {{"shown", false}};
		              if (seam != nullptr)
		              {
			              seam->AddToolTip(result);
		              }
		              return QueryResult::Value(std::move(result));
	              });
	provider->Add(Query("video", "The video playing: its frame, frame count and rate, its fade, the falling spell, its "
	                             "creature and its effects, whether films are on, and the 16-bit surface of the films"),
	              [](const QueryContext& /*c*/) {
		              Json result = Json::object();
		              // The fade of the whole screen, which the video fades to white at its end
		              if (Locator::screenFade::has_value())
		              {
			              const auto& fade = Locator::screenFade::value();
			              result["screen_fade"] = {{"amount", fade.GetAmount()}, {"turns", fade.GetTurns()}};
		              }
		              if (Locator::videoSystem::has_value())
		              {
			              const auto& videos = Locator::videoSystem::value();
			              result["playing"] = videos.IsPlaying();
			              result["covers_screen"] = videos.CoversScreen();
			              result["hides_world"] = videos.HidesWorld();
			              result["films"] = videos.AreFilmsEnabled();
			              const auto surface = videos.GetFilmSurface();
			              result["sixteen_bit_colour"] = surface.has_value();
			              result["film_surface"] = "none";
			              if (surface)
			              {
				              result["film_surface"] = *surface == graphics::film_surface::Format::Rgb555 ? "555" : "565";
			              }
			              if (const auto status = videos.GetStatus())
			              {
				              result["frame"] = status->frame;
				              result["frame_count"] = status->frameCount;
				              result["fps"] = status->fps;
				              result["alpha"] = status->alpha;
				              result["falling_spell"] = status->fallingSpell;
				              result["falling_spell_state"] = status->fallingSpellState;
			              }
		              }
		              // The copy of the player's creature that the falling spell's film shows, and its colours now
		              if (Locator::filmCreatureSystem::has_value())
		              {
			              const auto& film = Locator::filmCreatureSystem::value();
			              Json creature = {{"running", film.IsRunning()}};
			              if (const auto shown = film.GetShown())
			              {
				              creature["copy"] = Id(shown->copy);
				              creature["colour"] = Point(shown->colour);
				              creature["added"] = Point(shown->added);
				              creature["eye"] = Point(glm::vec3(glm::inverse(shown->view)[3]));
				              creature["shown"] = shown->copyShown;
				              creature["stars"] = shown->stars.size();
				              Json stars = Json::array();
				              for (const auto& star : shown->stars)
				              {
					              stars.push_back({{"pixel", {star.pixel.x, star.pixel.y}},
					                               {"depth", star.depth},
					                               {"half_size", star.halfSize},
					                               {"argb", star.argb}});
				              }
				              creature["star_sprites"] = std::move(stars);
				              creature["fans"] = shown->fans.size();
				              if (!shown->fans.empty() && !shown->fans.front().empty())
				              {
					              const auto& middle = shown->fans.front().front();
					              creature["burst_pixel"] = {middle.pixel.x, middle.pixel.y};
					              creature["burst_alpha"] = middle.argb >> 24u;
				              }
			              }
			              result["film_creature"] = std::move(creature);
		              }
		              return QueryResult::Value(std::move(result));
	              });
	provider->Add(Query("state", "Whether a camera path holds the camera, the camera's help, the advisors' cheers as the "
	                             "camera turns, and the knock readout"),
	              [](const QueryContext& /*c*/) {
		              Json result = Json::object();
		              if (Locator::cameraPathSystem::has_value())
		              {
			              auto& path = Locator::cameraPathSystem::value();
			              result["path"] = {
			                  {"pathing", path.IsPathing()}, {"paused", path.IsPaused()}, {"holds_camera", path.HoldsCamera()}};
		              }
		              if (Locator::cameraHelpSystem::has_value())
		              {
			              const auto& help = Locator::cameraHelpSystem::value().Get();
			              result["help"] = {{"features", help.features},
			                                {"auto_pitch", help.autoPitch},
			                                {"camera_keys", help.cameraKeys},
			                                {"hand_reach", help.handReach},
			                                {"interface_level",
			                                 help.interfaceLevel.has_value() ? Json(*help.interfaceLevel) : Json(nullptr)}};
		              }
		              if (Locator::turnCheerSystem::has_value())
		              {
			              const auto& cheer = Locator::turnCheerSystem::value();
			              const auto& cheering = cheer.GetCheering();
			              result["turn_cheer"] = {{"turn_speed", cheer.GetTurnSpeed()},
			                                      {"on", cheering.on},
			                                      {"last_cheer_turn", cheering.lastCheerTurn},
			                                      {"last_best_praise_turn", cheering.lastBestPraiseTurn}};
		              }
		              if (Locator::abodeKnockSystem::has_value())
		              {
			              const auto& knock = Locator::abodeKnockSystem::value();
			              result["knock_readout"] = {{"town", Id(knock.GetReadoutTown())}, {"scale", knock.GetReadoutScale()}};
		              }
		              return QueryResult::Value(std::move(result));
	              });
	provider->Add(Query("bookmarks", "The camera's bookmarks: each slot, whether it is set, and whether the camera follows it",
	                    {}, ResultKind::List),
	              Serve<Locator::cameraBookmarkSystem>(
	                  "the bookmarks", [](const ecs::systems::CameraBookmarkSystemInterface& bookmarks, const QueryContext&) {
		                  // The markers are things that only the frames make
		                  const auto* registry = DrawnRegistry();
		                  Json items = Json::array();
		                  for (size_t i = 0; i < bookmarks.GetBookmarks().size(); ++i)
		                  {
			                  const auto entity = bookmarks.GetBookmarks()[i];
			                  if (entity == entt::null || registry == nullptr || !registry->Valid(entity))
			                  {
				                  continue;
			                  }
			                  auto item = Listed(*registry, entity);
			                  item["slot"] = i;
			                  item["set"] = bookmarks.IsSet(i);
			                  item["followed"] = bookmarks.GetFollowed() == i;
			                  items.push_back(std::move(item));
		                  }
		                  return items;
	                  }));
	provider->Add(
	    Query("zones", "The camera zones the land's scripts set: their file, the fence the camera is kept inside, its "
	                   "height limits and the places it is kept out of, whether the camera is inside the fence, and the "
	                   "force field the camera lights up where it hits the fence (its strongest point, how often hit)"),
	    Serve<Locator::cameraZoneSystem>(
	        "the camera zones", [](const ecs::systems::CameraZoneSystemInterface& system, const QueryContext&) {
		        const auto& zones = system.GetZones();
		        Json fence = Json::array();
		        for (const auto& corner : zones.fence)
		        {
			        fence.push_back(Json::array({corner.x, corner.y, corner.z}));
		        }
		        Json exclusions = Json::array();
		        for (const auto& exclusion : zones.exclusions)
		        {
			        exclusions.push_back(
			            {{"position", Json::array({exclusion.position.x, exclusion.position.y, exclusion.position.z})},
			             {"radius", exclusion.radius},
			             {"height", exclusion.height},
			             {"kind", exclusion.kind == camera_zones::Exclusion::Kind::Cylinder ? "cylinder" : "dome"}});
		        }
		        Json result {{"file", system.GetFileName()},
		                     {"exclusions_on", zones.exclusionsOn},
		                     {"fence_on", zones.fenceOn},
		                     {"max_altitude", zones.useMaxAltitude ? Json(zones.maxAltitude) : Json(nullptr)},
		                     {"height_above_land", zones.useHeightAboveLand ? Json(zones.heightAboveLand) : Json(nullptr)},
		                     {"fence", std::move(fence)},
		                     {"exclusions", std::move(exclusions)}};
		        // The wall of light the camera lights up where it hits the fence
		        const auto& forceField = system.GetForceField();
		        const auto points = std::min(forceField.Points(), forceField.Strengths().size());
		        const auto strengths = std::span(forceField.Strengths()).first(points);
		        result["force_field"] = {
		            {"drawn", !forceField.Hidden()},
		            {"showing", !forceField.Hidden() && forceField.Showing()},
		            {"points", points},
		            {"strongest", strengths.empty() ? 0.0f : std::ranges::max(strengths)},
		            {"hits", system.GetFenceHits()},
		        };
		        if (Locator::camera::has_value())
		        {
			        const auto& camera = Locator::camera::value();
			        const auto origin = camera.GetOrigin();
			        result["camera_inside"] =
			            camera_zones::CrossFence(zones.fence, zones.fenceOn, origin, camera.GetFocus() - origin).inside;
			        if (Locator::terrainSystem::has_value())
			        {
				        result["height_limit"] = camera_zones::HeightLimit(
				            zones, Locator::terrainSystem::value().GetHeightAt(glm::vec2(origin.x, origin.z)));
			        }
		        }
		        return result;
	        }));
	provider->Add(
	    Query("cinematic",
	          "The fade, the wide screen, whether the interface is shown, and the words the scripts write on the screen"),
	    Serve<Locator::cinematicDirectorSystem>(
	        "the cinematics", [](const ecs::systems::CinematicDirectorSystemInterface& director, const QueryContext&) {
		        auto texts = Json::array();
		        for (const auto& text : director.GetTexts())
		        {
			        texts.push_back({{"words", gui::ToUtf8(text.words)},
			                         {"corner", {text.corner.x, text.corner.y}},
			                         {"extent", {text.extent.x, text.extent.y}},
			                         {"size", text.size},
			                         {"colour", {text.colour.r, text.colour.g, text.colour.b}},
			                         {"alpha", text.alpha},
			                         {"target", text.target},
			                         {"rate", text.rate}});
		        }
		        return Json {{"fade_finished", director.IsFadeFinished()},
		                     {"fade_colour", director.GetFadeColour()},
		                     {"wide_screen", director.IsWideScreenOn()},
		                     {"wide_screen_fraction", director.GetWideScreenFraction()},
		                     {"interface_active", director.IsInterfaceActive()},
		                     {"close_clipping", director.IsCloseClipping()},
		                     {"texts", texts}};
	        }));
	provider->Add(
	    Query("draw_list",
	          "The land's draw list: the things on the blocks in view within 700 units that are drawn, at most 2999, the "
	          "nearest blocks first; with an id, whether that thing is in it and drawn",
	          {{.name = "id", .type = "integer", .description = "A thing to look up in the list", .required = false}}),
	    Serve<Locator::objectDrawListSystem>(
	        "the land's draw list",
	        [](const ecs::systems::ObjectDrawListSystemInterface& drawList, const QueryContext& context) {
		        const auto summary = drawList.GetSummary();
		        Json result {{"active", summary.active},
		                     {"rebuilt", summary.frame.rebuilt},
		                     {"draw_all", summary.frame.drawAll},
		                     {"made_at_turn", Optional(summary.madeAt)},
		                     {"blocks_in_view", summary.blocksInView},
		                     {"blocks_walked", summary.blocksWalked},
		                     {"farthest_walked", summary.farthestWalked},
		                     {"things", summary.things},
		                     {"kept", summary.kept},
		                     {"full", summary.full},
		                     {"rebuilds_asked", summary.rebuildsAsked}};
		        if (const auto entity = EntityParam(context.params); entity.has_value())
		        {
			        result["listed"] = drawList.Listed(*entity);
			        if (const auto* registry = Registry(); registry != nullptr)
			        {
				        result["drawn"] = drawList.Drawn(*registry, *entity);
			        }
		        }
		        return result;
	        }));
	provider->Add(
	    Query("script_control", "The tasks with the camera and the dialogue, the scripts' spoken lines and the "
	                            "villagers drawn in high detail"),
	    [](const QueryContext& /*c*/) {
		    Json result = Json::object();
		    if (Locator::scriptControlSystem::has_value())
		    {
			    result["camera_owner"] = Locator::scriptControlSystem::value().GetCameraOwner();
		    }
		    if (Locator::dialogueControlSystem::has_value())
		    {
			    result["dialogue_owner"] = Locator::dialogueControlSystem::value().GetOwner();
		    }
		    if (Locator::helpSpeechSystem::has_value())
		    {
			    const auto& speech = Locator::helpSpeechSystem::value();
			    result["speech"] = {{"spoken_texts", speech.GetSpokenTextCount()}, {"lines_playing", speech.GetLineCount()}};
		    }
		    if (Locator::highDetailSystem::has_value())
		    {
			    Json items = Json::array();
			    if (const auto* registry = Registry(); registry != nullptr)
			    {
				    registry->Each<const HighDetail>([&items, registry](entt::entity entity, const HighDetail& detail) {
					    auto item = Listed(*registry, entity);
					    item["detailed_model"] = detail.usualModel.has_value();
					    item["face"] = detail.face.has_value() ? static_cast<int>(*detail.face) : -1;
					    item["follow_intro_hand"] = detail.orders.followIntroHand;
					    item["turn_at_once"] = detail.orders.turnAtOnce;
					    if (detail.eyes.has_value())
					    {
						    Json eyes = {{"blinking", detail.eyes->blink.blinking},
						                 {"until_next_blink", detail.eyes->blink.untilNext},
						                 {"into_blink", detail.eyes->blink.into},
						                 {"roll", detail.eyes->roll},
						                 {"drawn", detail.drawnEyes.has_value()}};
						    if (detail.drawnEyes.has_value())
						    {
							    const auto& right = detail.drawnEyes->eyes[0].eyeball[3];
							    eyes["closed"] = detail.drawnEyes->closed;
							    eyes["right_eye"] = {right.x, right.y, right.z};
						    }
						    item["eyes"] = std::move(eyes);
					    }
					    items.push_back(std::move(item));
				    });
			    }
			    result["high_detail"] = std::move(items);
		    }
		    return QueryResult::Value(std::move(result));
	    });
	provider->Add(Query("editor", "The editor: open, its tool, its camera and what is selected"),
	              Serve<Locator::editorSystem>(
	                  "the editor", [](const ecs::systems::EditorSystemInterface& editor, const QueryContext& /*c*/) {
		                  return Json {{"open", editor.IsOpen()},
		                               {"tool", static_cast<int>(editor.GetTool())},
		                               {"camera_mode", static_cast<int>(editor.GetCameraMode())},
		                               {"stepping", editor.IsStepping()},
		                               {"selected", Id(editor.GetSelection().Get())}};
	                  }));
	return provider;
}

// The advisors

/// How an advisor's mesh, posed with its bones this frame, lies before the camera: its depths along the view, the
/// vertices the projection clips, and its triangles by the way they wind on the screen
struct AdvisorDepths
{
	float nearestVertex {0.0f};
	float furthestVertex {0.0f};
	size_t vertices {0};
	size_t closerThanNear {0};
	size_t clippedNear {0};
	size_t clippedFar {0};
	size_t clockwise {0};
	size_t counterClockwise {0};
	size_t mirroredBones {0};
};

std::optional<AdvisorDepths> MeasureAdvisorDepths(const help::spirits::AdvisorModel& model, std::span<const glm::mat4> bones,
                                                  const Camera& camera)
{
	l3d::L3DFile file;
	if (bones.empty() || file.Open(model.file.mesh) != l3d::L3DResult::Success)
	{
		return std::nullopt;
	}
	const auto view = camera.GetViewMatrix(Camera::Interpolation::Current);
	const auto viewProjection = camera.GetProjectionMatrix(Camera::Projection::ReversedZ) * view;
	AdvisorDepths depths {.nearestVertex = std::numeric_limits<float>::max(),
	                      .furthestVertex = std::numeric_limits<float>::lowest()};
	for (const auto& bone : bones)
	{
		depths.mirroredBones += glm::determinant(glm::mat3(bone)) < 0.0f ? 1 : 0;
	}
	for (uint32_t submesh = 0; submesh < file.GetSubmeshHeaders().size(); ++submesh)
	{
		const auto& header = file.GetSubmeshHeaders()[submesh];
		// The drawn level of detail only, as the renderer picks it
		if (header.flags.isPhysics || header.flags.status != 0 || (header.flags.lodMask & 1) != 1)
		{
			continue;
		}
		const auto vertices = file.GetVertexSpan(submesh);
		std::vector<glm::vec4> clip(vertices.size(), glm::vec4(0.0f));
		size_t vertex = 0;
		for (const auto& group : file.GetVertexGroupSpan(submesh))
		{
			const auto& bone = bones[std::min<size_t>(group.boneIndex, bones.size() - 1)];
			for (uint16_t i = 0; i < group.vertexCount && vertex < vertices.size(); ++i, ++vertex)
			{
				const auto& position = vertices[vertex].position;
				const glm::vec4 world = bone * glm::vec4(position.x, position.y, position.z, 1.0f);
				// The view is left handed: it looks down its positive z
				const float depth = (view * world).z;
				depths.nearestVertex = std::min(depths.nearestVertex, depth);
				depths.furthestVertex = std::max(depths.furthestVertex, depth);
				depths.closerThanNear += depth < camera.GetNearClip() ? 1 : 0;
				clip[vertex] = viewProjection * world;
				// Reversed, depth runs from w at the near plane to 0 at the far plane; the renderer clips outside it
				depths.clippedNear += clip[vertex].z > clip[vertex].w ? 1 : 0;
				depths.clippedFar += clip[vertex].z < 0.0f ? 1 : 0;
				++depths.vertices;
			}
		}
		const auto indices = file.GetIndexSpan(submesh);
		uint32_t firstVertex = 0;
		uint32_t firstIndex = 0;
		for (const auto& primitive : file.GetPrimitiveSpan(submesh))
		{
			for (uint32_t triangle = 0; triangle < primitive.numTriangles; ++triangle)
			{
				std::array<glm::vec2, 3> corner {};
				bool inside = true;
				for (uint32_t k = 0; k < 3; ++k)
				{
					const uint32_t index = firstIndex + 3 * triangle + k;
					const size_t at = index < indices.size() ? size_t {indices[index]} + firstVertex : clip.size();
					if (at >= clip.size() || clip[at].w <= 0.0f)
					{
						inside = false;
						break;
					}
					corner.at(k) = glm::vec2(clip[at]) / clip[at].w;
				}
				if (!inside)
				{
					continue;
				}
				const glm::vec2 a = corner[1] - corner[0];
				const glm::vec2 b = corner[2] - corner[0];
				const float area = a.x * b.y - a.y * b.x;
				depths.counterClockwise += area > 0.0f ? 1 : 0;
				depths.clockwise += area < 0.0f ? 1 : 0;
			}
			firstVertex += primitive.numVertices;
			firstIndex += primitive.numTriangles * 3;
		}
	}
	return depths.vertices > 0 ? std::optional(depths) : std::nullopt;
}

/// Where an advisor hovers and clings on the screen, and where its bones are drawn there, even off the screen
void AddAdvisorPlacement(Json& item, const ecs::systems::AdvisorSystemInterface& advisors,
                         const ecs::systems::AdvisorSystemInterface::Draw& draw, const Camera* camera)
{
	const auto& controller = advisors.GetController();
	const auto& dude = controller.Dude(draw.advisor);
	const auto& screen = controller.GetScreen();
	const auto unit = static_cast<float>(screen.HoverUnit());
	const auto hover = dude.Hover();
	item["state"] = dude.State();
	item["hover"] = {hover.x, hover.y};
	// Both of the hover's axes are measured in hover units from the screen's middle
	item["hover_pixel"] = {hover.x * unit + static_cast<float>(screen.HalfWidth()),
	                       hover.y * unit + static_cast<float>(screen.HalfHeight())};
	item["screen"] = {
	    {"width", screen.width}, {"height", screen.height}, {"hover_unit", screen.HoverUnit()}, {"reach", screen.Reach()}};
	item["cling_edge"] = static_cast<int>(dude.ClingEdge());
	item["cling_target"] = {dude.ClingTarget().x, dude.ClingTarget().y};
	if (camera == nullptr || draw.bones.empty())
	{
		return;
	}
	const auto viewProjection = camera->GetViewProjectionMatrix();
	const auto toPixel = [&viewProjection, &screen](const glm::mat4& bone) -> std::optional<glm::vec2> {
		const auto clip = viewProjection * bone[3];
		if (clip.w <= 0.0f)
		{
			return std::nullopt;
		}
		const auto ndc = glm::vec2(clip) / clip.w;
		return glm::vec2((ndc.x + 1.0f) * 0.5f * static_cast<float>(screen.width),
		                 (1.0f - ndc.y) * 0.5f * static_cast<float>(screen.height));
	};
	if (const auto root = toPixel(draw.bones.front()))
	{
		item["root_pixel"] = {root->x, root->y};
	}
	// How far up and down the screen its bones reach
	float top = std::numeric_limits<float>::max();
	float bottom = std::numeric_limits<float>::lowest();
	for (const auto& bone : draw.bones)
	{
		if (const auto pixel = toPixel(bone))
		{
			top = std::min(top, pixel->y);
			bottom = std::max(bottom, pixel->y);
		}
	}
	if (top <= bottom)
	{
		item["bones_pixel_y"] = {top, bottom};
	}
}

std::unique_ptr<ProviderInterface> AdvisorsProvider()
{
	auto provider = std::make_unique<FunctionProvider>("advisors");
	provider->Add(
	    Query("draws",
	          "What is drawn of the advisors this frame: near the screen or in the world, alpha, how far out in "
	          "the world, where they hover and cling on the screen (and the screen they move on: its size, the pixels in "
	          "a hover unit and how many hover units out its sides lie) and where their bones are drawn, and how "
	          "close their posed meshes come to the camera beside its near plane",
	          {}, ResultKind::List),
	    Serve<Locator::advisorSystem>("the advisors", [](const ecs::systems::AdvisorSystemInterface& advisors,
	                                                     const QueryContext& /*c*/) {
		    const auto* camera = Locator::camera::has_value() ? &Locator::camera::value() : nullptr;
		    Json items = Json::array();
		    for (const auto& draw : advisors.GetDraws())
		    {
			    Json item = {{"advisor", draw.advisor},  {"near_screen", draw.nearScreen}, {"alpha", draw.alpha},
			                 {"in_world", draw.inWorld}, {"bones", draw.bones.size()},     {"sprites", draw.sprites.size()}};
			    if (!draw.bones.empty())
			    {
				    item["root"] = Point(glm::vec3(draw.bones.front()[3]));
			    }
			    AddAdvisorPlacement(item, advisors, draw, camera);
			    const auto* model = advisors.GetModel(draw.advisor);
			    if (camera != nullptr && model != nullptr)
			    {
				    item["near_clip"] = camera->GetNearClip();
				    if (const auto depths = MeasureAdvisorDepths(*model, draw.bones, *camera))
				    {
					    item["nearest_vertex_depth"] = depths->nearestVertex;
					    item["furthest_vertex_depth"] = depths->furthestVertex;
					    item["vertices"] = depths->vertices;
					    item["vertices_closer_than_near"] = depths->closerThanNear;
					    item["vertices_clipped_near"] = depths->clippedNear;
					    item["vertices_clipped_far"] = depths->clippedFar;
					    item["triangles_clockwise"] = depths->clockwise;
					    item["triangles_counter_clockwise"] = depths->counterClockwise;
					    item["mirrored_bones"] = depths->mirroredBones;
				    }
			    }
			    items.push_back(std::move(item));
		    }
		    return items;
	    }));
	return provider;
}

// The creatures' other systems

/// The tattoo places on the cave's creature as the tattoo editor shows them, and the place under the pointer
void AddTattooSites(Json& cave, const ecs::systems::CreatureCaveSystemInterface& system)
{
	Json sites = Json::array();
	for (const auto& site : system.GetTattooSitesOnScreen())
	{
		sites.push_back(
		    {{"site", site.site}, {"screen", {site.screen.x, site.screen.y}}, {"facing_camera", site.facingCamera}});
	}
	cave["tattoo_sites"] = std::move(sites);
	if (Locator::tattooEditorSystem::has_value())
	{
		cave["tattoo_hovered"] = Optional(Locator::tattooEditorSystem::value().GetHoveredSite());
	}
}

std::unique_ptr<ProviderInterface> CreaturesProvider()
{
	auto provider = std::make_unique<FunctionProvider>("creatures");
	provider->Add(Query("systems", "The creature systems' own state: the creature followed or in the cave, the hand on a "
	                               "creature, the hair, voices, footprints, physiology, the mind kept between lands and the "
	                               "web page"),
	              [](const QueryContext& /*c*/) {
		              Json result = Json::object();
		              if (Locator::creatureModeSystem::has_value())
		              {
			              const auto& mode = Locator::creatureModeSystem::value();
			              result["mode"] = {{"active", mode.IsActive()}, {"creature", Id(mode.GetCreature())}};
		              }
		              if (Locator::creatureCaveSystem::has_value())
		              {
			              const auto& cave = Locator::creatureCaveSystem::value();
			              result["cave"] = {{"open", cave.IsOpen()},
			                                {"in_temple", cave.InTemple()},
			                                {"creature", Id(cave.GetCreature())},
			                                {"creature_shown", cave.IsCreatureShown()},
			                                {"copy", Id(cave.GetCaveCreature())}};
			              // The copy of the creature by the cave's fire, and what it is doing
			              const auto copy = cave.GetCaveCreature();
			              if (copy.has_value() && Locator::temple::has_value() &&
			                  Locator::temple::value().GetRooms().Valid(*copy))
			              {
				              // The copy is a thing of the temple's rooms
				              const auto& registry = Locator::temple::value().GetRooms();
				              const auto* self = registry.TryGet<const ecs::components::CreatureCopy>(*copy);
				              const auto* state = registry.TryGet<const ecs::components::CaveCreature>(*copy);
				              const auto* transform = registry.TryGet<const ecs::components::Transform>(*copy);
				              const auto* animation = registry.TryGet<const ecs::components::CreatureAnimation>(*copy);
				              if (self != nullptr && state != nullptr && transform != nullptr && animation != nullptr)
				              {
					              const auto& s = state->state;
					              result["cave"]["copy_state"] = {
					                  {"size", self->size},
					                  {"position", Point(transform->position)},
					                  {"heading", s.heading},
					                  {"rest", s.rest == cave_creature::Rest::Sleep ? "sleep" : "sit"},
					                  {"turning", s.turn.has_value()},
					                  {"wanted_heading", s.wantedHeading.has_value() ? Json(*s.wantedHeading) : Json(nullptr)},
					                  {"zoomed", s.zoomed},
					                  {"body_animation", creature_layers::IsPlaying(animation->body)
					                                         ? Json(creature_layers::CurrentAnimation(animation->body))
					                                         : Json(nullptr)},
					                  {"body_time_ms", animation->body.timeMs},
					                  {"mirrored", animation->body.mirrored},
					                  {"gesture", animation->gesture.animation.has_value() ? Json(*animation->gesture.animation)
					                                                                       : Json(nullptr)},
					                  {"looks_at", animation->lookAt.has_value() ? Point(*animation->lookAt) : Json(nullptr)},
					                  {"light", Point(state->light)},
					                  {"fidgets", s.fidgets},
					                  {"face", Optional(animation->face.current)},
					              };
					              AddTattooSites(result["cave"], cave);
				              }
			              }
		              }
		              if (Locator::creatureHandSystem::has_value())
		              {
			              const auto& hand = Locator::creatureHandSystem::value();
			              result["hand"] = {{"creature", Id(hand.GetCreature())},
			                                {"under_cursor", Id(hand.CreatureUnderCursor())},
			                                {"feedback_sum", hand.GetFeedbackSum()}};
		              }
		              if (Locator::creatureHairSystem::has_value())
		              {
			              result["hair_shown"] = Locator::creatureHairSystem::value().IsShown();
		              }
		              if (Locator::creatureAudioSystem::has_value())
		              {
			              result["voices_muted"] = Locator::creatureAudioSystem::value().IsMuted();
		              }
		              if (Locator::footprintSystem::has_value())
		              {
			              result["footprints"] = Locator::footprintSystem::value().GetPrints().size();
		              }
		              if (Locator::creaturePhysiologySystem::has_value())
		              {
			              result["physiology_time_scale"] = Locator::creaturePhysiologySystem::value().GetTimeScale();
			              result["fainting"] = Locator::creaturePhysiologySystem::value().IsFaintingEnabled();
		              }
		              if (Locator::creatureFightSystem::has_value())
		              {
			              result["players_fighter"] = Id(Locator::creatureFightSystem::value().PlayersFighter());
		              }
		              if (Locator::creatureCarryOverSystem::has_value())
		              {
			              result["mind_kept"] = Locator::creatureCarryOverSystem::value().Kept() != nullptr;
		              }
		              // The web page: the captions of the five slots of pictures, and whether a picture waits for its frame
		              if (Locator::creatureWebPageSystem::has_value())
		              {
			              const auto& page = Locator::creatureWebPageSystem::value();
			              auto captions = Json::array();
			              for (const auto& caption : page.GetCaptions())
			              {
				              captions.push_back(gui::ToUtf8(caption));
			              }
			              result["web_page_captions"] = captions;
			              result["web_page_picture_waiting"] = page.AnyPictureWaiting();
		              }
		              // The temples' pens: each creature's shrunk size there is its Creature component's penSize
		              result["pens"] = Locator::creaturePenSystem::has_value();
		              if (Locator::creatureFizzSystem::has_value())
		              {
			              const auto scroll = Locator::creatureFizzSystem::value().EyeStaticScroll();
			              result["eye_static_scroll"] = {scroll.x, scroll.y};
		              }
		              // The tattoo editor: whether it is open, on which creature, and the tattoo site under the pointer
		              if (Locator::tattooEditorSystem::has_value())
		              {
			              const auto& editor = Locator::tattooEditorSystem::value();
			              result["tattoo_editor"] = {{"open", editor.IsOpen()},
			                                         {"creature", Id(editor.GetCreature())},
			                                         {"from_menu", editor.IsFromMenu()},
			                                         {"shown", Id(editor.GetShown())},
			                                         {"hovered_site", Optional(editor.GetHoveredSite())}};
		              }
		              return QueryResult::Value(std::move(result));
	              });
	provider->Add(Query("status",
	                    "A creature's body systems: moving, what its hands do, its leash, its fight, its animation and "
	                    "its skin",
	                    {IdParameter("The creature's entity id")}),
	              ServeRegistry([](const ecs::Registry& registry, const QueryContext& context) {
		              const auto entity = EntityParam(context.params);
		              if (!entity.has_value() || !registry.AllOf<Creature>(*entity))
		              {
			              return QueryResult::Error("no creature with that id");
		              }
		              const auto creature = *entity;
		              Json result = {{"id", ToId(creature)}};
		              if (Locator::creatureLocomotionSystem::has_value())
		              {
			              result["moving"] = Locator::creatureLocomotionSystem::value().IsMoving(creature);
		              }
		              if (Locator::creatureObjectActionSystem::has_value())
		              {
			              const auto& actions = Locator::creatureObjectActionSystem::value();
			              result["object_action"] = {{"state", static_cast<int>(actions.GetState(creature))},
			                                         {"progress", Optional(actions.GetProgress(creature))},
			                                         {"held", Id(actions.GetHeld(creature))},
			                                         {"catching", actions.IsCatching(creature)}};
		              }
		              if (Locator::leashSystem::has_value())
		              {
			              const auto& leash = Locator::leashSystem::value();
			              result["leash"] = {{"leashed", leash.IsLeashed(creature)},
			                                 {"type", static_cast<int>(leash.TypeOf(creature))},
			                                 {"tied_to", Id(leash.TiedTo(creature))},
			                                 {"order_target", Optional(leash.OrderTarget(creature))}};
		              }
		              if (Locator::creatureFightSystem::has_value())
		              {
			              const auto& fights = Locator::creatureFightSystem::value();
			              result["fight"] = {{"fighting", fights.IsFighting(creature)},
			                                 {"opponent", Id(fights.OpponentOf(creature))},
			                                 {"knocked_out", fights.IsKnockedOut(creature)},
			                                 {"forward_extent", fights.ForwardExtent(creature)}};
		              }
		              if (const auto* animation = registry.TryGet<const CreatureAnimation>(creature); animation != nullptr)
		              {
			              result["animation_system"] = Locator::creatureAnimationSystem::has_value();
		              }
		              if (const auto* skin = registry.TryGet<const CreatureSkin>(creature); skin != nullptr)
		              {
			              result["skin_revision"] = skin->revision;
			              result["skin_system"] = Locator::creatureSkinSystem::has_value();
		              }
		              return QueryResult::Value(std::move(result));
	              }));
	provider->Add(
	    Query("leash",
	          "A creature's leash (the local player's creature by default): the leashes it knows, the one worn and "
	          "picked, whether it works, is tied and to what, its rope (ends, lengths, tension), the area that holds the "
	          "creature and the place it walks back to, the order it carries out, the desire the leash makes rule it, and the "
	          "local "
	          "player's hand on the rope (away, over or holding)",
	          {{.name = "id", .type = "integer", .description = "The creature's entity id"}}),
	    ServeRegistry([](const ecs::Registry& registry, const QueryContext& context) {
		    if (!Locator::leashSystem::has_value())
		    {
			    return QueryResult::Error("no leash system");
		    }
		    const auto& leash = Locator::leashSystem::value();
		    // The god of this computer, whose hand is at the rope
		    const auto local =
		        Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE;
		    auto entity = EntityParam(context.params);
		    if (!context.params.contains("id"))
		    {
			    entity = leash.PlayersCreature(local);
		    }
		    if (!entity.has_value() || !registry.AllOf<Creature>(*entity))
		    {
			    return QueryResult::Error("no creature with that id");
		    }
		    const auto creature = *entity;
		    Json result = {
		        {"id", ToId(creature)},
		        {"rope_hand", static_cast<int>(leash.RopeHand(local))},
		        {"rope_hand_name", std::array {"away", "over", "holding"}.at(static_cast<size_t>(leash.RopeHand(local)))}};
		    const auto* leashes = registry.TryGet<const CreatureLeash>(creature);
		    if (leashes == nullptr)
		    {
			    result["leashed"] = false;
			    return QueryResult::Value(std::move(result));
		    }
		    result["known"] = {leashes->known.test(0), leashes->known.test(1), leashes->known.test(2)};
		    result["selected"] = static_cast<int>(leashes->selected);
		    result["leashed"] = leashes->worn.has_value();
		    result["area"] = {
		        {"centre", {leashes->confinementCentre.x, leashes->confinementCentre.y, leashes->confinementCentre.z}},
		        {"radius", leashes->confinementRadius}};
		    result["return_to"] = leashes->returnTo.has_value()
		                              ? Json {leashes->returnTo->x, leashes->returnTo->y, leashes->returnTo->z}
		                              : Json(nullptr);
		    result["order_target"] = Optional(leash.OrderTarget(creature));
		    if (Locator::creatureMindSystem::has_value())
		    {
			    const auto ruling = Locator::creatureMindSystem::value().DominantDesire(creature);
			    result["ruling_desire"] =
			        ruling.has_value() ? Json(std::string(creature_desires::Name(*ruling))) : Json(nullptr);
		    }
		    if (leashes->worn.has_value())
		    {
			    const auto& worn = *leashes->worn;
			    const auto* drawn = registry.TryGet<const ecs::components::CreatureLeashRope>(creature);
			    const auto rope = drawn != nullptr ? drawn->rope : leash_rope::Rope {};
			    result["type"] = static_cast<int>(worn.type);
			    result["works"] = worn.works;
			    result["tied_to"] = Id(worn.tiedTo);
			    result["held_rope"] = drawn != nullptr && drawn->held.has_value();
			    result["rope"] = {{"start", {rope.start.x, rope.start.y, rope.start.z}},
			                      {"end", {rope.end.x, rope.end.y, rope.end.z}},
			                      {"slack_length", worn.slackLength},
			                      {"max_length", worn.maxLength},
			                      {"tension", rope.tension}};
		    }
		    return QueryResult::Value(std::move(result));
	    }));
	auto fight = Query("fight",
	                   "Starts a fight between two creatures as the scripts and the leash start one, the first making the "
	                   "arena; how it went and both creatures' fight state",
	                   {IdParameter("The creature that starts the fight"),
	                    {.name = "opponent", .type = "integer", .description = "The other creature", .required = true}});
	fight.writes = true;
	provider->Add(std::move(fight), [](const QueryContext& context) {
		const auto* registry = Registry();
		if (registry == nullptr || !Locator::creatureFightSystem::has_value())
		{
			return QueryResult::Error("there are no fights without a land");
		}
		const auto creature = EntityParam(context.params);
		const auto opponentId = context.params.find("opponent");
		const auto opponent = opponentId == context.params.end() ? std::nullopt : FromId(*opponentId);
		if (!creature.has_value() || !opponent.has_value() || !registry->Valid(*opponent) ||
		    !registry->AllOf<Creature>(*creature) || !registry->AllOf<Creature>(*opponent))
		{
			return QueryResult::Error("creatures.fight needs two creatures: id and opponent");
		}
		auto& fights = Locator::creatureFightSystem::value();
		using Start = ecs::systems::CreatureFightSystemInterface::StartResult;
		constexpr std::array<std::string_view, 7> k_Results {"started",  "no_opponent", "busy",     "too_weak",
		                                                     "no_arena", "refused",     "finishing"};
		const auto started = fights.StartFight(*creature, *opponent);
		const auto index = std::min(static_cast<size_t>(started), k_Results.size() - 1);
		if (started != Start::Started)
		{
			return QueryResult::Error("the fight didn't start: " + std::string(k_Results.at(index)));
		}
		return QueryResult::Value({
		    {"result", k_Results.at(index)},
		    {"fighting", {fights.IsFighting(*creature), fights.IsFighting(*opponent)}},
		    {"opponent_of_id", Id(fights.OpponentOf(*creature))},
		});
	});
	auto cheat = Query("fight_cheat",
	                   "Sends the fight cheat of the player of this computer as a command: at the turn, the player's "
	                   "creature fights the nearest creature, or stops the duel that it is in. A game server takes the "
	                   "command only with its test commands on",
	                   {});
	cheat.writes = true;
	provider->Add(std::move(cheat), [](const QueryContext& /*context*/) {
		if (Registry() == nullptr || !Locator::playerCommandSystem::has_value())
		{
			return QueryResult::Error("there are no fights without a land");
		}
		Locator::playerCommandSystem::value().Submit(player_commands::CreatureFightCheat {});
		return QueryResult::Value({{"sent", true}});
	});
	auto move = Query("move",
	                  "Sends a creature walking or running to a point by its own legs (turning and stepping off as it "
	                  "does), or turning on the spot to face it",
	                  {IdParameter("The creature"),
	                   {.name = "point", .type = "array", .description = "[x, z] on the land", .required = true},
	                   {.name = "pace",
	                    .type = "string",
	                    .description = "walk (the default), run, face to turn on the spot towards the point, stop to stop "
	                                   "as the game stops a walk (the point is not used), or stop_at_once to stop dead",
	                    .required = false}});
	move.writes = true;
	provider->Add(std::move(move), [](const QueryContext& context) {
		const auto* registry = Registry();
		if (registry == nullptr || !Locator::creatureLocomotionSystem::has_value())
		{
			return QueryResult::Error("no creatures move without a land");
		}
		const auto creature = EntityParam(context.params);
		const auto point = context.params.find("point");
		if (!creature.has_value() || !registry->AllOf<Creature>(*creature) || point == context.params.end() ||
		    !point->is_array() || point->size() != 2 || !(*point)[0].is_number() || !(*point)[1].is_number())
		{
			return QueryResult::Error("creatures.move needs a creature's id and a point [x, z]");
		}
		const glm::vec2 target((*point)[0].get<float>(), (*point)[1].get<float>());
		const auto paceParam = context.params.find("pace");
		const auto pace =
		    paceParam != context.params.end() && paceParam->is_string() ? paceParam->get<std::string>() : std::string("walk");
		auto& locomotion = Locator::creatureLocomotionSystem::value();
		if (pace == "face")
		{
			return QueryResult::Value({{"result", locomotion.TurnToFace(*creature, target) ? "started" : "refused"}});
		}
		if (pace == "stop" || pace == "stop_at_once")
		{
			if (pace == "stop")
			{
				locomotion.Stop(*creature);
			}
			else
			{
				locomotion.StopAtOnce(*creature);
			}
			return QueryResult::Value({{"result", "stopped"}});
		}
		if (pace != "walk" && pace != "run")
		{
			return QueryResult::Error("pace is walk, run, face, stop or stop_at_once");
		}
		using Locomotion = ecs::systems::CreatureLocomotionSystemInterface;
		constexpr std::array<std::string_view, 3> k_Results {"invalid_destination", "busy", "started"};
		const auto result =
		    locomotion.MoveTo(*creature, target, pace == "run" ? Locomotion::Pace::Run : Locomotion::Pace::Walk, 0.0f, 1.0f);
		return QueryResult::Value({{"result", k_Results.at(static_cast<size_t>(result))}});
	});
	provider->Add(Query("route",
	                    "A creature's walk in full: where it is going, how far short of it it stops and how near it must get, "
	                    "the state of its walk, the route it follows (its legs, straight or round a circle, and points "
	                    "along it) and the circles it walks round (each grown by its own reach)",
	                    {IdParameter("The creature")}),
	              [](const QueryContext& context) {
		              const auto* registry = Registry();
		              const auto creature = EntityParam(context.params);
		              if (registry == nullptr || !creature.has_value())
		              {
			              return QueryResult::Error("creatures.route needs a creature's id");
		              }
		              const auto* locomotion = registry->TryGet<const CreatureLocomotion>(*creature);
		              if (locomotion == nullptr)
		              {
			              return QueryResult::Error("that isn't a creature that walks");
		              }
		              const auto& route = locomotion->follower.GetRoute();
		              Json points = Json::array();
		              for (const auto& point : route.Points(glm::radians(5.0f)))
		              {
			              points.push_back({point.x, point.y});
		              }
		              Json legs = Json::array();
		              for (const auto& leg : route.legs)
		              {
			              Json item {{"from", {leg.from.x, leg.from.y}}, {"to", {leg.to.x, leg.to.y}}, {"arc", leg.arc}};
			              if (leg.round.has_value())
			              {
				              item["centre"] = {leg.round->centre.x, leg.round->centre.y};
				              item["radius"] = leg.round->radius;
				              item["clockwise"] = leg.roundWay == creature_route::Way::Clockwise;
			              }
			              legs.push_back(std::move(item));
		              }
		              Json circles = Json::array();
		              for (const auto& obstacle : locomotion->follower.GetObstacles().All())
		              {
			              if (obstacle.active)
			              {
				              circles.push_back({{"centre", {obstacle.circle.centre.x, obstacle.circle.centre.y}},
				                                 {"radius", obstacle.circle.radius},
				                                 {"thing", Optional(obstacle.thing)}});
			              }
		              }
		              return QueryResult::Value({{"destination", Optional(locomotion->destination)},
		                                         {"motion", static_cast<int>(locomotion->motion)},
		                                         {"radius", locomotion->radius},
		                                         {"stop_short", locomotion->ring.min},
		                                         {"arrive_within", locomotion->ring.max},
		                                         {"failed", locomotion->failed},
		                                         {"state", static_cast<int>(locomotion->follower.GetState())},
		                                         {"segment", route.segment},
		                                         {"legs", std::move(legs)},
		                                         {"points", std::move(points)},
		                                         {"circles", std::move(circles)}});
	              });
	return provider;
}

// The scripts

std::unique_ptr<ProviderInterface> ScriptProvider(ScriptTargetInterface& scripts)
{
	auto provider = std::make_unique<FunctionProvider>("script");
	provider->Add(Query("vm", "The script machine: its scripts, tasks, globals and the task running"),
	              Serve<Locator::vm>("the script machine", [](const lhvm::LHVM& vm, const QueryContext& /*c*/) {
		              return Json {{"scripts", vm.GetScripts().size()},
		                           {"tasks", vm.GetTasks().size()},
		                           {"variables", vm.GetVariables().size()},
		                           {"instructions", vm.GetInstructions().size()},
		                           {"current_task", vm.GetCurrentTaskNumber()}};
	              }));
	provider->Add(
	    Query("tasks",
	          "The script tasks running: their scripts, where they are, whether they sleep and how many values are on their "
	          "stack; with a name, only the tasks whose names hold it (any case), each with its local variables",
	          {{.name = "name",
	            .type = "string",
	            .description = "Part of the task's name: only these tasks, with their local variables",
	            .required = false}},
	          ResultKind::List),
	    Serve<Locator::vm>("the script machine", [](const lhvm::LHVM& vm, const QueryContext& context) {
		    const auto upper = [](std::string text) {
			    std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
			    return text;
		    };
		    const bool filtered = context.params.is_object() && context.params.contains("name");
		    const auto filter = filtered ? upper(context.params.value("name", std::string())) : std::string();
		    Json items = Json::array();
		    for (const auto& [number, task] : vm.GetTasks())
		    {
			    if (filtered && upper(task.name).find(filter) == std::string::npos)
			    {
				    continue;
			    }
			    Json item {{"id", number},
			               {"name", task.name},
			               {"script", task.scriptId},
			               {"address", task.instructionAddress},
			               {"sleeping", task.sleeping},
			               {"waiting_for", task.waitingTaskId},
			               {"stack", task.stack.count},
			               {"held", vm.IsTaskHeld(number)}};
			    if (filtered)
			    {
				    // Each local variable by its type: a number, a truth, or the object's number in the script table
				    Json locals = Json::array();
				    for (const auto& variable : task.localVars)
				    {
					    Json value;
					    switch (variable.type)
					    {
					    case lhvm::DataType::Int:
						    value = variable.value.intVal;
						    break;
					    case lhvm::DataType::Object:
						    value = variable.value.uintVal;
						    break;
					    case lhvm::DataType::Boolean:
						    value = variable.value.floatVal != 0.0f;
						    break;
					    default:
						    value = variable.value.floatVal;
						    break;
					    }
					    locals.push_back({{"name", variable.name},
					                      {"type", static_cast<uint32_t>(variable.type)},
					                      {"value", std::move(value)}});
				    }
				    item["locals"] = std::move(locals);
			    }
			    items.push_back(std::move(item));
		    }
		    return items;
	    }));
	provider->Add(
	    Query("natives", "The script natives called that openblack hasn't written, most called first", {}, ResultKind::List),
	    Serve<Locator::chlapi>("the script natives", [](const chlapi::CHLApi& api, const QueryContext& /*c*/) {
		    auto entries = api.GetStubCalls().Entries();
		    std::ranges::stable_sort(entries, [](const auto& a, const auto& b) { return a.calls > b.calls; });
		    Json items = Json::array();
		    for (const auto& entry : entries)
		    {
			    items.push_back({{"native", entry.native},
			                     {"detail", entry.detail.has_value() ? Json(*entry.detail) : Json(nullptr)},
			                     {"calls", entry.calls}});
		    }
		    return items;
	    }));
	provider->Add(
	    Query("objects",
	          "How many things scripts hold and control, and the scripts' object table: places taken (of 511), times it "
	          "was full, and its places by kind (taken, referenced, made by a script, object gone); places: true lists "
	          "every taken place",
	          {{.name = "places", .type = "boolean", .description = "List every taken place", .required = false}}),
	    ServeRegistry([](const ecs::Registry& registry, const QueryContext& context) {
		    Json result {{"in_script", CountOf<InScript>(registry)},
		                 {"script_controlled", CountOf<ScriptControlled>(registry)},
		                 {"system", Locator::scriptObjects::has_value()}};
		    if (!Locator::scriptObjects::has_value())
		    {
			    return result;
		    }
		    const auto& scripts = Locator::scriptObjects::value();
		    const auto places = scripts.Places();
		    const bool listed = context.params.is_object() && context.params.value("places", false);
		    Json byKind = Json::object();
		    Json list = Json::array();
		    for (const auto& place : places)
		    {
			    const bool gone = place.object == entt::null || !registry.Valid(place.object);
			    const auto kind = gone ? std::string("gone") : Describe(registry, place.object, Info()).kind;
			    auto& counts = byKind[kind];
			    if (counts.is_null())
			    {
				    counts = {{"places", 0}, {"referenced", 0}, {"made_by_script", 0}};
			    }
			    counts["places"] = counts["places"].get<int>() + 1;
			    counts["referenced"] = counts["referenced"].get<int>() + (place.references != 0 ? 1 : 0);
			    counts["made_by_script"] = counts["made_by_script"].get<int>() + (place.createdByScript ? 1 : 0);
			    if (listed)
			    {
				    list.push_back({{"place", place.place},
				                    {"id", gone ? Json(nullptr) : Json(ToId(place.object))},
				                    {"kind", kind},
				                    {"references", place.references},
				                    {"made_by_script", place.createdByScript}});
			    }
		    }
		    result["places_taken"] = places.size();
		    result["times_full"] = scripts.TimesFull();
		    result["by_kind"] = std::move(byKind);
		    if (listed)
		    {
			    result["places"] = std::move(list);
		    }
		    return result;
	    }));
	provider->Add(
	    Query("highlights",
	          "The scrolls and signs scripts put up: kind, challenge or text, started, height, where drawn; the beat and "
	          "the switch that shows the scrolls",
	          {}, ResultKind::List),
	    Serve<Locator::scriptHighlightSystem>(
	        "the script highlights",
	        [](const ecs::systems::ScriptHighlightSystemInterface& highlights, const QueryContext& /*c*/) {
		        Json items = Json::array();
		        const auto* registry = Registry();
		        if (registry == nullptr)
		        {
			        return items;
		        }
		        const bool scrollsDrawn = !Locator::chlapi::has_value() || Locator::chlapi::value().IsHighlightDrawOn();
		        const auto& pulse = highlights.GetPulse();
		        registry->Each<const ScriptHighlight>([&](entt::entity entity, const ScriptHighlight& highlight) {
			        auto item = Listed(*registry, entity);
			        item["kind"] = static_cast<int>(highlight.kind);
			        item["script_id"] = highlight.scriptId;
			        item["category"] = highlight.category;
			        item["active"] = highlight.active;
			        item["draw_height"] = highlight.drawHeight.has_value() ? Json(*highlight.drawHeight) : Json(nullptr);
			        item["height_above"] = highlight.heightAbove;
			        item["y_angle"] = highlight.yAngle;
			        item["centre"] = Point(highlight.centre);
			        item["radius"] = highlight.radius;
			        item["glints"] = highlight.glints;
			        item["active_effect"] = highlight.activeEffect;
			        item["hidden"] = highlight.hidden;
			        item["scrolls_drawn"] = scrollsDrawn;
			        item["pulse"] = pulse.level;
			        items.push_back(std::move(item));
		        });
		        return items;
	        }));
	AddScriptControls(*provider, scripts);
	return provider;
}

/// The physics' bodies as the inspector reports them
std::optional<std::vector<BodyInfo>> Bodies()
{
	if (!Locator::dynamicsSystem::has_value())
	{
		return std::nullopt;
	}
	std::vector<BodyInfo> bodies;
	Locator::dynamicsSystem::value().ForEachEntry([&bodies](const ecs::PhysicsEntry& entry) {
		BodyInfo info {.entity = entry.entity,
		               .kind = std::string(KindName(entry.kind)),
		               .flags = entry.flags,
		               .player = entry.player.has_value() ? std::optional<int>(static_cast<int>(*entry.player)) : std::nullopt,
		               .thrower = entry.thrower,
		               .impact = entry.impact};
		if (entry.body != nullptr)
		{
			const auto& body = *entry.body;
			info.centre = body.Centre();
			info.velocity = body.velocity;
			info.speed = body.Speed();
			info.mass = body.Mass();
			info.radius = body.Radius();
			info.resting = body.resting;
			info.inWater = body.inWater;
			info.contacts = body.Contacts();
		}
		bodies.push_back(std::move(info));
	});
	return bodies;
}

// The help: the advisors and the scripts' dialogue

/// The number of the open land, which the saved games use for their land folders
uint32_t OpenLand()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return 0;
	}
	return static_cast<uint32_t>(std::max(Locator::entitiesRegistry::value().Context().mapScriptGlobals.landNumber, 0));
}

std::unique_ptr<ProviderInterface> SavesProvider()
{
	auto provider = std::make_unique<FunctionProvider>("saves");
	provider->Add(Query("slots", "The slots of the current player that have a saved game, for the land that is open", {},
	                    ResultKind::List),
	              [](const QueryContext& /*c*/) {
		              if (!Locator::saveGameSystem::has_value())
		              {
			              return QueryResult::Error("there are no saved games without a game");
		              }
		              Json items = Json::array();
		              for (const auto& slot : Locator::saveGameSystem::value().Slots())
		              {
			              items.push_back({{"slot", slot.slot},
			                               {"land", slot.land},
			                               {"date", gui::ToUtf8(slot.date)},
			                               {"name", gui::ToUtf8(slot.name)}});
		              }
		              return QueryResult::Value(std::move(items));
	              });
	const ParameterDescription slotParameter {
	    .name = "slot", .type = "integer", .description = "The slot, 0 to 20", .required = true};
	auto save =
	    Query("save",
	          "Saves the game into a slot: the whole world (every entity and its components, the clock, the random "
	          "numbers, the camera and the scripts) and every creature with its mind file and its leash. The slots 14 to 18 "
	          "save into a folder of the open land in the folder of the slot. The result gives the folder of the world "
	          "file in the folder of the player",
	          {slotParameter, {.name = "name", .type = "string", .description = "The name of the save", .required = false}});
	save.writes = true;
	provider->Add(std::move(save), [](const QueryContext& context) {
		const auto slot = NumberMember(context.params, "slot");
		if (!Locator::saveGameSystem::has_value() || !slot.has_value() || *slot < 0.0)
		{
			return QueryResult::Error("saves.save needs a game and a slot from 0 to 20");
		}
		const auto name = gui::ToUtf16(StringMember(context.params, "name").value_or(""));
		const auto number = static_cast<uint32_t>(*slot);
		const bool saved = Locator::saveGameSystem::value().Save(number, name);
		return QueryResult::Value(
		    {{"saved", saved}, {"slot", number}, {"folder", gamesave::SlotFolder(number, OpenLand()).generic_string()}});
	});
	auto load = Query("load",
	                  "Loads a slot. A save of the whole world loads its land and its world in place of the open land. An "
	                  "older save puts the creatures of the land that is open back: the values of each record, the mind of "
	                  "its mind file, the plan that the save kept and its leash. Slot 14 (the previous land) loads from the "
	                  "folder of the land before the open land, and slot 18 (the next land) from the folder of the land "
	                  "after it, as the save game room does. The result gives the folder that the load read",
	                  {slotParameter});
	load.writes = true;
	provider->Add(std::move(load), [](const QueryContext& context) {
		const auto slot = NumberMember(context.params, "slot");
		if (!Locator::saveGameSystem::has_value() || !slot.has_value() || *slot < 0.0)
		{
			return QueryResult::Error("saves.load needs a game and a slot from 0 to 20");
		}
		const auto number = static_cast<uint32_t>(*slot);
		// Find the folder before the load, because a load can open another land
		const auto folder = gamesave::LoadSlotFolder(number, OpenLand()).generic_string();
		const bool loaded = Locator::saveGameSystem::value().Load(number);
		return QueryResult::Value({{"loaded", loaded}, {"slot", number}, {"folder", folder}});
	});
	auto loadAndSave =
	    Query("load_and_save",
	          "Loads a slot and then saves the game into a second slot in the same call, with no frame and no turn between. A "
	          "save of a loaded world must have the same bytes as the save that the game loaded (tools/sim/save_diff.py "
	          "compares the two files)",
	          {slotParameter,
	           {.name = "into", .type = "integer", .description = "The slot of the second save, 0 to 20", .required = true},
	           {.name = "name", .type = "string", .description = "The name of the second save", .required = false}});
	loadAndSave.writes = true;
	provider->Add(std::move(loadAndSave), [](const QueryContext& context) {
		const auto slot = NumberMember(context.params, "slot");
		const auto into = NumberMember(context.params, "into");
		if (!Locator::saveGameSystem::has_value() || !slot.has_value() || *slot < 0.0 || !into.has_value() || *into < 0.0)
		{
			return QueryResult::Error("saves.load_and_save needs a game, a slot and an into slot from 0 to 20");
		}
		auto& saves = Locator::saveGameSystem::value();
		const bool loaded = saves.Load(static_cast<uint32_t>(*slot));
		const auto name = gui::ToUtf16(StringMember(context.params, "name").value_or(""));
		const bool saved = loaded && saves.Save(static_cast<uint32_t>(*into), name);
		return QueryResult::Value({{"loaded", loaded},
		                           {"saved", saved},
		                           {"slot", static_cast<uint32_t>(*slot)},
		                           {"into", static_cast<uint32_t>(*into)}});
	});
	auto worldStateRoundTrip =
	    Query("world_state_round_trip",
	          "Makes the world state that a game server sends, loads it at once and makes it again, with no frame and no turn "
	          "between. Writes the two states to the files first and again. The two must have the same bytes "
	          "(tools/sim/save_diff.py compares the two files)",
	          {{.name = "first", .type = "string", .description = "The file of the first state", .required = true},
	           {.name = "again", .type = "string", .description = "The file of the state after the load", .required = true}});
	worldStateRoundTrip.writes = true;
	provider->Add(std::move(worldStateRoundTrip), [](const QueryContext& context) {
		const auto firstPath = StringMember(context.params, "first");
		const auto againPath = StringMember(context.params, "again");
		if (!Locator::saveGameSystem::has_value() || !Locator::filesystem::has_value() || !firstPath.has_value() ||
		    !againPath.has_value())
		{
			return QueryResult::Error("saves.world_state_round_trip needs a game and the two files");
		}
		auto& saves = Locator::saveGameSystem::value();
		auto& files = Locator::filesystem::value();
		const auto first = saves.WorldState();
		const bool loaded = !first.empty() && saves.LoadWorldState(first);
		const auto again = loaded ? saves.WorldState() : std::vector<uint8_t> {};
		const bool written = files.WriteAll(std::filesystem::path(*firstPath), first) &&
		                     (!loaded || files.WriteAll(std::filesystem::path(*againPath), again));
		return QueryResult::Value({{"loaded", loaded},
		                           {"written", written},
		                           {"same", loaded && first == again},
		                           {"bytes", {first.size(), again.size()}}});
	});
	provider->Add(Query("vanilla_check",
	                    "After a load of a slot of the game itself: each creature of the game's file, and whether the record "
	                    "that a save writes for it now has the same bytes as the file's record",
	                    {}, ResultKind::List),
	              [](const QueryContext& /*c*/) {
		              if (!Locator::saveGameSystem::has_value())
		              {
			              return QueryResult::Error("there are no saved games without a game");
		              }
		              Json items = Json::array();
		              for (const auto& check : Locator::saveGameSystem::value().CheckVanillaRecords())
		              {
			              Json item {
			                  {"number", check.number}, {"entity", check.entity}, {"same", check.same}, {"size", check.size}};
			              if (check.firstDifference.has_value())
			              {
				              item["firstDifference"] = *check.firstDifference;
			              }
			              items.push_back(std::move(item));
		              }
		              return QueryResult::Value(std::move(items));
	              });
	return provider;
}

std::unique_ptr<ProviderInterface> HelpProvider()
{
	auto provider = std::make_unique<FunctionProvider>("help");
	provider->Add(
	    Query("advisors",
	          "The good and the evil advisor: what each does, where it hovers, whether it is talking and its mouth's lip sync",
	          {}, ResultKind::List),
	    Serve<Locator::advisorSystem>(
	        "the advisors", [](const ecs::systems::AdvisorSystemInterface& advisors, const QueryContext& /*c*/) -> Json {
		        if (!advisors.IsLoaded())
		        {
			        return Json::array();
		        }
		        const auto& controller = advisors.GetController();
		        const auto* voices =
		            Locator::advisorVoiceSystem::has_value() ? &Locator::advisorVoiceSystem::value().GetVoices() : nullptr;
		        Json items = Json::array();
		        for (int dude = 0; dude < 2; ++dude)
		        {
			        const auto& spirit = controller.Dude(dude);
			        // The mouth: the lip sync's three weights and the mouth clips posed this frame (clip, ms)
			        Json shapes = Json::array();
			        for (const auto& layer : spirit.Layers())
			        {
				        if (layer.kind == help::spirits::AnimLayer::Kind::Add && layer.clip >= help::spirits::anim::k_VowelE &&
				            layer.clip < help::spirits::anim::k_VowelE + 3)
				        {
					        shapes.push_back({{"clip", layer.clip}, {"ms", layer.milliseconds}});
				        }
			        }
			        Json weights = Json::array();
			        if (voices != nullptr)
			        {
				        for (const float w : voices->GetLipSyncKey(dude).weights)
				        {
					        // Not a number after a window of pure silence, as in the game
					        weights.push_back(std::isfinite(w) ? Json(w) : Json(nullptr));
				        }
			        }
			        items.push_back({{"advisor", dude == 0 ? "good" : "evil"},
			                         {"mouth", {{"weights", weights}, {"shapes", shapes}}},
			                         {"tags_left", spirit.TagsLeft()},
			                         {"control_state", static_cast<int>(controller.State(dude))},
			                         {"state", spirit.State()},
			                         {"hover", Point(spirit.Hover())},
			                         {"position", Point(spirit.Position())},
			                         {"alpha", spirit.Alpha()},
			                         {"playing_anim", spirit.IsPlayingAnim()},
			                         {"given_line", voices != nullptr && voices->IsActive(dude)},
			                         {"speaking", voices != nullptr && voices->GetSpeaker() == dude}});
		        }
		        return items;
	        }));
	provider->Add(
	    Query("dialogue", "The scripts' dialogue: the text shown, whether it is read or waits for a click, who holds the "
	                      "dialogue and the line being said"),
	    Serve<Locator::helpTextSystem>(
	        "the dialogue", [](const ecs::systems::HelpTextSystemInterface& help, const QueryContext& /*c*/) -> Json {
		        // A computer that shows nothing plays no voices: no speaker and no line
		        const auto* voices =
		            Locator::advisorVoiceSystem::has_value() ? &Locator::advisorVoiceSystem::value().GetVoices() : nullptr;
		        Json result {{"started", help.IsStarted()},
		                     {"text_read", help.IsTextRead()},
		                     {"speaker", voices != nullptr ? voices->GetSpeaker() : -1},
		                     {"sentence", voices != nullptr ? voices->GetSentence() : 0u},
		                     {"owner", Locator::dialogueControlSystem::has_value()
		                                   ? Json(Locator::dialogueControlSystem::value().GetOwner())
		                                   : Json(nullptr)},
		                     {"speech_banks", Locator::helpSpeechSystem::has_value()
		                                          ? Json(Locator::helpSpeechSystem::value().GetTable().GetBankCount(
		                                                audio::SpeechBank::HelpSprites))
		                                          : Json(nullptr)}};
		        if (const auto* dialogue = help.GetDialogue(); dialogue != nullptr)
		        {
			        result["text"] = dialogue->GetCurrentText();
			        result["narrator"] = help.GetNarrator(dialogue->GetCurrentText());
			        result["waiting_for_click"] = dialogue->IsWaitingForClick();
			        result["end_turn"] = dialogue->GetEndTurn();
			        result["end_ms"] = dialogue->GetEndMs();
			        result["start_turn"] = dialogue->GetStartTurn();
			        result["takes_click"] = dialogue->TakesClick();
			        result["last_click_turn"] = dialogue->GetLastClickTurn();
			        result["drawn"] = dialogue->IsDrawn();
			        result["box_shown"] = dialogue->GetDisplay().IsBoxShown();
		        }
		        return result;
	        }));
	provider->Add(
	    Query("profile",
	          "The help's count of what the player has done: its clock, and each event's total, seconds since "
	          "and rate, for the events that have happened",
	          {}, ResultKind::List),
	    Serve<Locator::helpProfileSystem>(
	        "the help profile", [](const ecs::systems::HelpProfileSystemInterface& system, const QueryContext& /*c*/) -> Json {
		        const auto& profile = system.Get();
		        Json items = Json::array();
		        for (uint32_t event = 0; event < help::profile::k_EventCount; ++event)
		        {
			        // A copy: reading the times may tidy them, and a query changes nothing
			        auto count = profile.Count(event);
			        if (count.Total() == 0)
			        {
				        continue;
			        }
			        items.push_back({{"event", event},
			                         {"clock", profile.Clock()},
			                         {"total", count.Total()},
			                         {"seconds_since", count.SecondsSince(profile.Clock())},
			                         {"per_second", count.PerSecond(profile.Clock())},
			                         {"smoothed_rate", count.SmoothedRate()}});
		        }
		        return items;
	        }));
	provider->Add(Query("creature",
	                    "The advisor's messages about the player's creature: whether they are on, the ones told lately "
	                    "(kind, parameter, text, turn), and the creature's waiting ones and prompts",
	                    {}, ResultKind::Object),
	              Serve<Locator::creatureHelpSystem>(
	                  "the creature help",
	                  [](const ecs::systems::CreatureHelpSystemInterface& system, const QueryContext& /*c*/) -> Json {
		                  Json told = Json::array();
		                  for (const auto& message : system.GetTold())
		                  {
			                  told.push_back({{"creature", entt::to_integral(message.creature)},
			                                  {"kind", static_cast<int>(message.kind)},
			                                  {"parameter", message.parameter},
			                                  {"text", message.text.has_value() ? Json(*message.text) : Json(nullptr)},
			                                  {"turn", message.turn}});
		                  }
		                  Json creatures = Json::array();
		                  const auto& registry = Locator::entitiesRegistry::value();
		                  registry.Each<const CreatureHelpMemory>(
		                      [&creatures](entt::entity entity, const CreatureHelpMemory& help) {
			                      Json waiting = Json::array();
			                      for (size_t kind = 0; kind < help.memory.entries.size(); ++kind)
			                      {
				                      for (const auto& entry : help.memory.entries.at(kind))
				                      {
					                      if (entry.waiting)
					                      {
						                      waiting.push_back({{"kind", kind}, {"parameter", entry.parameter}});
					                      }
				                      }
			                      }
			                      creatures.push_back({{"creature", entt::to_integral(entity)},
			                                           {"waiting", waiting},
			                                           {"hand_over", help.memory.handOver},
			                                           {"body_problem_turn", help.memory.physicalProblemTurn},
			                                           {"tutorial_turn", help.memory.tutorialTurn},
			                                           {"tutorial_done", help.memory.tutorialDone}});
		                      });
		                  return Json {{"on", system.IsHelpOn()}, {"told", told}, {"creatures", creatures}};
	                  }));
	provider->Add(
	    Query("messages",
	          "The help the game starts by itself: the help scripts it started lately (script, texts, a "
	          "town's need) and how many times each message set has been said",
	          {}, ResultKind::Object),
	    Serve<Locator::helpMessageSystem>(
	        "the help messages", [](const ecs::systems::HelpMessageSystemInterface& system, const QueryContext& /*c*/) -> Json {
		        Json started = Json::array();
		        for (const auto& message : system.GetStarted())
		        {
			        started.push_back({{"script", message.script},
			                           {"first", message.first},
			                           {"last", message.last},
			                           {"stat_text", message.statText.has_value() ? Json(*message.statText) : Json(nullptr)},
			                           {"need", message.need.has_value() ? Json(*message.need) : Json(nullptr)},
			                           {"turn", message.turn}});
		        }
		        Json sent = Json::object();
		        const auto& counts = system.GetSentCounts();
		        for (size_t set = 0; set < counts.size(); ++set)
		        {
			        if (counts.at(set) != 0)
			        {
				        sent[std::to_string(set)] = counts.at(set);
			        }
		        }
		        return Json {{"started", started}, {"sets_said", sent}};
	        }));
	provider->Add(
	    Query("guidance",
	          "The advisors' remarks on what the player does (towns attacked, buildings wrecked, alignment drifting, the "
	          "moon): the ones asked for lately (kind, text, turn, whether the advisors got to say it), the turns until "
	          "the moon is looked at again, and the player's guidance (each kind's last turn, the alignment's drift, the "
	          "one-offs said)",
	          {}, ResultKind::Object),
	    Serve<Locator::guidanceSystem>(
	        "the guidance", [](const ecs::systems::GuidanceSystemInterface& system, const QueryContext& /*c*/) -> Json {
		        Json remarks = Json::array();
		        for (const auto& remark : system.GetRemarks())
		        {
			        remarks.push_back({{"kind", static_cast<int>(remark.kind)},
			                           {"text", remark.text},
			                           {"turn", remark.turn},
			                           {"started", remark.started}});
		        }
		        Json heard = nullptr;
		        if (Locator::entitiesRegistry::has_value())
		        {
			        Locator::entitiesRegistry::value().Each<const ecs::components::GuidanceHeard>(
			            [&heard](entt::entity entity, const ecs::components::GuidanceHeard& guidance) {
				            heard = {{"player", entt::to_integral(entity)},
				                     {"last_turns", guidance.lastTurn},
				                     {"alignment_drift", guidance.alignmentDrift},
				                     {"one_offs_said", guidance.oneOffSaid}};
			            });
		        }
		        return Json {{"remarks", remarks}, {"moon_wait", system.GetMoonWait()}, {"heard", heard}};
	        }));
	provider->Add(
	    Query("bubble", "The tip bubble over the \"did you know\" sign tapped last: the sign, its tip, whether it is up, "
	                    "its time to show, the point it points at, its scroll and whether the hand is on it or grips it"),
	    Serve<Locator::tipBubbleSystem>(
	        "the tip bubble", [](const ecs::systems::TipBubbleSystemInterface& bubble, const QueryContext& /*c*/) -> Json {
		        const auto& scroll = bubble.GetScroll();
		        const auto anchor = bubble.GetAnchor();
		        return Json {
		            {"sign", bubble.GetSign() == entt::null ? Json(nullptr) : Json(entt::to_integral(bubble.GetSign()))},
		            {"text", bubble.GetText()},
		            {"up", bubble.IsUp()},
		            {"display_time", bubble.GetDisplayTime()},
		            {"anchor", anchor.has_value() ? Point(*anchor) : Json(nullptr)},
		            {"scroll_lines", scroll.lines},
		            {"content_height", scroll.contentHeight},
		            {"line_height", scroll.lineHeight},
		            {"more_above", scroll.moreAbove},
		            {"more_below", scroll.moreBelow},
		            {"under_hand", bubble.IsUnderHand()},
		            {"gripped", bubble.IsGripped()}};
	        }));
	provider->Add(Query("hand_demo",
	                    "The tutorial's hand demonstration playing: its name, the script task that started it, the record "
	                    "the turns are at, whether it holds at a mark, the record the frames are at, and the camera and "
	                    "hints it has"),
	              Serve<Locator::handDemoSystem>(
	                  "the hand demonstrations",
	                  [](const ecs::systems::HandDemoSystemInterface& demos, const QueryContext& /*c*/) -> Json {
		                  const auto status = demos.GetStatus();
		                  if (!status.has_value())
		                  {
			                  return Json {{"playing", false}};
		                  }
		                  Json result {{"playing", true},
		                               {"name", status->name},
		                               {"task", status->task},
		                               {"record", status->record},
		                               {"records", status->records},
		                               {"pause_on_trigger", status->pauseOnTrigger},
		                               {"trigger_reached", status->triggerReached},
		                               {"holding", status->holding},
		                               {"replaying", status->replaying},
		                               {"replay_record", status->replayRecord},
		                               {"replay_holding", status->replayHolding},
		                               {"hints", demos.GetHints()}};
		                  if (const auto camera = demos.GetCamera(); camera.has_value())
		                  {
			                  result["camera_origin"] = Point(camera->origin);
			                  result["camera_focus"] = Point(camera->focus);
		                  }
		                  return result;
	                  }));
	return provider;
}

/// Why the game ends an order to go to a thing at once, if it does: a cut scene of a script makes a creature that is free
/// to think for itself look at the camera, again and again, so it drops the order on the next turn
std::optional<std::string> CutSceneRefusal(const ecs::Registry& registry, entt::entity creature)
{
	if (registry.AllOf<ScriptControlled>(creature) || !Locator::cinematicDirectorSystem::has_value())
	{
		return std::nullopt;
	}
	const auto& director = Locator::cinematicDirectorSystem::value();
	const bool barsIn = director.IsWideScreenOn();
	if (!creature_mind::StartsLookAtCameraInCutScene(barsIn, barsIn ? director.GetWideScreenOwner() : 0u, false))
	{
		return std::nullopt;
	}
	return "a cut scene is playing: the game makes a creature that is free to think for itself look at the camera "
	       "again on each turn, so it would drop the order at once. Give the order when the cut scene is over";
}

/// What can end an order to go to a thing after the creature took it. The creature sees a thing that is near; for one
/// that is farther it walks to where it believes the thing is, and it gives up a walk in which it has not moved for 50
/// turns, as it does while a long route is still searched.
std::string FarThingWarning(const ecs::Registry& registry, entt::entity creature, entt::entity object)
{
	const auto* from = registry.TryGet<const Transform>(creature);
	const auto* to = registry.TryGet<const Transform>(object);
	if (from == nullptr || to == nullptr)
	{
		return {};
	}
	const auto distance = gutils::GetDistanceInMetres(from->position, to->position);
	if (distance < creature_mind::k_SeenWithinMetres)
	{
		return {};
	}
	return fmt::format("the thing is {:.0f} m away: the creature may not see it and then walks to where it believes it is. It "
	                   "gives up the order when it has not moved for {} turns, which a long route search can cause. Place "
	                   "the creature near the thing first (edit.move) to be sure",
	                   distance, creature_cast_moves::k_StallTurns);
}

/// The inspector's order to a creature, given as the game gives its own orders: a forced plan of the creature's mind,
/// which its body does
CreatureOrderResult GiveCreatureOrder(entt::entity creature, const CreatureOrder& order)
{
	if (!Locator::creatureMindSystem::has_value() || !Locator::creatureObjectActionSystem::has_value())
	{
		return {.detail = "the game has no creature mind or hands"};
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body == nullptr)
	{
		return {.detail = "that is not a creature"};
	}
	auto& minds = Locator::creatureMindSystem::value();
	const auto& hands = Locator::creatureObjectActionSystem::value();
	using ForcedPlan = ecs::systems::CreatureMindSystemInterface::ForcedPlan;
	using Kind = CreatureOrder::Kind;
	const auto carried = hands.GetCarried(creature);
	switch (order.kind)
	{
	case Kind::PickUp:
	{
		if (!order.object.has_value() || !registry.Valid(*order.object))
		{
			return {.detail = "the thing is gone"};
		}
		if (carried == order.object)
		{
			return {.detail = "it holds that thing already"};
		}
		if (const auto refusal = CutSceneRefusal(registry, creature); refusal.has_value())
		{
			return {.detail = *refusal};
		}
		if (!hands.CanPickUp(creature, *order.object))
		{
			// Name the size or the weight when that is what stops it
			const auto lift = hands.LiftFactsOf(creature, *order.object);
			if (lift.has_value() && lift->thingRadius + lift->creatureRadius > lift->reach)
			{
				return {.detail = fmt::format("it cannot pick that thing up: its radius {:.2f} and the creature's "
				                              "radius {:.2f} are more than the reach {:.2f}",
				                              lift->thingRadius, lift->creatureRadius, lift->reach)};
			}
			if (lift.has_value() && lift->thingWeight > lift->creatureWeight * creature_object_actions::k_MostLiftedWeightShare)
			{
				return {.detail = fmt::format("it cannot pick that thing up: its weight {:.2f} is more than the "
				                              "creature can lift ({:.2f})",
				                              lift->thingWeight,
				                              lift->creatureWeight * creature_object_actions::k_MostLiftedWeightShare)};
			}
			return {.detail = "it cannot pick that thing up"};
		}
		// As a leash order on a thing it can lift when its desires choose nothing else
		const bool taken = minds.ForcePlan(creature, ForcedPlan {.desire = creature_desires::Desire::Curiosity,
		                                                         .action = creature_leash_orders::k_HoldAction,
		                                                         .object = order.object,
		                                                         .ordered = true});
		return {.taken = taken,
		        .detail = taken ? "picks it up and holds it" : "its mind refused the plan",
		        .warning = taken ? FarThingWarning(registry, creature, *order.object) : std::string()};
	}
	case Kind::PutDown:
	case Kind::Throw:
	{
		if (!carried.has_value())
		{
			return {.detail = "it carries nothing"};
		}
		if (!order.point.has_value())
		{
			const bool taken = minds.ForcePlan(
			    creature, ForcedPlan {.desire = creature_desires::Desire::ObeyPlayer, .action = "PutDown", .ordered = true});
			return {.taken = taken, .detail = taken ? "puts it down where it stands" : "its mind refused the plan"};
		}
		// As a leash order on the land while it carries something: it throws the thing there when it cannot stand at
		// the point, or on the aggression leash
		const auto height = creature_morph::k_HeightAtSizeOne * ShownSize(*body);
		const bool standable = !Locator::creatureLocomotionSystem::has_value() ||
		                       Locator::creatureLocomotionSystem::value().GetWalkableLand().IsValid(
		                           *order.point, creature_route::k_DestinationClearance);
		const auto leash = Locator::leashSystem::has_value() ? Locator::leashSystem::value().TypeOf(creature) : LeashType::None;
		const auto ground =
		    creature_leash_orders::OnGround(*order.point, {.carrying = true,
		                                                   .placeReachable = standable,
		                                                   .leash = leash == LeashType::None ? LeashType::Rope : leash,
		                                                   .height = height});
		const bool throws = order.kind == Kind::Throw || ground.kind == creature_leash_orders::GroundOrder::Kind::ThrowAt;
		const bool taken =
		    minds.ForcePlan(creature, ForcedPlan {.desire = creature_desires::Desire::ObeyPlayer,
		                                          .action = creature_leash_orders::k_MoveAction,
		                                          .agenda = throws ? creature_leash_orders::ThrowAt(*order.point, height)
		                                                           : creature_leash_orders::PutDownAt(*order.point, height),
		                                          .ordered = true,
		                                          .walksBack = true});
		return {.taken = taken,
		        .detail = !taken   ? "its mind refused the plan"
		                  : throws ? "carries it near the point and throws it there"
		                           : "carries it to the point and puts it down"};
	}
	case Kind::Eat:
	{
		if (!order.object.has_value() || !registry.Valid(*order.object))
		{
			return {.detail = "the thing is gone"};
		}
		const auto* executor = creature_plan_actions::For(order.action);
		if (executor == nullptr || executor->activity != creature_mind::Activity::Eat)
		{
			return {.detail = "no eat action is called " + order.action};
		}
		if (const auto refusal = CutSceneRefusal(registry, creature); refusal.has_value())
		{
			return {.detail = *refusal};
		}
		const bool taken = minds.ForcePlan(creature, ForcedPlan {.desire = creature_desires::Desire::Hunger,
		                                                         .action = executor->action,
		                                                         .object = order.object,
		                                                         .ordered = true});
		return {.taken = taken,
		        .detail = taken ? std::string(executor->action) : "its mind refused the plan",
		        .warning = taken ? FarThingWarning(registry, creature, *order.object) : std::string()};
	}
	}
	return {.detail = "no such order"};
}

} // namespace

Json openblack::inspector::OwnHandsJson(const ecs::systems::HandSystemInterface& hands)
{
	const auto entities = hands.GetOwnHands();
	const auto positions = hands.GetOwnHandPositions();
	return Json {
	    {"player", static_cast<int>(hands.GetOwnPlayer())},
	    {"left", {{"id", Id(entities[0])}, {"position", Optional(positions[0])}}},
	    {"right", {{"id", Id(entities[1])}, {"position", Optional(positions[1])}}},
	};
}

std::span<const LocatorCoverage> openblack::inspector::CoveredServices()
{
	return k_Coverage;
}

GameProvider* openblack::inspector::AddGameProviders(Inspector& inspector, const entt::meta_ctx& reflection,
                                                     RunTargetInterface& runTarget, WorldEditInterface& worldEdit,
                                                     const GameControls& controls)
{
	const RegistrySources registrySources {.registry = &Registry, .info = &Info};
	inspector.Add(std::make_unique<RegistryProvider>(registrySources, reflection));
	// The same queries over the things that only the frames make: drawn.entities, drawn.entity and the others
	inspector.Add(std::make_unique<RegistryProvider>(
	    RegistrySources {.registry = &DrawnRegistry, .info = &Info, .name = "drawn"}, reflection));
	inspector.Add(std::make_unique<ObjectsProvider>(registrySources));
	inspector.Add(std::make_unique<EditProvider>(
	    EditSources {
	        .registry = []() -> ecs::Registry* {
		        return Locator::entitiesRegistry::has_value() ? &Locator::entitiesRegistry::value() : nullptr;
	        },
	        .info = &Info,
	    },
	    reflection, worldEdit));
	inspector.Add(SkyStateProvider(std::make_unique<SkyProvider>(SkySources {
	    .moon = []() -> std::optional<ecs::components::Moon> {
		    if (!Locator::skySystem::has_value() || !Locator::entitiesRegistry::has_value())
		    {
			    return std::nullopt;
		    }
		    const auto* moon =
		        Locator::entitiesRegistry::value().TryGet<ecs::components::Moon>(Locator::skySystem::value().GetMoon());
		    return moon != nullptr ? std::optional(*moon) : std::nullopt;
	    },
	    .cameraOrigin = []() -> std::optional<glm::vec3> {
		    if (!Locator::camera::has_value())
		    {
			    return std::nullopt;
		    }
		    return Locator::camera::value().GetOrigin();
	    },
	})));
	inspector.Add(std::make_unique<ParticlesProvider>(ParticleSources {
	    .rings = []() -> std::span<const water_rings::Ring> {
		    if (!Locator::waterRingSystem::has_value())
		    {
			    return {};
		    }
		    return Locator::waterRingSystem::value().GetRings();
	    },
	    .effects = []() -> std::vector<ParticleEffectInfo> {
		    if (!Locator::particleRuleSystem::has_value())
		    {
			    return {};
		    }
		    return Locator::particleRuleSystem::value().GetEffects();
	    },
	    .liquids = []() -> std::span<const liquid_particles::Particle> {
		    if (!Locator::liquidParticleSystem::has_value())
		    {
			    return {};
		    }
		    return Locator::liquidParticleSystem::value().Particles();
	    },
	    .playerLiquids = [](PlayerNames player) -> std::span<const liquid_particles::Particle> {
		    if (!Locator::liquidParticleSystem::has_value())
		    {
			    return {};
		    }
		    return Locator::liquidParticleSystem::value().PlayerParticles(player);
	    },
	    .belief = []() -> ecs::systems::ParticleRuleSystemInterface::BeliefSpritesSeen {
		    if (!Locator::particleRuleSystem::has_value())
		    {
			    return {};
		    }
		    return Locator::particleRuleSystem::value().GetBeliefSpritesSeen();
	    },
	    .nextRequest = []() -> std::optional<ecs::systems::EffectHandle> {
		    if (!Locator::effectRequestSystem::has_value())
		    {
			    return std::nullopt;
		    }
		    return Locator::effectRequestSystem::value().NextHandle();
	    },
	    .drawn = []() -> std::optional<ecs::systems::ParticleSystemInterface::DrawStats> {
		    if (!Locator::particleSystem::has_value())
		    {
			    return std::nullopt;
		    }
		    return Locator::particleSystem::value().GetDrawStats();
	    },
	}));

	inspector.Add(MakePhysicsProvider({.bodies = &Bodies}));
	inspector.Add(MakeCreatureProvider({
	    .world = World(),
	    .tables = []() -> const creature_mind_tables::Tables* {
		    return Locator::creatureMindSystem::has_value() ? Locator::creatureMindSystem::value().GetTables() : nullptr;
	    },
	    .beliefAbout = [](entt::entity creature, entt::entity thing) -> std::optional<creature_tree::Belief> {
		    return Locator::creatureMindSystem::has_value() ? Locator::creatureMindSystem::value().BeliefAbout(creature, thing)
		                                                    : std::nullopt;
	    },
	    .setReactionNotice =
	        [](entt::entity creature, bool notice) {
		        ecs::creature_reaction_notice::SetTakesNotice(Locator::entitiesRegistry::value(), creature, notice);
	        },
	    .cellClear =
	        [](entt::entity creature, int32_t x, int32_t z, bool livingBlocks) {
		        return Locator::creatureMindSystem::has_value() &&
		               Locator::creatureMindSystem::value().IsCellClearFor(creature, x, z, livingBlocks);
	        },
	    .clearArea = [](entt::entity creature, glm::vec2 point, float width, bool livingBlocks) -> std::optional<glm::vec2> {
		    return Locator::creatureMindSystem::has_value()
		               ? Locator::creatureMindSystem::value().FindClearAreaFor(creature, point, width, livingBlocks)
		               : std::nullopt;
	    },
	    .animationLog = [](entt::entity creature) -> const creature_animation_log::Log* {
		    return Locator::creatureAnimationLog::has_value() ? Locator::creatureAnimationLog::value().Find(creature) : nullptr;
	    },
	    .clipLine = [](entt::entity creature, size_t clip) -> std::optional<ClipLine> {
		    const auto* body = Locator::entitiesRegistry::value().TryGet<const Creature>(creature);
		    if (body == nullptr || !Locator::resources::has_value())
		    {
			    return std::nullopt;
		    }
		    const auto& rigs = Locator::resources::value().GetCreatureRigs();
		    const auto rigId = creature::GetRigId(body->species);
		    if (!rigs.Contains(rigId))
		    {
			    return std::nullopt;
		    }
		    const auto& lines = rigs.Handle(rigId)->specLines;
		    if (clip >= lines.size())
		    {
			    return std::nullopt;
		    }
		    return ClipLine {.name = lines[clip].name, .type = lines[clip].type, .set = lines[clip].set};
	    },
	    .clearAnimationLogs =
	        [] {
		        if (Locator::creatureAnimationLog::has_value())
		        {
			        Locator::creatureAnimationLog::value().Clear();
		        }
	        },
	    .order = &GiveCreatureOrder,
	}));
	inspector.Add(MakeMapProvider({
	    .world = World(),
	    .map = []() -> const ecs::MapInterface* {
		    return Locator::entitiesMap::has_value() ? &Locator::entitiesMap::value() : nullptr;
	    },
	    .shade = []() -> const ecs::systems::LandShadeSystemInterface* {
		    return Locator::landShadeSystem::has_value() ? &Locator::landShadeSystem::value() : nullptr;
	    },
	    .outlines = []() -> const ecs::systems::FixedOutlineSystemInterface* {
		    return Locator::fixedOutlineSystem::has_value() ? &Locator::fixedOutlineSystem::value() : nullptr;
	    },
	}));
	inspector.Add(MakeTownProvider(World()));
	inspector.Add(MakePuzzleProvider(World()));
	inspector.Add(MakeWorshipProvider(World()));
	inspector.Add(MakeInfluenceProvider({
	    .hand = [](int player) -> std::optional<glm::vec3> {
		    // Where the reports put the left hand of the player, none for a player with no hands
		    if (!Locator::handSystem::has_value() || player < 0 || player >= static_cast<int>(PlayerNames::_COUNT))
		    {
			    return std::nullopt;
		    }
		    return Locator::handSystem::value().GetHandPositionsOf(
		        static_cast<PlayerNames>(player))[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)];
	    },
	    .ownPlayer = []() -> int {
		    return static_cast<int>(Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer()
		                                                             : PlayerNames::PLAYER_ONE);
	    },
	    .at = [](int player, glm::vec3 point) -> std::optional<InfluenceAt> {
		    if (!Locator::influenceSystem::has_value())
		    {
			    return std::nullopt;
		    }
		    const auto& influence = Locator::influenceSystem::value();
		    const auto name = static_cast<PlayerNames>(player);
		    return InfluenceAt {
		        .influence = influence.PlayerInfluence(name, point),
		        .handPoint = influence.HandPointInfluence(name, map_coords::FromMetres({point.x, point.z})),
		        .raw = influence.PlayerRawInfluence(name, point),
		    };
	    },
	    .borderShown =
	        [](int player) {
		        return Locator::influenceSystem::has_value() &&
		               Locator::influenceSystem::value().IsBorderShown(static_cast<PlayerNames>(player));
	        },
	    .circles = []() -> size_t {
		    return Locator::influenceSystem::has_value() ? Locator::influenceSystem::value().GetCircles().size() : 0;
	    },
	}));
	inspector.Add(MakeCameraProvider(controls.camera));
	inspector.Add(MakeGuiProvider(controls.gui));
	inspector.Add(MakeFightTranscriptProvider());
	inspector.Add(MakeFightAutopilotProvider());
	inspector.Add(MakeLevelProvider(controls.levels));
	inspector.Add(MakeAudioProvider({
	    .world = World(),
	    .state = []() -> std::optional<AudioState> {
		    if (!Locator::sounds::has_value() || !Locator::sounds::value().HasAudio())
		    {
			    return std::nullopt;
		    }
		    auto& audio = Locator::sounds::value();
		    AudioState state {.globalVolume = audio.GetGlobalVolume(),
		                      .sfxVolume = audio.GetSfxVolume(),
		                      .musicVolume = audio.GetMusicVolume(),
		                      .musicActive = audio.IsMusicActive(),
		                      .listener = audio.GetListener().origin};
		    if (Locator::gameMusic::has_value())
		    {
			    const auto& music = Locator::gameMusic::value();
			    state.musicPlaying = audio::GetMusicTypeName(music.GetPlaying());
			    state.landMusic = audio::GetMusicTypeName(music.GetLandType());
			    state.alignmentMusic = music.IsAlignmentMusicEnabled();
			    for (const auto& attached : music.GetAttachedMusic())
			    {
				    state.attachedMusic.emplace_back(audio::GetMusicTypeName(attached.type), attached.thing);
			    }
		    }
		    if (Locator::scriptMusicSystem::has_value())
		    {
			    const auto& script = Locator::scriptMusicSystem::value().GetMusic();
			    state.scriptMusic = audio::GetMusicTypeName(script.GetType());
			    state.scriptMusicLine = script.GetLyrics().line;
			    state.scriptMusicWord = script.GetLyrics().word;
			    if (const auto time = script.GetChannelTime(); time.has_value())
			    {
				    state.scriptMusicMs = std::chrono::duration_cast<std::chrono::milliseconds>(*time).count();
			    }
		    }
		    for (auto& channel : audio.GetMusicChannelStates())
		    {
			    state.musicChannels.push_back({
			        .bank = std::move(channel.bank),
			        .volume = channel.volume,
			        .position = channel.position,
			    });
		    }
		    return state;
	    },
	    .simulateDeviceLoss =
	        [](bool lost) {
		        if (Locator::sounds::has_value())
		        {
			        Locator::sounds::value().SimulateDeviceLoss(lost);
		        }
	        },
	    .deviceLost = []() { return Locator::sounds::has_value() && Locator::sounds::value().IsDeviceLost(); },
	    .emitters = []() -> const ecs::Registry* {
		    return Locator::sounds::has_value() ? Locator::sounds::value().GetEmitters() : nullptr;
	    },
	}));

	inspector.Add(EngineProvider());
	inspector.Add(LandProvider());
	inspector.Add(LivingProvider());
	inspector.Add(PlayersProvider());
	inspector.Add(MagicProvider());
	inspector.Add(TempleProvider());
	inspector.Add(ViewProvider(controls.seam));
	inspector.Add(AdvisorsProvider());
	inspector.Add(CreaturesProvider());
	inspector.Add(ScriptProvider(controls.scripts));
	inspector.Add(HelpProvider());
	inspector.Add(SavesProvider());

	auto game = std::make_unique<GameProvider>(runTarget);
	auto* gameProvider = game.get();
	inspector.Add(std::move(game));
	return gameProvider;
}
