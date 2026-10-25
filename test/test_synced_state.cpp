/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include <entt/entity/registry.hpp>
#include <entt/meta/context.hpp>
#include <gtest/gtest.h>

#include "Common/WorkerPool.h"
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

/// The hash of the fixed world of the test TheHashOfAWorldStaysTheSame
constexpr uint64_t k_FixedWorldHash = 7309699159427777647ull;

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

TEST(SyncedState, TheNumbersOfTheCastEffectsOfEachComputerAreNotSynced)
{
	// Each computer numbers its particle effects. The computers that draw start more effects than the server.
	const auto parts = synced_state::PresentationParts();
	EXPECT_TRUE(synced_state::IsPresentationOnly(parts, "Spell", "effect"));
	EXPECT_TRUE(synced_state::IsPresentationOnly(parts, "Spell", "castEffect"));
	EXPECT_TRUE(synced_state::IsPresentationOnly(parts, "FlockSpell", "castEffect"));
	// The turn reads the rest of a miracle
	EXPECT_FALSE(synced_state::IsPresentationOnly(parts, "Spell", "magnitude"));
	EXPECT_FALSE(synced_state::IsPresentationOnly(parts, "FlockSpell", "castEffectStarted"));
}

TEST(SyncedState, TheDustOfARewardChestIsNotSynced)
{
	// The C library's random numbers place the dust, and the frames fade it. The game server draws neither.
	const auto parts = synced_state::PresentationParts();
	EXPECT_TRUE(synced_state::IsPresentationOnly(parts, "Reward", "dust"));
	EXPECT_TRUE(synced_state::IsPresentationOnly(parts, "Reward", "dustMilliseconds"));
	// The turn reads the state of the chest
	EXPECT_FALSE(synced_state::IsPresentationOnly(parts, "Reward", "state"));
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

TEST(SyncedState, AFieldWithAMarkerLeavesOutOnlyTheMarkedEntities)
{
	// The place of each body that has the glow mark, and the whole glow of each entity
	constexpr auto k_Parts = std::to_array<synced_state::PresentationPart>({
	    {.component = "FakeBody", .field = "place", .marker = "FakeGlowMark"},
	    {.component = "FakeGlow", .field = "", .marker = ""},
	});
	entt::meta_ctx context;
	Register(context);
	entt::registry registry;
	const auto marked = registry.create();
	registry.emplace<FakeGlowMark>(marked);
	registry.emplace<FakeBody>(marked, FakeBody {.place = 1.0f});
	const auto plain = registry.create();
	registry.emplace<FakeBody>(plain, FakeBody {.place = 1.0f});
	registry.emplace<FakeGlow>(plain, FakeGlow {.shine = 0.5f});
	const auto before = synced_state::HashOf(registry, context, k_Parts);
	// The glow and the place of the marked body are not in the hash
	registry.get<FakeGlow>(plain).shine = 0.9f;
	registry.get<FakeBody>(marked).place = 2.0f;
	const auto after = synced_state::HashOf(registry, context, k_Parts);
	EXPECT_EQ(after.hash, before.hash);
	EXPECT_EQ(std::ranges::count(after.parts, std::string("FakeGlow"), &std::pair<std::string, uint64_t>::first), 0);
	// The drawn place of the marked body is in the hash
	registry.get<FakeBody>(marked).drawnPlace = 2.0f;
	EXPECT_NE(synced_state::HashOf(registry, context, k_Parts).hash, before.hash);
	// The place of the body with no mark is in the hash
	registry.get<FakeBody>(marked).drawnPlace = 0.0f;
	registry.get<FakeBody>(plain).place = 2.0f;
	EXPECT_NE(synced_state::HashOf(registry, context, k_Parts).hash, before.hash);
}

TEST(SyncedState, APartWithAMarkerThatNoStorageHasStaysInTheHash)
{
	constexpr auto k_Parts = std::to_array<synced_state::PresentationPart>({
	    {.component = "FakeBody", .field = "place", .marker = "FakeNoSuchMark"},
	});
	entt::meta_ctx context;
	Register(context);
	entt::registry registry;
	const auto body = registry.create();
	registry.emplace<FakeBody>(body, FakeBody {.place = 1.0f});
	const auto before = synced_state::HashOf(registry, context, k_Parts).hash;
	registry.get<FakeBody>(body).place = 2.0f;
	EXPECT_NE(synced_state::HashOf(registry, context, k_Parts).hash, before);
}

TEST(SyncedState, TheHashOfAWorldStaysTheSame)
{
	// A fixed world gives a fixed hash, so that a change to the code of the hash does not change the checksums
	entt::meta_ctx context;
	Register(context);
	entt::registry registry;
	const auto body = registry.create();
	registry.emplace<FakeBody>(body, FakeBody {.place = 1.5f, .drawnPlace = 3.0f});
	const auto glow = registry.create();
	registry.emplace<FakeGlowMark>(glow, FakeGlowMark {.kind = 2});
	registry.emplace<FakeGlow>(glow, FakeGlow {.shine = 0.25f});
	registry.emplace<FakeHidden>(body);
	registry.emplace<FakeHidden>(glow);
	EXPECT_EQ(synced_state::HashOf(registry, context, k_Presentation).hash, k_FixedWorldHash);
}

TEST(SyncedState, TheThreadsOfAPoolGiveTheSameHash)
{
	entt::meta_ctx context;
	Register(context);
	entt::registry registry;
	for (int i = 0; i < 20; ++i)
	{
		const auto entity = registry.create();
		registry.emplace<FakeBody>(entity, FakeBody {.place = static_cast<float>(i), .drawnPlace = 0.5f});
		if (i % 3 == 0)
		{
			registry.emplace<FakeGlowMark>(entity, FakeGlowMark {.kind = i});
			registry.emplace<FakeGlow>(entity, FakeGlow {.shine = static_cast<float>(i) * 0.25f});
		}
		if (i % 4 == 0)
		{
			registry.emplace<FakeHidden>(entity);
		}
	}
	WorkerPool pool(3);
	for (const bool byField : {false, true})
	{
		const auto alone = synced_state::HashOf(registry, context, k_Presentation, byField);
		const auto shared = synced_state::HashOf(registry, context, k_Presentation, byField, {}, &pool);
		EXPECT_EQ(shared.hash, alone.hash);
		EXPECT_EQ(shared.parts, alone.parts);
	}
}
