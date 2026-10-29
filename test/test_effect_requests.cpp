/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <utility>
#include <variant>
#include <vector>

#include <entt/meta/context.hpp>
#include <gtest/gtest.h>

#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Systems/EffectRequestSystemInterface.h"

#define LOCATOR_IMPLEMENTATIONS

#include "ECS/Systems/Implementations/EffectRequestSystem.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{

/// The particles for the test: they keep what the simulation asked for
class FakeServer final: public EffectServerInterface
{
public:
	std::vector<std::pair<EffectRequest, EffectHandle>> served;
	std::vector<std::pair<EffectHandle, glm::vec3>> moved;
	std::vector<EffectHandle> stopped;
	std::vector<EffectHandle> removed;

	void Serve(const EffectRequest& request, EffectHandle handle) override { served.emplace_back(request, handle); }
	void Move(EffectHandle handle, glm::vec3 position) override { moved.emplace_back(handle, position); }
	void Stop(EffectHandle handle) override { stopped.push_back(handle); }
	void Remove(EffectHandle handle) override { removed.push_back(handle); }
};

} // namespace

TEST(EffectRequests, EachStartTakesTheNextNumberAndThePresentationUsesIt)
{
	FakeServer server;
	EffectRequestSystem effects([&server]() -> EffectServerInterface* { return &server; });
	const auto first = effects.Start({.type = ParticleType::SpelldispenserVortex, .synced = false, .step = EffectStep::Frame});
	const auto second = effects.Start({.type = SpotVisualType::ObjectAppear, .position = {1.0f, 2.0f, 3.0f}});
	EXPECT_EQ(first, k_FirstEffectHandle);
	EXPECT_EQ(second, k_FirstEffectHandle + 1);
	EXPECT_EQ(effects.NextHandle(), k_FirstEffectHandle + 2);
	ASSERT_EQ(server.served.size(), 2u);
	EXPECT_EQ(server.served[0].second, first);
	EXPECT_EQ(std::get<ParticleType>(server.served[0].first.type), ParticleType::SpelldispenserVortex);
	EXPECT_EQ(server.served[0].first.step, EffectStep::Frame);
	EXPECT_FALSE(server.served[0].first.synced);
	EXPECT_EQ(server.served[1].second, second);
	EXPECT_EQ(std::get<SpotVisualType>(server.served[1].first.type), SpotVisualType::ObjectAppear);
	EXPECT_EQ(server.served[1].first.position, glm::vec3(1.0f, 2.0f, 3.0f));
}

TEST(EffectRequests, AnEffectThatPlaysOutByItselfTakesNoNumber)
{
	FakeServer server;
	EffectRequestSystem effects([&server]() -> EffectServerInterface* { return &server; });
	effects.Show({.type = SpotVisualType::TownFireworks});
	EXPECT_EQ(effects.NextHandle(), k_FirstEffectHandle);
	ASSERT_EQ(server.served.size(), 1u);
	EXPECT_EQ(server.served[0].second, k_NoEffectHandle);
}

TEST(EffectRequests, AComputerThatShowsNothingGivesTheSameNumbers)
{
	FakeServer server;
	EffectRequestSystem shown([&server]() -> EffectServerInterface* { return &server; });
	EffectRequestSystem unshown([]() -> EffectServerInterface* { return nullptr; });
	for (auto* effects : {&shown, &unshown})
	{
		effects->Show({.type = SpotVisualType::GripLandscape});
		[[maybe_unused]] const auto first = effects->Start({.type = ParticleType::StormCast});
		effects->Move(first, glm::vec3(1.0f));
		effects->Stop(first);
		effects->Remove(first);
	}
	EXPECT_EQ(shown.NextHandle(), unshown.NextHandle());
	EXPECT_EQ(server.served.size(), 2u);
}

TEST(EffectRequests, MovesStopsAndRemovesGoToThePresentationByTheNumber)
{
	FakeServer server;
	EffectRequestSystem effects([&server]() -> EffectServerInterface* { return &server; });
	const auto handle = effects.Start({.type = ParticleType::StormCast});
	effects.Move(handle, {4.0f, 5.0f, 6.0f});
	effects.Stop(handle);
	effects.Remove(handle);
	// No effect: nothing goes to the presentation
	effects.Move(k_NoEffectHandle, glm::vec3(0.0f));
	effects.Stop(k_NoEffectHandle);
	effects.Remove(k_NoEffectHandle);
	ASSERT_EQ(server.moved.size(), 1u);
	EXPECT_EQ(server.moved[0].first, handle);
	EXPECT_EQ(server.moved[0].second, glm::vec3(4.0f, 5.0f, 6.0f));
	EXPECT_EQ(server.stopped, std::vector {handle});
	EXPECT_EQ(server.removed, std::vector {handle});
}

TEST(EffectRequests, ANewLandNumbersFromTheFirstAgain)
{
	EffectRequestSystem effects([]() -> EffectServerInterface* { return nullptr; });
	[[maybe_unused]] const auto first = effects.Start({.type = ParticleType::StormCast});
	effects.Reset();
	EXPECT_EQ(effects.Start({.type = ParticleType::StormCast}), k_FirstEffectHandle);
}

TEST(EffectRequests, ASaveKeepsTheNextNumber)
{
	const entt::meta_ctx context;
	EffectRequestSystem saved([]() -> EffectServerInterface* { return nullptr; });
	[[maybe_unused]] const auto first = saved.Start({.type = ParticleType::StormCast});
	[[maybe_unused]] const auto second = saved.Start({.type = ParticleType::StormCast});
	ecs::reflection::BinaryWriter out(context);
	saved.SaveState(out);

	EffectRequestSystem loaded([]() -> EffectServerInterface* { return nullptr; });
	const auto bytes = out.Bytes();
	const ecs::reflection::EntityRemap identity;
	ecs::reflection::BinaryReader in(context, bytes, identity);
	ASSERT_TRUE(loaded.LoadState(in));
	EXPECT_EQ(in.Remaining(), 0u);
	EXPECT_EQ(loaded.NextHandle(), saved.NextHandle());
}
