/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <string>
#include <vector>

#include <entt/entity/registry.hpp>
#include <entt/meta/context.hpp>
#include <gtest/gtest.h>

#include "3D/CameraPath.h"
#include "ECS/Components/SnowLevel.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/Reflection/WorldArchive.h"
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

namespace
{

/// A component that nothing registered with the reflection
struct FakeUnregistered
{
	int value {0};
};

} // namespace

TEST(SyncedStateUnregistered, AWorldOfRegisteredComponentsHasNoneUnregistered)
{
	entt::meta_ctx context;
	Register(context);
	entt::registry registry;
	const auto body = registry.create();
	registry.emplace<FakeBody>(body);
	registry.emplace<FakeHidden>(body);
	EXPECT_TRUE(synced_state::HashOf(registry, context, k_Presentation).unregistered.empty());
	EXPECT_TRUE(UnregisteredStorages(registry, context).empty());
}

TEST(SyncedStateUnregistered, AComponentWithNoRegistrationIsNamedInTheHashAndLeftOutOfIt)
{
	entt::meta_ctx context;
	Register(context);
	entt::registry registry;
	const auto body = registry.create();
	registry.emplace<FakeBody>(body);
	const auto before = synced_state::HashOf(registry, context, k_Presentation);
	registry.emplace<FakeUnregistered>(body, FakeUnregistered {.value = 7});
	const auto after = synced_state::HashOf(registry, context, k_Presentation);
	EXPECT_EQ(after.unregistered, std::vector<std::string> {"FakeUnregistered"});
	EXPECT_EQ(UnregisteredStorages(registry, context), after.unregistered);
	// The hash cannot see the component, which is why the game must have none
	EXPECT_EQ(after.hash, before.hash);
	registry.get<FakeUnregistered>(body).value = 8;
	EXPECT_EQ(synced_state::HashOf(registry, context, k_Presentation).hash, before.hash);
}

TEST(SyncedStateUnregistered, AnEmptyStorageIsNotUnregistered)
{
	entt::meta_ctx context;
	Register(context);
	entt::registry registry;
	const auto entity = registry.create();
	registry.emplace<FakeUnregistered>(entity);
	registry.erase<FakeUnregistered>(entity);
	EXPECT_TRUE(UnregisteredStorages(registry, context).empty());
}

TEST(SyncedStateUnregistered, TheReporterTellsOfEachNameOnce)
{
	synced_state::UnregisteredReporter reporter;
	const std::vector<std::string> first {"Alpha", "Beta"};
	EXPECT_EQ(reporter.Fresh(first), first);
	EXPECT_TRUE(reporter.Fresh(first).empty());
	const std::vector<std::string> more {"Beta", "Gamma"};
	EXPECT_EQ(reporter.Fresh(more), std::vector<std::string> {"Gamma"});
}

TEST(SyncedStateUnregistered, TheLineHasTheWordsThatTheToolsLookFor)
{
	const std::vector<std::string> names {"Alpha", "Beta"};
	EXPECT_EQ(synced_state::UnregisteredLine(names), "unregistered component storage: Alpha, Beta");
}

// Every component that the game's reflection registers gives no unregistered storage, with the real registration
TEST(SyncedStateUnregistered, TheGamesOwnRegistrationCoversACameraPoint)
{
	entt::meta_ctx context;
	RegisterComponents(context);
	entt::registry registry;
	const auto entity = registry.create();
	registry.emplace<CameraPoint>(entity);
	EXPECT_TRUE(UnregisteredStorages(registry, context).empty());
}

TEST(SyncedState, TheSnowThatAThingShowsDoesNotChangeTheHash)
{
	entt::meta_ctx context;
	Register(context);
	Reflect<ecs::components::SnowLevel>(context).Field<&ecs::components::SnowLevel::level>("level");
	entt::registry registry;
	const auto body = registry.create();
	registry.emplace<FakeBody>(body, FakeBody {.place = 1.0f});
	const auto tree = registry.create();
	registry.emplace<FakeBody>(tree, FakeBody {.place = 3.0f});
	registry.emplace<ecs::components::SnowLevel>(tree, ecs::components::SnowLevel {.level = 10});
	const auto parts = synced_state::PresentationParts();
	const auto before = synced_state::HashOf(registry, context, parts).hash;
	// The frames of one computer draw the two things in the snow: the snow eases on the tree and starts on the body
	registry.get<ecs::components::SnowLevel>(tree).level = 40;
	registry.emplace<ecs::components::SnowLevel>(body, ecs::components::SnowLevel {.level = 5});
	EXPECT_EQ(synced_state::HashOf(registry, context, parts).hash, before);
	// The turn moves the tree: the synced state changes
	registry.get<FakeBody>(tree).place = 4.0f;
	EXPECT_NE(synced_state::HashOf(registry, context, parts).hash, before);
}
