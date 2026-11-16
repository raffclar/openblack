/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>
#include <limits>
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "3D/LandReuse.h"
#include "Commands/PlayerCommands.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Editor/EditorHistory.h"
#include "Editor/EditorLandEdits.h"
#include "Editor/LandBrush.h"

using namespace openblack;
using namespace openblack::editor;
using player_commands::EditorLandStroke;
using player_commands::EditorPath;

namespace
{

/// A land of flat cells in a square of corners, with a height of one metre for each altitude step. The corners out of the
/// square have no land.
class FakeLand final: public land_edits::EditableLand
{
public:
	FakeLand(int corners, uint8_t altitude, int countries)
	    : _countries(countries)
	{
		for (int x = 0; x < corners; ++x)
		{
			for (int z = 0; z < corners; ++z)
			{
				_cells[{x, z}] = {altitude, 0};
			}
		}
	}
	[[nodiscard]] std::optional<uint8_t> AltitudeAt(glm::u16vec2 corner) const override
	{
		const auto found = _cells.find({corner.x, corner.y});
		return found != _cells.end() ? std::optional(found->second.first) : std::nullopt;
	}
	[[nodiscard]] std::optional<uint8_t> CountryAt(glm::u16vec2 corner) const override
	{
		const auto found = _cells.find({corner.x, corner.y});
		return found != _cells.end() ? std::optional(found->second.second) : std::nullopt;
	}
	[[nodiscard]] int Countries() const override { return _countries; }
	[[nodiscard]] float HeightAt(glm::vec2 point) const override
	{
		// The height of the nearest corner
		const auto corner = glm::ivec2(glm::round(point / land_brush::k_CornerSpacing));
		const auto found = _cells.find({corner.x, corner.y});
		return found != _cells.end() ? static_cast<float>(found->second.first) : 0.0f;
	}
	void SetAltitude(glm::u16vec2 corner, uint8_t altitude) override { _cells[{corner.x, corner.y}].first = altitude; }
	void SetCountry(glm::u16vec2 corner, uint8_t country) override { _cells[{corner.x, corner.y}].second = country; }

private:
	int _countries;
	std::map<std::pair<int, int>, std::pair<uint8_t, uint8_t>> _cells;
};

constexpr EditorLandStroke k_Raise {.mode = static_cast<uint8_t>(land_brush::Mode::Raise),
                                    .innerRadius = 10.0f,
                                    .outerRadius = 20.0f,
                                    .step = 4,
                                    .country = 0,
                                    .centre = {100.0f, 100.0f}};

entt::entity StandingAt(ecs::Registry& registry, glm::vec3 position)
{
	const auto entity = registry.Create();
	registry.Assign<ecs::components::Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
	return entity;
}

} // namespace

TEST(EditorLandEdits, AStrokeGoesToTheCommandAndBackAsTheSameBrush)
{
	const land_brush::Brush brush {
	    .mode = land_brush::Mode::Paint, .innerRadius = 12.5f, .outerRadius = 33.0f, .step = 7, .country = 5};
	const auto stroke = land_edits::StrokeOf(brush, {40.0f, 60.0f});
	EXPECT_EQ(stroke.mode, static_cast<uint8_t>(land_brush::Mode::Paint));
	EXPECT_EQ(stroke.centre, glm::vec2(40.0f, 60.0f));
	const auto back = land_edits::BrushOf(stroke);
	EXPECT_EQ(back.mode, brush.mode);
	EXPECT_EQ(back.innerRadius, brush.innerRadius);
	EXPECT_EQ(back.outerRadius, brush.outerRadius);
	EXPECT_EQ(back.step, brush.step);
	EXPECT_EQ(back.country, brush.country);
}

TEST(EditorLandEdits, TheWorldRefusesAStrokeOutOfTheLimitsOfTheBrush)
{
	EXPECT_FALSE(land_edits::LandStrokeRefusal(k_Raise, 4).has_value());
	const auto refused = [](auto change, std::optional<int> countries = 4) {
		auto stroke = k_Raise;
		change(stroke);
		return land_edits::LandStrokeRefusal(stroke, countries).has_value();
	};
	EXPECT_TRUE(refused([](EditorLandStroke& stroke) { stroke.mode = 3; }));
	EXPECT_TRUE(refused([](EditorLandStroke& stroke) { stroke.innerRadius = std::numeric_limits<float>::infinity(); }));
	EXPECT_TRUE(refused([](EditorLandStroke& stroke) { stroke.centre.y = std::numeric_limits<float>::quiet_NaN(); }));
	EXPECT_TRUE(refused([](EditorLandStroke& stroke) { stroke.innerRadius = -1.0f; }));
	EXPECT_TRUE(refused([](EditorLandStroke& stroke) { stroke.outerRadius = 5.0f; }));
	EXPECT_TRUE(refused([](EditorLandStroke& stroke) { stroke.outerRadius = land_brush::k_MostRadius + 1.0f; }));
	EXPECT_TRUE(refused([](EditorLandStroke& stroke) { stroke.step = 0; }));
	EXPECT_TRUE(refused([](EditorLandStroke& stroke) { stroke.step = land_brush::k_MostStep + 1; }));
	EXPECT_TRUE(refused([](EditorLandStroke& stroke) { stroke.country = land_brush::k_MostCountries; }));
	EXPECT_TRUE(refused([](EditorLandStroke& stroke) { stroke.centre.x = -0.5f; }));
	EXPECT_TRUE(refused([](EditorLandStroke& stroke) { stroke.centre.y = land_edits::k_MapSide; }));
	// A paint needs a country of the land. A raise does not use the country.
	const auto paint = [](EditorLandStroke& stroke) {
		stroke.mode = static_cast<uint8_t>(land_brush::Mode::Paint);
		stroke.country = 4;
	};
	EXPECT_TRUE(refused(paint));
	EXPECT_FALSE(refused(paint, 5));
	EXPECT_FALSE(refused(paint, std::nullopt));
	EXPECT_FALSE(refused([](EditorLandStroke& stroke) { stroke.country = 9; }));
	// The largest values of the brush are in its limits
	EXPECT_FALSE(refused([](EditorLandStroke& stroke) {
		stroke.innerRadius = land_brush::k_MostRadius;
		stroke.outerRadius = land_brush::k_MostRadius;
		stroke.step = land_brush::k_MostStep;
	}));
}

TEST(EditorLandEdits, AStrokeRaisesTheLandAndTheThingsInReachKeepTheirHeightAboveIt)
{
	ecs::Registry registry;
	FakeLand land(30, 10, 2);
	// A thing that stands on the middle, a thing two metres above the land, a thing out of reach and a thing in a hand
	const auto onTheLand = StandingAt(registry, {100.0f, 10.0f, 100.0f});
	const auto above = StandingAt(registry, {110.0f, 12.0f, 100.0f});
	const auto away = StandingAt(registry, {250.0f, 10.0f, 250.0f});
	const auto held = StandingAt(registry, {100.0f, 30.0f, 100.0f});
	registry.Assign<ecs::components::InPhysics>(held);
	const auto changed = land_edits::ApplyLandStroke(registry, land, k_Raise);
	// The inner circle takes the whole step and the corners at the outer circle take none
	EXPECT_EQ(land.AltitudeAt({10, 10}), 14);
	EXPECT_EQ(land.AltitudeAt({11, 10}), 14);
	EXPECT_EQ(land.AltitudeAt({12, 10}), 10);
	EXPECT_GT(changed, 0u);
	using ecs::components::Transform;
	EXPECT_FLOAT_EQ(registry.Get<Transform>(onTheLand).position.y, 14.0f);
	EXPECT_FLOAT_EQ(registry.Get<Transform>(above).position.y, 16.0f);
	EXPECT_FLOAT_EQ(registry.Get<Transform>(away).position.y, 10.0f);
	EXPECT_FLOAT_EQ(registry.Get<Transform>(held).position.y, 30.0f);
	// A lower goes back down by the same step
	auto lower = k_Raise;
	lower.mode = static_cast<uint8_t>(land_brush::Mode::Lower);
	land_edits::ApplyLandStroke(registry, land, lower);
	EXPECT_EQ(land.AltitudeAt({10, 10}), 10);
	EXPECT_FLOAT_EQ(registry.Get<Transform>(onTheLand).position.y, 10.0f);
}

TEST(EditorLandEdits, APaintGivesTheCountryOnlyWhenTheLandHasIt)
{
	ecs::Registry registry;
	FakeLand land(30, 10, 3);
	auto paint = k_Raise;
	paint.mode = static_cast<uint8_t>(land_brush::Mode::Paint);
	paint.country = 2;
	const auto thing = StandingAt(registry, {100.0f, 10.0f, 100.0f});
	EXPECT_GT(land_edits::ApplyLandStroke(registry, land, paint), 0u);
	EXPECT_EQ(land.CountryAt({10, 10}), 2);
	EXPECT_EQ(land.CountryAt({12, 10}), 2);
	EXPECT_EQ(land.CountryAt({13, 10}), 0);
	// A paint changes no altitude and moves nothing
	EXPECT_EQ(land.AltitudeAt({10, 10}), 10);
	EXPECT_FLOAT_EQ(registry.Get<ecs::components::Transform>(thing).position.y, 10.0f);
	// The land has no fourth country
	paint.country = 3;
	EXPECT_EQ(land_edits::ApplyLandStroke(registry, land, paint), 0u);
	EXPECT_EQ(land.CountryAt({10, 10}), 2);
}

TEST(EditorLandEdits, TheDistanceOfACornerHasTheSameBitsOnEveryComputer)
{
	// The square of the distance has two products and a sum. A fused multiply and add gives another last bit for this
	// corner, so the bits show that each operation rounds on its own, as on the server and on every client.
	const glm::vec2 centre {std::bit_cast<float>(uint32_t {1123972455}), std::bit_cast<float>(uint32_t {1160327423})};
	const glm::vec2 corner {13.0f * land_brush::k_CornerSpacing, 271.0f * land_brush::k_CornerSpacing};
	const auto distance = glm::distance(corner, centre);
	EXPECT_EQ(std::bit_cast<uint32_t>(distance), 1082210556u);
	EXPECT_EQ(std::bit_cast<uint32_t>(land_brush::Weight(distance, 2.0f, 9.0f)), 1060468155u);
}

TEST(EditorLandEdits, TheWorldRefusesAPathWithTooFewOrTooManyPointsOrOffTheMap)
{
	const EditorPath good {.kind = 0, .points = {{10.0f, 10.0f}, {50.0f, 10.0f}}};
	EXPECT_FALSE(land_edits::PathRefusal(good).has_value());
	auto path = good;
	path.points.pop_back();
	EXPECT_TRUE(land_edits::PathRefusal(path).has_value());
	path.points.assign(player_commands::k_MostEditorPathPoints, glm::vec2 {20.0f, 20.0f});
	EXPECT_FALSE(land_edits::PathRefusal(path).has_value());
	path.points.emplace_back(20.0f, 20.0f);
	EXPECT_TRUE(land_edits::PathRefusal(path).has_value());
	path = good;
	path.points[1].x = land_edits::k_MapSide + 1.0f;
	EXPECT_TRUE(land_edits::PathRefusal(path).has_value());
	path = good;
	path.points[0].y = std::numeric_limits<float>::infinity();
	EXPECT_TRUE(land_edits::PathRefusal(path).has_value());
	path = good;
	path.kind = 2;
	EXPECT_TRUE(land_edits::PathRefusal(path).has_value());
}

TEST(EditorLandEdits, AFootpathTakesItsHeightsFromTheLandAndLeadsFromItsBuildings)
{
	using namespace ecs::components;
	ecs::Registry registry;
	FakeLand land(30, 7, 1);
	const auto home = StandingAt(registry, {20.0f, 7.0f, 20.0f});
	registry.Assign<Abode>(home);
	const auto tree = StandingAt(registry, {90.0f, 7.0f, 20.0f});
	paths::DrawnPaths drawn;
	const EditorPath path {.kind = static_cast<uint8_t>(paths::Kind::Footpath),
	                       .points = {{20.0f, 20.0f}, {50.0f, 20.0f}, {90.0f, 20.0f}},
	                       .firstHolder = home,
	                       .lastHolder = tree};
	const auto made = land_edits::ApplyPath(registry, land, path, &drawn);
	ASSERT_TRUE(made.has_value());
	EXPECT_FALSE(made->stream.has_value());
	// The thing that is not a building keeps no link
	EXPECT_EQ(made->links, 1u);
	const auto& footpath = registry.Get<Footpath>(made->entity);
	ASSERT_EQ(footpath.nodes.size(), 3u);
	EXPECT_FLOAT_EQ(footpath.nodes[1].position.y, 7.0f);
	EXPECT_EQ(footpath.nodes[1].place, paths::PlaceOf({50.0f, 0.0f, 20.0f}));
	ASSERT_TRUE(registry.AllOf<FootpathLink>(home));
	EXPECT_EQ(registry.Get<FootpathLink>(home).footpaths, std::vector<Footpath::Id> {static_cast<Footpath::Id>(made->entity)});
	EXPECT_FALSE(registry.AllOf<FootpathLink>(tree));
	EXPECT_EQ(drawn.footpaths, std::vector<entt::entity> {made->entity});

	// A second footpath with both ends on the same building leads from it one time, and its link comes first
	const EditorPath loop {.kind = static_cast<uint8_t>(paths::Kind::Footpath),
	                       .points = {{20.0f, 20.0f}, {20.0f, 60.0f}},
	                       .firstHolder = home,
	                       .lastHolder = home};
	const auto second = land_edits::ApplyPath(registry, land, loop, nullptr);
	ASSERT_TRUE(second.has_value());
	EXPECT_EQ(second->links, 1u);
	const auto& links = registry.Get<FootpathLink>(home).footpaths;
	ASSERT_EQ(links.size(), 2u);
	EXPECT_EQ(links.front(), static_cast<Footpath::Id>(second->entity));
}

TEST(EditorLandEdits, TheCountriesOfTheLandGoToTheWorldStateAndBack)
{
	const std::vector<land_reuse::CellCountry> countries {{.x = 300, .z = 2, .country = 5}, {.x = 1, .z = 7, .country = 15}};
	// A corner on the edge of a block comes once for each block that keeps a copy
	auto twice = countries;
	twice.push_back(countries[0]);
	const auto bytes = land_reuse::WriteCountries(twice);
	ASSERT_EQ(bytes.size(), 4u + 2u * 5u);
	const auto back = land_reuse::ReadCountries(bytes);
	ASSERT_TRUE(back.has_value());
	// The corners come in the order of the map cells
	EXPECT_EQ(*back, (std::vector<land_reuse::CellCountry> {countries[1], countries[0]}));
	auto cut = bytes;
	cut.pop_back();
	EXPECT_FALSE(land_reuse::ReadCountries(cut).has_value());
	const auto chunk = land_reuse::CountriesChunk(countries);
	const gamesave::WorldFile world {.chunks = {chunk}};
	EXPECT_EQ(land_reuse::KeptCountries(world), back);
	// A file of an older build keeps no countries
	EXPECT_EQ(land_reuse::KeptCountries(gamesave::WorldFile {}), std::vector<land_reuse::CellCountry> {});
}

TEST(EditorLandEdits, TheDigestOfTheLandChangesWithEachAltitudeAndCountry)
{
	const auto digest = land_reuse::CornerDigest(3, 4, 10, 0) ^ land_reuse::CornerDigest(4, 3, 10, 0);
	// The place, the altitude and the country each change the part of a corner
	EXPECT_NE(land_reuse::CornerDigest(3, 4, 10, 0), land_reuse::CornerDigest(4, 3, 10, 0));
	EXPECT_NE(land_reuse::CornerDigest(3, 4, 10, 0), land_reuse::CornerDigest(3, 4, 11, 0));
	EXPECT_NE(land_reuse::CornerDigest(3, 4, 10, 0), land_reuse::CornerDigest(3, 4, 10, 1));
	// A change of one corner and back gives the digest that the land had
	auto changed = digest ^ land_reuse::CornerDigest(3, 4, 10, 0) ^ land_reuse::CornerDigest(3, 4, 12, 0);
	EXPECT_NE(changed, digest);
	changed ^= land_reuse::CornerDigest(3, 4, 12, 0) ^ land_reuse::CornerDigest(3, 4, 10, 0);
	EXPECT_EQ(changed, digest);
	// The digest is the same on every computer
	EXPECT_EQ(land_reuse::CornerDigest(0, 0, 0, 0), 0xe220a8397b1dcdafULL);
}

TEST(EditorLandEdits, AThingPlacedAtATurnBecomesAThingThatTheEditorMade)
{
	ecs::Registry registry;
	const auto first = registry.Create();
	const auto second = registry.Create();
	EditorHistory history;
	history.AddMade(first);
	history.AddMade(second);
	history.AddMade(first);
	history.AddMade(entt::null);
	ASSERT_EQ(history.MadeRefs().size(), 2u);
	EXPECT_EQ(history.Resolve(history.MadeRefs()[0]), first);
	EXPECT_EQ(history.Resolve(history.MadeRefs()[1]), second);
	// The history has no step for them
	EXPECT_FALSE(history.CanUndo());
}
