/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Graphics/ObjectDrawWay.h"

using namespace openblack::graphics;
using object_draw::Way;

// Only the flag 0x200 of a mesh marks it to be sorted whole
TEST(ObjectDrawWay, OnlyTheFlag0x200SortsAMeshWhole)
{
	EXPECT_TRUE(object_draw::SortedWhole(0x200u));
	// The phials and the band round an extreme miracle have the flag among others
	EXPECT_TRUE(object_draw::SortedWhole(0xA2200u));
	EXPECT_FALSE(object_draw::SortedWhole(0x23116u));
	EXPECT_FALSE(object_draw::SortedWhole(0u));
}

// A draw at once is cut when the model is further than 300 of its radii, its radius at its scale
TEST(ObjectDrawWay, TheFarCullCutsAModelFurtherThan300OfItsRadii)
{
	const glm::vec3 camera {0.0f, 0.0f, 0.0f};
	// A radius of 2 at a scale of 0.5 reaches 300 units
	EXPECT_FALSE(object_draw::FarCulled(2.0f, 0.5f, {0.0f, 0.0f, 300.0f}, camera));
	EXPECT_TRUE(object_draw::FarCulled(2.0f, 0.5f, {0.0f, 0.0f, 300.1f}, camera));
	EXPECT_TRUE(object_draw::FarCulled(2.0f, 0.5f, {200.0f, 100.0f, 201.0f}, camera));
	// A model at exactly 300 of its radii is not cut
	EXPECT_FALSE(object_draw::FarCulled(2.0f, 0.5f, {200.0f, 100.0f, 200.0f}, camera));
	// A model of no size is cut at once
	EXPECT_TRUE(object_draw::FarCulled(2.0f, 0.0f, {0.0f, 0.0f, 1.0f}, camera));
}

// The object draw cuts a model after its fade and gives no draw
TEST(ObjectDrawWay, TheObjectDrawCutsTheModelAfterTheFade)
{
	const mesh_detail::Choice gone {.mesh = std::nullopt, .alpha = std::nullopt};
	EXPECT_FALSE(object_draw::DrawBanded(gone, 255, false).has_value());
	EXPECT_FALSE(object_draw::DrawBanded(gone, 34, true).has_value());
}

// In the fade a model takes the fade's alpha, unless it keeps its own alpha
TEST(ObjectDrawWay, TheFadeGivesItsAlphaUnlessTheModelKeepsItsOwn)
{
	const mesh_detail::Choice fading {.mesh = mesh_detail::Mesh::Low, .alpha = 200};
	const auto faded = object_draw::DrawBanded(fading, 255, false);
	ASSERT_TRUE(faded.has_value());
	EXPECT_EQ(faded->mesh, mesh_detail::Mesh::Low);
	EXPECT_EQ(faded->alpha, 200);
	// A seed of less than full alpha keeps its alpha
	const auto kept = object_draw::DrawBanded(fading, 149, true);
	ASSERT_TRUE(kept.has_value());
	EXPECT_EQ(kept->alpha, 149);
	// The band round an extreme miracle does not keep its alpha: the fade gives it a greater alpha than its own
	const auto ring = object_draw::DrawBanded(fading, 59, false);
	ASSERT_TRUE(ring.has_value());
	EXPECT_EQ(ring->alpha, 200);
}

// Before the fade the model keeps its own alpha and draws the mesh of its band
TEST(ObjectDrawWay, BeforeTheFadeTheModelKeepsItsAlphaAndItsBand)
{
	const mesh_detail::Choice standard {.mesh = mesh_detail::Mesh::Standard, .alpha = std::nullopt};
	const auto drawn = object_draw::DrawBanded(standard, 59, false);
	ASSERT_TRUE(drawn.has_value());
	EXPECT_EQ(drawn->mesh, mesh_detail::Mesh::Standard);
	EXPECT_EQ(drawn->alpha, 59);
}

// Only the object draw sorts a mesh marked to be sorted whole, by the key of a placed model at its place. Every other
// draw comes after the solid models and before every sorted thing.
TEST(ObjectDrawWay, OnlyTheObjectDrawSortsAModelMarkedToBeSortedWhole)
{
	const glm::vec3 place {3.0f, 4.0f, 5.0f};
	const glm::vec3 camera {1.0f, 1.0f, 1.0f};
	EXPECT_TRUE(object_draw::AmongSorted(Way::Banded, true));
	EXPECT_FALSE(object_draw::AmongSorted(Way::Banded, false));
	EXPECT_FALSE(object_draw::AmongSorted(Way::AtOnce, true));
	EXPECT_EQ(object_draw::BlendDepth(Way::Banded, true, place, camera), zsort::ModelDepth(place, camera));
	EXPECT_EQ(object_draw::BlendDepth(Way::Banded, false, place, camera), zsort::k_AfterSolidBeforeSorted);
	EXPECT_EQ(object_draw::BlendDepth(Way::AtOnce, true, place, camera), zsort::k_AfterSolidBeforeSorted);
	// A draw at once comes before every sorted key
	EXPECT_GT(zsort::k_AfterSolidBeforeSorted, zsort::ModelDepth(place, camera));
}

// The game draws a standing model that fades or is a see-through copy at once, unless its mesh or its own draw sorts it
TEST(ObjectDrawWay, AFadingModelBlendsAtOnceUnlessItIsSorted)
{
	EXPECT_TRUE(object_draw::BlendsAtOnce({.blended = true}));
	EXPECT_FALSE(object_draw::BlendsAtOnce({.blended = false}));
	EXPECT_FALSE(object_draw::BlendsAtOnce({.blended = true, .sortedWhole = true}));
	EXPECT_FALSE(object_draw::BlendsAtOnce({.blended = true, .sortedByItsDraw = true}));
}
