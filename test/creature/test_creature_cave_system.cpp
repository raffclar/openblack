/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The Creature Cave's system on fakes: what it knows of the player's creature, the tattoo editor's edits through the
// tattoo service, and the screen following the temple's creature room, or shown on its own without a temple. The
// cave is only reached by F5 and the temple's doors, which no recorded run presses, so these tests stand in for them.

#define LOCATOR_IMPLEMENTATIONS

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "3D/CreatureBody.h"
#include "3D/OrientedText.h"
#include "3D/TempleDoors.h"
#include "3D/TempleInteriorInterface.h"
#include "Creature/CreatureFightHud.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureWatching.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/Implementations/CreatureCaveSystem.h"
#include "Input/GameActionMapInterface.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using input::BindableActionMap;
using input::UnbindableActionMap;
using openblack::ecs::systems::CreatureCaveSystem;

namespace
{
/// F5 held, and pressed this frame
class FakeActions final: public input::GameActionInterface
{
public:
	void PressCreatureRoom(bool pressed) { _pressed = pressed; }

	[[nodiscard]] bool GetBindable(BindableActionMap action) const override
	{
		return _pressed && action == BindableActionMap::ZOOM_TO_CREATURE_ROOM;
	}
	[[nodiscard]] bool GetUnbindable(UnbindableActionMap) const override { return false; }
	[[nodiscard]] bool GetBindableChanged(BindableActionMap action) const override { return GetBindable(action); }
	[[nodiscard]] bool GetUnbindableChanged(UnbindableActionMap) const override { return false; }
	[[nodiscard]] bool GetBindableRepeat(BindableActionMap) const override { return false; }
	[[nodiscard]] bool GetUnbindableRepeat(UnbindableActionMap) const override { return false; }
	[[nodiscard]] glm::uvec2 GetMousePosition() const override { return {}; }
	[[nodiscard]] glm::ivec2 GetMouseDelta() const override { return {}; }
	[[nodiscard]] std::array<std::optional<glm::vec3>, 2> GetHandPositions() const override { return {}; }
	void Frame() override {}
	void ProcessEvent(const SDL_Event&) override {}

private:
	bool _pressed {false};
};

/// Creature Mode as far as the cave asks of it: the player's creature, and whether it was left
class FakeCreatureMode final: public ecs::systems::CreatureModeSystemInterface
{
public:
	std::optional<entt::entity> playersCreature;
	int leaves {0};

	void Update(std::chrono::microseconds, const Frame&) override {}
	void PressCreatureKey() override {}
	bool Enter(entt::entity) override { return false; }
	void Leave() override { ++leaves; }
	[[nodiscard]] bool IsActive() const override { return false; }
	[[nodiscard]] std::optional<entt::entity> GetCreature() const override { return std::nullopt; }
	[[nodiscard]] std::optional<entt::entity> PlayersCreature() const override { return playersCreature; }
	[[nodiscard]] std::optional<creature_follow::View> GetView() const override { return std::nullopt; }
	void ClearView() override {}
};

/// The temple as far as the cave asks of it: whether the player is in it, in which room, and where it was sent
class FakeTemple final: public TempleInteriorInterface
{
public:
	bool active {false};
	TempleRoom room {TempleRoom::Main};
	std::optional<TempleRoom> activatedIn;
	std::optional<TempleRoom> wentTo;
	bool leaveRequested {false};

	[[nodiscard]] bool Active() const override { return active; }
	[[nodiscard]] glm::vec3 GetPosition() const override { return {}; }
	[[nodiscard]] TempleRoom GetCurrentRoom() const override { return room; }
	void SetCurrentRoom(TempleRoom newRoom) override { room = newRoom; }
	[[nodiscard]] std::optional<TempleRoom> GetTransitionRoom() const override { return std::nullopt; }
	void SetTransitionRoom(std::optional<TempleRoom>) override {}
	void GoToRoom(TempleRoom newRoom) override { wentTo = newRoom; }
	void EnterRoom(TempleRoom newRoom) override { wentTo = newRoom; }
	[[nodiscard]] bool IsRoomDrawn(TempleRoom drawn) const override { return active && drawn == room; }
	[[nodiscard]] TempleDoors& GetDoors() override { return _doors; }
	[[nodiscard]] const TempleDoors& GetDoors() const override { return _doors; }
	[[nodiscard]] std::optional<TempleCursorHit> GetCursorHit() const override { return std::nullopt; }
	void SetInterface(gui::GameInterface*) override {}
	bool HoldControl(bool, float) override { return false; }
	[[nodiscard]] std::vector<TempleSubMeshTexture> GetScrollTextures(TempleRoom) const override { return {}; }
	[[nodiscard]] std::vector<uint32_t> GetHiddenSubMeshes(TempleRoom) const override { return {}; }
	[[nodiscard]] std::vector<TempleSubMeshGlow> GetControlGlows(TempleRoom) const override { return {}; }
	[[nodiscard]] const std::vector<OrientedTextVertex>& GetText() const override { return _text; }
	[[nodiscard]] const graphics::Texture2D* GetTextTexture() const override { return nullptr; }
	[[nodiscard]] const std::vector<OrientedTextVertex>& GetMap() const override { return _text; }
	[[nodiscard]] uint32_t GetVisits() const override { return 0; }
	[[nodiscard]] const std::vector<TempleMapMarker>& GetMapMarkers() const override { return _markers; }
	[[nodiscard]] const TempleLight& GetLight() const override { return _light; }
	[[nodiscard]] const std::vector<TempleCaveTrophy>& GetCaveTrophies() const override { return _trophies; }
	[[nodiscard]] float GetMapMarkerTurn() const override { return 0.0f; }
	[[nodiscard]] float GetPoolTime() const override { return 0.0f; }
	void Escape() override {}
	void RequestLeave() override { leaveRequested = true; }
	void FadeToWhite() override {}
	void Update(std::chrono::microseconds) override {}
	void ProcessTurn() override {}
	[[nodiscard]] glm::vec2 GetWaterfallSlide() const override { return {}; }
	void Activate() override { Activate(TempleRoom::Main); }
	void Activate(TempleRoom newRoom) override { activatedIn = newRoom; }
	void Deactivate() override {}

private:
	TempleDoors _doors {[](const temple_sounds::Options&) {}};
	std::vector<OrientedTextVertex> _text;
	std::vector<TempleMapMarker> _markers;
	std::vector<TempleCaveTrophy> _trophies;
	TempleLight _light;
};

/// A land with the player's creature, Creature Mode that finds it, F5, and no temple unless a test makes one; all put
/// back as they were afterwards
class CreatureCaveSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::temple::reset();
		_actions = &static_cast<FakeActions&>(Locator::gameActionSystem::emplace<FakeActions>());
		_mode = &static_cast<FakeCreatureMode&>(Locator::creatureModeSystem::emplace<FakeCreatureMode>());
		_registry = &Locator::entitiesRegistry::emplace<ecs::Registry>();
		_creature = _registry->Create();
		_registry->Assign<ecs::components::Creature>(_creature, ecs::components::Creature {
		                                                            .owner = PlayerNames::PLAYER_ONE,
		                                                            .species = CreatureType::Cow,
		                                                            .alignment = -0.25f,
		                                                            .fatness = 0.75f,
		                                                            .strength = 0.6f,
		                                                            .size = 1.0f,
		                                                        });
		_mode->playersCreature = _creature;
	}

	FakeTemple& MakeTemple() { return static_cast<FakeTemple&>(Locator::temple::emplace<FakeTemple>()); }

	FakeActions* _actions {nullptr};
	FakeCreatureMode* _mode {nullptr};
	ecs::Registry* _registry {nullptr};
	entt::entity _creature {entt::null};

private:
	test::RestoreService<Locator::temple> _restoreTemple;
	test::RestoreService<Locator::gameActionSystem> _restoreActions;
	test::RestoreService<Locator::creatureModeSystem> _restoreMode;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
};
} // namespace

TEST_F(CreatureCaveSystemTest, WithoutACreatureThereIsNothingToTell)
{
	// As the game has it until the leash service finds the players' creatures
	_mode->playersCreature.reset();
	CreatureCaveSystem cave;
	EXPECT_FALSE(cave.GetCreature().has_value());
	EXPECT_FALSE(cave.Snapshot().has_value());
	EXPECT_FALSE(cave.ApplyTattoo(0, 0, {}));
	EXPECT_FALSE(cave.RemoveTattoo(0));
	EXPECT_FALSE(cave.IsOpen());
}

TEST_F(CreatureCaveSystemTest, TheBodyAndItsNeedsWithoutAMind)
{
	ecs::components::CreatureNeeds needs;
	needs.needs.age = 7;
	needs.needs.life = 0.5f;
	_registry->Assign<ecs::components::CreatureNeeds>(_creature, needs);
	CreatureCaveSystem cave;
	ASSERT_EQ(cave.GetCreature(), _creature);
	const auto snapshot = cave.Snapshot();
	ASSERT_TRUE(snapshot.has_value());
	EXPECT_EQ(snapshot->name, creature_fight_hud::SpeciesName(CreatureType::Cow));
	EXPECT_FLOAT_EQ(snapshot->strength, 0.6f);
	EXPECT_FLOAT_EQ(snapshot->fatness, 0.75f);
	EXPECT_FLOAT_EQ(snapshot->alignment, -0.25f);
	EXPECT_EQ(snapshot->needs.age, 7u);
	EXPECT_FLOAT_EQ(snapshot->needs.life, 0.5f);
	EXPECT_TRUE(snapshot->likes.empty());
	EXPECT_TRUE(snapshot->opinions.empty());
}

TEST_F(CreatureCaveSystemTest, TheMindWithoutItsTablesNamesTheActionsByNumber)
{
	ecs::components::CreatureMindState mind;
	mind.attitudeToPlayer = 0.4f;
	mind.secondsAlone = 30.0f;
	creature_mind_model::Learnt learnt;
	learnt.name = u"Bob";
	learnt.opinions = {0.5f, -0.25f};
	learnt.creatures.resize(2);
	mind.learnt = learnt;
	_registry->Assign<ecs::components::CreatureMindState>(_creature, std::move(mind));
	CreatureCaveSystem cave;
	const auto snapshot = cave.Snapshot();
	ASSERT_TRUE(snapshot.has_value());
	EXPECT_EQ(snapshot->name, u"Bob");
	EXPECT_FLOAT_EQ(snapshot->attitudeToPlayer, 0.4f);
	EXPECT_FLOAT_EQ(snapshot->secondsAlone, 30.0f);
	EXPECT_EQ(snapshot->creaturesKnown, 2u);
	ASSERT_EQ(snapshot->opinions.size(), 2u);
	EXPECT_EQ(snapshot->opinions[0].action, "0");
	EXPECT_FLOAT_EQ(snapshot->opinions[1].opinion, -0.25f);
	// Without the tables nothing is known of its skills and miracles
	EXPECT_TRUE(snapshot->skills.empty());
	EXPECT_TRUE(snapshot->miracles.empty());
}

TEST_F(CreatureCaveSystemTest, TheMindsTablesNameItsActionsSkillsAndMiracles)
{
	creature_mind_tables::Tables tables;
	tables.actions = {{.name = "dance"}, {.name = "eat"}};
	tables.skills = {{.name = "build"}, {.name = "field"}};
	tables.miracles = {{.name = "none"}, {.name = "fireball"}, {.name = "heal"}};
	ecs::components::CreatureMindState mind;
	creature_mind_model::Learnt learnt;
	learnt.opinions = {0.5f, -0.25f};
	learnt.knowledge.skillsKnown = {false, true};
	learnt.knowledge.miraclesKnown = {true, false, false};
	const auto needed =
	    std::max(creature_watching::TimesToLearn(tables.miracles[2].timesToSee,
	                                             creature_mind_tables::MiracleMultiplier(creature::InfoRow(CreatureType::Cow))),
	             1u);
	learnt.knowledge.miraclesSeen.resize(3);
	learnt.knowledge.miraclesSeen[2].count = needed / 2;
	mind.learnt = learnt;
	_registry->Assign<ecs::components::CreatureMindState>(_creature, std::move(mind));
	CreatureCaveSystem cave({.mindTables = [&tables]() { return &tables; }});
	const auto snapshot = cave.Snapshot();
	ASSERT_TRUE(snapshot.has_value());
	ASSERT_EQ(snapshot->opinions.size(), 2u);
	EXPECT_EQ(snapshot->opinions[0].action, "dance");
	EXPECT_EQ(snapshot->opinions[1].action, "eat");
	ASSERT_EQ(snapshot->skills.size(), 2u);
	EXPECT_FALSE(snapshot->skills[0].known);
	EXPECT_TRUE(snapshot->skills[1].known);
	ASSERT_EQ(snapshot->miracles.size(), 3u);
	EXPECT_EQ(snapshot->miracles[0].percent, 100);
	EXPECT_EQ(snapshot->miracles[1].percent, 0);
	EXPECT_EQ(snapshot->miracles[2].percent, static_cast<int32_t>((needed / 2) * 100 / needed));
}

TEST_F(CreatureCaveSystemTest, TattoosGoThroughTheTattooService)
{
	_registry->Assign<ecs::components::CreatureTattoos>(_creature);
	struct Edit
	{
		entt::entity creature;
		size_t slot;
		creature_tattoo::Slot tattoo;
	};
	std::vector<Edit> edits;
	CreatureCaveSystem cave(
	    {.setTattoo = [this, &edits](entt::entity creature, size_t slot, const creature_tattoo::Slot& tattoo) {
		    edits.push_back({creature, slot, tattoo});
		    _registry->Get<ecs::components::CreatureTattoos>(creature).slots.at(slot) = tattoo;
	    }});
	EXPECT_TRUE(cave.ApplyTattoo(3, 5, {10, 20, 30}));
	ASSERT_EQ(edits.size(), 1u);
	EXPECT_EQ(edits[0].creature, _creature);
	EXPECT_EQ(edits[0].slot, 0u);
	EXPECT_EQ(edits[0].tattoo.site, 3);
	EXPECT_EQ(edits[0].tattoo.design, 5);
	// No such symbol, and nothing on a place to take off
	EXPECT_FALSE(cave.ApplyTattoo(3, creature_tattoo::k_DesignCount, {}));
	EXPECT_FALSE(cave.RemoveTattoo(6));
	EXPECT_EQ(edits.size(), 1u);
	EXPECT_TRUE(cave.RemoveTattoo(3));
	ASSERT_EQ(edits.size(), 2u);
	EXPECT_TRUE(edits[1].tattoo.Empty());
}

TEST_F(CreatureCaveSystemTest, WithoutTheTattooServiceTheTattoosStayAsTheyAre)
{
	_registry->Assign<ecs::components::CreatureTattoos>(_creature);
	CreatureCaveSystem cave;
	EXPECT_FALSE(cave.ApplyTattoo(3, 5, {}));
	EXPECT_TRUE(_registry->Get<ecs::components::CreatureTattoos>(_creature).slots.at(0).Empty());
}

TEST_F(CreatureCaveSystemTest, InTheTempleTheScreenFollowsTheCreatureRoom)
{
	auto& temple = MakeTemple();
	CreatureCaveSystem cave;
	EXPECT_TRUE(cave.InTemple());
	cave.Update();
	EXPECT_FALSE(cave.IsOpen());
	// F5 is the temple's key there: the cave does not read it
	_actions->PressCreatureRoom(true);
	cave.Update();
	EXPECT_FALSE(cave.IsOpen());
	temple.active = true;
	cave.Update();
	EXPECT_FALSE(cave.IsOpen());
	temple.room = TempleRoom::CreatureCave;
	cave.Update();
	EXPECT_TRUE(cave.IsOpen());
	// Escape is the temple's own there
	EXPECT_FALSE(cave.Escape());
	temple.room = TempleRoom::Main;
	cave.Update();
	EXPECT_FALSE(cave.IsOpen());
}

TEST_F(CreatureCaveSystemTest, InTheTempleOpeningGoesToTheCreatureRoomAndClosingLeaves)
{
	auto& temple = MakeTemple();
	CreatureCaveSystem cave;
	cave.Open();
	EXPECT_EQ(temple.activatedIn, TempleRoom::CreatureCave);
	EXPECT_FALSE(temple.wentTo.has_value());
	// Closing outside the temple does nothing
	cave.Close();
	EXPECT_FALSE(temple.leaveRequested);
	temple.active = true;
	cave.Open();
	EXPECT_EQ(temple.wentTo, TempleRoom::CreatureCave);
	cave.Close();
	EXPECT_TRUE(temple.leaveRequested);
	EXPECT_EQ(_mode->leaves, 0);
}

TEST_F(CreatureCaveSystemTest, WithoutATempleF5ShowsTheCaveOnItsOwn)
{
	CreatureCaveSystem cave;
	EXPECT_FALSE(cave.InTemple());
	cave.Update();
	EXPECT_FALSE(cave.IsOpen());
	_actions->PressCreatureRoom(true);
	cave.Update();
	EXPECT_TRUE(cave.IsOpen());
	// Going in lets go of the creature the camera followed
	EXPECT_EQ(_mode->leaves, 1);
	cave.Update();
	EXPECT_FALSE(cave.IsOpen());
	_actions->PressCreatureRoom(false);
	cave.Open();
	EXPECT_TRUE(cave.Escape());
	EXPECT_FALSE(cave.IsOpen());
	EXPECT_FALSE(cave.Escape());
}
