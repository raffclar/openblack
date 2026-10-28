/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <Inspector.h>
#include <gtest/gtest.h>

using namespace openblack::inspector;

namespace
{

/// A provider with one query that reads and one query that writes, which counts the queries that run
class FakeWorldProvider final: public ProviderInterface
{
public:
	explicit FakeWorldProvider(std::string name)
	    : _name(std::move(name))
	{
	}
	[[nodiscard]] std::string_view Name() const override { return _name; }
	[[nodiscard]] std::vector<QueryDescription> Describe() const override
	{
		return {
		    {.name = "look", .description = "Reads", .parameters = {}, .kind = ResultKind::Object, .needsNear = false},
		    {.name = "change",
		     .description = "Writes",
		     .parameters = {},
		     .kind = ResultKind::Object,
		     .needsNear = false,
		     .writes = true},
		};
	}
	[[nodiscard]] QueryResult Run([[maybe_unused]] std::string_view query,
	                              [[maybe_unused]] const QueryContext& context) override
	{
		++ran;
		return QueryResult::Value(Json {{"done", true}});
	}

	int ran {0};

private:
	std::string _name;
};

/// An inspector with a world provider and an input provider, and a network game that the test starts and stops
struct Fixture
{
	Fixture()
	{
		auto worldProvider = std::make_unique<FakeWorldProvider>("edit");
		auto inputProvider = std::make_unique<FakeWorldProvider>("input");
		world = worldProvider.get();
		input = inputProvider.get();
		inspector.Add(std::move(worldProvider));
		inspector.Add(std::move(inputProvider));
		inspector.SetNetworkGameLive([this]() { return live; });
	}

	QueryResult Ask(std::string query) const { return inspector.Answer(Request {.id = 1, .query = std::move(query)}); }

	Inspector inspector;
	FakeWorldProvider* world {nullptr};
	FakeWorldProvider* input {nullptr};
	bool live {false};
};

} // namespace

TEST(InspectorNetworkGate, WritesRunWithNoNetworkGame)
{
	Fixture fixture;
	EXPECT_TRUE(fixture.Ask("edit.change").Ok());
	EXPECT_EQ(fixture.world->ran, 1);
}

TEST(InspectorNetworkGate, LiveNetworkGameRefusesWorldWrites)
{
	Fixture fixture;
	fixture.live = true;
	const auto answer = fixture.Ask("edit.change");
	EXPECT_FALSE(answer.Ok());
	EXPECT_NE(answer.error.find("network game"), std::string::npos);
	EXPECT_EQ(fixture.world->ran, 0);
	// The refusal is in the list of the last writes
	const auto writes = fixture.Ask("writes");
	ASSERT_TRUE(writes.Ok());
	EXPECT_NE(writes.value.dump().find("edit.change"), std::string::npos);
}

TEST(InspectorNetworkGate, LiveNetworkGameKeepsReadsAndLocalInput)
{
	Fixture fixture;
	fixture.live = true;
	EXPECT_TRUE(fixture.Ask("edit.look").Ok());
	EXPECT_TRUE(fixture.Ask("input.change").Ok());
	EXPECT_EQ(fixture.world->ran, 1);
	EXPECT_EQ(fixture.input->ran, 1);
}

TEST(InspectorNetworkGate, WritesRunAgainWhenTheNetworkGameEnds)
{
	Fixture fixture;
	fixture.live = true;
	EXPECT_FALSE(fixture.Ask("edit.change").Ok());
	fixture.live = false;
	EXPECT_TRUE(fixture.Ask("edit.change").Ok());
}

TEST(InspectorNetworkGate, RefusedQueries)
{
	// Each query that writes the world, the scenario load and the run controls
	for (const auto* query : {"edit.create", "edit.destroy", "script.run", "level.load", "level.testbed", "saves.load",
	                          "magic.town_miracle", "gui.press", "creatures.fight"})
	{
		EXPECT_TRUE(RefusedInNetworkGame(query, true)) << query;
	}
	for (const auto* query : {"game.scenario", "game.pause", "game.resume", "game.step", "game.speed"})
	{
		EXPECT_TRUE(RefusedInNetworkGame(query, false)) << query;
	}
	// The queries that only read, and the writes that act only on this computer
	for (const auto* query : {"game.state", "ecs.entities", "level.list"})
	{
		EXPECT_FALSE(RefusedInNetworkGame(query, false)) << query;
	}
	for (const auto* query : {"input.key", "input.pointer", "camera.set", "screenshot.take", "sim.record", "sim.feedback",
	                          "fights.transcript_folder", "fights.clear_transcripts", "audio.lose_device", "autopilot.start",
	                          "autopilot.stop", "creatures.fight_cheat"})
	{
		EXPECT_FALSE(RefusedInNetworkGame(query, true)) << query;
	}
}
