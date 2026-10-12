/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <MindFile.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "Creature/CreatureDesires.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "Enums.h"
#include "Locator.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "ScriptHeaders/ScriptPropertyRules.h"

using namespace openblack;
using creature_desires::Desire;

// The natives the guide creature and the Ogre are set up with: their names, their desires, the reaction the guide
// makes, and how a creature is made from another

namespace
{
/// Every object a script names is itself
class GuideScriptObjects final: public ecs::systems::ScriptObjectsSystemInterface
{
public:
	bool Register(entt::entity, bool) override { return true; }
	void EnterNative(uint32_t) override {}
	entt::entity Fetch(entt::entity object) override { return object; }
	void AddReference(entt::entity) override {}
	void RemoveReference(entt::entity) override {}
	void ReleaseFromScript(entt::entity) override {}
	void Replace(entt::entity, entt::entity) override {}
	void Reset() override {}
	void ReleaseUnreferenced() override {}
	bool Disband(entt::entity) override { return false; }
	[[nodiscard]] std::vector<ecs::systems::ScriptObjectPlace> Places() const override { return {}; }
	[[nodiscard]] uint32_t TimesFull() const override { return 0; }
	void ClearForNewLand() override {}
};

/// One creature's desires, which the natives change, and the priority last given to what it carries out
class GuideMinds final: public ecs::systems::CreatureMindSystemInterface
{
public:
	void ProcessTurn() override {}
	void PlanTurn() override {}
	void LearnTurn() override {}
	void LoadMind(entt::entity, std::shared_ptr<const creaturemind::MindFileData>) override {}
	[[nodiscard]] std::optional<creaturemind::MindFileData> SaveMind(entt::entity) const override { return std::nullopt; }
	void ClearLearning(entt::entity) override {}
	void SeeSkill(const glm::vec3&, size_t) override {}
	void SeeMiracle(const glm::vec3&, size_t) override {}
	void PlayerDid(size_t, const glm::vec3&, std::optional<entt::entity>, std::optional<PlayerNames>) override {}
	[[nodiscard]] const creature_mind_tables::Tables* GetTables() override { return nullptr; }
	bool PlayAction(entt::entity, size_t) override { return false; }
	bool PlayGesture(entt::entity, size_t) override { return false; }
	void PullFace(entt::entity, size_t) override {}
	std::optional<creature_face::Request> ShowFeeling(entt::entity, creature_face::Cue) override { return std::nullopt; }
	bool SitDown(entt::entity) override { return false; }
	void StandUp(entt::entity) override {}
	void ReceiveFeedback(entt::entity, float) override {}
	bool ForceAction(entt::entity, size_t, bool, std::optional<creature_face::Request>, float) override { return false; }
	bool Sleep(entt::entity) override { return false; }
	bool Eat(entt::entity, std::optional<entt::entity>) override { return false; }
	bool Drink(entt::entity) override { return false; }
	bool Poo(entt::entity) override { return false; }
	bool Puke(entt::entity) override { return false; }
	bool Faint(entt::entity) override { return false; }
	void Wake(entt::entity) override {}
	void FoughtFight(entt::entity, bool) override {}
	void ChangeDesires(entt::entity creature, const std::function<void(creature_desires::Desires&)>& change) override
	{
		changed = creature;
		change(desires);
	}
	void SetAgendaPriority(entt::entity creature, float value) override { priority = std::pair(creature, value); }

	creature_desires::Desires desires;
	entt::entity changed {entt::null};
	std::optional<std::pair<entt::entity, float>> priority;
};

/// The reactions made and taken away
class GuideReactions final: public ecs::systems::ReactionSystemInterface
{
public:
	uint32_t Create(const Source& source) override
	{
		made.push_back(source);
		return static_cast<uint32_t>(made.size());
	}
	void Move(uint32_t, const glm::vec3&, const glm::vec3&, float) override {}
	[[nodiscard]] std::optional<Active> Find(uint32_t) const override { return std::nullopt; }
	void RemoveFrom(entt::entity) override {}
	void RemoveFrom(entt::entity initiator, Reaction type) override { removed.emplace_back(initiator, type); }
	void Remove(uint32_t) override {}
	void SetInitiator(uint32_t, entt::entity) override {}
	[[nodiscard]] bool IsActive(uint32_t) const override { return false; }
	void StopReacting(entt::entity) override {}
	[[nodiscard]] bool HasReaction(entt::entity) const override { return false; }
	void ProcessTurn() override {}
	void Reset() override {}
	void SetLandBalance(Balance, float) override {}
	[[nodiscard]] float GetLandBalance(Balance) const override { return 1.0f; }
	[[nodiscard]] std::span<const Active> GetReactions() const override { return {}; }
	[[nodiscard]] std::vector<Active> ReactionsAt(const glm::vec3&) const override { return {}; }

	std::vector<Source> made;
	std::vector<std::pair<entt::entity, Reaction>> removed;
};

using Results = std::vector<std::pair<lhvm::VMValue, lhvm::DataType>>;

class ScriptGuideOgreNatives: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (spdlog::get("scripting") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		Locator::entitiesRegistry::emplace();
		Locator::scriptObjects::emplace<GuideScriptObjects>();
		_minds = &static_cast<GuideMinds&>(Locator::creatureMindSystem::emplace<GuideMinds>());
		_reactions = &static_cast<GuideReactions&>(Locator::reactionSystem::emplace<GuideReactions>());
		Locator::vm::emplace();
		Locator::vm::value().Initialise(&_api.GetFunctionsTable(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
		auto& registry = Locator::entitiesRegistry::value();
		// A script's object 0 is none
		[[maybe_unused]] const auto none = registry.Create();
		_creature = registry.Create();
		registry.Assign<ecs::components::Creature>(_creature);
		registry.Assign<ecs::components::Transform>(_creature, glm::vec3(10.0f, 2.0f, 30.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		_rock = registry.Create();
		registry.Assign<ecs::components::Transform>(_rock, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		for (auto& state : _minds->desires.desires)
		{
			state.sources = {{.type = creature_desires::sources::k_HungerFromLowEnergy, .value = 0.2f}};
		}
	}

	void TearDown() override
	{
		Locator::vm::reset();
		Locator::reactionSystem::reset();
		Locator::creatureMindSystem::reset();
		Locator::scriptObjects::reset();
		Locator::entitiesRegistry::reset();
	}

	[[nodiscard]] uint32_t Native(std::string_view name, int stackIn)
	{
		const auto& table = _api.GetFunctionsTable();
		const auto found = std::ranges::find_if(
		    table, [name, stackIn](const auto& function) { return function.name == name && function.stackIn == stackIn; });
		return static_cast<uint32_t>(std::distance(table.begin(), found));
	}

	void Call(std::string_view name, const std::vector<lhvm::NativeArgument>& arguments)
	{
		Results results;
		EXPECT_TRUE(Locator::vm::value().CallNative(Native(name, static_cast<int>(arguments.size())), arguments, results));
		EXPECT_TRUE(results.empty());
	}

	static lhvm::NativeArgument Object(entt::entity object)
	{
		return std::pair(lhvm::VMValue(static_cast<uint32_t>(object)), lhvm::DataType::Object);
	}
	static lhvm::NativeArgument Int(int32_t value) { return std::pair(lhvm::VMValue(value), lhvm::DataType::Int); }
	static lhvm::NativeArgument Float(float value) { return std::pair(lhvm::VMValue(value), lhvm::DataType::Float); }

	chlapi::CHLApi _api;
	GuideMinds* _minds {nullptr};
	GuideReactions* _reactions {nullptr};
	entt::entity _creature {entt::null};
	entt::entity _rock {entt::null};
};
} // namespace

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptGuideOgreNatives, TakeAndGiveWhatTheGameDoes)
{
	const auto& table = _api.GetFunctionsTable();
	const std::array<std::tuple<std::string_view, uint32_t, int, uint32_t>, 9> natives {{
	    {"CREATURE_CREATE_RELATIVE_TO_CREATURE", 69, 6, 1},
	    {"CREATURE_SET_AGENDA_PRIORITY", 72, 2, 0},
	    {"CREATURE_SET_DESIRE_VALUE", 77, 3, 0},
	    {"CREATURE_SET_DESIRE_ACTIVATED", 78, 3, 0},
	    {"CREATURE_SET_DESIRE_ACTIVATED", 79, 2, 0},
	    {"CREATURE_SET_DESIRE_MAXIMUM", 80, 3, 0},
	    {"CREATE_REACTION", 90, 2, 0},
	    {"REMOVE_REACTION_OF_TYPE", 258, 2, 0},
	    {"SET_CREATURE_NAME", 343, 2, 0},
	}};
	for (const auto& [name, number, in, out] : natives)
	{
		EXPECT_EQ(Native(name, in), number) << name;
		EXPECT_EQ(table[number].stackIn, in) << name;
		EXPECT_EQ(table[number].stackOut, out) << name;
	}
}

// "if MyCreature type CREATURE CREATURE_TYPE_COW": a creature is of its species' subtype
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptGuideOgreNatives, ACreatureIsOfItsSpecies)
{
	Locator::entitiesRegistry::value().Get<ecs::components::Creature>(_creature).species = CreatureType::Cow;
	const auto isOfType = [this](int32_t subtype) {
		const std::vector<lhvm::NativeArgument> arguments {
		    Object(_creature), Int(static_cast<int32_t>(script::ObjectType::Creature)), Int(subtype)};
		Results results;
		EXPECT_TRUE(Locator::vm::value().CallNative(Native("IS_OF_TYPE", 3), arguments, results));
		return !results.empty() && results.front().first.floatVal != 0.0f;
	};
	EXPECT_TRUE(isOfType(1));    // CREATURE_TYPE_COW
	EXPECT_FALSE(isOfType(2));   // CREATURE_TYPE_TIGER
	EXPECT_TRUE(isOfType(5000)); // any
}

// The Ogre's desires: all off, then hunger and anger on, wanted a little and no more
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptGuideOgreNatives, TheOgresDesires)
{
	const auto hunger = static_cast<int32_t>(Desire::Hunger);
	const auto anger = static_cast<int32_t>(Desire::Anger);
	Call("CREATURE_SET_DESIRE_ACTIVATED", {Object(_creature), Int(0)});
	EXPECT_EQ(_minds->changed, _creature);
	EXPECT_TRUE(std::ranges::none_of(_minds->desires.desires, [](const auto& state) { return state.activated; }));
	Call("CREATURE_SET_DESIRE_ACTIVATED", {Object(_creature), Int(hunger), Int(1)});
	Call("CREATURE_SET_DESIRE_VALUE", {Object(_creature), Int(hunger), Float(0.1f)});
	Call("CREATURE_SET_DESIRE_MAXIMUM", {Object(_creature), Int(hunger), Float(0.1f)});
	Call("CREATURE_SET_DESIRE_ACTIVATED", {Object(_creature), Int(anger), Int(1)});
	Call("CREATURE_SET_DESIRE_VALUE", {Object(_creature), Int(anger), Float(0.05f)});
	Call("CREATURE_SET_DESIRE_MAXIMUM", {Object(_creature), Int(anger), Float(0.05f)});
	const auto& desires = _minds->desires;
	EXPECT_TRUE(desires[Desire::Hunger].activated);
	EXPECT_TRUE(desires[Desire::Anger].activated);
	EXPECT_FALSE(desires[Desire::Play].activated);
	EXPECT_FLOAT_EQ(desires[Desire::Hunger].value, 0.1f);
	EXPECT_FLOAT_EQ(desires[Desire::Hunger].max, 0.1f);
	EXPECT_FLOAT_EQ(desires[Desire::Anger].value, 0.05f);
	EXPECT_FLOAT_EQ(desires[Desire::Anger].max, 0.05f);
	// A desire past the last changes nothing
	Call("CREATURE_SET_DESIRE_VALUE", {Object(_creature), Int(40), Float(0.5f)});
	// Not a creature: its desires aren't asked for
	_minds->changed = entt::null;
	Call("CREATURE_SET_DESIRE_VALUE", {Object(_rock), Int(hunger), Float(0.5f)});
	EXPECT_TRUE(_minds->changed == entt::null);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptGuideOgreNatives, AgendaPriorityGoesToTheCreaturesMind)
{
	Call("CREATURE_SET_AGENDA_PRIORITY", {Object(_creature), Float(0.00001f)});
	ASSERT_TRUE(_minds->priority.has_value());
	EXPECT_EQ(_minds->priority->first, _creature);
	EXPECT_FLOAT_EQ(_minds->priority->second, 0.00001f);
	_minds->priority.reset();
	Call("CREATURE_SET_AGENDA_PRIORITY", {Object(_rock), Float(1.0f)});
	EXPECT_FALSE(_minds->priority.has_value());
}

// "attach reaction Guide REACTION_FLEE_FROM_OBJECT" and its detaching
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptGuideOgreNatives, AReactionIsMadeAtTheThingAndTakenAwayByKind)
{
	Call("CREATE_REACTION", {Object(_creature), Int(static_cast<int32_t>(Reaction::FleeFromObject))});
	ASSERT_EQ(_reactions->made.size(), 1u);
	const auto& made = _reactions->made.front();
	EXPECT_EQ(made.initiator, _creature);
	EXPECT_EQ(made.type, Reaction::FleeFromObject);
	EXPECT_EQ(made.position, glm::vec3(10.0f, 2.0f, 30.0f));
	// It belongs to no one and starts its time when first taken up
	EXPECT_TRUE(made.playerless);
	EXPECT_FALSE(made.onCast);
	// The kind is a byte, its last number none
	Call("CREATE_REACTION", {Object(_creature), Int(255)});
	Call("CREATE_REACTION", {Object(static_cast<entt::entity>(0)), Int(0)});
	EXPECT_EQ(_reactions->made.size(), 1u);

	Call("REMOVE_REACTION_OF_TYPE", {Object(_creature), Int(static_cast<int32_t>(Reaction::FleeFromObject))});
	ASSERT_EQ(_reactions->removed.size(), 1u);
	EXPECT_EQ(_reactions->removed.front(), std::pair(_creature, Reaction::FleeFromObject));
}

// Exactly all of a desire also fills what drives it, but anger from being hurt and restoring health from its life
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureDesiresFromScript, AllOfADesireFillsItsSources)
{
	namespace sources = creature_desires::sources;
	creature_desires::Desires desires;
	desires[Desire::Anger].sources = {{.type = sources::k_AngerFromDamage, .value = 0.2f},
	                                  {.type = sources::k_InnateAggression, .value = 0.2f}};
	desires[Desire::RestoreHealth].sources = {{.type = sources::k_RestoreHealthFromLife, .value = 0.3f}};
	// Another desire with a source of the same kind is filled too
	desires[Desire::Play].sources = {{.type = sources::k_InnateAggression, .value = 0.1f}};

	creature_desires::SetValueFromScript(desires, Desire::Anger, 1.5f);
	EXPECT_FLOAT_EQ(desires[Desire::Anger].value, 1.0f);
	EXPECT_FLOAT_EQ(desires[Desire::Anger].sources[1].value, 0.2f);

	creature_desires::SetValueFromScript(desires, Desire::Anger, -0.5f);
	EXPECT_FLOAT_EQ(desires[Desire::Anger].value, 0.0f);

	creature_desires::SetValueFromScript(desires, Desire::Anger, 1.0f);
	EXPECT_FLOAT_EQ(desires[Desire::Anger].value, 1.0f);
	EXPECT_FLOAT_EQ(desires[Desire::Anger].sources[0].value, 0.2f);
	EXPECT_FLOAT_EQ(desires[Desire::Anger].sources[1].value, 1.0f);
	EXPECT_FLOAT_EQ(desires[Desire::Play].sources[0].value, 1.0f);

	creature_desires::SetValueFromScript(desires, Desire::RestoreHealth, 1.0f);
	EXPECT_FLOAT_EQ(desires[Desire::RestoreHealth].sources[0].value, 0.3f);
}

// "SPEED of Guide = 40.5": a creature takes a share of its top speed, all of it at most
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureScriptProperties, SpeedIsAShareOfTopSpeed)
{
	using script::property_rules::CreatureSpeedFraction;
	EXPECT_FLOAT_EQ(CreatureSpeedFraction(40.5f), 1.0f);
	EXPECT_FLOAT_EQ(CreatureSpeedFraction(8.0f), 1.0f);
	EXPECT_FLOAT_EQ(CreatureSpeedFraction(0.5f), 0.5f);
	EXPECT_FLOAT_EQ(CreatureSpeedFraction(-1.0f), 0.0f);
}

// The Ogre is made from the player's creature at its scale, and belongs to the first player with no creature
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureScriptProperties, ACopyTakesItsSizeAndStrengthAndAnOwner)
{
	using namespace script::property_rules;
	const auto body = CopyBody(1.5f, 0.6f, 1.0f);
	EXPECT_FLOAT_EQ(body.size, 1.5f);
	EXPECT_FLOAT_EQ(body.strength, 0.6f);
	const auto bigger = CopyBody(1.5f, 0.6f, 2.0f);
	EXPECT_FLOAT_EQ(bigger.size, 3.0f);
	EXPECT_FLOAT_EQ(bigger.strength, 1.2f);

	std::array<bool, 7> have {true, false, false, false, false, false, false};
	EXPECT_EQ(OwnerOfCopy(have), PlayerNames::PLAYER_TWO);
	have = {false, true, true, true, true, true, true};
	EXPECT_EQ(OwnerOfCopy(have), PlayerNames::PLAYER_ONE);
	have.fill(true);
	EXPECT_EQ(OwnerOfCopy(have), std::nullopt);
}
