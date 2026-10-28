/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <entt/meta/context.hpp>
#include <entt/signal/sigh.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Firefly.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/Registry.h"
#include "Enums.h"
#include "Nature/Fireflies.h"
#include "Nature/FireflyWorld.h"

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/FireflySystem.h"

using namespace openblack;
using namespace openblack::fireflies;
using openblack::ecs::components::Firefly;

namespace
{
/// The hours of full night, the start and end of dusk, and full day, as the land's cycle sets them by default
constexpr std::array<float, 4> k_Hours {4.5f, 7.0f, 7.5f, 8.25f};
constexpr float k_Evening = 18.0f;
constexpr float k_Morning = 10.0f;
constexpr size_t k_Heal = 10;
constexpr size_t k_Fire = 1;

/// A land of its own: flat ground at height 0, its things kept by spot, a clock set by the test, random numbers that
/// always toss heads (1) and give half of what is asked. As in the game's map, a new firefly goes to the front of the
/// fireflies, and a firefly that moves to another cell goes to the front again. A search of a cell meets the other
/// things first.
class FakeWorld final: public FireflyWorldInterface
{
public:
	FakeWorld()
	{
		_made = registry.OnConstruct<Firefly>().connect<&FakeWorld::OnFireflyMade>(*this);
		_gone = registry.OnDestroy<Firefly>().connect<&FakeWorld::OnFireflyGone>(*this);
	}

	enum class Kind : uint8_t
	{
		Tree,
		Rock,
		Building,
	};
	struct Thing
	{
		Kind kind;
		map_coords::MapCoords spot;
		float height;
	};

	ecs::Registry registry;
	std::unordered_map<entt::entity, Thing> things;
	std::vector<std::pair<size_t, map_coords::MapCoords>> rewards;
	float hour {k_Evening};
	glm::vec3 camera {0.0f};
	std::optional<ecs::components::Sprite> look;
	/// The fireflies in the map's cells, the front first, each with the cell that it is in
	std::vector<std::pair<entt::entity, glm::ivec2>> filed;
	/// How many times a firefly that moved asked for the cell of its new place
	size_t moves {0};

	entt::entity Add(Kind kind, glm::vec2 metres, float height = 0.0f)
	{
		const auto entity = registry.Create();
		if (kind == Kind::Tree)
		{
			registry.Assign<ecs::components::Tree>(entity, TreeInfo::Oak, 1.0f);
		}
		else if (kind == Kind::Rock)
		{
			registry.Assign<ecs::components::MobileStatic>(entity, MobileStaticInfo::Rock);
		}
		things.emplace(entity, Thing {.kind = kind, .spot = map_coords::FromMetres(metres), .height = height});
		return entity;
	}
	void Remove(entt::entity thing)
	{
		things.erase(thing);
		registry.Destroy(thing);
	}

	ecs::Registry& Entities() override { return registry; }
	uint32_t GameRand(uint32_t n) override { return n > 1 ? 1 : 0; }
	float GameFloatRand(float x) override { return x * 0.5f; }
	[[nodiscard]] float VisualHour() const override { return hour; }
	[[nodiscard]] std::array<float, 4> SkyHours() const override { return k_Hours; }
	[[nodiscard]] float TurnSeconds() const override { return 0.1f; }
	[[nodiscard]] glm::vec3 ToWorld(const map_coords::MapCoords& coords) const override
	{
		const auto metres = map_coords::ToMetres(coords);
		return {metres.x, coords.altitude, metres.y};
	}
	[[nodiscard]] map_coords::MapCoords FromWorld(glm::vec3 point) const override
	{
		auto coords = map_coords::FromMetres({point.x, point.z});
		coords.altitude = point.y;
		return coords;
	}
	[[nodiscard]] std::vector<entt::entity> ThingsInCell(glm::ivec2 cell) const override
	{
		std::vector<entt::entity> found;
		for (const auto& [entity, thing] : things)
		{
			if (map_coords::Cell(thing.spot) == cell)
			{
				found.push_back(entity);
			}
		}
		for (const auto& [entity, at] : filed)
		{
			if (at == cell)
			{
				found.push_back(entity);
			}
		}
		return found;
	}
	void Moved(entt::entity thing) override
	{
		++moves;
		const auto found = std::ranges::find(filed, thing, &std::pair<entt::entity, glm::ivec2>::first);
		const auto now = map_coords::Cell(registry.Get<const Firefly>(thing).at);
		if (found != filed.end() && found->second != now)
		{
			filed.erase(found);
			filed.insert(filed.begin(), {thing, now});
		}
	}
	[[nodiscard]] std::optional<map_coords::MapCoords> SpotOf(entt::entity thing) const override
	{
		const auto found = things.find(thing);
		return found == things.end() ? std::nullopt : std::optional(found->second.spot);
	}
	[[nodiscard]] bool IsHidingPlace(entt::entity thing) const override
	{
		const auto found = things.find(thing);
		return found != things.end() && found->second.kind != Kind::Building;
	}
	[[nodiscard]] bool IsRock(entt::entity thing) const override
	{
		const auto found = things.find(thing);
		return found != things.end() && found->second.kind == Kind::Rock;
	}
	[[nodiscard]] bool IsBuilding(entt::entity thing) const override
	{
		const auto found = things.find(thing);
		return found != things.end() && found->second.kind == Kind::Building;
	}
	[[nodiscard]] float HeightOf(entt::entity thing) const override { return things.at(thing).height; }
	[[nodiscard]] std::optional<size_t> MagicKindNamed(std::string_view name) const override
	{
		if (name == "HEAL")
		{
			return k_Heal;
		}
		if (name == "FIRE")
		{
			return k_Fire;
		}
		return std::nullopt;
	}
	void MakeReward(size_t kind, const map_coords::MapCoords& spot) override { rewards.emplace_back(kind, spot); }
	[[nodiscard]] glm::vec3 CameraPosition() const override { return camera; }
	[[nodiscard]] bool InView(glm::vec3 /*point*/) const override { return true; }
	[[nodiscard]] std::optional<ecs::components::Sprite> Look() const override { return look; }

private:
	void OnFireflyMade(entt::registry& made, entt::entity entity)
	{
		filed.insert(filed.begin(), {entity, map_coords::Cell(made.get<Firefly>(entity).at)});
	}
	void OnFireflyGone(entt::registry& /*registry*/, entt::entity entity)
	{
		std::erase_if(filed, [entity](const auto& entry) { return entry.first == entity; });
	}

	entt::scoped_connection _made;
	entt::scoped_connection _gone;
};

/// Where the village stands, well inside the map
constexpr glm::vec2 k_Here {2000.0f, 2000.0f};

class Fireflies: public ::testing::Test
{
protected:
	void SetUp() override
	{
		auto world = std::make_unique<FakeWorld>();
		_world = world.get();
		_system = std::make_unique<ecs::systems::FireflySystem>(std::move(world));
	}

	[[nodiscard]] const Firefly& Only() const
	{
		EXPECT_EQ(_system->GetFireflies().size(), 1u);
		return _world->registry.Get<const Firefly>(_system->GetFireflies().front());
	}

	void Turns(int count)
	{
		for (int i = 0; i < count; ++i)
		{
			_system->ProcessTurn();
		}
	}

	FakeWorld* _world {nullptr};
	std::unique_ptr<ecs::systems::FireflySystem> _system;
};
} // namespace

TEST(FireflyRules, TheEveningSendsThemOutAndTheMorningHome)
{
	// Past the evening dusk until midnight: out
	EXPECT_EQ(SendingAt(18.0f, k_Hours), Sending::Out);
	EXPECT_EQ(SendingAt(23.9f, k_Hours), Sending::Out);
	// The evening dusk itself, and the afternoon: nothing
	EXPECT_EQ(SendingAt(16.75f, k_Hours), Sending::Nothing);
	EXPECT_EQ(SendingAt(13.0f, k_Hours), Sending::Nothing);
	// After midnight until the morning dusk ends: nothing, however dark
	EXPECT_EQ(SendingAt(2.0f, k_Hours), Sending::Nothing);
	EXPECT_EQ(SendingAt(7.2f, k_Hours), Sending::Nothing);
	// Past the morning dusk until noon: home
	EXPECT_EQ(SendingAt(7.8f, k_Hours), Sending::Home);
	EXPECT_EQ(SendingAt(11.9f, k_Hours), Sending::Home);
	EXPECT_FLOAT_EQ(NightStage(6.0f, k_Hours), 1.4f);
	EXPECT_FLOAT_EQ(NightStage(18.0f, k_Hours), 1.4f);
}

TEST(FireflyRules, FlightsTakeTheirLengthAtThreeMetresASecondAndEase)
{
	EXPECT_FLOAT_EQ(FlightSeconds(30.0f, 1.0f), 10.0f);
	EXPECT_FLOAT_EQ(FlightSeconds(1.0f, 1.4f), 0.5f);
	EXPECT_FLOAT_EQ(Ease(0.0f), 0.0f);
	EXPECT_FLOAT_EQ(Ease(0.5f), 0.5f);
	EXPECT_FLOAT_EQ(Ease(1.0f), 1.0f);
	EXPECT_FLOAT_EQ(Ease(0.25f), 0.15625f);
}

TEST(FireflyRules, TheDriftGrowsOutAndDiesHome)
{
	EXPECT_FLOAT_EQ(Amplitude(State::Resting, 0.5f), 0.0f);
	EXPECT_FLOAT_EQ(Amplitude(State::Hovering, 0.0f), 1.0f);
	EXPECT_FLOAT_EQ(Amplitude(State::FlyingOut, 0.1f), 0.5f);
	EXPECT_FLOAT_EQ(Amplitude(State::FlyingOut, 0.5f), 1.0f);
	EXPECT_FLOAT_EQ(Amplitude(State::FlyingHome, 0.5f), 1.0f);
	EXPECT_NEAR(Amplitude(State::FlyingHome, 0.9f), 0.5f, 1e-5f);
}

TEST(FireflyRules, TheLoopsAreEightMetresAndOneAcross)
{
	Drift drift;
	// All angles at nothing: the slow loop 8 m along x and the quick 1 m, both level
	const auto start = DriftOffset(drift, 0.0f, 1.0f);
	EXPECT_FLOAT_EQ(start.x, 9.0f);
	EXPECT_FLOAT_EQ(start.y, 0.0f);
	EXPECT_FLOAT_EQ(start.z, 0.0f);
	// Up angles at a quarter turn: half as high as wide
	drift.phases = {0.0f, k_TwoPi / 4.0f, 0.0f, 0.0f, k_TwoPi / 4.0f, 0.0f};
	const auto up = DriftOffset(drift, 0.0f, 1.0f);
	EXPECT_FLOAT_EQ(up.y, 4.5f);
	EXPECT_FLOAT_EQ(DriftOffset(drift, 0.0f, 0.0f).y, 0.0f);
}

TEST(FireflyRules, TheyFadeBySquaredDistanceToNothingAt300Metres)
{
	EXPECT_EQ(OpacityAt(50.0f * 50.0f), 190);
	EXPECT_EQ(OpacityAt(10000.0f), 190);
	EXPECT_EQ(OpacityAt(40000.0f), 118);
	EXPECT_EQ(OpacityAt(90000.0f), 0);
	EXPECT_FALSE(OpacityAt(90001.0f).has_value());
}

TEST(FireflyRules, TheSearchCoversA600MetreSquare)
{
	EXPECT_EQ(SearchCells(k_SearchReach), 3600);
	const auto from = map_coords::FromMetres({100.0f, 200.0f});
	const auto nowhere = NowhereFrom(from, k_NowhereHoverHeight);
	EXPECT_NEAR(map_coords::ToMetres(nowhere.x), 115.0f, 0.001f);
	EXPECT_EQ(nowhere.z, from.z);
	EXPECT_FLOAT_EQ(nowhere.altitude, 4.0f);
}

TEST(FireflyRules, EachDrawsTwoSpeedsThenSixAngles)
{
	std::vector<float> asked;
	const auto drift = DrawDrift([&asked](float x) {
		asked.push_back(x);
		return x * 0.5f;
	});
	ASSERT_EQ(asked.size(), 8u);
	EXPECT_FLOAT_EQ(asked[0], 0.8f);
	EXPECT_FLOAT_EQ(asked[1], 0.8f);
	for (size_t i = 2; i < asked.size(); ++i)
	{
		EXPECT_FLOAT_EQ(asked[i], k_TwoPi);
	}
	EXPECT_FLOAT_EQ(drift.flightSpeed, 1.0f);
	EXPECT_FLOAT_EQ(drift.quickSpeed, 1.0f);
}

TEST(FireflyRules, TheRewardIsDrawnByWeight)
{
	RewardTable table;
	EXPECT_FALSE(table.Pick(0.5f).has_value());
	table.SetWeight(k_Fire, 1.0f);
	table.SetWeight(k_Heal, 3.0f);
	table.SetWeight(99, 5.0f);
	EXPECT_FLOAT_EQ(table.Total(), 4.0f);
	EXPECT_FALSE(table.Pick(0.0f).has_value());
	EXPECT_EQ(table.Pick(0.5f), k_Fire);
	EXPECT_EQ(table.Pick(1.0f), k_Fire);
	EXPECT_EQ(table.Pick(1.01f), k_Heal);
	EXPECT_EQ(table.Pick(4.0f), k_Heal);
	EXPECT_FALSE(table.Pick(4.5f).has_value());
	// A weight on the first kind is a chance of nothing
	table.SetWeight(0, 1.0f);
	EXPECT_FALSE(table.Pick(0.5f).has_value());
	// A land closing clears the weights but keeps the running totals until the next weight
	table.ClearWeights();
	EXPECT_FLOAT_EQ(table.Weight(k_Heal), 0.0f);
	EXPECT_FLOAT_EQ(table.Total(), 5.0f);
	table.SetWeight(k_Fire, 2.0f);
	EXPECT_FLOAT_EQ(table.Total(), 2.0f);
}

TEST_F(Fireflies, NightfallTopsUpToFiftyAndOneATurnComesOut)
{
	_world->Add(FakeWorld::Kind::Tree, k_Here);
	_world->Add(FakeWorld::Kind::Building, k_Here + glm::vec2(20.0f, 0.0f), 3.0f);
	const auto out = [this] {
		return std::ranges::count_if(_system->GetFireflies(), [this](entt::entity entity) {
			return !_world->registry.Get<const Firefly>(entity).hidden;
		});
	};
	// The first evening turn makes 50 in the only tree. They all share its spot, and the first in line comes out.
	Turns(1);
	EXPECT_EQ(_system->GetFireflies().size(), 50u);
	EXPECT_EQ(out(), 1);
	// One a turn comes out, until all are out
	Turns(49);
	EXPECT_EQ(_system->GetFireflies().size(), 50u);
	EXPECT_EQ(out(), 50);
	// Each firefly is in the map's cells
	EXPECT_EQ(_world->filed.size(), 50u);
}

TEST_F(Fireflies, AFireflyThatFliesToAnotherCellGoesToTheFrontOfTheCellsFireflies)
{
	_world->Add(FakeWorld::Kind::Rock, k_Here);
	_world->Add(FakeWorld::Kind::Building, k_Here + glm::vec2(30.0f, 0.0f), 3.0f);
	const auto first = _system->Create(map_coords::FromMetres(k_Here));
	const auto second = _system->Create(map_coords::FromMetres(k_Here));
	ASSERT_EQ(_world->filed.front().first, second);
	// The newer one comes out first. The older one comes out a turn later, and so it is the last to come into each cell.
	Turns(1);
	EXPECT_GT(_world->moves, 0u);
	Turns(200);
	const auto& older = _world->registry.Get<const Firefly>(first);
	ASSERT_EQ(older.state, State::Hovering);
	ASSERT_EQ(_world->filed.size(), 2u);
	EXPECT_EQ(_world->filed.front().first, first);
	EXPECT_EQ(_world->filed.front().second, map_coords::Cell(older.at));
}

TEST_F(Fireflies, ACatchTakesTheFireflyThatTheCellMeetsFirst)
{
	_world->Add(FakeWorld::Kind::Tree, k_Here);
	_world->hour = 13.0f;
	const auto older = _system->Create(map_coords::FromMetres(k_Here));
	const auto newer = _system->Create(map_coords::FromMetres(k_Here));
	// The newer firefly is first in the line to come out, but the cell meets the older one first
	ASSERT_EQ(_system->GetFireflies().front(), newer);
	ASSERT_EQ(_world->filed.size(), 2u);
	std::swap(_world->filed.front(), _world->filed.back());
	EXPECT_TRUE(_system->Catch(map_coords::FromMetres(k_Here)));
	EXPECT_FALSE(_world->registry.Valid(older));
	EXPECT_TRUE(_world->registry.Valid(newer));
}

TEST_F(Fireflies, OutTheyHoverTwoMetresAboveTheBuilding)
{
	// Hidden in a rock: with no trees on the land the evening's top up makes none
	_world->Add(FakeWorld::Kind::Building, k_Here + glm::vec2(30.0f, 0.0f), 3.0f);
	_world->Add(FakeWorld::Kind::Rock, k_Here);
	_system->Create(map_coords::FromMetres(k_Here));
	Turns(1);
	const auto& firefly = Only();
	EXPECT_EQ(firefly.state, State::FlyingOut);
	EXPECT_EQ(firefly.hover.x, map_coords::FromMetres(k_Here + glm::vec2(30.0f, 0.0f)).x);
	EXPECT_FLOAT_EQ(firefly.hover.altitude, 5.0f);
	// 30 m at its speed of 1, three metres a second, takes 10 s: a hundred turns, the first flown as it sets off
	EXPECT_NEAR(firefly.flightSeconds, 10.0f, 0.01f);
	int turns = 1;
	while (firefly.state == State::FlyingOut && turns < 200)
	{
		Turns(1);
		++turns;
	}
	EXPECT_NEAR(turns, 100, 1);
	EXPECT_EQ(firefly.state, State::Hovering);
	EXPECT_EQ(firefly.at, firefly.hover);
}

TEST_F(Fireflies, WithNoBuildingTheyHover15MetresEast)
{
	_world->Add(FakeWorld::Kind::Rock, k_Here);
	_system->Create(map_coords::FromMetres(k_Here));
	Turns(1);
	const auto& firefly = Only();
	EXPECT_NEAR(map_coords::ToMetres(firefly.hover.x), k_Here.x + 15.0f, 0.001f);
	EXPECT_FLOAT_EQ(firefly.hover.altitude, 4.0f);
}

TEST_F(Fireflies, OneWhoseTreeIsGoneIsRemovedAtNightfall)
{
	const auto tree = _world->Add(FakeWorld::Kind::Tree, k_Here);
	_world->Add(FakeWorld::Kind::Rock, k_Here + glm::vec2(50.0f));
	_system->Create(map_coords::FromMetres(k_Here));
	_world->Remove(tree);
	_world->hour = 13.0f;
	Turns(1);
	EXPECT_EQ(_system->GetFireflies().size(), 1u);
	// The top up waits for the evening: the coin's heads give trees, and there are none left, so it stops at once
	_world->hour = k_Evening;
	Turns(1);
	EXPECT_TRUE(_system->GetFireflies().empty());
}

TEST_F(Fireflies, AtDawnTheyHideInTheNearestRockOrTreeAndCanBeCaughtThere)
{
	_world->Add(FakeWorld::Kind::Building, k_Here + glm::vec2(10.0f, 0.0f), 3.0f);
	_world->Add(FakeWorld::Kind::Rock, k_Here);
	const auto rock = map_coords::FromMetres(k_Here + glm::vec2(12.0f, 0.0f));
	_world->Add(FakeWorld::Kind::Rock, k_Here + glm::vec2(12.0f, 0.0f));
	_system->Create(map_coords::FromMetres(k_Here));
	_system->SetRewardWeight("HEAL", 1.0f);
	_system->SetRewardWeight("NOT A MIRACLE", 1.0f);
	Turns(200);
	EXPECT_EQ(Only().state, State::Hovering);
	// Out, it can't be caught at its tree
	EXPECT_FALSE(_system->Catch(map_coords::FromMetres(k_Here)));

	_world->hour = k_Morning;
	Turns(200);
	const auto& firefly = Only();
	EXPECT_TRUE(firefly.hidden);
	EXPECT_EQ(firefly.state, State::Resting);
	// The second rock is nearer the building than the one it came from
	EXPECT_EQ(firefly.at, rock);

	EXPECT_FALSE(_system->Catch(map_coords::FromMetres(k_Here)));
	EXPECT_TRUE(_system->Catch(rock));
	EXPECT_TRUE(_system->GetFireflies().empty());
	ASSERT_EQ(_world->rewards.size(), 1u);
	EXPECT_EQ(_world->rewards.front().first, k_Heal);
	EXPECT_EQ(_world->rewards.front().second, rock);
	// One pick-up catches one
	EXPECT_FALSE(_system->Catch(rock));
}

TEST_F(Fireflies, WithNoWeightsACatchGivesNothing)
{
	_world->Add(FakeWorld::Kind::Tree, k_Here);
	_world->hour = 13.0f;
	_system->Create(map_coords::FromMetres(k_Here));
	EXPECT_TRUE(_system->Catch(map_coords::FromMetres(k_Here)));
	EXPECT_TRUE(_world->rewards.empty());
}

TEST_F(Fireflies, ALandClosingRemovesThemAndTopsUpAgain)
{
	_world->Add(FakeWorld::Kind::Tree, k_Here);
	Turns(1);
	EXPECT_FALSE(_system->GetFireflies().empty());
	_system->Reset();
	EXPECT_TRUE(_system->GetFireflies().empty());
	Turns(1);
	EXPECT_EQ(_system->GetFireflies().size(), 49u);
}

TEST_F(Fireflies, OutTheyAreDrawnBetweenTheirLastTwoTurnsAndFadeWithDistance)
{
	_world->look = ecs::components::Sprite {.tint = glm::vec4(1.0f)};
	_world->Add(FakeWorld::Kind::Building, k_Here + glm::vec2(30.0f, 0.0f), 3.0f);
	_world->Add(FakeWorld::Kind::Rock, k_Here);
	const auto entity = _system->Create(map_coords::FromMetres(k_Here));
	// Hidden, it isn't drawn
	_system->Update(16.0f, 0.5f);
	EXPECT_FALSE(_world->registry.AllOf<ecs::components::Sprite>(entity));

	Turns(10);
	auto& firefly = _world->registry.Get<Firefly>(entity);
	// Its drift set to a plain one, so where it is drawn can be worked out
	firefly.drift = {};
	_world->camera = _world->ToWorld(firefly.at) + glm::vec3(0.0f, 0.0f, 200.0f);
	_system->Update(0.0f, 0.5f);
	const auto previous = _world->ToWorld(firefly.previous);
	const auto now = _world->ToWorld(firefly.at);
	const auto offset = DriftOffset(firefly.drift, firefly.clock, Amplitude(firefly.state, firefly.progress));
	const auto expected = (previous + now) * 0.5f + offset;
	const auto drawn = firefly.drawn;
	EXPECT_NEAR(drawn.x, expected.x, 1e-3f);
	EXPECT_NEAR(drawn.y, expected.y, 1e-3f);
	EXPECT_NEAR(drawn.z, expected.z, 1e-3f);
	// The frame does not move the firefly in the world: its Transform stays at its turn place
	const auto& position = _world->registry.Get<const ecs::components::Transform>(entity).position;
	EXPECT_NEAR(position.x, now.x, 1e-3f);
	EXPECT_NEAR(position.y, now.y, 1e-3f);
	EXPECT_NEAR(position.z, now.z, 1e-3f);
	ASSERT_TRUE(_world->registry.AllOf<ecs::components::Sprite>(entity));
	// The opacity is taken from where it was drawn the frame before: here its hiding place, 200 m away
	EXPECT_NEAR(_world->registry.Get<const ecs::components::Sprite>(entity).tint.a, 118.0f / 255.0f, 0.01f);

	// Beyond 300 m it isn't drawn, and its loops don't turn
	_world->camera = drawn + glm::vec3(0.0f, 0.0f, 400.0f);
	const float clock = firefly.clock;
	_system->Update(100.0f, 0.5f);
	EXPECT_FALSE(_world->registry.AllOf<ecs::components::Sprite>(entity));
	EXPECT_FLOAT_EQ(firefly.clock, clock);
}

TEST_F(Fireflies, UndrawnTheyStandWhereTheyAreOnTheMap)
{
	_world->Add(FakeWorld::Kind::Building, k_Here + glm::vec2(30.0f, 0.0f), 3.0f);
	_world->Add(FakeWorld::Kind::Rock, k_Here);
	const auto entity = _system->Create(map_coords::FromMetres(k_Here));
	Turns(50);
	const auto& firefly = _world->registry.Get<const Firefly>(entity);
	EXPECT_EQ(firefly.state, State::FlyingOut);
	const auto& position = _world->registry.Get<const ecs::components::Transform>(entity).position;
	const auto at = _world->ToWorld(firefly.at);
	EXPECT_NEAR(position.x, at.x, 1e-3f);
	EXPECT_NEAR(position.y, at.y, 1e-3f);
	EXPECT_NEAR(position.z, at.z, 1e-3f);
}

TEST_F(Fireflies, EachFrameCountsThoseOutInViewInReachAndDrawn)
{
	_world->look = ecs::components::Sprite {.tint = glm::vec4(1.0f)};
	_world->Add(FakeWorld::Kind::Building, k_Here + glm::vec2(30.0f, 0.0f), 3.0f);
	_world->Add(FakeWorld::Kind::Rock, k_Here);
	_world->Add(FakeWorld::Kind::Rock, k_Here + glm::vec2(0.0f, 5.0f));
	_system->Create(map_coords::FromMetres(k_Here));
	_system->Create(map_coords::FromMetres(k_Here + glm::vec2(0.0f, 5.0f)));
	_system->Update(16.0f, 0.0f);
	auto frame = _system->GetLastFrame();
	EXPECT_EQ(frame.out, 0u);
	EXPECT_EQ(frame.drawn, 0u);
	EXPECT_TRUE(frame.pictureReady);
	// One out, near the camera
	Turns(1);
	_world->camera = _world->ToWorld(map_coords::FromMetres(k_Here));
	_system->Update(16.0f, 0.0f);
	frame = _system->GetLastFrame();
	EXPECT_EQ(frame.out, 1u);
	EXPECT_EQ(frame.inView, 1u);
	EXPECT_EQ(frame.inReach, 1u);
	EXPECT_EQ(frame.drawn, 1u);
	// Too far away to be seen
	_world->camera += glm::vec3(0.0f, 0.0f, 400.0f);
	_system->Update(16.0f, 0.0f);
	frame = _system->GetLastFrame();
	EXPECT_EQ(frame.out, 1u);
	EXPECT_EQ(frame.inView, 1u);
	EXPECT_EQ(frame.inReach, 0u);
	EXPECT_EQ(frame.drawn, 0u);
	// Without its picture none is drawn
	_world->look.reset();
	_world->camera = _world->ToWorld(map_coords::FromMetres(k_Here));
	_system->Update(16.0f, 0.0f);
	frame = _system->GetLastFrame();
	EXPECT_FALSE(frame.pictureReady);
	EXPECT_EQ(frame.drawn, 0u);
}

TEST_F(Fireflies, ASavedWorldKeepsTheFirefliesTheRewardsTheLimitAndTheTopUp)
{
	_world->Add(FakeWorld::Kind::Tree, k_Here);
	Turns(1);
	_system->SetRewardWeight("HEAL", 1.0f);
	ASSERT_EQ(_system->GetFireflies().size(), 49u);

	entt::meta_ctx context;
	ecs::reflection::RegisterComponents(context);
	ecs::reflection::BinaryWriter out(context);
	_system->SaveState(out);
	EXPECT_TRUE(out.Unsaved().empty());
	const auto bytes = out.Bytes();

	// A new system takes the state back: it saves the same bytes again
	ecs::systems::FireflySystem loaded(std::make_unique<FakeWorld>());
	const ecs::reflection::EntityRemap identity;
	ecs::reflection::BinaryReader in(context, bytes, identity);
	ASSERT_TRUE(loaded.LoadState(in));
	EXPECT_EQ(in.Remaining(), 0u);
	EXPECT_TRUE(std::ranges::equal(loaded.GetFireflies(), _system->GetFireflies()));
	EXPECT_EQ(loaded.IsTopUpDue(), _system->IsTopUpDue());
	ecs::reflection::BinaryWriter again(context);
	loaded.SaveState(again);
	EXPECT_EQ(again.Bytes(), bytes);
}
