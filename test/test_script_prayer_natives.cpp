/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <array>
#include <bit>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <gtest/gtest.h>

#include "CHLApi.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/SpellIconSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;

namespace
{
/// Every object a script names is itself, and the scripts' table records what the natives register
class PrayerScriptObjects final: public ecs::systems::ScriptObjectsSystemInterface
{
public:
	bool Register(entt::entity object, bool made) override
	{
		registered.emplace_back(object, made);
		return true;
	}
	void EnterNative(uint32_t) override {}
	entt::entity Fetch(entt::entity object) override { return object; }
	void AddReference(entt::entity) override {}
	void RemoveReference(entt::entity) override {}
	void ReleaseFromScript(entt::entity) override {}
	void Replace(entt::entity, entt::entity) override {}
	void Reset() override {}
	void ReleaseUnreferenced() override {}
	bool Disband(entt::entity) override { return false; }
	[[nodiscard]] std::vector<ecs::systems::ScriptObjectPlace> Places() const override { return {}; }
	[[nodiscard]] uint32_t TimesFull() const override { return 0; }
	void ClearForNewLand() override {}

	std::vector<std::pair<entt::entity, bool>> registered;
};

/// Icons that know one icon and its site, and record what the natives ask of them
class FakeIcons final: public ecs::systems::SpellIconSystemInterface
{
public:
	bool TownHolds(entt::entity, MagicType) override { return false; }
	bool TownReleases(entt::entity, MagicType) override { return false; }
	[[nodiscard]] std::optional<size_t> CentreMiraclesShown(entt::entity) const override { return std::nullopt; }
	void CentreWorking(entt::entity) override {}
	void TownChangedHands(entt::entity, PlayerNames) override {}
	void TownJoinedSite(entt::entity, entt::entity) override {}
	void TownLeftSite(entt::entity, entt::entity) override {}
	void SiteRemoved(entt::entity) override {}
	[[nodiscard]] bool ValidToTap(entt::entity, PlayerNames) const override { return false; }
	bool Tap(entt::entity, PlayerNames) override { return false; }
	[[nodiscard]] entt::entity SiteOfIcon(entt::entity entity) const override { return entity == icon ? iconSite : entt::null; }
	void EmptyIconStores(entt::entity site) override { emptied.push_back(site); }
	void CancelChargesFor(entt::entity temple, PlayerNames charger) override { cancelled.emplace_back(temple, charger); }
	[[nodiscard]] entt::entity IconInTemple(entt::entity temple, SpellSeedType seed) const override
	{
		asked.emplace_back(temple, seed);
		return seed == iconSeed ? icon : entt::null;
	}
	[[nodiscard]] bool AnyIconStores(entt::entity temple, std::optional<MagicType> type) const override
	{
		storesAsked.emplace_back(temple, type);
		return type != MagicType::Water;
	}
	[[nodiscard]] bool HasIconFor(PlayerNames, SpellSeedType) const override { return false; }
	[[nodiscard]] bool IsIcon(entt::entity entity) const override { return entity == icon || entity == homelessIcon; }
	[[nodiscard]] bool IsHandReady(PlayerNames) const override { return false; }
	[[nodiscard]] std::vector<SpellSeedType> SeedsForSelection(PlayerNames, GestureType) const override { return {}; }
	[[nodiscard]] bool ValidForRequest(PlayerNames, SpellSeedType) const override { return false; }
	bool RequestSeed(PlayerNames, SpellSeedType) override { return false; }
	[[nodiscard]] SpellSeedType LastSeedGained(PlayerNames) const override { return SpellSeedType::None; }
	[[nodiscard]] std::optional<float> ShownCharge(entt::entity) const override { return std::nullopt; }
	[[nodiscard]] std::vector<magic::WorshipIconCharge> ChargesAt(entt::entity) const override { return {}; }
	void ChargedAt(entt::entity, std::span<const magic::WorshipIconCharge>) override {}
	void Update(float) override {}
	void Reset() override {}

	entt::entity icon {entt::null};
	entt::entity iconSite {entt::null};
	/// An icon without a site, as a stone hand of a town that has none
	entt::entity homelessIcon {entt::null};
	SpellSeedType iconSeed {SpellSeedType::Wood};
	std::vector<entt::entity> emptied;
	std::vector<std::pair<entt::entity, PlayerNames>> cancelled;
	mutable std::vector<std::pair<entt::entity, SpellSeedType>> asked;
	mutable std::vector<std::pair<entt::entity, std::optional<MagicType>>> storesAsked;
};

/// Sites that keep a battery each, and record what the natives ask of them
class FakeSites final: public ecs::systems::WorshipSiteSystemInterface
{
public:
	void AddTemple(entt::entity, float, bool) override {}
	void TempleBuilt(entt::entity) override {}
	void PersonJoinedTown(entt::entity) override {}
	void TownChangedHands(entt::entity) override {}
	void LandLaidOut() override {}
	entt::entity MakeBuiltSite(PlayerNames, Tribe) override { return entt::null; }
	void SetCanHaveSites(entt::entity, bool) override {}
	entt::entity DebugAllowSite(entt::entity, bool) override { return entt::null; }
	void RemoveSites(entt::entity) override {}
	void UpdateTurn() override {}
	void ProcessChants() override {}
	void SetDancers(entt::entity, uint32_t) override {}
	[[nodiscard]] uint32_t Dancers(entt::entity) const override { return 0; }
	float UseChants(entt::entity, float) override { return 0.0f; }
	float UseCreateChants(entt::entity, float) override { return 0.0f; }
	float MaintainSpell(entt::entity, float) override { return 0.0f; }
	void ReturnChants(entt::entity, float) override {}
	[[nodiscard]] float StoredChants(entt::entity site) const override { return site == this->site ? battery : 0.0f; }
	void SetStoredChants(entt::entity entity, float chants) override { set.emplace_back(entity, chants); }
	[[nodiscard]] float ChantsAvailable(entt::entity) const override { return 0.0f; }
	[[nodiscard]] float TotalChantsAvailable(entt::entity) const override { return 0.0f; }
	float TakeChantsForVirtualInfluence(PlayerNames, uint32_t) override { return 0.0f; }
	void BuildBy(entt::entity, float) override {}
	void SetBuilt(entt::entity, float) override {}
	[[nodiscard]] bool IsBuilt(entt::entity) const override { return true; }
	[[nodiscard]] entt::entity TempleOf(PlayerNames player) const override
	{
		return player == PlayerNames::PLAYER_TWO ? temple : entt::null;
	}
	[[nodiscard]] std::optional<glm::vec2> PlaceAt(entt::entity, SitePlace) const override { return std::nullopt; }
	[[nodiscard]] std::optional<float> StandingHeight(entt::entity) const override { return std::nullopt; }
	void AddWorshipper(entt::entity, entt::entity) override {}
	void RemoveWorshipper(entt::entity, entt::entity) override {}
	[[nodiscard]] bool IsWorshipper(entt::entity, entt::entity) const override { return false; }
	[[nodiscard]] float WorshipSpeed(entt::entity) const override { return 0.0f; }
	[[nodiscard]] float ChantsPerDancer(entt::entity) const override { return 0.0f; }
	[[nodiscard]] uint32_t FoodInPot(entt::entity) const override { return 0; }
	[[nodiscard]] int32_t FoodNeededFromTown(entt::entity) const override { return 0; }
	[[nodiscard]] float DesireForFood(entt::entity) const override { return 0.0f; }
	void RemoveFoodOnTheWay(entt::entity, int32_t) override {}

	entt::entity site {entt::null};
	entt::entity temple {entt::null};
	float battery {0.0f};
	std::vector<std::pair<entt::entity, float>> set;
};

using Results = std::vector<std::pair<lhvm::VMValue, lhvm::DataType>>;

constexpr uint32_t k_IsSpellCharging = 330;
constexpr uint32_t k_IsThatSpellCharging = 331;
constexpr uint32_t k_GameSetMana = 355;
constexpr uint32_t k_GetManaForSpell = 403;
constexpr uint32_t k_GetMana = 422;
constexpr uint32_t k_ClearPlayerSpellCharging = 423;
constexpr uint32_t k_GetSpellIconInTemple = 453;

class ScriptPrayerNatives: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace();
		_objects = &static_cast<PrayerScriptObjects&>(Locator::scriptObjects::emplace<PrayerScriptObjects>());
		Locator::vm::emplace();
		Locator::vm::value().Initialise(&_api.GetFunctionsTable(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
		auto& registry = Locator::entitiesRegistry::value();
		// A script's object 0 is none, so nothing the scripts are given is the registry's first entity
		[[maybe_unused]] const auto none = registry.Create();
		_icons = &static_cast<FakeIcons&>(Locator::spellIconSystem::emplace<FakeIcons>());
		_sites = &static_cast<FakeSites&>(Locator::worshipSiteSystem::emplace<FakeSites>());
		_site = registry.Create();
		registry.Assign<ecs::components::WorshipSite>(_site);
		_temple = registry.Create();
		registry.Assign<ecs::components::Temple>(_temple, PlayerNames::PLAYER_TWO);
		_icons->icon = registry.Create();
		_icons->iconSite = _site;
		_icons->homelessIcon = registry.Create();
		_sites->site = _site;
		_sites->temple = _temple;
	}

	void TearDown() override
	{
		Locator::infoConstants::reset();
		Locator::worshipSiteSystem::reset();
		Locator::spellIconSystem::reset();
		Locator::vm::reset();
		Locator::scriptObjects::reset();
		Locator::entitiesRegistry::reset();
	}

	static Results Call(uint32_t native, const std::vector<lhvm::NativeArgument>& arguments)
	{
		Results results;
		EXPECT_TRUE(Locator::vm::value().CallNative(native, arguments, results));
		return results;
	}

	static lhvm::NativeArgument Object(entt::entity entity)
	{
		return std::pair(lhvm::VMValue(static_cast<uint32_t>(entity)), lhvm::DataType::Object);
	}

	static lhvm::NativeArgument Float(float value) { return std::pair(lhvm::VMValue(value), lhvm::DataType::Float); }

	static lhvm::NativeArgument Int(int32_t value) { return std::pair(lhvm::VMValue(value), lhvm::DataType::Int); }

	chlapi::CHLApi _api;
	PrayerScriptObjects* _objects {nullptr};
	FakeIcons* _icons {nullptr};
	FakeSites* _sites {nullptr};
	entt::entity _site {entt::null};
	entt::entity _temple {entt::null};
};
} // namespace

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptPrayerNatives, TakeAndGiveWhatTheGameDoes)
{
	struct Expected
	{
		uint32_t native;
		std::string_view name;
		int stackIn;
		uint32_t stackOut;
	};
	constexpr auto k_Natives = std::to_array<Expected>({
	    {k_IsSpellCharging, "IS_SPELL_CHARGING", 1, 1},
	    {k_IsThatSpellCharging, "IS_THAT_SPELL_CHARGING", 2, 1},
	    {k_GameSetMana, "GAME_SET_MANA", 2, 0},
	    {k_GetManaForSpell, "GET_MANA_FOR_SPELL", 1, 1},
	    {k_GetMana, "GET_MANA", 1, 1},
	    {k_ClearPlayerSpellCharging, "CLEAR_PLAYER_SPELL_CHARGING", 1, 0},
	    {k_GetSpellIconInTemple, "GET_SPELL_ICON_IN_TEMPLE", 2, 1},
	});
	const auto& table = _api.GetFunctionsTable();
	for (const auto& expected : k_Natives)
	{
		ASSERT_LT(expected.native, table.size());
		EXPECT_EQ(table[expected.native].name, expected.name);
		EXPECT_EQ(table[expected.native].stackIn, expected.stackIn) << expected.name;
		EXPECT_EQ(table[expected.native].stackOut, expected.stackOut) << expected.name;
	}
}

// A site, or the site of an icon, gets the prayer power, and the icons of the site lose their stores first
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptPrayerNatives, SetManaFillsTheBatteryOfASiteOrOfTheSiteOfAnIcon)
{
	EXPECT_TRUE(Call(k_GameSetMana, {Object(_site), Float(125.5f)}).empty());
	EXPECT_TRUE(Call(k_GameSetMana, {Object(_icons->icon), Float(900.0f)}).empty());
	const std::vector<std::pair<entt::entity, float>> set {{_site, 125.5f}, {_site, 900.0f}};
	EXPECT_EQ(_sites->set, set);
	EXPECT_EQ(_icons->emptied, (std::vector<entt::entity> {_site, _site}));
	// An icon without a site, a temple and nothing change no battery
	EXPECT_TRUE(Call(k_GameSetMana, {Object(_icons->homelessIcon), Float(1.0f)}).empty());
	EXPECT_TRUE(Call(k_GameSetMana, {Object(_temple), Float(1.0f)}).empty());
	EXPECT_TRUE(Call(k_GameSetMana, {Object(entt::null), Float(1.0f)}).empty());
	EXPECT_EQ(_sites->set.size(), 2u);
	EXPECT_EQ(_icons->emptied.size(), 2u);
	EXPECT_EQ(_api.GetStubCalls().Total(), 0u);
}

// Only a site gives its battery; an icon gives none
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptPrayerNatives, GetManaGivesTheBatteryOfASiteOnly)
{
	_sites->battery = 37.25f;
	auto results = Call(k_GetMana, {Object(_site)});
	ASSERT_EQ(results.size(), 1u);
	EXPECT_EQ(results.front().second, lhvm::DataType::Float);
	EXPECT_EQ(results.front().first.floatVal, 37.25f);
	results = Call(k_GetMana, {Object(_icons->icon)});
	ASSERT_EQ(results.size(), 1u);
	EXPECT_EQ(results.front().first.floatVal, 0.0f);
	results = Call(k_GetMana, {Object(entt::null)});
	ASSERT_EQ(results.size(), 1u);
	EXPECT_EQ(results.front().first.floatVal, 0.0f);
	EXPECT_EQ(_api.GetStubCalls().Total(), 0u);
}

// The cost of a miracle is the prayer power that its magic type costs to create, to the bit
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptPrayerNatives, GetManaForSpellGivesTheCostToCreate)
{
	// The tables are large: made on the heap
	auto info = std::make_unique<InfoConstants>();
	const auto cost = std::bit_cast<float>(0x43C80001u);
	info->magicEffect[static_cast<size_t>(MagicType::Food)].costToCreate = cost;
	Locator::infoConstants::emplace(*info);
	auto results = Call(k_GetManaForSpell, {Int(static_cast<int32_t>(MagicType::Food))});
	ASSERT_EQ(results.size(), 1u);
	EXPECT_EQ(results.front().second, lhvm::DataType::Float);
	EXPECT_EQ(std::bit_cast<uint32_t>(results.front().first.floatVal), 0x43C80001u);
	// A number that is not a magic type costs nothing
	results = Call(k_GetManaForSpell, {Int(42)});
	ASSERT_EQ(results.size(), 1u);
	EXPECT_EQ(results.front().first.floatVal, 0.0f);
	EXPECT_EQ(_api.GetStubCalls().Total(), 0u);
}

// The native stops the charges for the hand at this computer, so it needs the players to know that hand
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptPrayerNatives, ClearSpellChargingAsksOnlyAPlayerWithATemple)
{
	EXPECT_TRUE(Call(k_ClearPlayerSpellCharging, {Float(3.0f)}).empty());
	EXPECT_TRUE(_icons->cancelled.empty());
	EXPECT_EQ(_api.GetStubCalls().Total(), 0u);
}

// The icon of the first seed of the magic type comes from the temple, and the scripts' table takes it as found
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptPrayerNatives, GetSpellIconInTempleFindsTheIconOfTheFirstSeedOfTheType)
{
	auto info = std::make_unique<InfoConstants>();
	info->spellSeed[static_cast<size_t>(SpellSeedType::Wood)].magicTypes.fill(MagicType::Wood);
	Locator::infoConstants::emplace(*info);
	auto results = Call(k_GetSpellIconInTemple, {Int(static_cast<int32_t>(MagicType::Wood)), Object(_temple)});
	ASSERT_EQ(results.size(), 1u);
	EXPECT_EQ(results.front().second, lhvm::DataType::Object);
	EXPECT_EQ(results.front().first.uintVal, static_cast<uint32_t>(_icons->icon));
	EXPECT_EQ(_icons->asked, (std::vector<std::pair<entt::entity, SpellSeedType>> {{_temple, SpellSeedType::Wood}}));
	EXPECT_EQ(_objects->registered, (std::vector<std::pair<entt::entity, bool>> {{_icons->icon, false}}));
	// A magic type that no seed has, and a thing that is not a temple, give nothing
	results = Call(k_GetSpellIconInTemple, {Int(static_cast<int32_t>(MagicType::Water)), Object(_temple)});
	ASSERT_EQ(results.size(), 1u);
	EXPECT_EQ(results.front().first.uintVal, 0u);
	results = Call(k_GetSpellIconInTemple, {Int(static_cast<int32_t>(MagicType::Wood)), Object(_site)});
	ASSERT_EQ(results.size(), 1u);
	EXPECT_EQ(results.front().first.uintVal, 0u);
	EXPECT_EQ(_icons->asked.size(), 1u);
	EXPECT_EQ(_api.GetStubCalls().Total(), 0u);
}

// A miracle charges for a script while an icon of the player's temple has prayer power stored; a player without a
// temple has none
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ScriptPrayerNatives, SpellChargingAsksTheIconsOfThePlayersTemple)
{
	auto results = Call(k_IsSpellCharging, {Float(2.0f)});
	ASSERT_EQ(results.size(), 1u);
	EXPECT_EQ(results.front().second, lhvm::DataType::Boolean);
	EXPECT_NE(results.front().first.intVal, 0);
	results = Call(k_IsThatSpellCharging, {Float(2.0f), Int(static_cast<int32_t>(MagicType::Water))});
	ASSERT_EQ(results.size(), 1u);
	EXPECT_EQ(results.front().first.intVal, 0);
	const std::vector<std::pair<entt::entity, std::optional<MagicType>>> asked {{_temple, std::nullopt},
	                                                                            {_temple, MagicType::Water}};
	EXPECT_EQ(_icons->storesAsked, asked);
	results = Call(k_IsSpellCharging, {Float(1.0f)});
	ASSERT_EQ(results.size(), 1u);
	EXPECT_EQ(results.front().first.intVal, 0);
	EXPECT_EQ(_icons->storesAsked.size(), 2u);
	EXPECT_EQ(_api.GetStubCalls().Total(), 0u);
}
