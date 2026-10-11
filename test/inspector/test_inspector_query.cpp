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
#include <vector>

#include <Inspector.h>
#include <InspectorQuery.h>
#include <gtest/gtest.h>

using namespace openblack::inspector;

namespace
{

/// Things along the x axis, one every ten units, with a life that falls along the line
Json Things(int count)
{
	Json items = Json::array();
	for (int i = 0; i < count; ++i)
	{
		items.push_back({
		    {"id", i},
		    {"label", "Thing " + std::to_string(i)},
		    {"position", {i * 10.0, 0.0, 0.0}},
		    {"life", 1.0 - (i * 0.1)},
		    {"state", {{"busy", i % 2 == 0}, {"name", "idle"}}},
		});
	}
	return items;
}

/// A provider with one object query and one list query, counting how often it is asked
class FakeProvider final: public ProviderInterface
{
public:
	[[nodiscard]] std::string_view Name() const override { return "fake"; }
	[[nodiscard]] std::vector<QueryDescription> Describe() const override
	{
		return {
		    {.name = "one",
		     .description = "One thing",
		     .parameters = {{.name = "id", .type = "integer", .description = "Which", .required = true}},
		     .kind = ResultKind::Object,
		     .needsNear = false},
		    {.name = "all", .description = "Every thing", .parameters = {}, .kind = ResultKind::List, .needsNear = false},
		    {.name = "near",
		     .description = "Things about a point",
		     .parameters = {},
		     .kind = ResultKind::List,
		     .needsNear = true},
		};
	}
	[[nodiscard]] QueryResult Run(std::string_view query, const QueryContext& context) override
	{
		++asked;
		if (query == "one")
		{
			const auto id = NumberMember(context.params, "id");
			if (!id.has_value() || *id < 0 || *id >= 30)
			{
				return QueryResult::Error("no such thing");
			}
			return QueryResult::Value(Things(30)[static_cast<size_t>(*id)]);
		}
		return QueryResult::Value(Things(30));
	}

	int asked {0};
};

/// A provider with so many long-described queries that its catalogue is larger than an ordinary answer may be
class ManyQueriesProvider final: public ProviderInterface
{
public:
	[[nodiscard]] std::string_view Name() const override { return "many"; }
	[[nodiscard]] std::vector<QueryDescription> Describe() const override
	{
		std::vector<QueryDescription> queries;
		for (int i = 0; i < 40; ++i)
		{
			queries.push_back({.name = "query" + std::to_string(i),
			                   .description = std::string(600, 'd'),
			                   .parameters = {},
			                   .kind = ResultKind::Object,
			                   .needsNear = false});
		}
		return queries;
	}
	[[nodiscard]] QueryResult Run(std::string_view, const QueryContext&) override { return QueryResult::Value(Json()); }
};

Json Ask(const Inspector& inspector, const std::string& line)
{
	const auto answer = Parse(inspector.Handle(line));
	EXPECT_TRUE(answer.has_value());
	return answer.value_or(Json());
}

} // namespace

TEST(InspectorQuery, ListsGiveAPageAndACursor)
{
	QueryOptions options;
	options.limit = 4;
	const auto page = ShapeList(Things(10), options);
	EXPECT_EQ(page["items"].size(), 4u);
	EXPECT_EQ(page["total"], 10);
	EXPECT_EQ(page["next_cursor"], 4);
	EXPECT_EQ(page["truncated"], false);

	options.cursor = 8;
	const auto last = ShapeList(Things(10), options);
	EXPECT_EQ(last["items"].size(), 2u);
	EXPECT_EQ(last["items"][0]["id"], 8);
	EXPECT_TRUE(last["next_cursor"].is_null());
}

TEST(InspectorQuery, DefaultLimitApplies)
{
	const auto page = ShapeList(Things(50), QueryOptions {});
	EXPECT_EQ(page["items"].size(), k_DefaultLimit);
	EXPECT_EQ(page["total"], 50);
}

TEST(InspectorQuery, FiltersCompareFields)
{
	QueryOptions options;
	options.where.push_back({.field = "life", .op = FilterOp::Less, .value = 0.55});
	options.where.push_back({.field = "state.busy", .op = FilterOp::Equal, .value = true});
	const auto page = ShapeList(Things(10), options);
	// Life below 0.55 from the 5th on, and busy on the even ones: 6 and 8
	ASSERT_EQ(page["total"], 2);
	EXPECT_EQ(page["items"][0]["id"], 6);
	EXPECT_EQ(page["items"][1]["id"], 8);
}

TEST(InspectorQuery, ContainsAndExists)
{
	const Json item = {{"label", "Oak Tree"}, {"tags", {"wood", "tall"}}, {"empty", nullptr}};
	EXPECT_TRUE(Matches(item, {.field = "label", .op = FilterOp::Contains, .value = "Oak"}));
	EXPECT_TRUE(Matches(item, {.field = "tags", .op = FilterOp::Contains, .value = "tall"}));
	EXPECT_FALSE(Matches(item, {.field = "tags", .op = FilterOp::Contains, .value = "short"}));
	EXPECT_TRUE(Matches(item, {.field = "label", .op = FilterOp::Exists, .value = nullptr}));
	EXPECT_FALSE(Matches(item, {.field = "empty", .op = FilterOp::Exists, .value = nullptr}));
	EXPECT_FALSE(Matches(item, {.field = "missing", .op = FilterOp::Equal, .value = 1}));
	EXPECT_TRUE(Matches(item, {.field = "missing", .op = FilterOp::NotEqual, .value = 1}));
	// Values of kinds that can't be ordered never pass an ordering
	EXPECT_FALSE(Matches(item, {.field = "label", .op = FilterOp::Less, .value = 3}));
}

TEST(InspectorQuery, RadiusSearchIsNearestFirst)
{
	QueryOptions options;
	options.near = Near {.point = {42.0, 0.0, 0.0}, .planar = false, .radius = 15.0};
	const auto page = ShapeList(Things(10), options);
	// Things at 30, 40 and 50 are within 15 of 42: 40 first, then 50, then 30
	ASSERT_EQ(page["total"], 3);
	EXPECT_EQ(page["items"][0]["id"], 4);
	EXPECT_EQ(page["items"][1]["id"], 5);
	EXPECT_EQ(page["items"][2]["id"], 3);
	EXPECT_DOUBLE_EQ(page["items"][0]["distance"].get<double>(), 2.0);
}

TEST(InspectorQuery, PlanarSearchIgnoresHeight)
{
	Json items = Json::array();
	items.push_back({{"id", 1}, {"position", {0.0, 100.0, 0.0}}});
	QueryOptions options;
	options.near = Near {.point = {0.0, 0.0, 3.0}, .planar = true, .radius = 5.0};
	const auto page = ShapeList(items, options);
	ASSERT_EQ(page["total"], 1);
	EXPECT_DOUBLE_EQ(page["items"][0]["distance"].get<double>(), 3.0);
}

TEST(InspectorQuery, CountOnly)
{
	QueryOptions options;
	options.countOnly = true;
	options.where.push_back({.field = "life", .op = FilterOp::GreaterEqual, .value = 0.65});
	const auto result = ShapeList(Things(10), options);
	EXPECT_EQ(result, Json({{"count", 4}}));
}

TEST(InspectorQuery, SummaryInsteadOfItems)
{
	QueryOptions options;
	options.summary = "position.0";
	const auto result = ShapeList(Things(5), options);
	EXPECT_EQ(result["count"], 5);
	EXPECT_DOUBLE_EQ(result["min"].get<double>(), 0.0);
	EXPECT_DOUBLE_EQ(result["max"].get<double>(), 40.0);
	EXPECT_DOUBLE_EQ(result["mean"].get<double>(), 20.0);
	EXPECT_FALSE(result.contains("items"));
}

TEST(InspectorQuery, FieldSelectionKeepsOnlyThePaths)
{
	QueryOptions options;
	options.fields = {"id", "state.name"};
	options.limit = 1;
	const auto page = ShapeList(Things(3), options);
	EXPECT_EQ(page["items"][0], Json({{"id", 0}, {"state", {{"name", "idle"}}}}));
}

TEST(InspectorQuery, SizeCapTruncatesAndMovesTheCursor)
{
	QueryOptions options;
	options.limit = 50;
	options.maxBytes = 600;
	const auto page = ShapeList(Things(50), options);
	EXPECT_EQ(page["truncated"], true);
	const auto kept = page["items"].size();
	EXPECT_GT(kept, 0u);
	EXPECT_LT(kept, 50u);
	EXPECT_EQ(page["next_cursor"], kept);
	EXPECT_LE(Dump(page).size(), options.maxBytes);
}

TEST(InspectorQuery, LargeObjectsListTheirKeys)
{
	Json big = {{"a", std::string(5000, 'x')}, {"b", 1}};
	QueryOptions options;
	options.maxBytes = 1000;
	const auto result = ShapeObject(big, options);
	EXPECT_EQ(result["truncated"], true);
	EXPECT_EQ(result["keys"], Json({"a", "b"}));

	options.fields = {"b"};
	EXPECT_EQ(ShapeObject(big, options), Json({{"b", 1}}));
}

TEST(Inspector, DescribeListsProvidersAndQueries)
{
	Inspector inspector;
	inspector.Add(std::make_unique<FakeProvider>());

	const auto all = Ask(inspector, R"({"id": 1, "query": "describe"})");
	EXPECT_EQ(all["ok"], true);
	EXPECT_EQ(all["result"]["providers"]["fake"]["one"], "One thing");

	const auto one = Ask(inspector, R"({"query": "describe", "params": {"query": "fake.one"}})");
	EXPECT_EQ(one["result"]["query"], "fake.one");
	EXPECT_EQ(one["result"]["parameters"][0]["name"], "id");
	EXPECT_EQ(one["result"]["parameters"][0]["required"], true);

	const auto provider = Ask(inspector, R"({"query": "describe", "params": {"provider": "fake"}})");
	EXPECT_EQ(provider["result"]["queries"].size(), 3u);
}

TEST(Inspector, DescribeAnswersTheWholeCataloguePastAnOrdinaryAnswersSize)
{
	Inspector inspector;
	inspector.Add(std::make_unique<ManyQueriesProvider>());

	const auto all = Ask(inspector, R"({"query": "describe"})");
	ASSERT_EQ(all["ok"], true);
	ASSERT_TRUE(all["result"].contains("providers"));
	EXPECT_EQ(all["result"]["providers"]["many"].size(), 40u);
	EXPECT_GT(Dump(all["result"]).size(), k_DefaultMaxBytes);

	// A caller's own max_bytes still holds
	const auto small = Ask(inspector, R"({"query": "describe", "max_bytes": 1000})");
	EXPECT_EQ(small["result"]["truncated"], true);
}

TEST(Inspector, RunsQueriesAndShapesThem)
{
	Inspector inspector;
	inspector.Add(std::make_unique<FakeProvider>());

	const auto one = Ask(inspector, R"({"id": "a", "query": "fake.one", "params": {"id": 3}, "fields": ["label"]})");
	EXPECT_EQ(one["id"], "a");
	EXPECT_EQ(one["result"], Json({{"label", "Thing 3"}}));

	const auto all = Ask(inspector, R"({"query": "fake.all", "limit": 2})");
	EXPECT_EQ(all["result"]["items"].size(), 2u);
	EXPECT_EQ(all["result"]["total"], 30);
}

TEST(Inspector, ExplainsWhatIsWrong)
{
	Inspector inspector;
	auto provider = std::make_unique<FakeProvider>();
	auto* fake = provider.get();
	inspector.Add(std::move(provider));

	const auto missing = Ask(inspector, R"({"id": 9, "query": "fake.one"})");
	EXPECT_EQ(missing["ok"], false);
	EXPECT_EQ(missing["id"], 9);
	EXPECT_NE(missing["error"].get<std::string>().find("id"), std::string::npos);

	EXPECT_EQ(Ask(inspector, R"({"query": "fake.near"})")["ok"], false);
	EXPECT_EQ(Ask(inspector, R"({"query": "nobody.one"})")["ok"], false);
	EXPECT_EQ(Ask(inspector, R"({"query": "fake.none"})")["ok"], false);
	EXPECT_EQ(Ask(inspector, R"({"query": "fake.one", "params": {"id": 99}})")["error"], "no such thing");
	EXPECT_EQ(Ask(inspector, R"({"id": 4, "query": 5})")["id"], 4);
	// Only the last was run: the others were refused before reaching the provider
	EXPECT_EQ(fake->asked, 1);
}

// A parameter a query doesn't take is refused, naming it and the ones the query does take, rather than ignored
TEST(Inspector, RefusesParametersAQueryDoesNotTake)
{
	Inspector inspector;
	auto provider = std::make_unique<FakeProvider>();
	auto* fake = provider.get();
	inspector.Add(std::move(provider));

	const auto one = Ask(inspector, R"({"query": "fake.one", "params": {"id": 3, "colour": "red"}})");
	EXPECT_EQ(one["ok"], false);
	const auto error = one["error"].get<std::string>();
	EXPECT_NE(error.find("colour"), std::string::npos) << error;
	EXPECT_NE(error.find("it takes id"), std::string::npos) << error;

	// A query that takes none says so
	const auto all = Ask(inspector, R"({"query": "fake.all", "params": {"component": "Temple"}})");
	EXPECT_EQ(all["ok"], false);
	EXPECT_NE(all["error"].get<std::string>().find("no parameters"), std::string::npos) << all["error"];

	// The queries every inspector has are checked too
	EXPECT_EQ(Ask(inspector, R"({"query": "describe", "params": {"provider": "fake", "verbose": true}})")["ok"], false);
	EXPECT_EQ(Ask(inspector, R"({"query": "ping", "params": {"x": 1}})")["ok"], false);
	EXPECT_EQ(Ask(inspector, R"({"query": "describe", "params": {"provider": "fake"}})")["ok"], true);
	EXPECT_EQ(fake->asked, 0);

	EXPECT_EQ(Ask(inspector, R"({"query": "fake.one", "params": {"id": 3}})")["ok"], true);
}

TEST(Inspector, Ping)
{
	const Inspector inspector;
	EXPECT_EQ(Ask(inspector, R"({"query": "ping"})")["result"]["pong"], true);
}

// A ping answers what the game said of itself, for tools telling running games apart
TEST(Inspector, PingAnswersTheIdentity)
{
	Inspector inspector;
	inspector.SetIdentity({{"pid", 1234}, {"port", 47801}});
	const auto answer = Ask(inspector, R"({"query": "ping"})")["result"];
	EXPECT_EQ(answer["pong"], true);
	EXPECT_EQ(answer["pid"], 1234);
	EXPECT_EQ(answer["port"], 47801);
}

// Pinging looks for a game without taking control of it; every other line, even one that isn't a request, does
TEST(Inspector, OnlyAPingLeavesControl)
{
	EXPECT_FALSE(Inspector::TakesControl(R"({"query": "ping"})"));
	EXPECT_FALSE(Inspector::TakesControl(R"({"id": 3, "query": "ping"})"));
	EXPECT_TRUE(Inspector::TakesControl(R"({"query": "describe"})"));
	EXPECT_TRUE(Inspector::TakesControl(R"({"query": "sky.moon"})"));
	EXPECT_TRUE(Inspector::TakesControl("not json"));
}

// Every answer names the game it came from, so that a tool talking to several games can tell a crossed answer
TEST(Inspector, EveryAnswerNamesItsGame)
{
	Inspector inspector;
	auto fake = std::make_unique<FakeProvider>();
	inspector.Add(std::move(fake));
	inspector.SetIdentity({{"pid", 1234}, {"port", 47801}, {"worktree", "C:/wt"}, {"land", "testbed"}});
	const Json game = {{"pid", 1234}, {"port", 47801}, {"worktree", "C:/wt"}};
	EXPECT_EQ(Ask(inspector, R"({"query": "fake.all"})")["game"], game);
	EXPECT_EQ(Ask(inspector, R"({"query": "fake.none"})")["game"], game);
	EXPECT_EQ(Ask(inspector, "not json")["game"], game);
}

// A game that hasn't said what it is answers as before, with nothing more
TEST(Inspector, AnswersWithoutAnIdentityNameNoGame)
{
	const Inspector inspector;
	EXPECT_FALSE(Ask(inspector, R"({"query": "ping"})").contains("game"));
}

TEST(Inspector, APingSaysTheGameIsReady)
{
	const Inspector inspector;
	EXPECT_EQ(Ask(inspector, R"({"query": "ping"})")["result"]["ready"], true);
}

// While the game loads it can't answer for its state: a ping and describe still answer, game.state says it isn't
// ready, and everything else is refused as loading, so that tools wait and ask again rather than time out
TEST(Inspector, WhileLoadingOnlyWhatReadsNothingOfTheGameIsAnswered)
{
	Inspector inspector;
	auto fake = std::make_unique<FakeProvider>();
	auto* provider = fake.get();
	inspector.Add(std::move(fake));
	inspector.SetIdentity({{"pid", 7}, {"port", 50000}, {"worktree", "C:/wt"}});
	const auto ask = [&inspector](const std::string& line) {
		const auto answer = Parse(inspector.HandleWhileLoading(line, "Land1.txt"));
		EXPECT_TRUE(answer.has_value());
		return answer.value_or(Json());
	};

	const auto ping = ask(R"({"id": 1, "query": "ping"})");
	EXPECT_EQ(ping["ok"], true);
	EXPECT_EQ(ping["result"]["ready"], false);
	EXPECT_EQ(ping["result"]["loading"], "Land1.txt");
	EXPECT_EQ(ping["result"]["pid"], 7);
	EXPECT_EQ(ping["loading"], "Land1.txt");
	EXPECT_EQ(ping["game"]["pid"], 7);

	EXPECT_EQ(ask(R"({"query": "describe"})")["ok"], true);

	const auto state = ask(R"({"id": 2, "query": "game.state"})");
	EXPECT_EQ(state["id"], 2);
	EXPECT_EQ(state["ok"], true);
	EXPECT_EQ(state["result"]["ready"], false);
	EXPECT_EQ(state["result"]["loading"], "Land1.txt");

	const auto other = ask(R"({"id": 3, "query": "fake.all"})");
	EXPECT_EQ(other["id"], 3);
	EXPECT_EQ(other["ok"], false);
	EXPECT_EQ(other["loading"], "Land1.txt");
	EXPECT_EQ(other["game"]["pid"], 7);
	// The game's providers aren't asked while it loads
	EXPECT_EQ(provider->asked, 0);

	EXPECT_EQ(ask("not json")["ok"], false);
}

// The catalogue is every query in full, as tools build their schemas from it
TEST(Inspector, TheCatalogueDescribesEveryQueryInFull)
{
	Inspector inspector;
	inspector.Add(std::make_unique<FakeProvider>());
	const auto catalogue = inspector.Catalogue();
	ASSERT_EQ(catalogue.size(), 3u);
	EXPECT_EQ(catalogue[0]["query"], "fake.one");
	EXPECT_EQ(catalogue[0]["parameters"][0]["name"], "id");
	EXPECT_EQ(catalogue[0]["parameters"][0]["required"], true);
	EXPECT_EQ(catalogue[2]["needs_near"], true);
}
