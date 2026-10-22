/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>

#include <entt/entity/registry.hpp>
#include <entt/meta/context.hpp>
#include <gtest/gtest.h>

#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/SyncedState.h"

using namespace openblack;
using namespace openblack::ecs::reflection;

namespace
{

struct FakeBody
{
	float place {0.0f};
	float drawnPlace {0.0f};
};

struct FakeGlow
{
	float shine {0.0f};
};

struct FakeGlowMark
{
	int kind {0};
};

/// A component with no values, as a mark that the frames put on and take off
struct FakeHidden
{
};

void Register(entt::meta_ctx& context)
{
	Reflect<FakeBody>(context).Field<&FakeBody::place>("place").Field<&FakeBody::drawnPlace>("drawnPlace");
	Reflect<FakeGlow>(context).Field<&FakeGlow::shine>("shine");
	Reflect<FakeGlowMark>(context).Field<&FakeGlowMark::kind>("kind");
	Reflect<FakeHidden> {context};
}

// The drawn place of each body, and every component of an entity that the glow mark marks
constexpr auto k_Presentation = std::to_array<synced_state::PresentationPart>({
    {.component = "FakeBody", .field = "drawnPlace", .marker = ""},
    {.component = "*", .field = "", .marker = "FakeGlowMark"},
});

} // namespace

TEST(SyncedState, ThePresentationPartsDoNotChangeTheHash)
{
	entt::meta_ctx context;
	Register(context);
	entt::registry registry;
	const auto body = registry.create();
	registry.emplace<FakeBody>(body, FakeBody {.place = 1.0f, .drawnPlace = 1.0f});
	const auto glow = registry.create();
	registry.emplace<FakeGlowMark>(glow);
	registry.emplace<FakeGlow>(glow, FakeGlow {.shine = 0.5f});
	const auto before = synced_state::HashOf(registry, context, k_Presentation).hash;
	// The frames move the drawn place and the shine of the marked glow: the synced state stays the same
	registry.get<FakeBody>(body).drawnPlace = 2.0f;
	registry.get<FakeGlow>(glow).shine = 0.9f;
	EXPECT_EQ(synced_state::HashOf(registry, context, k_Presentation).hash, before);
	// The turn moves the place: the synced state changes
	registry.get<FakeBody>(body).place = 2.0f;
	EXPECT_NE(synced_state::HashOf(registry, context, k_Presentation).hash, before);
}

TEST(SyncedState, AGlowWithNoMarkIsPartOfTheSyncedState)
{
	entt::meta_ctx context;
	Register(context);
	entt::registry registry;
	const auto glow = registry.create();
	registry.emplace<FakeGlow>(glow, FakeGlow {.shine = 0.5f});
	const auto before = synced_state::HashOf(registry, context, k_Presentation).hash;
	registry.get<FakeGlow>(glow).shine = 0.9f;
	EXPECT_NE(synced_state::HashOf(registry, context, k_Presentation).hash, before);
}

TEST(SyncedState, TheChecksumFollowsTheStateTheSeedAndTheDraws)
{
	const auto checksum = synced_state::Checksum(1, 2, 3);
	EXPECT_EQ(synced_state::Checksum(1, 2, 3), checksum);
	EXPECT_NE(synced_state::Checksum(4, 2, 3), checksum);
	EXPECT_NE(synced_state::Checksum(1, 5, 3), checksum);
	EXPECT_NE(synced_state::Checksum(1, 2, 6), checksum);
}

TEST(SyncedState, TheGamesPresentationPartsHaveAComponentEach)
{
	for (const auto& part : synced_state::PresentationParts())
	{
		EXPECT_FALSE(part.component.empty());
	}
}

TEST(SyncedState, AMarkThatOnlyThePresentationHasDoesNotChangeTheHash)
{
	entt::meta_ctx context;
	Register(context);
	entt::registry registry;
	const auto body = registry.create();
	registry.emplace<FakeBody>(body, FakeBody {.place = 1.0f});
	const auto glow = registry.create();
	registry.emplace<FakeGlowMark>(glow);
	const auto before = synced_state::HashOf(registry, context, k_Presentation).hash;
	// The frames hide the marked glow: no entity of the synced state has the mark, so the hash stays the same
	registry.emplace<FakeHidden>(glow);
	EXPECT_EQ(synced_state::HashOf(registry, context, k_Presentation).hash, before);
	// The turn hides the body: the synced state changes
	registry.emplace<FakeHidden>(body);
	EXPECT_NE(synced_state::HashOf(registry, context, k_Presentation).hash, before);
}
