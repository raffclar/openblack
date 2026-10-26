/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The running state that each system keeps beside the entities goes into a saved world and comes back the same: a save,
// a load into a new system and a save again give the same bytes.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <entt/meta/context.hpp>
#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "3D/CameraTrack.h"
#include "3D/WaterRings.h"
#include "Audio/GameMusic.h"
#include "Camera/ScriptCameraModel.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/Systems/Implementations/AnimalSystem.h"
#include "ECS/Systems/Implementations/CameraBookmarkSystem.h"
#include "ECS/Systems/Implementations/CameraHelpSystem.h"
#include "ECS/Systems/Implementations/CameraPathSystem.h"
#include "ECS/Systems/Implementations/CameraZoneSystem.h"
#include "ECS/Systems/Implementations/CinematicDirectorSystem.h"
#include "ECS/Systems/Implementations/CreatureAudioSystem.h"
#include "ECS/Systems/Implementations/CreatureHelpSystem.h"
#include "ECS/Systems/Implementations/CreatureMindSystem.h"
#include "ECS/Systems/Implementations/CreaturePhysiologySystem.h"
#include "ECS/Systems/Implementations/DialogueControlSystem.h"
#include "ECS/Systems/Implementations/ExplosionSystem.h"
#include "ECS/Systems/Implementations/FireSystem.h"
#include "ECS/Systems/Implementations/ForestSystem.h"
#include "ECS/Systems/Implementations/GuidanceSystem.h"
#include "ECS/Systems/Implementations/HelpMessageSystem.h"
#include "ECS/Systems/Implementations/HelpTextSystem.h"
#include "ECS/Systems/Implementations/InfluenceSystem.h"
#include "ECS/Systems/Implementations/LiquidParticleSystem.h"
#include "ECS/Systems/Implementations/MagicShieldSystem.h"
#include "ECS/Systems/Implementations/MagicSystem.h"
#include "ECS/Systems/Implementations/MiracleFxSystem.h"
#include "ECS/Systems/Implementations/ScriptControlSystem.h"
#include "ECS/Systems/Implementations/SnowSystem.h"
#include "ECS/Systems/Implementations/TeleportSystem.h"
#include "ECS/Systems/Implementations/TownSystem.h"
#include "ECS/Systems/Implementations/WaterRingSystem.h"
#include "ECS/Systems/Implementations/WeatherSystem.h"
#include "ECS/Systems/SavedStateInterface.h"
#include "Help/StoryHistory.h"

using namespace openblack;
using namespace openblack::ecs::systems;
namespace reflection = openblack::ecs::reflection;

namespace
{

const entt::meta_ctx& Context()
{
	static const auto context = []() {
		auto made = std::make_unique<entt::meta_ctx>();
		reflection::RegisterComponents(*made);
		return made;
	}();
	return *context;
}

std::vector<uint8_t> StateOf(const SavedStateInterface& system)
{
	reflection::BinaryWriter out(Context());
	system.SaveState(out);
	// Every value goes in: none is left out as a value the archive cannot keep
	for (const auto& unsaved : out.Unsaved())
	{
		ADD_FAILURE() << "not kept: " << unsaved.place << " (" << unsaved.type << ")";
	}
	return std::move(out.Bytes());
}

/// Loads the state of one system into another and checks that the other gives the same bytes
void LoadInto(SavedStateInterface& system, const std::vector<uint8_t>& bytes)
{
	const reflection::EntityRemap identity;
	reflection::BinaryReader in(Context(), bytes, identity);
	ASSERT_TRUE(system.LoadState(in));
	EXPECT_EQ(in.Remaining(), 0u);
	EXPECT_EQ(StateOf(system), bytes);
}

} // namespace

TEST(SavedSystemState, TheScriptsScreenComesBack)
{
	CinematicDirectorSystem saved;
	saved.FadeTo(10, 20, 30, 2);
	saved.SetWideScreen(true, 7);
	saved.SetCloseClipping(true);
	saved.SetTextColour({200, 100, 50});
	saved.WriteText(u"Hello", {0.1f, 0.2f}, {0.5f, 0.1f}, 0.05f, 1.0f);
	saved.ProcessTurn();
	CinematicDirectorSystem loaded;
	LoadInto(loaded, StateOf(saved));
	EXPECT_EQ(loaded.GetFadeColour(), saved.GetFadeColour());
	EXPECT_TRUE(loaded.IsWideScreenOn());
	EXPECT_EQ(loaded.GetWideScreenOwner(), 7u);
	// The close clipping of the scripts is not kept: the game clears it when it loads a game
	EXPECT_FALSE(loaded.IsCloseClipping());
	ASSERT_EQ(loaded.GetTexts().size(), 1u);
	EXPECT_EQ(loaded.GetTexts().front().words, u"Hello");
}

TEST(SavedSystemState, TheOwnersOfTheDialogueAndOfTheSpeedComeBack)
{
	DialogueControlSystem dialogue;
	dialogue.Request(12, 0);
	DialogueControlSystem loadedDialogue;
	LoadInto(loadedDialogue, StateOf(dialogue));
	EXPECT_EQ(loadedDialogue.GetOwner(), 12u);

	ScriptControlSystem control;
	control.StartGameSpeed(5);
	control.StoreCameraDetails({.origin = {1.0f, 2.0f, 3.0f}, .focus = {4.0f, 5.0f, 6.0f}});
	ScriptControlSystem loadedControl;
	LoadInto(loadedControl, StateOf(control));
	// Another task cannot end the speed that task 5 holds
	EXPECT_FALSE(loadedControl.EndGameSpeed(6));
	EXPECT_EQ(loadedControl.GetCameraDetails().focus, glm::vec3(4.0f, 5.0f, 6.0f));
}

TEST(SavedSystemState, TheCreaturesVoiceSwitchesComeBack)
{
	CreatureAudioSystem saved;
	saved.SetMuted(true);
	saved.SetOtherVoicesEnabled(false);
	CreatureAudioSystem loaded;
	LoadInto(loaded, StateOf(saved));
	EXPECT_TRUE(loaded.IsMuted());
}

TEST(SavedSystemState, TheSnowAndTheRingsOnTheWaterComeBack)
{
	SnowSystem snow;
	snow.Cover(42.0f);
	SnowSystem loadedSnow;
	LoadInto(loadedSnow, StateOf(snow));
	EXPECT_EQ(loadedSnow.GetDepth({1000.0f, 1000.0f}), snow.GetDepth({1000.0f, 1000.0f}));

	WaterRingSystem rings;
	ASSERT_TRUE(rings.Add({.position = {1.0f, 2.0f, 3.0f}, .age = 40, .growth = 2.0f, .cell = 0x31}));
	rings.Update(std::chrono::milliseconds(20), std::chrono::milliseconds(20));
	WaterRingSystem loadedRings;
	LoadInto(loadedRings, StateOf(rings));
	ASSERT_EQ(loadedRings.GetRings().size(), 1u);
	EXPECT_EQ(loadedRings.GetRings().front().age, rings.GetRings().front().age);
}

TEST(SavedSystemState, TheWeathersSwitchesComeBack)
{
	WeatherSystem saved;
	saved.SetClimateSystemEnabled(false);
	saved.SetStormCreationEnabled(false);
	WeatherSystem loaded;
	LoadInto(loaded, StateOf(saved));
	EXPECT_FALSE(loaded.IsClimateSystemEnabled());
	EXPECT_FALSE(loaded.IsStormCreationEnabled());
}

TEST(SavedSystemState, ALoadedGameHasCalmBackgroundWeather)
{
	WeatherSystem saved;
	saved.SetBackgroundWeather({.temperature = 11, .rain = 40, .overcast = 60});
	WeatherSystem loaded;
	loaded.SetBackgroundWeather({.temperature = 5, .snow = 20});
	LoadInto(loaded, StateOf(saved));
	const auto background = loaded.GetBackgroundWeather();
	EXPECT_EQ(background.temperature, 0);
	EXPECT_EQ(background.rain, 0);
	EXPECT_EQ(background.snow, 0);
	EXPECT_EQ(background.overcast, 0);
}

TEST(SavedSystemState, AFireListWithNoFireComesBack)
{
	FireSystem saved;
	FireSystem loaded;
	LoadInto(loaded, StateOf(saved));
}

TEST(SavedSystemState, TheShakesOfTheCameraAndTheCreaturesHelpComeBack)
{
	ExplosionSystem explosions;
	explosions.AddShake({1.0f, 2.0f, 3.0f}, 50.0f, 2.0f, 1000, false);
	ExplosionSystem loadedExplosions;
	LoadInto(loadedExplosions, StateOf(explosions));
	EXPECT_TRUE(loadedExplosions.IsShaking());

	CreatureHelpSystem help;
	help.SetHelpOn(false);
	CreatureHelpSystem loadedHelp;
	LoadInto(loadedHelp, StateOf(help));
	EXPECT_FALSE(loadedHelp.IsHelpOn());
}

TEST(SavedSystemState, TheStateOfANewLandComesBack)
{
	// Each system as a new land has it gives the same bytes after a load
	const auto check = [](auto saved, auto loaded) { LoadInto(loaded, StateOf(saved)); };
	check(AnimalSystem {}, AnimalSystem {});
	check(CreatureMindSystem {}, CreatureMindSystem {});
	check(CreaturePhysiologySystem {}, CreaturePhysiologySystem {});
	check(ForestSystem {}, ForestSystem {});
	check(GuidanceSystem {}, GuidanceSystem {});
	check(HelpMessageSystem {}, HelpMessageSystem {});
	check(InfluenceSystem {}, InfluenceSystem {});
	check(TeleportSystem {}, TeleportSystem {});
}

TEST(SavedSystemState, TheStoryHistoryComesBack)
{
	help::StoryHistory history;
	history.Add({.text = 12, .interaction = 1, .number = 2.5f, .narrator = 3});
	history.Add({.text = 40, .interaction = 0, .number = 0.0f, .narrator = 2});
	reflection::BinaryWriter out(Context());
	reflection::Save(out, history);
	help::StoryHistory loaded;
	const reflection::EntityRemap identity;
	reflection::BinaryReader in(Context(), out.Bytes(), identity);
	reflection::Load(in, loaded);
	ASSERT_FALSE(in.Failed());
	EXPECT_EQ(loaded.Count(), 2u);
	ASSERT_TRUE(loaded.Back(1).has_value());
	EXPECT_EQ(loaded.Back(1)->text, 12u);
	EXPECT_EQ(loaded.Back(1)->number, 2.5f);

	// The help's texts with no dialogue started give the same bytes after a load
	HelpTextSystem texts;
	HelpTextSystem loadedTexts;
	LoadInto(loadedTexts, StateOf(texts));
}

TEST(SavedSystemState, TheScriptsCameraComesBackAsItWas)
{
	const auto flat = [](float, float) { return 0.0f; };
	ScriptCameraModel saved({0.0f, 50.0f, 0.0f}, {10.0f, 0.0f, 10.0f}, flat);
	saved.MoveOrigin({100.0f, 60.0f, 40.0f}, 3.0f);
	saved.SetFollowSettings(30.0f, 0.5f, 1.0f, false);
	const auto bytes = [](const ScriptCameraModel& model) {
		reflection::BinaryWriter out(Context());
		reflection::Save(out, model.GetSaved());
		return std::move(out.Bytes());
	};
	const auto savedBytes = bytes(saved);
	ScriptCameraModel::Saved read;
	const reflection::EntityRemap identity;
	reflection::BinaryReader in(Context(), savedBytes, identity);
	reflection::Load(in, read);
	ASSERT_FALSE(in.Failed());
	EXPECT_EQ(in.Remaining(), 0u);
	ScriptCameraModel loaded({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, flat);
	loaded.Restore(read, {});
	EXPECT_EQ(bytes(loaded), savedBytes);
	EXPECT_EQ(loaded.GetTargetOrigin(), saved.GetTargetOrigin());
	EXPECT_FALSE(loaded.Arrived());
}

TEST(SavedSystemState, TheMagicSystemsHandAndListsComeBack)
{
	MagicSystem saved;
	saved.SetIgnoreInfluence(true);
	MagicSystem loaded;
	LoadInto(loaded, StateOf(saved));
	EXPECT_TRUE(loaded.IsIgnoringInfluence());
	EXPECT_FALSE(loaded.GetHeldSeed().has_value());
}

TEST(SavedSystemState, TheShieldsObjectsAndTheirReactionsComeBack)
{
	const auto spell = static_cast<entt::entity>(3);
	const auto dome = static_cast<entt::entity>(4);
	// The bytes as the shield system writes them: each shield miracle's object, the reactions, and the next to spread
	reflection::BinaryWriter out(Context());
	reflection::Save(out, std::unordered_map<entt::entity, entt::entity> {{spell, dome}});
	out.WriteSize(1);
	reflection::Save(out, uint32_t {7});
	reflection::Save(out, dome);
	reflection::Save(out, static_cast<Reaction>(1));
	reflection::Save(out, 25.0f);
	reflection::Save(out, uint32_t {100});
	reflection::Save(out, std::vector<entt::entity> {static_cast<entt::entity>(9)});
	reflection::Save(out, size_t {0});
	MagicShieldSystem loaded;
	LoadInto(loaded, out.Bytes());
	EXPECT_TRUE(loaded.HasObject(spell));
	EXPECT_FALSE(loaded.HasObject(dome));
}

TEST(SavedSystemState, TheTribesNamesThatSpinComeBackWithTheRingOfTheHand)
{
	MiracleFxSystem saved;
	saved.TribalPowerColumn(Tribe::CELTIC, {1.0f, 2.0f, 3.0f}, PlayerNames::PLAYER_TWO);
	saved.StartTribalPowerRing(Tribe::NORSE);
	MiracleFxSystem loaded;
	LoadInto(loaded, StateOf(saved));
	// The ring is the hand's ring again: stopping it in both leaves the same names
	saved.StopTribalPowerRing();
	loaded.StopTribalPowerRing();
	EXPECT_EQ(StateOf(loaded), StateOf(saved));
}

TEST(SavedSystemState, ThePauseOfTheCameraPathComesBack)
{
	CameraPathSystem saved;
	saved.Pause();
	CameraPathSystem loaded;
	LoadInto(loaded, StateOf(saved));
	EXPECT_TRUE(loaded.IsPaused());
	EXPECT_FALSE(loaded.HoldsCamera());
}

TEST(SavedSystemState, TheDropsTheDustAndTheMotesComeBack)
{
	const auto sources = []() {
		return LiquidParticleSystem::Sources {.random = []() { return 7; },
		                                      .altitude = [](int32_t /*x*/, int32_t /*z*/) { return 0.0f; },
		                                      .drawDue = [](frame_pace::Pace& /*pace*/) { return true; }};
	};
	LiquidParticleSystem saved(sources());
	saved.Add({1.0f, 20.0f, 3.0f}, {0.0f, 1.0f, 0.0f}, 2.0f, 0xFF800000u, liquid_particles::Kind::Liquid);
	saved.Add({4.0f, 5.0f, 6.0f}, {1.0f, 0.0f, 0.0f}, 1.0f, 0x80FFFFFFu, liquid_particles::Kind::Dust);
	saved.UpdateFrame(0.1f, true);
	LiquidParticleSystem loaded(sources());
	LoadInto(loaded, StateOf(saved));
	ASSERT_EQ(loaded.Particles().size(), saved.Particles().size());
	for (size_t i = 0; i < saved.Particles().size(); ++i)
	{
		EXPECT_EQ(loaded.Particles()[i].position, saved.Particles()[i].position);
		EXPECT_EQ(loaded.Particles()[i].age, saved.Particles()[i].age);
		EXPECT_EQ(loaded.Particles()[i].kind, saved.Particles()[i].kind);
	}
	// Both carry on the same
	saved.UpdateFrame(0.1f, true);
	loaded.UpdateFrame(0.1f, true);
	EXPECT_EQ(StateOf(loaded), StateOf(saved));
}

TEST(SavedSystemState, TheMusicThatTheScriptsChoseComesBack)
{
	audio::GameMusic saved;
	saved.AttachMusic(7, audio::MusicType::CelticChantVox);
	saved.AttachMusic(9, audio::MusicType::ScriptPiperTune);
	saved.StartScriptMusic(audio::MusicType::NorseChant);
	saved.SetAlignmentMusicEnabled(false);
	saved.OnScriptMusicCue("L3");
	saved.OnScriptMusicCue("W");
	audio::GameMusic loaded;
	LoadInto(loaded, StateOf(saved));
	// The most recently attached stays first
	ASSERT_EQ(loaded.GetAttachedMusic().size(), 2u);
	EXPECT_EQ(loaded.GetAttachedMusic()[0].thing, 9u);
	EXPECT_EQ(loaded.GetAttachedMusic()[1].type, audio::MusicType::CelticChantVox);
	EXPECT_EQ(loaded.GetScriptType(), audio::MusicType::NorseChant);
	EXPECT_FALSE(loaded.IsAlignmentMusicEnabled());
	EXPECT_EQ(loaded.GetMusicLyrics().line, 3);
	EXPECT_EQ(loaded.GetMusicLyrics().word, 2);
	// No music plays after the load until a game turn picks it
	EXPECT_EQ(loaded.GetPlaying(), audio::MusicType::None);
}

TEST(SavedSystemState, EachVillageCentreKeepsTheEffectOfItsSymbols)
{
	const std::unordered_map<entt::entity, uint32_t> symbols {{static_cast<entt::entity>(5), 9u},
	                                                          {static_cast<entt::entity>(12), 31u}};
	reflection::BinaryWriter out(Context());
	reflection::Save(out, symbols);
	TownSystem towns;
	LoadInto(towns, out.Bytes());
}

TEST(SavedSystemState, WhatTheCameraLetsThePlayerDoComesBack)
{
	CameraHelpSystem saved;
	saved.Get().features = 0x15;
	saved.Get().autoPitch = 0.25f;
	saved.Get().autoPitchHeight = 80.0f;
	saved.Get().handReach = 123.0f;
	CameraHelpSystem loaded;
	LoadInto(loaded, StateOf(saved));
	EXPECT_EQ(loaded.Get().features, 0x15u);
	EXPECT_FLOAT_EQ(loaded.Get().autoPitch, 0.25f);
	EXPECT_FLOAT_EQ(loaded.Get().autoPitchHeight, 80.0f);
	// The game keeps no more of it
	EXPECT_FLOAT_EQ(loaded.Get().handReach, camera_help::CameraHelp {}.handReach);
}

TEST(SavedSystemState, TheCameraZonesComeBack)
{
	reflection::BinaryWriter out(Context());
	reflection::Save(out, std::string("land1.exc"));
	reflection::Save(out, false);
	reflection::Save(out, true);
	reflection::Save(out, true);
	reflection::Save(out, false);
	reflection::Save(out, 700.0f);
	reflection::Save(out, 200.0f);
	reflection::Save(out, std::vector<glm::vec3> {{1.0f, 0.0f, 2.0f}, {3.0f, 0.0f, 4.0f}, {5.0f, 0.0f, 1.0f}});
	reflection::Save(out, size_t {1});
	reflection::Save(out, glm::vec3 {10.0f, 2.0f, 20.0f});
	reflection::Save(out, 30.0f);
	reflection::Save(out, 40.0f);
	reflection::Save(out, camera_zones::Exclusion::Kind::Cylinder);
	CameraZoneSystem zones;
	LoadInto(zones, out.Bytes());
	EXPECT_EQ(zones.GetFileName(), "land1.exc");
	EXPECT_FALSE(zones.GetZones().exclusionsOn);
	EXPECT_TRUE(zones.GetZones().fenceOn);
	EXPECT_FLOAT_EQ(zones.GetZones().maxAltitude, 700.0f);
	ASSERT_EQ(zones.GetZones().fence.size(), 3u);
	ASSERT_EQ(zones.GetZones().exclusions.size(), 1u);
	EXPECT_EQ(zones.GetZones().exclusions.front().kind, camera_zones::Exclusion::Kind::Cylinder);
}

TEST(SavedSystemState, TheGoBackViewTheFollowAndTheBookmarkSwitchComeBack)
{
	reflection::BinaryWriter out(Context());
	reflection::Save(out, std::optional(glm::vec3 {100.0f, 5.0f, 200.0f}));
	reflection::Save(out, std::optional(glm::vec3 {0.0f, 60.0f, -40.0f}));
	reflection::Save(out, size_t {3});
	reflection::Save(out, std::optional(size_t {3}));
	reflection::Save(out, false);
	// The place that the town key keeps
	reflection::Save(out, std::optional(glm::vec3 {300.0f, 8.0f, 400.0f}));
	reflection::Save(out, std::optional(glm::vec3 {-24.0f, 28.0f, 47.0f}));
	CameraBookmarkSystem bookmarks;
	LoadInto(bookmarks, out.Bytes());
	EXPECT_EQ(bookmarks.GetFollowed(), std::optional(size_t {3}));
	EXPECT_FALSE(bookmarks.IsEnabled());
}

TEST(SavedSystemState, BookmarksSavedBeforeTheTownKeyComeBack)
{
	reflection::BinaryWriter out(Context());
	reflection::Save(out, std::optional<glm::vec3>());
	reflection::Save(out, std::optional<glm::vec3>());
	reflection::Save(out, size_t {2});
	reflection::Save(out, std::optional<size_t>());
	reflection::Save(out, true);
	CameraBookmarkSystem bookmarks;
	const reflection::EntityRemap identity;
	reflection::BinaryReader in(Context(), out.Bytes(), identity);
	ASSERT_TRUE(bookmarks.LoadState(in));
	EXPECT_EQ(in.Remaining(), 0u);
	EXPECT_TRUE(bookmarks.IsEnabled());
}

TEST(SavedSystemState, BookmarksWithNoGoBackViewAndNoFollowComeBack)
{
	CameraBookmarkSystem saved;
	CameraBookmarkSystem bookmarks;
	LoadInto(bookmarks, StateOf(saved));
	EXPECT_FALSE(bookmarks.GetFollowed().has_value());
	EXPECT_TRUE(bookmarks.IsEnabled());
}
