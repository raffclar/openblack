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
#include <ranges>
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
#include <entt/core/hashed_string.hpp>
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
#include "ECS/Components/Football.h"
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
#include "ECS/Systems/ClanCreatureSystemInterface.h"
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
#include "ECS/Systems/DesignedWaterfallSystemInterface.h"
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
#include "ECS/Systems/FootballSystemInterface.h"
#include "ECS/Systems/FootpathSystemInterface.h"
#include "ECS/Systems/FootprintSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/GameEndSystemInterface.h"
#include "ECS/Systems/GatheringBoxSystemInterface.h"
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
#include "GameProviderShared.h"
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
#include "Sound/LoadedSound.h"
#include "Sound/MusicMood.h"
#include "Sound/SoundRequestInterface.h"
#include "SystemProviders.h"
#include "Temple/TemplePeople.h"
#include "Visual/RendererRequestsInterface.h"
#include "Windowing/WindowingInterface.h"
#include "WorldProviders.h"

using namespace openblack;
using namespace openblack::inspector;
using namespace openblack::ecs::components;
using namespace openblack::inspector::game_providers;

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
    LocatorCoverage {"designedWaterfallSystem", "particles.waterfall"},
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
    LocatorCoverage {"puzzleTotemSystem", "puzzle.totems"},
    LocatorCoverage {"wonderSystem", "town.wonders"},
    LocatorCoverage {"danceSystem", "living.dances"},
    LocatorCoverage {"footballSystem", "living.football"},
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
    LocatorCoverage {"clanCreatureSystem", "engine.online"},
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
    LocatorCoverage {"gatheringBoxSystem", "view.chat"},
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
	          "the moon is looked at again, the player's guidance (each kind's last turn, the alignment's drift, the "
	          "one-offs said), and the night whisper (the sample of the player's name, the count of real-night checks, "
	          "the turns to the next check and the volume of the next whisper)",
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
		        const auto& whisper = system.GetNightWhisper();
		        const Json whisperJson {
		            {"sample", whisper.GetSample().has_value() ? Json(*whisper.GetSample()) : Json(nullptr)},
		            {"count", whisper.GetCount()},
		            {"countdown", whisper.GetCountdown()},
		            {"volume", whisper.GetVolume()}};
		        return Json {{"remarks", remarks},
		                     {"moon_wait", system.GetMoonWait()},
		                     {"heard", heard},
		                     {"night_whisper", whisperJson}};
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
		    return Locator::liquidParticleSystem::value().PlayerParticles({.player = player});
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
	    .waterfall = []() -> std::optional<WaterfallInfo> {
		    if (!Locator::designedWaterfallSystem::has_value())
		    {
			    return std::nullopt;
		    }
		    const auto& waterfall = Locator::designedWaterfallSystem::value();
		    return WaterfallInfo {.model = waterfall.HasModel(), .slide = waterfall.GetSlide(), .sound = waterfall.GetSound()};
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
	    .soundName = [](entt::id_type id) -> std::string {
		    if (!Locator::sounds::has_value() || !Locator::resources::has_value() ||
		        !Locator::resources::value().GetSounds().Contains(id))
		    {
			    return {};
		    }
		    const auto sound = Locator::resources::value().GetSounds().Handle(id);
		    // The name of the sound is the path of its wave file. The tools show only the name of the file.
		    const auto& path = sound->name;
		    const auto slash = path.find_last_of("\\/");
		    const auto wave = slash == std::string::npos ? path : path.substr(slash + 1);
		    for (const auto& group : Locator::sounds::value().GetSoundGroups() | std::views::keys)
		    {
			    if (entt::hashed_string(group.c_str()).value() == sound->bank)
			    {
				    return fmt::format("{}/{} {}", group, sound->id, wave);
			    }
		    }
		    return fmt::format("{} {}", sound->id, wave);
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
