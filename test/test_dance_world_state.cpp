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
#include <memory>
#include <optional>
#include <vector>

#include <DanceFile.h>
#include <entt/entity/registry.hpp>
#include <entt/meta/context.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Dance.h"
#include "ECS/Dances.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/Reflection/WorldArchive.h"
#include "ECS/Registry.h"
#include "ECS/WorldSave.h"
#include "Enums.h"

using namespace openblack;
using namespace openblack::ecs;
using openblack::ecs::components::Dance;
using openblack::ecs::components::DanceGroup;

namespace
{
void Put(std::vector<uint8_t>& bytes, uint32_t value)
{
	for (int i = 0; i < 4; ++i)
	{
		bytes.push_back(static_cast<uint8_t>(value >> (8 * i)));
	}
}

void PutAction(std::vector<uint8_t>& bytes, const std::vector<uint32_t>& groups, uint32_t type, uint32_t first, uint32_t second)
{
	Put(bytes, static_cast<uint32_t>(groups.size()));
	for (const auto group : groups)
	{
		Put(bytes, group);
	}
	Put(bytes, type);
	Put(bytes, first);
	Put(bytes, second);
	for (int i = 2; i < 14; ++i)
	{
		Put(bytes, 0);
	}
}

/// A dance of two groups that share their dancers half and half. A later key frame makes the first group grow, so its
/// move changes the group in each turn after that key frame.
std::shared_ptr<const dance::DanceFile> GrowingDance()
{
	std::vector<uint8_t> bytes;
	Put(bytes, 1); // version
	Put(bytes, 2); // key frames
	Put(bytes, std::bit_cast<uint32_t>(0.0f));
	Put(bytes, 1);
	Put(bytes, 2); // actions
	PutAction(bytes, {0}, 6, 0, 50);
	PutAction(bytes, {1}, 6, 0, 50);
	Put(bytes, std::bit_cast<uint32_t>(20.0f));
	Put(bytes, 0);
	Put(bytes, 1);
	PutAction(bytes, {1}, 16, 2, 0);
	Put(bytes, std::bit_cast<uint32_t>(0.0f)); // clock
	Put(bytes, 2);                             // groups
	Put(bytes, 14);
	Put(bytes, 0);
	Put(bytes, 1); // loops
	for (const char name : {'M', 'W'})
	{
		Put(bytes, 1);
		bytes.push_back(static_cast<uint8_t>(name));
	}
	auto file = std::make_shared<dance::DanceFile>();
	EXPECT_EQ(file->Open(bytes), dance::DanceResult::Success);
	dance::DanceAction grow;
	grow.groups = {0};
	grow.type = 3;
	grow.arguments[4] = 11;
	grow.arguments[5] = std::bit_cast<uint32_t>(15.0f);
	grow.arguments[7] = 100;
	file->keyFrames.at(1).actions.push_back(grow);
	return file;
}

/// One turn of each dance of the registry, as the turn of the game does it, with no hand near
void DanceTurn(Registry& registry, uint32_t turn)
{
	const dances::TurnContext context {.turn = turn, .available = [](entt::entity) { return true; }};
	std::vector<entt::entity> all;
	registry.Each<const Dance>([&all](entt::entity entity, const auto&...) { all.push_back(entity); });
	for (const auto entity : all)
	{
		dances::ProcessTurn(registry, entity, context);
	}
}

/// A dance with three dancers, played to the turn before its second key frame
struct DancingWorld
{
	Registry registry;
	entt::entity dance {entt::null};
	uint32_t turn {0};

	explicit DancingWorld(const std::shared_ptr<const dance::DanceFile>& file)
	{
		dance = dances::Create(registry, {.type = DanceInfo::Worship, .autostart = true}, file);
		for (int i = 0; i < 3; ++i)
		{
			const auto dancer = registry.Create();
			EXPECT_TRUE(dances::AddDancer(registry, dance, dancer, i % 2 == 0 ? DanceGroup::k_Men : DanceGroup::k_Women));
		}
		for (; turn < 15; ++turn)
		{
			DanceTurn(registry, turn);
		}
	}
};

/// The world that a client gets from the bytes of the world state of another computer
void LoadInto(Registry& loaded, const entt::meta_ctx& context, const std::vector<uint8_t>& bytes,
              const std::shared_ptr<const dance::DanceFile>& file)
{
	ASSERT_TRUE(reflection::LoadWorld(loaded.Underlying(), context, bytes, reflection::WorldLoadMode::Exact).has_value());
	if (file != nullptr)
	{
		world_save::TakeDanceFilesAgain(loaded.Underlying(), [&file](DanceInfo) { return file; });
	}
}
} // namespace

// A client that joins takes the world state of the server while a dance has dancers. The loaded dance gives the same
// synced bytes as the server's dance in each later turn, through its key frames and the moves of its groups.
TEST(DanceWorldState, ALoadedDanceStaysTheSameAsTheDanceOfTheServer)
{
	entt::meta_ctx context;
	reflection::RegisterComponents(context);
	const auto file = GrowingDance();
	DancingWorld server(file);
	ASSERT_EQ(server.registry.Get<const Dance>(server.dance).state, Dance::State::Dancing);
	const auto state = reflection::SaveWorld(server.registry.Underlying(), context);

	Registry client;
	LoadInto(client, context, state, file);
	EXPECT_EQ(reflection::SaveWorld(client.Underlying(), context), state);
	for (auto turn = server.turn; turn < server.turn + 200; ++turn)
	{
		DanceTurn(server.registry, turn);
		DanceTurn(client, turn);
		ASSERT_EQ(reflection::SaveWorld(client.Underlying(), context),
		          reflection::SaveWorld(server.registry.Underlying(), context))
		    << "turn " << turn;
	}
	// The key frame at beat 20 started the move of the first group
	EXPECT_NE(client.Get<const Dance>(server.dance).groups.all.at(0).radius, 20.0f);
}

// The world state does not keep the file of a dance. A loaded dance without its file does not do its key frames, so the
// moves of its groups part from the server at the next key frame.
TEST(DanceWorldState, ADanceWithoutItsFileAfterTheLoadPartsFromTheServer)
{
	entt::meta_ctx context;
	reflection::RegisterComponents(context);
	const auto file = GrowingDance();
	DancingWorld server(file);
	const auto state = reflection::SaveWorld(server.registry.Underlying(), context);

	Registry client;
	LoadInto(client, context, state, nullptr);
	std::optional<uint32_t> parted;
	for (auto turn = server.turn; turn < server.turn + 200 && !parted.has_value(); ++turn)
	{
		DanceTurn(server.registry, turn);
		DanceTurn(client, turn);
		if (reflection::SaveWorld(client.Underlying(), context) != reflection::SaveWorld(server.registry.Underlying(), context))
		{
			parted = turn;
		}
	}
	ASSERT_TRUE(parted.has_value());
	// Only the server's first group grows
	EXPECT_NE(client.Get<const Dance>(server.dance).groups.all.at(0).radiusRate,
	          server.registry.Get<const Dance>(server.dance).groups.all.at(0).radiusRate);
}
