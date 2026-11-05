/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TestbedScenarioRegistry.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <iterator>

#include <fmt/format.h>
#include <glm/geometric.hpp>

#include "3D/FlatLand.h"
#include "3D/TempleInteriorInterface.h"
#include "Common/PortableMath.h"
#include "Creature/CreatureCave.h"
#include "Creature/CreatureFace.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureObjectActions.h"
#include "Particles/ParticleTypes.h"
#include "Sound/GameMusic.h"
#include "TestbedDispenserGrid.h"
#include "TestbedScenarioBuilders.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

// How to add a scenario: append one to the list Build() returns, in the section of its facet. Give it a new id
// ("facet.what", never reused or renamed, as tests and the command line pick scenarios by it), a name, its facet, a
// description of what it sets up and what to look for, and then its data: the environment (land, hour, weather, body
// time), the framing, the creatures with the bodies, needs and desires they start with, the objects on the land, and
// the commands, which play in turn (commands on objects name them by their place in the scenario's objects). Offsets are from
// the middle of the map, x east and y north; the testbed's camera looks north from 120 units south of the middle, and the lake
// lies beyond the middle (k_Lake, its open water 100 units a side, ringed by 20 units of shallows and a bank). Keep other
// scenarios clear of it. A new facet goes into the Facet enum and its Name. test_testbed_scenarios
// checks the data of every scenario; anything a scenario needs that the runner can't yet do goes into these types and
// TestbedScenarioRunner together.

namespace
{
std::vector<Scenario> Build()
{
	std::vector<Scenario> all;
	AddIdle(all);
	AddExpressions(all);
	AddSenses(all);
	AddNeeds(all);
	AddRestAndHome(all);
	AddGrowth(all);
	AddAppearance(all);
	AddLight(all);
	AddMovement(all);
	AddFootprints(all);
	AddAudio(all);
	AddObjects(all);
	AddHand(all);
	AddLeash(all);
	AddCombat(all);
	AddMind(all);
	AddParticles(all);
	AddEditor(all);
	AddMiracleScenarios(all);
	AddGlobeScenarios(all);
	AddLifeLightScenarios(all);
	AddCreatureCastingScenarios(all);
	AddFriendActionScenarios(all);
	AddBenchmark(all);
	AddCreatureModeScenarios(all);
	AddSavedCameraScenarios(all);
	AddGestureScenarios(all);
	AddStormScenarios(all);
	AddFlockScenarios(all);
	AddBirdScenarios(all);
	AddTeleportScenarios(all);
	AddVortexScenarios(all);
	AddLandFiveQuestScenarios(all);
	AddLandsFourFiveScrollScenarios(all);
	AddLand4StoneScenarios(all);
	AddLand4SlegScenarios(all);
	AddPuzzleQuestScenarios(all);
	AddUndeadTotemScenarios(all);
	AddRewardScenarios(all);
	AddTornadoScenarios(all);
	AddPhysicsScenarios(all);
	AddNatureScenarios(all);
	AddHandNavigationScenarios(all);
	AddHandLookScenarios(all);
	AddGateScenarios(all);
	AddVillageTotemScenarios(all);
	AddTempleVillagerScenarios(all);
	AddFishScenarios(all);
	AddKnockScenarios(all);
	AddFootballScenarios(all);
	AddVillagerSpeedScenarios(all);
	AddDiscipleScenarios(all);
	AddVillagerLabelScenarios(all);
	AddTempleKeyScenarios(all);
	AddHighlightScenarios(all);
	AddSkyScenarios(all);
	AddAdvisorScenarios(all);
	AddStory(all);
	AddExplorersBoat(all);
	AddTownGrowthScenarios(all);
	AddStatisticsScenarios(all);
	AddMultiplayerFrontEndScenarios(all);
	AddOtherGodScenarios(all);
	AddJoinOnlineScenarios(all);
	AddTutorialScenarios(all);
	AddCreatureDanceScenarios(all);
	AddCreatureLearningScenarios(all);
	AddComputerPlayerScenarios(all);
	AddGameEndScenarios(all);
	AddEndingScenarios(all);
	AddLastStandScenarios(all);
	AddWorldSoundScenarios(all);
	AddTownMiracleScenarios(all);
	AddFixedKindScenarios(all);
	AddCreatureExplorationScenarios(all);
	AddCreatureActionScenarios(all);
	AddScriptTaskRelease(all);
	AddLandOneScriptStubScenarios(all);
	AddVillagerSizeAndReachScenarios(all);
	AddSwayAndFrightScenarios(all);
	AddStoreScenarios(all);
	AddPoisonScenarios(all);
	AddWorkshopScenarios(all);
	AddGodsRingMapScenarios(all);
	AddSelfCheckScenarios(all);
	AddWorshipSiteScenarios(all);
	AddLateSilverScrollScenarios(all);
	return all;
}

bool InRange(float value, float low, float high)
{
	return value >= low && value <= high;
}

void CheckNeeds(const NeedOverrides& needs, std::vector<std::string>& problems, std::string_view who)
{
	const std::array<std::pair<std::string_view, std::optional<float>>, 5> k_UnitNeeds {{
	    {"exhaustion", needs.exhaustion},
	    {"dehydration", needs.dehydration},
	    {"poo", needs.poo},
	    {"life", needs.life},
	    {"energy", needs.energy},
	}};
	for (const auto& [name, value] : k_UnitNeeds)
	{
		if (value.has_value() && !InRange(*value, 0.0f, creature_physiology::k_MaxGrownSize))
		{
			problems.push_back(fmt::format("{}: {} {} out of range", who, name, *value));
		}
	}
	if (needs.warmth.has_value() && !InRange(*needs.warmth, -1.0f, 1.0f))
	{
		problems.push_back(fmt::format("{}: warmth {} out of range", who, *needs.warmth));
	}
}

bool ValidAnimation(Kind kind, size_t animation)
{
	switch (kind)
	{
	case Kind::PlayAction:
		return animation >= animations::k_FirstAction && animation < animations::k_FirstAction + animations::k_ActionCount;
	case Kind::PlayGesture:
		return animation >= animations::k_FirstGesture && animation < animations::k_FirstGesture + animations::k_GestureCount;
	case Kind::PullFace:
		return animation >= animations::k_FirstFace && animation < animations::k_FirstFace + animations::k_FaceCount;
	default:
		return true;
	}
}

bool ValidOffset(glm::vec2 offset)
{
	return std::abs(offset.x) <= k_MaxOffset && std::abs(offset.y) <= k_MaxOffset;
}

/// A script run names a script, and every object, creature and marker it hands over is the scenario's and on the map.
/// An object that a native call gave back must come from a native call before this command.
bool ScriptArgumentsFit(const Command& command, const Scenario& scenario)
{
	const auto before =
	    std::ranges::find_if(scenario.commands, [&command](const Command& other) { return &other == &command; });
	const auto earlierCalls = static_cast<size_t>(
	    std::count_if(scenario.commands.begin(), before, [](const Command& other) { return other.kind == Kind::CallNative; }));
	const auto fits = [&scenario, &command, earlierCalls](const ScriptArgument& argument) {
		switch (argument.kind)
		{
		case ScriptArgument::Kind::Number:
		case ScriptArgument::Kind::Integer:
		case ScriptArgument::Kind::Result:
		case ScriptArgument::Kind::LastResult:
		case ScriptArgument::Kind::Town:
			return true;
		case ScriptArgument::Kind::Marker:
		case ScriptArgument::Kind::Position:
			return ValidOffset(argument.point);
		case ScriptArgument::Kind::Object:
			return argument.index < scenario.objects.size();
		case ScriptArgument::Kind::Creature:
			return argument.index < scenario.creatures.size();
		case ScriptArgument::Kind::Temple:
			return argument.index < scenario.temples.size();
		case ScriptArgument::Kind::Flock:
			// The flock needs a villager or an animal of the scenario
			return std::ranges::any_of(scenario.objects, [](const ObjectSetup& object) {
				return std::holds_alternative<VillagerInfo>(object.type) || std::holds_alternative<AnimalInfo>(object.type);
			});
		case ScriptArgument::Kind::Made:
			return argument.index < earlierCalls;
		case ScriptArgument::Kind::Text:
			return command.kind == Command::Kind::CallNative && !argument.text.empty();
		}
		return false;
	};
	return !command.script.empty() && std::ranges::all_of(command.scriptArguments, fits) &&
	       std::ranges::all_of(command.scriptGlobals,
	                           [&fits](const auto& global) { return !global.first.empty() && fits(global.second); });
}

/// What is wrong with what a command acts on, if anything: the object it picks up must be of a kind a creature may pick
/// up (a thing, pot, animal, villager, static, tree or magic mushroom), what it knocks down a thing or a tree, the
/// part it strokes, the way it looks something over and the leash put on real ones
std::string_view CommandProblem(const Command& command, std::span<const ObjectSetup> objects)
{
	const auto* object = command.object < objects.size() ? &objects[command.object] : nullptr;
	switch (command.kind)
	{
	case Kind::PickUp:
		return object != nullptr &&
		               (std::holds_alternative<MobileObjectInfo>(object->type) ||
		                std::holds_alternative<AnimalInfo>(object->type) ||
		                std::holds_alternative<VillagerInfo>(object->type) || std::holds_alternative<PotInfo>(object->type) ||
		                std::holds_alternative<MobileStaticInfo>(object->type) ||
		                std::holds_alternative<TreeInfo>(object->type) ||
		                (std::holds_alternative<FeatureInfo>(object->type) &&
		                 std::get<FeatureInfo>(object->type) == FeatureInfo::MagicMushroom))
		           ? ""
		           : "picks up no thing";
	case Kind::KnockDown:
		return object != nullptr && !std::holds_alternative<FeatureInfo>(object->type) ? "" : "knocks down no thing or tree";
	case Kind::TieLeash:
		return object != nullptr ? "" : "ties the leash to nothing";
	case Kind::JoinFootball:
		return object != nullptr && std::holds_alternative<VillagerInfo>(object->type) ? ""
		                                                                               : "sends no villager to the football";
	case Kind::Examine:
		return command.value < creature_object_actions::k_KeepAnimationCount ? "" : "no such way of looking it over";
	case Kind::HandStroke:
		return command.bodyPart < creature_feedback::k_BodyPartCount ? "" : "no such part of the body";
	case Kind::HandSlap:
		return command.slapHeight > 0.0f && command.slapHeight < creature_feedback::k_SlapAbove ? "" : "slaps above it";
	case Kind::PutOnLeash:
		return command.leash == LeashType::Rope || command.leash == LeashType::Evil || command.leash == LeashType::Good
		           ? ""
		           : "no such leash";
	case Kind::ConfineToHome:
		return command.radius > 0.0f ? "" : "keeps it nowhere";
	case Kind::LeashKey:
		return command.value < 3 ? "" : "no such leash key";
	case Kind::HandTapLeash:
		return command.player < PlayerNames::_COUNT ? "" : "no such player";
	case Kind::FightBlow:
		return command.value < 3 && command.chargeMs >= 0.0f && command.chargeMs <= creature_fight::k_MaxChargeMs
		           ? ""
		           : "no such blow or charge";
	case Kind::FightStep:
		return command.value < 4 ? "" : "no such step";
	default:
		return "";
	}
}
} // namespace

bool testbed_scenarios::NeedsNoCreature(Command::Kind kind)
{
	return kind == Kind::SetHour || kind == Kind::RestoreDeviceInDomeFollow || kind == Kind::RestoreDevice ||
	       kind == Kind::PutLoadedArena || kind == Kind::HoldSeed || kind == Kind::DrawGesture || kind == Kind::SummonSeed ||
	       kind == Kind::PressKey || kind == Kind::HandTakeFireBall || kind == Kind::SetAlignment || kind == Kind::WideScreen ||
	       kind == Kind::SetInterfaceLevel || kind == Kind::FlyToBookmark || kind == Kind::TabToTown ||
	       kind == Kind::SetOpenClose || kind == Kind::LayGateStone || kind == Kind::HandTapObject || kind == Kind::Advisor ||
	       kind == Kind::RunText || kind == Kind::PlayHandDemo || kind == Kind::Reframe || kind == Kind::DestroyTemple ||
	       kind == Kind::RunHelpMessage || kind == Kind::EndGuidanceWaits || kind == Kind::SetTownBeliefInRival ||
	       kind == Kind::GiveLandTown || kind == Kind::RecordChallenge || kind == Kind::EnterTemple || kind == Kind::HandTake ||
	       kind == Kind::HandApply || kind == Kind::HandTakeShownSeed || kind == Kind::RunScript || kind == Kind::ClickScroll ||
	       kind == Kind::ClickCreatureAt || kind == Kind::CameraLooksAt || kind == Kind::HandTakeNear ||
	       kind == Kind::ClickCreatureNear || kind == Kind::TapRewardNear || kind == Kind::PlayBoatFilm ||
	       kind == Kind::HandPour || kind == Kind::HandRelease || kind == Kind::SetReportLatency ||
	       kind == Kind::RemoveObject || kind == Kind::ReadTip || kind == Kind::LookAtTempleScroll ||
	       kind == Kind::HandGripTotem || kind == Kind::ProfileFolderCheck || kind == Kind::PlayCustomMusic ||
	       kind == Kind::ShowCustomMusicControls || kind == Kind::OwnTattooPicture || kind == Kind::OpenTattooMenu ||
	       kind == Kind::OpenControlsBox || kind == Kind::OpenMenuPage || kind == Kind::PressMenuKey ||
	       kind == Kind::StopScript || kind == Kind::StartScriptMusic || kind == Kind::SaveGame || kind == Kind::LoadGame ||
	       kind == Kind::QuickSave || kind == Kind::ContinueGame || kind == Kind::PlayLogoEffect ||
	       kind == Kind::PlayFallingSpellFilm || kind == Kind::PlayIntroFilm || kind == Kind::SayLine ||
	       kind == Kind::PlayGuidanceVoice || kind == Kind::ScriptRelease || kind == Kind::FillSiteIcons ||
	       kind == Kind::ChargeIconWithSeedHeld || kind == Kind::TakeIconSeed || kind == Kind::TapSiteIcon ||
	       kind == Kind::TapTempleEntrance || kind == Kind::FinishBuilding || kind == Kind::FillBuiltAmount ||
	       kind == Kind::ScaffoldStartsSite || kind == Kind::DestroyScaffoldSite || kind == Kind::ScaffoldClearsGround ||
	       kind == Kind::CallNative || kind == Kind::PauseGame || kind == Kind::SetGameSpeed ||
	       kind == Kind::RemoveTeleportStone || kind == Kind::MakeDance || kind == Kind::OpenMultiplayerBox ||
	       kind == Kind::BuildWorshipSites || kind == Kind::ScriptBuildsWorshipSites || kind == Kind::PressPlainKey ||
	       kind == Kind::PlainKeyDown || kind == Kind::PlainKeyRepeat || kind == Kind::PlainKeyUp ||
	       kind == Kind::OtherGodHand || kind == Kind::ObserverCamera || kind == Kind::OwnGod || kind == Kind::JoinOnline ||
	       kind == Kind::EndBox || kind == Kind::BringCreature || kind == Kind::TattooEditorAsNetworkGame ||
	       kind == Kind::ShowDemoPing || kind == Kind::AddIncomingMessage || kind == Kind::Chat ||
	       kind == Kind::ComputerPersonality || kind == Kind::ComputerUntaughtSends || kind == Kind::SkirmishBox ||
	       kind == Kind::LookAtSaveGamePicture || kind == Kind::HandCastOnDispenser || kind == Kind::HandCastOnCreature ||
	       kind == Kind::CloseHeldMiracle || kind == Kind::JoinFootball || IsPointerCommand(kind);
}

std::string_view testbed_scenarios::Name(Facet facet)
{
	constexpr std::array<std::string_view, k_FacetCount> k_Names {
	    "Idle",       "Expressions", "Senses",    "Needs",         "Growth",      "Appearance", "Light",   "Movement",
	    "Footprints", "Audio",       "Objects",   "Hand",          "Leash",       "Combat",     "Mind",    "Particles",
	    "Editor",     "Miracles",    "Benchmark", "Creature Mode", "Physics",     "Nature",     "Animals", "Sky",
	    "Advisors",   "Story",       "Tutorial",  "Interface",     "Multiplayer",
	};
	return k_Names.at(static_cast<size_t>(facet));
}

std::string_view testbed_scenarios::Name(Weather weather)
{
	constexpr std::array<std::string_view, 5> k_Names {"clear", "rain", "thunderstorm", "snow", "blizzard"};
	return k_Names.at(static_cast<size_t>(weather));
}

std::string_view testbed_scenarios::Name(Shot shot)
{
	constexpr std::array<std::string_view, 5> k_Names {"testbed", "overview", "follow", "head", "placed"};
	return k_Names.at(static_cast<size_t>(shot));
}

std::string_view testbed_scenarios::Name(Command::Kind kind)
{
	constexpr std::array<std::string_view, 231> k_Names {
	    "walk to",
	    "run to",
	    "follow",
	    "flee from",
	    "turn to face",
	    "face the camera",
	    "stop",
	    "play action",
	    "gesture",
	    "pull face",
	    "draw a gesture",
	    "show feeling",
	    "sit down",
	    "stand up",
	    "sleep",
	    "wake",
	    "eat",
	    "drink",
	    "poo",
	    "fart",
	    "puke",
	    "faint",
	    "stroke",
	    "slap",
	    "set hour",
	    "pick up",
	    "put down",
	    "toss away",
	    "eat it",
	    "look it over",
	    "throw at",
	    "knock down",
	    "point at",
	    "hand strokes",
	    "hand slaps",
	    "hand lets go",
	    "hand takes hold",
	    "put on leash",
	    "tie leash to",
	    "tie leash to desire flag",
	    "tie leash to worship site flag",
	    "untie leash",
	    "take off leash",
	    "keep at home",
	    "make leashable",
	    "hand taps to leash",
	    "leash key",
	    "shake the hand",
	    "order to a point",
	    "order on the object",
	    "hang the temple's leashes",
	    "tap a temple leash",
	    "tap a building with the leash",
	    "start fight",
	    "fight blow",
	    "fight block",
	    "fight step",
	    "fight special",
	    "fight by itself",
	    "fight miracle",
	    "show the fight",
	    "pointer on opponent",
	    "pointer on the cave creature",
	    "fight autopilot",
	    "fight lesson check",
	    "knock out",
	    "bring round",
	    "abort fight",
	    "tie leash to creature",
	    "set desire",
	    "set stage",
	    "reward if",
	    "see skill",
	    "see miracle",
	    "player did",
	    "player did it to the object",
	    "press C",
	    "double click",
	    "camera keys",
	    "clear the view",
	    "give the camera back",
	    "saved follow camera",
	    "saved turning camera",
	    "saved free camera",
	    "saved camera below the land",
	    "press F5",
	    "tattoo",
	    "take tattoo off",
	    "open tattoo editor",
	    "hold seed",
	    "draw gesture",
	    "summon seed",
	    "know miracle",
	    "cast miracle",
	    "press key",
	    "hand takes fireball",
	    "pointer to",
	    "press button",
	    "let go of button",
	    "move mouse",
	    "turn wheel",
	    "set alignment",
	    "open or close",
	    "lay gate stone",
	    "cinema bars",
	    "set interface level",
	    "set fight lean",
	    "set miracle sightings",
	    "hand tap",
	    "tell an advisor",
	    "walk home",
	    "run a help message",
	    "end the guidance's waits",
	    "set town belief in rival",
	    "destroy temple",
	    "script play",
	    "run text",
	    "play hand demo",
	    "reframe",
	    "hand take",
	    "hand give",
	    "hand take shown seed",
	    "hand apply",
	    "swap minds",
	    "record challenge",
	    "enter temple",
	    "play boat film",
	    "run script",
	    "hand release",
	    "hand pour",
	    "do action",
	    "make friends",
	    "only desire",
	    "remove object",
	    "read tip",
	    "look at temple scroll",
	    "look at a save game picture",
	    "set alight",
	    "hand grips totem",
	    "pointer on altar",
	    "pointer on rope",
	    "pointer on object",
	    "set fizz",
	    "check the profile folder",
	    "give the player a picture of their own",
	    "open the tattoo editor from the menu",
	    "open the box of the controls page",
	    "open a page of the menu",
	    "press a key in the menu",
	    "play custom music",
	    "show the custom music controls",
	    "save the game",
	    "load the game",
	    "script reaction",
	    "script place",
	    "stop script",
	    "pointer on creature",
	    "start script music",
	    "write the web page",
	    "fly to a bookmark",
	    "press the Tab key",
	    "keep the creature in a bookmark and follow it",
	    "play the falling spell film",
	    "play the intro film",
	    "give hand glows",
	    "give a creed power",
	    "attach a script sound",
	    "attach a script sound to the object",
	    "say a line",
	    "play a guidance voice",
	    "stop at once",
	    "turn to face when still",
	    "script release",
	    "fill the worship site icons",
	    "quick save",
	    "continue the game",
	    "play the logo effect",
	    "charge an icon with its seed in the hand",
	    "finish the building",
	    "fill the built amount",
	    "start a site with the scaffold",
	    "destroy the site of the scaffold",
	    "clear the ground of the scaffold",
	    "call a script native",
	    "animal flees",
	    "hold the route search",
	    "pause the game",
	    "set the game's speed",
	    "force an action",
	    "remove the teleport stone",
	    "make a dance",
	    "open a multiplayer box",
	    "build the worship sites",
	    "press a key on the keyboard",
	    "show another god's hand",
	    "show an observer's camera",
	    "be the god of a player at this computer",
	    "press Join Online Game of the page",
	    "end box",
	    "bring a creature into a network game",
	    "give the hand's reports a latency",
	    "tattoo editor as in a network game",
	    "show a made-up ping",
	    "add an incoming message",
	    "chat",
	    "computer personality",
	    "computer sends with no lesson",
	    "skirmish box",
	    "put a key down",
	    "repeat a held key",
	    "let go of a key",
	    "a god orders its creature",
	    "take a seed from its icon",
	    "tap a worship site icon",
	    "change the body's look",
	    "tap the temple's entrance",
	    "give a town of the land to the player",
	    "cast the held miracle on a dispenser",
	    "cast the held miracle on a creature",
	    "close the held miracle and let go",
	    "click a scroll",
	    "put the camera to look at a point",
	    "join the football of the town",
	    "a script builds the worship sites",
	    "take the mobile object nearest a point into the hand",
	    "click the creature nearest a point",
	    "tap the reward chest nearest a point",
	    "click a creature",
	    "make the graphics device again while the dome follows the sky",
	    "make the graphics device again",
	    "put an arena of a loaded file between two creatures",
	};
	return k_Names.at(static_cast<size_t>(kind));
}

bool testbed_scenarios::IsPointerCommand(Command::Kind kind)
{
	return kind == Kind::PointerTo || kind == Kind::PointerPress || kind == Kind::PointerRelease ||
	       kind == Kind::PointerSweep || kind == Kind::WheelTurn || kind == Kind::PointerOnOpponent ||
	       kind == Kind::PointerOnCaveCreature || kind == Kind::PointerOnCreature || kind == Kind::PointerOnAltar ||
	       kind == Kind::PointerOnRope || kind == Kind::PointerOnObject;
}

bool NeedOverrides::Empty() const
{
	return !energy && !exhaustion && !dehydration && !poo && !life && !warmth && !age;
}

std::span<const Scenario> testbed_scenarios::All()
{
	static const std::vector<Scenario> k_All = Build();
	return k_All;
}

const Scenario* testbed_scenarios::Find(std::string_view id)
{
	const auto all = All();
	const auto found = std::ranges::find(all, id, &Scenario::id);
	return found != all.end() ? &*found : nullptr;
}

std::vector<std::string> testbed_scenarios::Problems(const Scenario& scenario)
{
	std::vector<std::string> problems;
	if (scenario.id.empty() || scenario.name.empty() || scenario.description.empty() || scenario.expected.empty())
	{
		problems.emplace_back("missing its id, name, description or what to expect");
	}
	if (static_cast<size_t>(scenario.facet) >= k_FacetCount)
	{
		problems.emplace_back("no such facet");
	}
	const auto& environment = scenario.environment;
	if (!InRange(environment.hour, 0.0f, k_HoursPerDay) || environment.bodyTimeScale < 0.0f)
	{
		problems.emplace_back("hour or body time out of range");
	}
	// A land other than the testbed brings its own things
	if (environment.land == Land::Testbed && scenario.creatures.empty() && scenario.particles.empty() &&
	    scenario.miracles.empty() && scenario.dispensers.empty() && scenario.birdFlocks.empty() && scenario.temples.empty() &&
	    !environment.dispenserGrid && !scenario.crowd.has_value() && !environment.playerAlignment.has_value() &&
	    scenario.throws.empty() && scenario.objects.empty() && scenario.vortices.empty() && scenario.rewards.empty() &&
	    scenario.fireflyRewards.empty() && scenario.landStorms.empty() && scenario.highlights.empty() &&
	    scenario.landScript.empty() &&
	    std::ranges::none_of(scenario.commands, [](const Command& command) { return NeedsNoCreature(command.kind); }))
	{
		problems.emplace_back("no creatures, things, particles, miracles, dispensers, birds, temples, vortices, reward "
		                      "chests, crowd, fireflies' rewards, land storms, scrolls or signs, land script lines, player's "
		                      "commands or alignment for the "
		                      "hand");
	}
	for (const auto& storm : scenario.landStorms)
	{
		if (!ValidOffset(storm.offset) || !ValidOffset(storm.destination) || storm.age < 0.0f || storm.cloudCount < 0)
		{
			problems.emplace_back("a land storm off the map, or with a negative age or cloud count");
		}
	}
	if ((environment.playerAlignment && !InRange(*environment.playerAlignment, -1.0f, 1.0f)) ||
	    (environment.cursor && (!InRange(environment.cursor->x, 0.0f, 1.0f) || !InRange(environment.cursor->y, 0.0f, 1.0f))))
	{
		problems.emplace_back("alignment or cursor out of range");
	}
	if (scenario.crowd.has_value() && (scenario.crowd->count == 0 || scenario.crowd->perFrame == 0))
	{
		problems.emplace_back("a crowd of no one, or spawned none a frame");
	}
	for (const auto& miracle : scenario.miracles)
	{
		const auto type = static_cast<size_t>(miracle.type);
		if (type == 0 || type >= k_Miracles)
		{
			problems.emplace_back("a miracle of no such magic type");
		}
		if (miracle.target == MiracleCast::Target::Creature && miracle.creature >= scenario.creatures.size())
		{
			problems.emplace_back("a miracle cast on a creature that isn't there");
		}
		if (!ValidOffset(miracle.point) || !ValidOffset(miracle.handOffset) || miracle.delaySeconds < 0.0f ||
		    miracle.holdSeconds.value_or(0.0f) < 0.0f || miracle.repeatSeconds.value_or(1.0f) <= 0.0f)
		{
			problems.emplace_back("a miracle's points or times out of range");
		}
	}
	for (const auto& dispenser : scenario.dispensers)
	{
		const auto type = static_cast<size_t>(dispenser.type);
		if (type == 0 || type >= k_Miracles || !ValidOffset(dispenser.offset))
		{
			problems.emplace_back("a dispenser of no such magic type, or off the map");
		}
	}
	for (const auto& particle : scenario.particles)
	{
		if (particle.file.empty() && particles::ParticleTypeFile(particle.type).empty())
		{
			problems.emplace_back("a particle type that has no file");
		}
		if (particle.targetsCreatures && scenario.creatures.empty())
		{
			problems.emplace_back("a particle effect given creatures to act on, without creatures");
		}
		if (particle.magnitude < 0.0f || particle.restartSeconds < 0.0f)
		{
			problems.emplace_back("a particle effect's magnitude or restart out of range");
		}
	}
	const auto creatures = scenario.creatures.size();
	// The commands can also name the creatures of the filled slots: one for each temple at most
	const auto others = creatures + (scenario.fillEmptySlots ? scenario.temples.size() : 0);
	const auto& framing = scenario.framing;
	if ((framing.shot == Shot::Follow || framing.shot == Shot::Head) && framing.creature >= creatures)
	{
		problems.emplace_back("the camera frames a creature that isn't there");
	}
	if (framing.distance <= 0.0f || !std::ranges::all_of(framing.include, ValidOffset))
	{
		problems.emplace_back("the framing's distance or points are out of range");
	}
	for (size_t i = 0; i < creatures; ++i)
	{
		const auto& creature = scenario.creatures.at(i);
		const auto who = fmt::format("creature {}", i);
		const auto species = static_cast<int32_t>(creature.species);
		if (species < 1 || species >= static_cast<int32_t>(CreatureType::_COUNT))
		{
			problems.push_back(fmt::format("{}: no such species", who));
		}
		if (static_cast<size_t>(creature.owner) >= static_cast<size_t>(PlayerNames::_COUNT))
		{
			problems.push_back(fmt::format("{}: no such owner", who));
		}
		if (!ValidOffset(creature.offset))
		{
			problems.push_back(fmt::format("{}: off the map", who));
		}
		if ((creature.alignment && !InRange(*creature.alignment, -1.0f, 1.0f)) ||
		    (creature.fatness && !InRange(*creature.fatness, 0.0f, 1.0f)) ||
		    (creature.strength && !InRange(*creature.strength, 0.0f, 1.0f)) ||
		    (creature.size && !InRange(*creature.size, 0.05f, 4.0f)))
		{
			problems.push_back(fmt::format("{}: body out of range", who));
		}
		if (!creature.mindFile.empty() && !creature.mindFile.starts_with("chosen:") && !creature.mindFile.starts_with("game:"))
		{
			problems.push_back(fmt::format("{}: a mind file is chosen: or game:", who));
		}
		if (creature.phase && *creature.phase > k_LastPhase)
		{
			problems.push_back(fmt::format("{}: no such stage of growing up", who));
		}
		CheckNeeds(creature.needs, problems, who);
		for (const auto& desire : creature.desires)
		{
			if (static_cast<size_t>(desire.desire) >= creature_desires::k_DesireCount || !InRange(desire.fraction, 0.0f, 1.0f))
			{
				problems.push_back(fmt::format("{}: desire out of range", who));
			}
		}
		for (const auto& tattoo : creature.tattoos)
		{
			if (tattoo.design >= creature_tattoo::k_DesignCount || tattoo.Empty())
			{
				problems.push_back(fmt::format("{}: no such tattoo design or site", who));
			}
		}
		if (creature.tattoos.size() > creature_tattoo::k_SlotCount)
		{
			problems.push_back(fmt::format("{}: more tattoos than slots", who));
		}
		for (const auto& mark : creature.wounds)
		{
			if (mark.type >= creature_marks::k_WoundLifetimes.size() || mark.column >= 8 || mark.skin >= 4)
			{
				problems.push_back(fmt::format("{}: no such wound", who));
			}
		}
	}
	for (const auto& object : scenario.objects)
	{
		const bool valid = std::visit(
		    []<typename T>(T type) {
			    const auto index = static_cast<int32_t>(type);
			    return index >= 0 && index < static_cast<int32_t>(T::_COUNT);
		    },
		    object.type);
		if (!valid || object.scale <= 0.0f || !ValidOffset(object.offset))
		{
			problems.emplace_back("an object of no such kind, size or place");
		}
		// The model of this kind of food pile is the grain at the side of a worship site. It lies about 50 m from the
		// point where the pile is, so you do not see the pile there. Use a different kind of food pile.
		if (const auto* pot = std::get_if<PotInfo>(&object.type); pot != nullptr && *pot == PotInfo::FoodPile)
		{
			problems.emplace_back("a food pile of the kind whose model is far from its point");
		}
		// On a land of the story, the scenario's town is a town of the testbed, not a town of the land. Name the land's
		// town with landTown. Only a land of the story has towns of its own.
		const bool storyLand = scenario.environment.land == Land::Story;
		if (object.joinTown && storyLand)
		{
			problems.emplace_back("an object that joins the scenario's town on a land of the story; use landTown");
		}
		if (object.landTown.has_value() && !storyLand)
		{
			problems.emplace_back("an object with a town of the land on a land that is not of the story");
		}
	}
	for (const auto& footpath : scenario.footpaths)
	{
		const auto outOfRange = [&scenario](size_t object) { return object >= scenario.objects.size(); };
		if ((footpath.startsAt.has_value() && outOfRange(*footpath.startsAt)) ||
		    std::ranges::any_of(footpath.leadsFrom, outOfRange) ||
		    !std::ranges::all_of(footpath.nodes, [](glm::vec2 node) { return ValidOffset(node); }) || footpath.nodes.empty() ||
		    footpath.leadsFrom.empty())
		{
			problems.emplace_back("a footpath with no nodes, a node off the map or an object that isn't there");
		}
	}
	for (const auto& throwing : scenario.throws)
	{
		const auto things = throwing.reward ? scenario.rewards.size() : scenario.objects.size();
		if (throwing.object >= things || (throwing.from.has_value() && !ValidOffset(*throwing.from)) || throwing.height < 0.0f)
		{
			problems.emplace_back("a throw of no such object, or from no such place");
		}
	}
	for (size_t i = 0; i < scenario.commands.size(); ++i)
	{
		const auto& command = scenario.commands.at(i);
		const auto what = fmt::format("command {} ({})", i, Name(command.kind));
		// An object set alight (value 1 or 2) needs no creature, but the object must be there
		const bool objectAlight = command.kind == Kind::SetAlight && (command.value == 1 || command.value == 2);
		if (objectAlight && command.object >= scenario.objects.size())
		{
			problems.push_back(fmt::format("{}: no such object", what));
		}
		if (!NeedsNoCreature(command.kind) && !objectAlight && command.creature >= creatures)
		{
			problems.push_back(fmt::format("{}: no such creature", what));
		}
		if (command.kind == Kind::SetAlignment && !InRange(command.alignment, -1.0f, 1.0f))
		{
			problems.push_back(fmt::format("{}: alignment out of range", what));
		}
		if ((command.kind == Kind::SetOpenClose && command.object >= scenario.objects.size()) ||
		    (command.kind == Kind::LayGateStone &&
		     (command.object >= scenario.objects.size() || command.value >= scenario.objects.size())))
		{
			problems.push_back(fmt::format("{}: no such object", what));
		}
		if ((command.kind == Kind::Follow || command.kind == Kind::StartFight || command.kind == Kind::TieLeashToCreature ||
		     command.kind == Kind::MakeFriends) &&
		    (command.value >= others || command.value == command.creature))
		{
			problems.push_back(fmt::format("{}: acts on no other creature", what));
		}
		if (!ValidAnimation(command.kind, command.value))
		{
			problems.push_back(fmt::format("{}: animation {} isn't of its kind", what, command.value));
		}
		if (const auto problem = CommandProblem(command, scenario.objects); !problem.empty())
		{
			problems.push_back(fmt::format("{}: {}", what, problem));
		}
		if ((command.kind == Kind::RunScript || command.kind == Kind::CallNative) && !ScriptArgumentsFit(command, scenario))
		{
			problems.push_back(fmt::format("{}: runs no script, or hands it what the scenario hasn't", what));
		}
		const bool offMapOrder = command.kind == Kind::LeashOrderAt && std::abs(command.point.x) <= k_MaxOrderOffset &&
		                         std::abs(command.point.y) <= k_MaxOrderOffset;
		if ((!ValidOffset(command.point) && !offMapOrder) || command.delaySeconds < 0.0f ||
		    !InRange(command.hour, 0.0f, k_HoursPerDay))
		{
			problems.push_back(fmt::format("{}: point, delay or hour out of range", what));
		}
		if ((command.kind == Kind::SetDesire &&
		     (command.value >= creature_desires::k_DesireCount || !InRange(command.amount, 0.0f, 1.0f))) ||
		    (command.kind == Kind::SetPhase && command.value > k_LastPhase) ||
		    (command.kind == Kind::SetFightLean && !InRange(command.amount, -1.0f, 1.0f)) ||
		    (command.kind == Kind::SetMiracleSightings && (command.value >= k_Miracles || command.amount < 0.0f)) ||
		    (command.kind == Kind::ShowFeeling && command.value >= creature_face::k_CueCount) ||
		    (command.kind == Kind::SeeSkill && command.value >= k_Skills) ||
		    ((command.kind == Kind::SeeMiracle || command.kind == Kind::KnowMiracle || command.kind == Kind::CastMiracle ||
		      command.kind == Kind::FightSpell) &&
		     command.value >= k_Miracles) ||
		    (command.kind == Kind::CastMiracle &&
		     (command.atCreature.has_value() ? *command.atCreature >= scenario.creatures.size()
		                                     : command.object >= scenario.objects.size())) ||
		    ((command.kind == Kind::PlayerDid || command.kind == Kind::PlayerDidOn) && command.value >= k_Deeds) ||
		    (command.kind == Kind::CameraKeys && (command.value >= 4 || command.amount <= 0.0f)) ||
		    (command.kind == Kind::PointerTo &&
		     (!InRange(command.point.x, 0.0f, 1.0f) || !InRange(command.point.y, 0.0f, 1.0f))) ||
		    ((command.kind == Kind::PointerPress || command.kind == Kind::PointerRelease) &&
		     (command.value < 1 || command.value > 3)) ||
		    (command.kind == Kind::PointerSweep && command.amount <= 0.0f) ||
		    (command.kind == Kind::OpenCreatureCave && command.value >= creature_cave::k_PageCount) ||
		    ((command.kind == Kind::ApplyTattoo || command.kind == Kind::RemoveTattoo) &&
		     (command.value >= creature_tattoo::k_DesignCount || command.bodyPart >= creature_tattoo::k_SlotCount)) ||
		    ((command.kind == Kind::HoldSeed || command.kind == Kind::SummonSeed) && command.value >= k_SeedCount) ||
		    (command.kind == Kind::DrawGesture && (command.value == 0 || command.value > k_LastGesture)))
		{
			problems.push_back(fmt::format("{}: value {} out of range", what, command.value));
		}
	}
	if (scenario.repeatFrom.has_value() && *scenario.repeatFrom >= scenario.commands.size())
	{
		problems.emplace_back("repeats from a command that isn't there");
	}
	return problems;
}

glm::vec2 testbed_scenarios::MapPoint(glm::vec2 middle, glm::vec2 offset)
{
	return middle + offset;
}

std::optional<uint32_t> testbed_scenarios::HelpTextNumber(std::span<const std::string> names, std::string_view name)
{
	const auto found = std::ranges::find(names, name);
	if (found == names.end())
	{
		return std::nullopt;
	}
	return static_cast<uint32_t>(std::distance(names.begin(), found));
}

float testbed_scenarios::CreatureHeight(float size)
{
	// A creature of size 1 stands about 15 units tall, whatever its species' mesh
	constexpr float k_HeightPerSize = 15.0f;
	return k_HeightPerSize * std::max(size, 0.05f);
}

CameraPlacement testbed_scenarios::Overview(glm::vec3 centre, glm::vec2 halfSize, glm::vec2 fieldOfView, float distanceFactor)
{
	// Looking down from the south at this angle, far enough back for the box's width to fit across the view and its
	// depth, foreshortened, up and down it, with a margin round it for the creatures' heights
	constexpr float k_Pitch = 0.65f;
	constexpr float k_MinDistance = 40.0f;
	constexpr float k_Margin = 25.0f;
	const auto halfAngles = glm::clamp(fieldOfView * 0.5f, 0.1f, 1.5f);
	// The near side of the box is closer to the camera than its middle, so its width takes a wider angle
	const auto across =
	    ((halfSize.x + k_Margin) / portable_math::Tan(halfAngles.x)) + ((halfSize.y + k_Margin) * portable_math::Cos(k_Pitch));
	const auto upAndDown = (halfSize.y + k_Margin) * portable_math::Sin(k_Pitch) / portable_math::Tan(halfAngles.y);
	const auto distance = distanceFactor * std::max({across, upAndDown, k_MinDistance});
	return {
	    .origin = centre + glm::vec3(0.0f, distance * portable_math::Sin(k_Pitch), -distance * portable_math::Cos(k_Pitch)),
	    .focus = centre,
	};
}

CameraPlacement testbed_scenarios::Follow(glm::vec3 position, float height, float distanceFactor)
{
	constexpr float k_MinDistance = 25.0f;
	const glm::vec3 focus = position + glm::vec3(0.0f, height * 0.5f, 0.0f);
	const auto distance = distanceFactor * std::max(height * 3.5f, k_MinDistance);
	return {.origin = focus + glm::vec3(0.0f, distance * 0.45f, -distance * 0.9f), .focus = focus};
}

CameraPlacement testbed_scenarios::Head(glm::vec3 position, glm::vec2 ahead, float height, float distanceFactor)
{
	const auto length = glm::length(ahead);
	const auto forward = length > 0.0f ? ahead / length : glm::vec2(0.0f, -1.0f);
	const glm::vec3 forward3 {forward.x, 0.0f, forward.y};
	// The head is high on the body and a little forward of its middle
	const glm::vec3 focus = position + glm::vec3(0.0f, height * 0.75f, 0.0f) + (forward3 * (height * 0.3f));
	const auto distance = distanceFactor * height;
	return {.origin = focus + (forward3 * distance) + glm::vec3(0.0f, height * 0.15f, 0.0f), .focus = focus};
}

Bounds testbed_scenarios::BoundsOf(std::span<const glm::vec2> points, glm::vec2 minimumHalfSize)
{
	if (points.empty())
	{
		return {.centre = glm::vec2(0.0f), .halfSize = minimumHalfSize};
	}
	auto low = points.front();
	auto high = points.front();
	for (const auto& point : points)
	{
		low = glm::min(low, point);
		high = glm::max(high, point);
	}
	return {.centre = (low + high) * 0.5f, .halfSize = glm::max((high - low) * 0.5f, minimumHalfSize)};
}

void testbed_scenarios::Apply(const NeedOverrides& overrides, creature_physiology::Needs& needs, float size)
{
	const auto set = [](float& value, const std::optional<float>& wanted, float low, float high) {
		if (wanted.has_value())
		{
			value = std::clamp(*wanted, low, high);
		}
	};
	set(needs.energy, overrides.energy, 0.0f, std::max(1.0f, size));
	set(needs.exhaustion, overrides.exhaustion, 0.0f, 1.0f);
	set(needs.dehydration, overrides.dehydration, 0.0f, 1.0f);
	set(needs.poo, overrides.poo, 0.0f, 1.0f);
	set(needs.life, overrides.life, 0.0f, 1.0f);
	set(needs.warmth, overrides.warmth, -1.0f, 1.0f);
	if (overrides.age.has_value())
	{
		needs.age = *overrides.age;
	}
}

void testbed_scenarios::Apply(std::span<const DesireOverride> overrides, creature_desires::Desires& desires)
{
	for (const auto& override : overrides)
	{
		auto& state = desires[override.desire];
		state.activated = true;
		state.value = std::clamp(override.fraction, 0.0f, 1.0f) * std::max(state.max, 0.0f);
	}
}

std::vector<size_t> testbed_scenarios::GivesAt(const std::map<uint32_t, std::vector<size_t>>& gives, uint32_t turn,
                                               size_t commands)
{
	std::vector<size_t> given;
	if (const auto found = gives.find(turn); found != gives.end())
	{
		std::ranges::copy_if(found->second, std::back_inserter(given), [commands](size_t place) { return place < commands; });
	}
	return given;
}

std::vector<size_t> testbed_scenarios::Advance(Timeline& timeline, std::span<const Command> commands,
                                               std::optional<size_t> repeatFrom, float seconds,
                                               const std::function<bool(const Command& command)>& isFree)
{
	std::vector<size_t> due;
	if (commands.empty() || timeline.next >= commands.size())
	{
		timeline.done = true;
	}
	if (timeline.done)
	{
		return due;
	}
	timeline.seconds += seconds;
	timeline.sinceGiven += seconds;
	// At most a round of the commands at once, so commands that go round again without waiting can't run away
	while (!timeline.done && due.size() < commands.size())
	{
		const auto& command = commands[timeline.next];
		if (command.waitUntilFree && !timeline.freed)
		{
			if (timeline.sinceGiven < k_SettleSeconds || !isFree(command))
			{
				// Its delay counts from when the creature is free
				timeline.seconds = 0.0f;
				break;
			}
			timeline.freed = true;
		}
		if (timeline.seconds < command.delaySeconds)
		{
			break;
		}
		due.push_back(timeline.next);
		timeline.seconds = 0.0f;
		timeline.sinceGiven = 0.0f;
		timeline.freed = false;
		if (++timeline.next >= commands.size())
		{
			if (repeatFrom.has_value() && *repeatFrom < commands.size())
			{
				timeline.next = *repeatFrom;
			}
			else
			{
				timeline.done = true;
			}
		}
	}
	return due;
}

bool testbed_scenarios::KeepsDispenserGrid(const Scenario& scenario)
{
	if (!scenario.environment.dispenserGrid)
	{
		return false;
	}
	const auto onGrid = [](const auto& setup) { return testbed_dispensers::InGridArea(setup.offset); };
	return std::ranges::none_of(scenario.creatures, onGrid) && std::ranges::none_of(scenario.objects, onGrid);
}
