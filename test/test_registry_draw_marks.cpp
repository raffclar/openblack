/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{

/// A renderer that only counts the marks of the draw
class CountingRenderer final: public systems::RenderingSystemInterface
{
public:
	void SetDirty() override { ++moved; }
	void SetLayoutDirty() override { ++layouts; }
	void PrepareDraw(bool /*drawBoundingBox*/, bool /*drawFootpaths*/, bool /*drawStreams*/, systems::DrawUse /*use*/) override
	{
	}
	const systems::RenderContext& GetContext() override { return _context; }

	int moved {0};
	int layouts {0};

private:
	systems::RenderContext _context;
};

class RegistryDrawMarks: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_renderer = &static_cast<CountingRenderer&>(Locator::rendereringSystem::emplace<CountingRenderer>());
	}
	void TearDown() override { Locator::rendereringSystem::reset(); }

	CountingRenderer* _renderer {nullptr};
};

/// Makes a thing with a place, moves it, then removes it
void MakeMoveRemove(Registry& registry)
{
	const auto entity = registry.Create();
	registry.Assign<components::Transform>(entity);
	registry.AssignOrReplace<components::Transform>(entity);
	registry.Destroy(entity);
}

} // namespace

TEST_F(RegistryDrawMarks, DrawnRegistryMarksTheDraw)
{
	Registry registry;
	MakeMoveRemove(registry);
	EXPECT_GT(_renderer->layouts, 0);
}

TEST_F(RegistryDrawMarks, RegistryThatIsNotDrawnDoesNotMarkTheDraw)
{
	Registry registry(Registry::Drawn::No);
	MakeMoveRemove(registry);
	registry.SetDirty();
	registry.Reset();
	EXPECT_EQ(_renderer->moved, 0);
	EXPECT_EQ(_renderer->layouts, 0);
}
