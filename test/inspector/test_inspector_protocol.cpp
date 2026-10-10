/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <string>
#include <variant>

#include <InspectorJson.h>
#include <InspectorProtocol.h>
#include <gtest/gtest.h>

using namespace openblack::inspector;

TEST(InspectorProtocol, DecodesAFullRequest)
{
	const auto decoded = DecodeRequest(
	    R"({"id": 7, "query": "objects.find", "params": {"component": "Tree"}, "near": [1, 2, 3], "radius": 30,
	        "fields": ["id", "label"], "where": [{"field": "life", "op": "<", "value": 0.5}],
	        "limit": 5, "cursor": 10, "count": true, "summary": "life", "max_bytes": 4096})");
	ASSERT_TRUE(std::holds_alternative<Request>(decoded)) << std::get<std::string>(decoded);
	const auto& request = std::get<Request>(decoded);
	EXPECT_EQ(request.id, 7);
	EXPECT_EQ(request.query, "objects.find");
	EXPECT_EQ(request.params["component"], "Tree");
	ASSERT_TRUE(request.options.near.has_value());
	EXPECT_FALSE(request.options.near->planar);
	EXPECT_DOUBLE_EQ(request.options.near->point[1], 2.0);
	EXPECT_DOUBLE_EQ(request.options.near->radius, 30.0);
	ASSERT_EQ(request.options.fields.size(), 2u);
	ASSERT_EQ(request.options.where.size(), 1u);
	EXPECT_EQ(request.options.where[0].op, FilterOp::Less);
	EXPECT_EQ(request.options.limit, 5u);
	EXPECT_EQ(request.options.cursor, 10u);
	EXPECT_TRUE(request.options.countOnly);
	EXPECT_EQ(request.options.summary, "life");
	EXPECT_EQ(request.options.maxBytes, 4096u);
}

TEST(InspectorProtocol, DefaultsKeepAnswersSmall)
{
	const auto decoded = DecodeRequest(R"({"query": "ecs.entities"})");
	ASSERT_TRUE(std::holds_alternative<Request>(decoded));
	const auto& options = std::get<Request>(decoded).options;
	EXPECT_EQ(options.limit, k_DefaultLimit);
	EXPECT_EQ(options.maxBytes, k_DefaultMaxBytes);
	EXPECT_FALSE(options.countOnly);
}

TEST(InspectorProtocol, LimitsAreCapped)
{
	const auto decoded = DecodeRequest(R"({"query": "ecs.entities", "limit": 100000, "max_bytes": 999999999})");
	ASSERT_TRUE(std::holds_alternative<Request>(decoded));
	const auto& options = std::get<Request>(decoded).options;
	EXPECT_EQ(options.limit, k_MostItems);
	EXPECT_EQ(options.maxBytes, k_MostBytes);
}

TEST(InspectorProtocol, TwoCoordinatesSearchTheGround)
{
	const auto decoded = DecodeRequest(R"({"query": "x.y", "near": [4, 9], "radius": 1})");
	ASSERT_TRUE(std::holds_alternative<Request>(decoded));
	const auto& near = std::get<Request>(decoded).options.near;
	ASSERT_TRUE(near.has_value());
	EXPECT_TRUE(near->planar);
	EXPECT_DOUBLE_EQ(near->point[0], 4.0);
	EXPECT_DOUBLE_EQ(near->point[2], 9.0);
}

TEST(InspectorProtocol, RefusesBadRequestsWithoutThrowing)
{
	EXPECT_TRUE(std::holds_alternative<std::string>(DecodeRequest("not json")));
	EXPECT_TRUE(std::holds_alternative<std::string>(DecodeRequest("[1, 2]")));
	EXPECT_TRUE(std::holds_alternative<std::string>(DecodeRequest(R"({"id": 1})")));
	EXPECT_TRUE(std::holds_alternative<std::string>(DecodeRequest(R"({"query": "a.b", "params": 3})")));
	EXPECT_TRUE(std::holds_alternative<std::string>(DecodeRequest(R"({"query": "a.b", "near": [1, 2, 3]})")));
	EXPECT_TRUE(std::holds_alternative<std::string>(DecodeRequest(R"({"query": "a.b", "near": "here", "radius": 2})")));
	EXPECT_TRUE(
	    std::holds_alternative<std::string>(DecodeRequest(R"({"query": "a.b", "where": [{"field": "x", "op": "~"}]})")));
	EXPECT_TRUE(std::holds_alternative<std::string>(DecodeRequest(R"({"query": "a.b", "limit": -1})")));
	EXPECT_TRUE(std::holds_alternative<std::string>(DecodeRequest(R"({"query": "a.b", "radius": 5})")));
	EXPECT_TRUE(std::holds_alternative<std::string>(DecodeRequest(R"({"query": "a.b", "count": "yes"})")));
	EXPECT_TRUE(std::holds_alternative<std::string>(DecodeRequest(R"({"query": "a.b", "summary": 3})")));
	EXPECT_TRUE(std::holds_alternative<std::string>(DecodeRequest(R"({"query": "a.b", "where": [3]})")));
}

// A query's parameter put beside params rather than inside it would run the query without it: such a request is
// refused, naming what it doesn't know and what a request may have
TEST(InspectorProtocol, RefusesUnknownMembersNamingThem)
{
	const auto decoded = DecodeRequest(R"({"query": "ecs.entities", "component": "Temple", "kinds": "Abode", "limit": 5})");
	ASSERT_TRUE(std::holds_alternative<std::string>(decoded));
	const auto& error = std::get<std::string>(decoded);
	EXPECT_NE(error.find("component"), std::string::npos) << error;
	EXPECT_NE(error.find("kinds"), std::string::npos) << error;
	EXPECT_NE(error.find("params"), std::string::npos) << error;
	EXPECT_NE(error.find("max_bytes"), std::string::npos) << error;
	EXPECT_EQ(error.find("limit,"), error.rfind("limit,")) << "limit is named only among the allowed: " << error;

	const auto filter = DecodeRequest(R"({"query": "a.b", "where": [{"field": "x", "value": 1, "operator": "<"}]})");
	ASSERT_TRUE(std::holds_alternative<std::string>(filter));
	EXPECT_NE(std::get<std::string>(filter).find("operator"), std::string::npos);

	// Every member a request may have is still taken
	EXPECT_TRUE(std::holds_alternative<Request>(DecodeRequest(R"({"query": "a.b", "id": 1, "params": {"x": 1}})")));
}

TEST(InspectorProtocol, EncodedRequestsDecodeTheSame)
{
	Request request;
	request.id = "abc";
	request.query = "particles.splash";
	request.params = {{"kind", "ring"}};
	request.options.near = Near {.point = {1.0, 2.0, 3.0}, .planar = false, .radius = 4.0};
	request.options.fields = {"id"};
	request.options.where.push_back({.field = "age", .op = FilterOp::GreaterEqual, .value = 3});
	request.options.limit = 3;

	const auto decoded = DecodeRequest(EncodeRequest(request));
	ASSERT_TRUE(std::holds_alternative<Request>(decoded));
	const auto& again = std::get<Request>(decoded);
	EXPECT_EQ(again.id, "abc");
	EXPECT_EQ(again.query, request.query);
	EXPECT_EQ(again.params, request.params);
	ASSERT_TRUE(again.options.near.has_value());
	EXPECT_DOUBLE_EQ(again.options.near->radius, 4.0);
	ASSERT_EQ(again.options.where.size(), 1u);
	EXPECT_EQ(again.options.where[0].op, FilterOp::GreaterEqual);
	EXPECT_EQ(again.options.limit, 3u);
}

TEST(InspectorProtocol, AnswersAreOneLine)
{
	const auto ok = EncodeResult(5, {{"a", 1}});
	EXPECT_EQ(ok.find('\n'), std::string::npos);
	const auto parsed = Parse(ok);
	ASSERT_TRUE(parsed.has_value());
	EXPECT_EQ((*parsed)["id"], 5);
	EXPECT_EQ((*parsed)["ok"], true);
	EXPECT_EQ((*parsed)["result"]["a"], 1);

	const auto failed = Parse(EncodeError(nullptr, "no"));
	ASSERT_TRUE(failed.has_value());
	EXPECT_EQ((*failed)["ok"], false);
	EXPECT_EQ((*failed)["error"], "no");
}

TEST(InspectorProtocol, InvalidTextIsReplacedNotRefused)
{
	// A name in the game's old 8-bit text is not UTF-8
	const std::string name = "Caf\xe9";
	const auto line = EncodeResult(1, {{"label", name}});
	EXPECT_TRUE(Parse(line).has_value());
}
