/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/SpellIconSystem.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// Magic that only records the seeds it is asked to delete, and deletes them
class SeedDeletingMagic final: public MagicSystemInterface
{
public:
	entt::entity CastAtPoint(MagicType, PlayerNames, glm::vec3, const magic::SpellCastData&,
	                         const particles::ProcessInfo&) override
	{
		return entt::null;
	}
	entt::entity CastOnObject(MagicType, PlayerNames, entt::entity, const magic::SpellCastData&,
	                          const particles::ProcessInfo&) override
	{
		return entt::null;
	}
	void CloseDown(entt::entity) override {}
	[[nodiscard]] bool CanCastAt(MagicType, PlayerNames, glm::vec3) override { return false; }
	bool SpellEvent(entt::entity, const particles::SpellEventInfo&) override { return false; }
	void PayForSpell(entt::entity, float) override {}
	entt::entity CreateDispenser(glm::vec3, MagicType, float) override { return entt::null; }
	void SetDispenserPeriod(entt::entity, float) override {}
	entt::entity CreateOneOffSeed(glm::vec3, SpellSeedType, int, float) override { return entt::null; }
	entt::entity CreateOneOffSeedFor(glm::vec3, MagicType) override { return entt::null; }
	bool Remove(entt::entity) override { return false; }
	entt::entity GiveSeedToHand(PlayerNames, SpellSeedType, int, float) override { return entt::null; }
	entt::entity SummonSeed(PlayerNames, SpellSeedType, int) override { return entt::null; }
	entt::entity SummonSeedAtSite(entt::entity, SpellSeedType, int) override { return entt::null; }
	void DiscardHeldSeed() override {}
	void DeleteSeed(entt::entity seed) override
	{
		deleted.push_back(seed);
		Locator::entitiesRegistry::value().Destroy(seed);
	}
	void UpdateHand(const HandFrame&, float) override {}
	bool TapAction() override { return false; }
	bool PressOrb(uint32_t, uint32_t) override { return false; }
	void ReleaseOrb(uint32_t, uint32_t) override {}
	[[nodiscard]] std::optional<entt::entity> OrbUnderHand() const override { return std::nullopt; }
	bool PressAction() override { return false; }
	void ReleaseAction() override {}
	[[nodiscard]] bool IsHandBusy() const override { return false; }
	[[nodiscard]] magic::PourPose GetHandPour(float) const override { return {}; }
	[[nodiscard]] HandCastState GetHandCastState() const override { return {}; }
	[[nodiscard]] std::optional<entt::entity> GetHeldSeed() const override { return std::nullopt; }
	[[nodiscard]] HandResult GetLastHandResult() const override { return {}; }
	bool SendSpellEvent(entt::entity, const particles::SpellEventInfo&) override { return false; }
	float ForcePayForSpell(entt::entity, float) override { return 0.0f; }
	[[nodiscard]] float SpellStrength(entt::entity) override { return 0.0f; }
	void ProcessTurn() override {}
	void Update(float) override {}
	void Reset() override {}
	void SetIgnoreInfluence(bool) override {}
	void DriveHand(std::optional<HandFrame>) override {}
	[[nodiscard]] std::optional<HandFrame> GetDrivenHand() const override { return std::nullopt; }
	[[nodiscard]] bool IsIgnoringInfluence() const override { return false; }
	[[nodiscard]] std::vector<SpellInfo> GetSpells() const override { return {}; }
	void RainOnFire(const glm::vec3&) override {}
	[[nodiscard]] std::vector<DispenserInfo> GetDispensers() const override { return {}; }

	std::vector<entt::entity> deleted;
};

class SpellIconSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<Registry>();
		_magic = &static_cast<SeedDeletingMagic&>(Locator::magicSystem::emplace<SeedDeletingMagic>());
	}
	void TearDown() override
	{
		Locator::magicSystem::reset();
		Locator::entitiesRegistry::reset();
	}

	[[nodiscard]] static Registry& Entities() { return Locator::entitiesRegistry::value(); }

	/// An icon of a site, charging its miracle with some prayer power stored, with a seed out from it or none
	entt::entity Icon(entt::entity site, SpellSeedType seed, entt::entity seedOut)
	{
		auto& registry = Entities();
		const auto icon = registry.Create();
		registry.Assign<WorshipIcon>(icon, WorshipIcon {.site = site,
		                                                .seed = seed,
		                                                .charging = true,
		                                                .charger = PlayerNames::PLAYER_ONE,
		                                                .store = 50.0f,
		                                                .seedOut = seedOut});
		registry.Get<SiteIcons>(site).icons.push_back(icon);
		return icon;
	}

	SeedDeletingMagic* _magic {nullptr};
	SpellIconSystem _system;
};

TEST_F(SpellIconSystemTest, AGoneSiteTakesItsIconsAndTheSeedsOutFromThemButNotTheCentresHands)
{
	auto& registry = Entities();
	const auto site = registry.Create();
	registry.Assign<SiteIcons>(site);
	const auto seed = registry.Create();
	registry.Assign<SpellSeed>(seed, SpellSeed {.seedType = SpellSeedType::Heal});
	const auto heal = Icon(site, SpellSeedType::Heal, seed);
	const auto water = Icon(site, SpellSeedType::Water, entt::null);
	// A hand on the village centre showing the same miracle belongs to the town
	const auto hand = registry.Create();
	registry.Assign<TownCentreIcon>(hand, TownCentreIcon {.seed = SpellSeedType::Heal});
	// Another site's icon is not touched
	const auto other = registry.Create();
	registry.Assign<SiteIcons>(other);
	const auto otherIcon = Icon(other, SpellSeedType::Heal, entt::null);

	_system.SiteRemoved(site);

	EXPECT_FALSE(registry.Valid(heal));
	EXPECT_FALSE(registry.Valid(water));
	EXPECT_TRUE(registry.Get<const SiteIcons>(site).icons.empty());
	EXPECT_EQ(_magic->deleted, std::vector<entt::entity> {seed});
	EXPECT_TRUE(registry.Valid(hand));
	EXPECT_TRUE(registry.Valid(otherIcon));
	EXPECT_EQ(registry.Get<const SiteIcons>(other).icons, std::vector<entt::entity> {otherIcon});
}

TEST_F(SpellIconSystemTest, ASeedAlreadyGoneFromAnIconIsNotDeletedAgain)
{
	auto& registry = Entities();
	const auto site = registry.Create();
	registry.Assign<SiteIcons>(site);
	const auto seed = registry.Create();
	registry.Assign<SpellSeed>(seed);
	const auto icon = Icon(site, SpellSeedType::Heal, seed);
	registry.Destroy(seed);

	_system.SiteRemoved(site);

	EXPECT_FALSE(registry.Valid(icon));
	EXPECT_TRUE(_magic->deleted.empty());
}

TEST_F(SpellIconSystemTest, ASiteWithoutIconsLosesNothing)
{
	auto& registry = Entities();
	const auto site = registry.Create();
	_system.SiteRemoved(site);
	_system.SiteRemoved(entt::null);
	EXPECT_TRUE(_magic->deleted.empty());
}
} // namespace
