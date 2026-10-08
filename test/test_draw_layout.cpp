/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The two levels of the draw's dirty flag: which components change the draw layout (ecs::k_ChangesDrawLayout), what
// the Registry tells the rendering system for each operation (a fake one, injected through the Locator), and what a
// PrepareDraw does for each combination of flags (systems::ChooseDrawUpdate), and when a frame prepares the draw at all
// (systems::ShouldPrepareDraw).

#include <stdexcept>
#include <utility>

#include <gtest/gtest.h>

#include "ECS/Components/Abode.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/DrawMesh.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/NotDrawn.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SpecularColour.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/DrawLayoutComponents.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DrawUpdate.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using openblack::ecs::systems::ChooseDrawUpdate;
using openblack::ecs::systems::DrawUpdate;
using openblack::ecs::systems::ShouldPrepareDraw;

namespace
{
/// Counts what the Registry asks of the draw
class FakeRendering final: public systems::RenderingSystemInterface
{
public:
	void SetDirty() override { ++dirty; }
	void SetLayoutDirty() override { ++layout; }
	void PrepareDraw(bool /*drawBoundingBox*/, bool /*drawFootpaths*/, bool /*drawStreams*/) override {}
	[[nodiscard]] const systems::RenderContext& GetContext() override { throw std::logic_error("no context"); }
	int dirty {0};
	int layout {0};
};

class DrawLayoutRegistry: public ::testing::Test
{
protected:
	void SetUp() override { Locator::rendereringSystem::emplace<FakeRendering>(); }
	[[nodiscard]] static FakeRendering& Fake() { return static_cast<FakeRendering&>(Locator::rendereringSystem::value()); }
	/// The counts since the last call: {value, layout}
	static std::pair<int, int> Take()
	{
		auto& fake = Fake();
		const std::pair<int, int> counts {fake.dirty, fake.layout};
		fake.dirty = 0;
		fake.layout = 0;
		return counts;
	}

	test::RestoreService<Locator::rendereringSystem> _restore;
	Registry _registry;
};

constexpr std::pair<int, int> k_Value {1, 0};
constexpr std::pair<int, int> k_Layout {0, 1};
constexpr std::pair<int, int> k_Nothing {0, 0};
} // namespace

TEST(DrawLayout, TheComponentsOfTheLayout)
{
	// the ranges and the drawn mesh
	EXPECT_TRUE(k_ChangesDrawLayout<Mesh>);
	EXPECT_TRUE(k_ChangesDrawLayout<const Transform>);
	EXPECT_TRUE(k_ChangesDrawLayout<DrawMesh>);
	EXPECT_TRUE(k_ChangesDrawLayout<MorphWithTerrain>);
	EXPECT_TRUE(k_ChangesDrawLayout<Alpha>);
	EXPECT_TRUE(k_ChangesDrawLayout<NotDrawn>);
	EXPECT_TRUE(k_ChangesDrawLayout<Unavailable>);
	// the static shadow casters
	EXPECT_TRUE(k_ChangesDrawLayout<Abode>);
	EXPECT_TRUE(k_ChangesDrawLayout<Field>);
	EXPECT_TRUE(k_ChangesDrawLayout<Pot>);
	// what only changes a row
	EXPECT_FALSE(k_ChangesDrawLayout<UvScroll>);
	EXPECT_FALSE(k_ChangesDrawLayout<SpecularColour>);
	EXPECT_TRUE((k_AnyChangesDrawLayout<UvScroll, Alpha>));
	EXPECT_FALSE((k_AnyChangesDrawLayout<UvScroll, SpecularColour>));
}

TEST_F(DrawLayoutRegistry, AssignAndRemove)
{
	const auto entity = _registry.Create();
	_registry.Assign<Alpha>(entity);
	EXPECT_EQ(Take(), k_Layout);
	_registry.Assign<UvScroll>(entity);
	EXPECT_EQ(Take(), k_Value);
	_registry.AssignOrReplace<UvScroll>(entity);
	EXPECT_EQ(Take(), k_Value);
	// a layout component replaced keeps the entity's ranges
	_registry.AssignOrReplace<Alpha>(entity, 0.5f);
	EXPECT_EQ(Take(), k_Value);
	_registry.Remove<UvScroll>(entity);
	EXPECT_EQ(Take(), k_Value);
	// several at once: any of the layout's that the entity has
	_registry.Assign<UvScroll>(entity);
	Take();
	_registry.Remove<UvScroll, Alpha>(entity);
	EXPECT_EQ(Take(), k_Layout);
	// removing what the entity does not have changes nothing in the draw lists
	_registry.Remove<Alpha, NotDrawn>(entity);
	EXPECT_EQ(Take(), k_Value);
	// a layout component gained through AssignOrReplace
	_registry.AssignOrReplace<NotDrawn>(entity);
	EXPECT_EQ(Take(), k_Layout);
	_registry.Remove<Alpha, NotDrawn>(entity);
	EXPECT_EQ(Take(), k_Layout);
}

TEST_F(DrawLayoutRegistry, SetDirtyDestroyResetAndState)
{
	const auto entity = _registry.Create();
	_registry.SetDirty();
	EXPECT_EQ(Take(), k_Value);
	// the simulation-only operations say nothing to the draw
	_registry.AssignState<SpecularColour>(entity);
	_registry.RemoveState<SpecularColour>(entity);
	EXPECT_EQ(Take(), k_Nothing);
	// an entity gone, or all of them
	_registry.Destroy(entity);
	EXPECT_EQ(Take(), k_Layout);
	_registry.Reset();
	EXPECT_EQ(Take(), k_Layout);
}

TEST(DrawLayout, WhatAPrepareDrawDoes)
{
	// nothing changed
	EXPECT_EQ(ChooseDrawUpdate(false, false, false, false), DrawUpdate::None);
	EXPECT_EQ(ChooseDrawUpdate(false, false, false, true), DrawUpdate::None);
	// only values: a refill, unless the debug boxes are drawn (only a rebuild writes their rows)
	EXPECT_EQ(ChooseDrawUpdate(true, false, false, false), DrawUpdate::Refill);
	EXPECT_EQ(ChooseDrawUpdate(true, false, false, true), DrawUpdate::Rebuild);
	// the layout, or a debug view turned on or off
	EXPECT_EQ(ChooseDrawUpdate(true, true, false, false), DrawUpdate::Rebuild);
	EXPECT_EQ(ChooseDrawUpdate(false, true, false, false), DrawUpdate::Rebuild);
	EXPECT_EQ(ChooseDrawUpdate(false, false, true, false), DrawUpdate::Rebuild);
	EXPECT_EQ(ChooseDrawUpdate(true, false, true, true), DrawUpdate::Rebuild);
}

TEST(DrawLayout, NoPrepareDrawWhileAFilmHidesTheWorld)
{
	EXPECT_TRUE(ShouldPrepareDraw(true, false));
	// a full screen film, or the falling spell's: the scene is not drawn
	EXPECT_FALSE(ShouldPrepareDraw(true, true));
	// the debug option that turns the entities off
	EXPECT_FALSE(ShouldPrepareDraw(false, false));
	EXPECT_FALSE(ShouldPrepareDraw(false, true));
}
