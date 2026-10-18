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

#include <string>
#include <vector>

#include <entt/meta/context.hpp>
#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "3D/WaterRings.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/Systems/Implementations/CinematicDirectorSystem.h"
#include "ECS/Systems/Implementations/CreatureAudioSystem.h"
#include "ECS/Systems/Implementations/DialogueControlSystem.h"
#include "ECS/Systems/Implementations/FireSystem.h"
#include "ECS/Systems/Implementations/ScriptControlSystem.h"
#include "ECS/Systems/Implementations/SnowSystem.h"
#include "ECS/Systems/Implementations/WaterRingSystem.h"
#include "ECS/Systems/Implementations/WeatherSystem.h"
#include "ECS/Systems/SavedStateInterface.h"

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
	EXPECT_TRUE(loaded.IsCloseClipping());
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

TEST(SavedSystemState, AFireListWithNoFireComesBack)
{
	FireSystem saved;
	FireSystem loaded;
	LoadInto(loaded, StateOf(saved));
}
