/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameProviders.h"

#include <array>
#include <chrono>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include <Inspector.h>
#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>

#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "CHLApi.h"
#include "Camera/Camera.h"
#include "Camera/CameraZones.h"
#include "Creature/CreatureRig.h"
#include "Creature/LeashOrders.h"
#include "Debug/TempleVillagersSurvey.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Sky.h"
#include "ECS/Components/VillageLight.h"
#include "ECS/CreatureReactionNotice.h"
#include "ECS/DrawnRegistry.h"
#include "ECS/Map.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureAnimationLogSystemInterface.h"
#include "ECS/Systems/CreatureMindLogSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/DesignedWaterfallSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/EffectRequestSystemInterface.h"
#include "ECS/Systems/GestureEventsInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/LiquidParticleSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/ScriptMusicSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/WaterRingSystemInterface.h"
#include "EditProviders.h"
#include "FightProviders.h"
#include "GameControls.h"
#include "GameProviderShared.h"
#include "GameSeamInterface.h"
#include "Help/Spirits.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "RegistryProviders.h"
#include "Resources/ResourcesInterface.h"
#include "RunControl.h"
#include "Sound/CustomMusicPlayerInterface.h"
#include "Sound/GameMusic.h"
#include "Sound/LoadedSound.h"
#include "Sound/SoundRequestInterface.h"
#include "SystemProviders.h"
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
    LocatorCoverage {"toolRunSystem", "engine.tool_run"},
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
    LocatorCoverage {"gameAudioSystem", "audio.state"},
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
    LocatorCoverage {"creatureMindLog", "creature.plan"},
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
    LocatorCoverage {"landDetailSystem", "view.draw_list"},
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
    LocatorCoverage {"ownReportSystem", "players.own_reports"},
    LocatorCoverage {"handPressSystem", "players.hand_press"},
    LocatorCoverage {"gameKeysSystem", "players.game_keys"},
    LocatorCoverage {"syncChecksumSystem", "players.checksum"},
    LocatorCoverage {"landSystem", "level.current"},
    LocatorCoverage {"videoSystem", "view.video"},
    LocatorCoverage {"loadingScreenSystem", "view.loading"},
    LocatorCoverage {"loadFinishSystem", "view.loading"},
    LocatorCoverage {"screenFade", "view.video"},
    LocatorCoverage {"gameInterface", "gui.windows"},
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
	    .mindTrace = [](entt::entity creature) -> const creature_mind_view::Trace* {
		    return Locator::creatureMindLog::has_value() ? Locator::creatureMindLog::value().Find(creature) : nullptr;
	    },
	    .recordMind =
	        [](entt::entity creature, bool record) {
		        if (Locator::creatureMindLog::has_value())
		        {
			        Locator::creatureMindLog::value().Watch(creature, record);
		        }
	        },
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
