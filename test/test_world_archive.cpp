/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstring>

#include <algorithm>
#include <array>
#include <bitset>
#include <future>
#include <limits>
#include <memory>
#include <optional>
#include <regex>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/meta/context.hpp>
#include <entt/meta/factory.hpp>
#include <entt/meta/meta.hpp>
#include <entt/meta/resolve.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <gtest/gtest.h>

#include "Common/GameHeading.h"
#include "Creature/CreatureExplorationMap.h"
#include "Creature/CreatureRouteFollow.h"
#include "ECS/Components/AudioEmitter.h"
#include "ECS/Components/CreatureArena.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/PlayerStatistics.h"
#include "ECS/Components/SoundTag.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/TownAggression.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/Reflection/WorldArchive.h"
#include "Enums.h"
#include "Graphics/GraphicsHandle.h"
#include "Graphics/SpriteTextures.h"
#include "Particles/LightSheet.h"
#include "PlayerSlots.h"

using namespace openblack;
using namespace openblack::ecs::reflection;

namespace
{

struct FakeSpot
{
	glm::vec3 position {0.0f};
	float heat {0.0f};
	entt::entity owner {entt::null};

	bool operator==(const FakeSpot&) const = default;
};

struct FakeWalker
{
	entt::entity target {entt::null};
	std::vector<entt::entity> friends;
	std::optional<entt::entity> leader;
	std::vector<FakeSpot> path;
	std::string name;
	std::u16string title;
	std::unordered_map<uint32_t, entt::entity> byNumber;
	std::bitset<10> flags;
	std::array<int16_t, 3> counts {};
	GameHeading heading;
	std::variant<int, FakeSpot> goal;
	std::vector<bool> steps;

	bool operator==(const FakeWalker&) const = default;
};

struct FakeTag
{
};

struct FakeHidden
{
	std::shared_ptr<int> pointer;
	int kept {0};
};

struct FakeUnregistered
{
	int value {0};
};

/// A value with private data, which a codec of its own writes
class FakeCounter
{
public:
	void Add(int amount) { _total += amount; }
	[[nodiscard]] int Total() const { return _total; }

	static void Save(const FakeCounter& counter, BinaryWriter& out) { out.WriteRaw(counter._total); }
	static void Load(FakeCounter& counter, BinaryReader& in) { in.ReadRaw(counter._total); }

private:
	int _total {0};
};

struct FakeCounted
{
	FakeCounter counter;
};

/// The same component in an older and a newer build: the newer build drops one field and adds another
struct FakeOld
{
	int dropped {0};
	int kept {0};
};

struct FakeNew
{
	int kept {0};
	int added {7};
};

void Register(entt::meta_ctx& context)
{
	Reflect<FakeSpot>(context, ValueOnly {})
	    .Field<&FakeSpot::position>("position")
	    .Field<&FakeSpot::heat>("heat")
	    .Field<&FakeSpot::owner>("owner");
	Reflect<FakeWalker>(context)
	    .Field<&FakeWalker::target>("target")
	    .Field<&FakeWalker::friends>("friends")
	    .Field<&FakeWalker::leader>("leader")
	    .Field<&FakeWalker::path>("path")
	    .Field<&FakeWalker::name>("name")
	    .Field<&FakeWalker::title>("title")
	    .Field<&FakeWalker::byNumber>("byNumber")
	    .Field<&FakeWalker::flags>("flags")
	    .Field<&FakeWalker::counts>("counts")
	    .Field<&FakeWalker::heading>("heading")
	    .Field<&FakeWalker::goal>("goal")
	    .Field<&FakeWalker::steps>("steps");
	Reflect<FakeTag> {context};
	Reflect<FakeHidden>(context).Field<&FakeHidden::pointer>("pointer").Field<&FakeHidden::kept>("kept");
	Reflect<FakeCounter>(context, ValueOnly {})
	    .Property<&FakeCounter::Total, nullptr>("total")
	    .Codec<&FakeCounter::Save, &FakeCounter::Load>();
	Reflect<FakeCounted>(context).Field<&FakeCounted::counter>("counter");
}

FakeWalker SampleWalker(entt::entity a, entt::entity b)
{
	FakeWalker walker;
	walker.target = a;
	walker.friends = {a, b, entt::null};
	walker.leader = b;
	walker.path = {{.position = {1.5f, -0.0f, std::numeric_limits<float>::denorm_min()}, .heat = 0.1f, .owner = b}};
	walker.name = "Bob";
	walker.title = u"él";
	walker.byNumber = {{3, a}, {1, b}, {2, entt::null}};
	walker.flags.set(0).set(9);
	walker.counts = {-1, 2, 3};
	walker.heading = GameHeading(1.25f);
	walker.goal = FakeSpot {.position = {2.0f, 3.0f, 4.0f}, .heat = 5.0f, .owner = a};
	walker.steps = {true, false, true};
	return walker;
}

uint32_t Bits(float value)
{
	uint32_t bits = 0;
	std::memcpy(&bits, &value, sizeof(bits));
	return bits;
}

/// A registry with destroyed entities among its live ones, and components that refer to each other
void Populate(entt::registry& registry)
{
	std::vector<entt::entity> made(6);
	registry.create(made.begin(), made.end());
	registry.destroy(made[1]);
	registry.destroy(made[4]);
	const auto again = registry.create();
	registry.emplace<FakeWalker>(made[0], SampleWalker(made[2], again));
	registry.emplace<FakeWalker>(made[3], SampleWalker(made[0], made[5]));
	registry.emplace<FakeTag>(made[2]);
	registry.emplace<FakeHidden>(made[5], FakeHidden {.pointer = std::make_shared<int>(3), .kept = 9});
	FakeCounted counted;
	counted.counter.Add(41);
	registry.emplace<FakeCounted>(again, counted);
	registry.emplace<FakeUnregistered>(made[0]);
}

} // namespace

TEST(WorldArchive, ValuesRoundTripWithNoLoss)
{
	entt::meta_ctx context;
	Register(context);
	const auto walker = SampleWalker(entt::entity {4}, entt::entity {9});
	BinaryWriter out(context);
	Save(out, walker);
	EXPECT_TRUE(out.Unsaved().empty());

	const EntityRemap identity;
	BinaryReader in(context, out.Bytes(), identity);
	FakeWalker read;
	Load(in, read);
	ASSERT_FALSE(in.Failed());
	EXPECT_EQ(in.Remaining(), 0u);
	EXPECT_EQ(read, walker);
	// Floats keep their bits: the sign of a zero and the smallest number
	EXPECT_EQ(Bits(read.path[0].position.y), Bits(-0.0f));
	EXPECT_EQ(Bits(read.path[0].position.z), Bits(std::numeric_limits<float>::denorm_min()));
}

TEST(WorldArchive, MapsGiveTheSameBytesInAnyOrder)
{
	entt::meta_ctx context;
	Register(context);
	std::unordered_map<uint32_t, entt::entity> first;
	std::unordered_map<uint32_t, entt::entity> second;
	for (uint32_t i = 0; i < 50; ++i)
	{
		first.emplace(i, entt::entity {i});
		second.emplace(49 - i, entt::entity {49 - i});
	}
	BinaryWriter a(context);
	BinaryWriter b(context);
	Save(a, first);
	Save(b, second);
	EXPECT_EQ(a.Bytes(), b.Bytes());
}

TEST(WorldArchive, RemapChangesEveryReference)
{
	entt::meta_ctx context;
	Register(context);
	const auto walker = SampleWalker(entt::entity {4}, entt::entity {9});
	BinaryWriter out(context);
	Save(out, walker);

	// Entity 4 becomes 40; entity 9 is not in the table, so it becomes null
	const EntityRemap remap({{entt::entity {4}, entt::entity {40}}});
	BinaryReader in(context, out.Bytes(), remap);
	FakeWalker read;
	Load(in, read);
	ASSERT_FALSE(in.Failed());
	EXPECT_EQ(read.target, entt::entity {40});
	EXPECT_EQ(read.friends, (std::vector<entt::entity> {entt::entity {40}, entt::null, entt::null}));
	EXPECT_EQ(read.leader, entt::entity {entt::null});
	EXPECT_EQ(read.path[0].owner, entt::entity {entt::null});
	EXPECT_EQ(read.byNumber.at(3), entt::entity {40});
	EXPECT_EQ(std::get<FakeSpot>(read.goal).owner, entt::entity {40});
}

TEST(WorldArchive, ShortBytesFailTheReader)
{
	entt::meta_ctx context;
	Register(context);
	BinaryWriter out(context);
	Save(out, SampleWalker(entt::entity {1}, entt::entity {2}));
	auto bytes = out.Bytes();
	bytes.resize(bytes.size() / 2);
	const EntityRemap identity;
	BinaryReader in(context, bytes, identity);
	FakeWalker read;
	Load(in, read);
	EXPECT_TRUE(in.Failed());
}

TEST(WorldArchive, SaveLoadSaveGivesTheSameBytes)
{
	entt::meta_ctx context;
	Register(context);
	entt::registry original;
	Populate(original);
	WorldSaveReport saveReport;
	const auto first = SaveWorld(original, context, &saveReport);
	EXPECT_EQ(saveReport.entities, 5u);
	EXPECT_EQ(saveReport.components, 5u);

	entt::registry loaded;
	const auto result = LoadWorld(loaded, context, first, WorldLoadMode::Exact);
	ASSERT_TRUE(result.has_value());
	EXPECT_TRUE(result->remap.IsIdentity());
	EXPECT_EQ(result->report.entities, 5u);
	EXPECT_EQ(result->report.components, 5u);
	EXPECT_TRUE(result->report.unknownComponents.empty());
	EXPECT_TRUE(result->report.damagedFields.empty());

	const auto second = SaveWorld(loaded, context);
	EXPECT_EQ(first, second);

	// The same entities with their versions, and the next new entity is the same in both
	const auto view = original.view<FakeWalker>();
	for (const auto entity : view)
	{
		ASSERT_TRUE(loaded.valid(entity));
		EXPECT_EQ(loaded.get<FakeWalker>(entity), original.get<FakeWalker>(entity));
	}
	EXPECT_EQ(loaded.create(), original.create());
	EXPECT_EQ(loaded.create(), original.create());
}

TEST(WorldArchive, ComponentOrderStaysTheSame)
{
	entt::meta_ctx context;
	Register(context);
	entt::registry original;
	std::vector<entt::entity> made(5);
	original.create(made.begin(), made.end());
	for (const auto entity : {made[3], made[0], made[4], made[1]})
	{
		original.emplace<FakeTag>(entity);
	}
	entt::registry loaded;
	ASSERT_TRUE(LoadWorld(loaded, context, SaveWorld(original, context), WorldLoadMode::Exact).has_value());
	const auto& before = original.storage<FakeTag>();
	const auto& after = loaded.storage<FakeTag>();
	ASSERT_EQ(before.size(), after.size());
	EXPECT_TRUE(std::equal(before.begin(), before.end(), after.begin()));
}

TEST(WorldArchive, ReportsWhatItCannotKeep)
{
	entt::meta_ctx context;
	Register(context);
	entt::registry original;
	Populate(original);
	WorldSaveReport report;
	const auto bytes = SaveWorld(original, context, &report);
	EXPECT_EQ(report.unregisteredComponents, std::vector<std::string> {"FakeUnregistered"});
	ASSERT_EQ(report.unsavedValues.size(), 1u);
	EXPECT_EQ(report.unsavedValues.begin()->place, "FakeHidden.pointer");

	// The other field of the component and the value with a codec of its own come back
	entt::registry loaded;
	ASSERT_TRUE(LoadWorld(loaded, context, bytes, WorldLoadMode::Exact).has_value());
	const auto hidden = loaded.view<FakeHidden>();
	ASSERT_EQ(hidden.size(), 1u);
	EXPECT_EQ(loaded.get<FakeHidden>(hidden.front()).kept, 9);
	EXPECT_EQ(loaded.get<FakeHidden>(hidden.front()).pointer, nullptr);
	const auto counted = loaded.view<FakeCounted>();
	ASSERT_EQ(counted.size(), 1u);
	EXPECT_EQ(loaded.get<FakeCounted>(counted.front()).counter.Total(), 41);
}

TEST(WorldArchive, MergeMakesNewEntitiesAndRemapsReferences)
{
	entt::meta_ctx context;
	Register(context);
	entt::registry original;
	const auto a = original.create();
	const auto b = original.create();
	original.emplace<FakeWalker>(a, SampleWalker(b, a));
	original.emplace<FakeTag>(b);

	entt::registry target;
	std::vector<entt::entity> present(2);
	target.create(present.begin(), present.end());
	target.emplace<FakeTag>(present[0]);

	const auto result = LoadWorld(target, context, SaveWorld(original, context), WorldLoadMode::Merge);
	ASSERT_TRUE(result.has_value());
	const auto newA = result->remap(a);
	const auto newB = result->remap(b);
	EXPECT_NE(newA, a);
	EXPECT_NE(newB, b);
	ASSERT_TRUE(target.valid(newA));
	const auto& walker = target.get<FakeWalker>(newA);
	EXPECT_EQ(walker.target, newB);
	EXPECT_EQ(walker.leader, newA);
	EXPECT_EQ(walker.byNumber.at(3), newB);
	EXPECT_TRUE(target.all_of<FakeTag>(newB));
	EXPECT_TRUE(target.all_of<FakeTag>(present[0]));
	EXPECT_EQ(target.storage<FakeTag>().size(), 2u);
}

TEST(WorldArchive, ExactLoadNeedsAnEmptyRegistry)
{
	entt::meta_ctx context;
	Register(context);
	entt::registry original;
	Populate(original);
	const auto bytes = SaveWorld(original, context);
	entt::registry busy;
	static_cast<void>(busy.create());
	EXPECT_FALSE(LoadWorld(busy, context, bytes, WorldLoadMode::Exact).has_value());

	// A registry whose entities all went is empty
	entt::registry cleared;
	static_cast<void>(cleared.create());
	cleared.clear();
	EXPECT_TRUE(LoadWorld(cleared, context, bytes, WorldLoadMode::Exact).has_value());
}

TEST(WorldArchive, DamagedArchivesDoNotLoad)
{
	entt::meta_ctx context;
	Register(context);
	entt::registry original;
	Populate(original);
	auto bytes = SaveWorld(original, context);
	bytes[0] = 'X';
	entt::registry first;
	EXPECT_FALSE(LoadWorld(first, context, bytes, WorldLoadMode::Exact).has_value());
	bytes = SaveWorld(original, context);
	bytes.resize(bytes.size() - 3);
	entt::registry second;
	EXPECT_FALSE(LoadWorld(second, context, bytes, WorldLoadMode::Exact).has_value());
}

TEST(WorldArchive, NewerBuildSkipsOldFieldsAndKeepsNewDefaults)
{
	entt::meta_ctx older;
	Reflect<FakeOld>(older).Field<&FakeOld::dropped>("dropped").Field<&FakeOld::kept>("kept");
	entt::registry original;
	const auto entity = original.create();
	original.emplace<FakeOld>(entity, FakeOld {.dropped = 1, .kept = 2});
	auto bytes = SaveWorld(original, older);

	// The newer build names its component as the older one did
	entt::meta_ctx newer;
	Reflect<FakeNew>(newer).Field<&FakeNew::kept>("kept").Field<&FakeNew::added>("added");
	const std::string oldName = "FakeOld";
	const std::string newName = "FakeNew";
	const auto at = std::search(bytes.begin(), bytes.end(), oldName.begin(), oldName.end());
	ASSERT_NE(at, bytes.end());
	std::copy(newName.begin(), newName.end(), at);

	entt::registry loaded;
	const auto result = LoadWorld(loaded, newer, bytes, WorldLoadMode::Exact);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->report.unknownFields, std::vector<std::string> {"FakeNew.dropped"});
	const auto& component = loaded.get<FakeNew>(entity);
	EXPECT_EQ(component.kept, 2);
	EXPECT_EQ(component.added, 7);
}

TEST(WorldArchive, ValuesOutsideTheRegistryRoundTrip)
{
	entt::meta_ctx context;
	Register(context);
	const auto walker = SampleWalker(entt::entity {3}, entt::entity {5});
	const auto bytes = SaveValue(context, walker);
	FakeWalker read;
	ASSERT_TRUE(LoadValue(context, bytes, read, EntityRemap {}));
	EXPECT_EQ(read, walker);
}

TEST(WorldArchive, TheCreaturesMapOfTheLandGoesWithTheWorld)
{
	// A computer that takes the world of another computer gets its map of the land, made from the towns at the start
	entt::meta_ctx context;
	RegisterComponents(context);
	openblack::creature_exploration::RegionMap map;
	map.regions.resize(3);
	map.regions[1] = {
	    .highest = 12.5f, .highestCell = {9, 10}, .firstCoast = glm::ivec2 {8, 11}, .firstSea = std::nullopt, .kinds = 0x13};
	map.highest = 12.5f;
	std::set<UnsavedValue> unsaved;
	const auto bytes = SaveValue(context, map, &unsaved);
	EXPECT_TRUE(unsaved.empty());
	openblack::creature_exploration::RegionMap read;
	ASSERT_TRUE(LoadValue(context, bytes, read, EntityRemap {}));
	ASSERT_EQ(read.regions.size(), 3u);
	EXPECT_EQ(read.regions[1].highest, 12.5f);
	EXPECT_EQ(read.regions[1].highestCell, glm::ivec2(9, 10));
	EXPECT_EQ(read.regions[1].firstCoast, std::optional(glm::ivec2(8, 11)));
	EXPECT_FALSE(read.regions[1].firstSea.has_value());
	EXPECT_EQ(read.regions[1].kinds, 0x13);
	EXPECT_EQ(read.highest, 12.5f);
}

TEST(WorldArchive, EveryComponentOfTheGameCanBeMadeForALoad)
{
	// A load makes each component with no arguments and then reads its fields into it
	entt::meta_ctx context;
	RegisterComponents(context);
	std::vector<std::string> refused;
	for (auto&& [id, type] : entt::resolve(context))
	{
		const ComponentInfo* info = type.custom();
		if (info != nullptr && info->storage != nullptr && !type.construct())
		{
			refused.push_back(ShortTypeName(type.info()));
		}
	}
	EXPECT_TRUE(refused.empty()) << refused.front();
}

TEST(WorldArchive, ACodecKeepsTheFieldsOfItsType)
{
	entt::meta_ctx context;
	Register(context);
	const auto type = entt::resolve<FakeCounter>(context);
	ASSERT_TRUE(type);
	const ValueInfo* value = type.custom();
	ASSERT_NE(value, nullptr);
	EXPECT_NE(value->codec.save, nullptr);
	bool found = false;
	for (const auto& [id, data] : type.data())
	{
		const FieldInfo* info = data.custom();
		found = found || (info != nullptr && info->name == "total");
	}
	EXPECT_TRUE(found);
}

TEST(WorldArchive, TheGamesTemplatesEnumsAndPrivateValuesAreKept)
{
	entt::meta_ctx context;
	RegisterComponents(context);
	entt::registry original;
	const auto town = original.create();
	original.emplace<Tribe>(town, Tribe::NORSE);
	original.emplace<ecs::components::MoveStateOrbitTag>(
	    town, ecs::components::MoveStateOrbitTag {.clockwise = ecs::components::MoveStateClockwise::Clockwise,
	                                              .stepGoal = {3.0f, 4.0f}});
	auto& statistics = original.emplace<ecs::components::PlayerStatistics>(town);
	statistics.belief.Check(5.0f);
	statistics.population.Check(12u);
	auto& reaction = original.emplace<ecs::components::LivingReaction>(town);
	reaction.creatureMemory.Record(7, 120);

	WorldSaveReport report;
	const auto bytes = SaveWorld(original, context, &report);
	EXPECT_TRUE(report.unregisteredComponents.empty());
	for (const auto& value : report.unsavedValues)
	{
		ADD_FAILURE() << value.place << " (" << value.type << ")";
	}
	entt::registry loaded;
	ASSERT_TRUE(LoadWorld(loaded, context, bytes, WorldLoadMode::Exact).has_value());
	EXPECT_EQ(loaded.get<Tribe>(town), Tribe::NORSE);
	EXPECT_EQ(loaded.get<ecs::components::MoveStateOrbitTag>(town).stepGoal, glm::vec2(3.0f, 4.0f));
	EXPECT_EQ(loaded.get<ecs::components::PlayerStatistics>(town).belief.highest, 5.0f);
	EXPECT_EQ(loaded.get<ecs::components::PlayerStatistics>(town).population.highest, 12u);
	EXPECT_EQ(loaded.get<ecs::components::LivingReaction>(town).creatureMemory.LastReacted(7), 120u);
}

TEST(WorldArchive, SharedValuesOfRegisteredTypesComeBackAsValues)
{
	struct Holder
	{
		std::shared_ptr<const FakeSpot> spot;
		std::shared_ptr<const FakeSpot> none;
	};
	entt::meta_ctx context;
	Register(context);
	Reflect<Holder>(context, ValueOnly {}).Field<&Holder::spot>("spot").Field<&Holder::none>("none");
	Holder holder {.spot = std::make_shared<const FakeSpot>(FakeSpot {.position = {1.0f, 2.0f, 3.0f}, .heat = 4.0f}),
	               .none = nullptr};
	BinaryWriter out(context);
	Save(out, holder);
	EXPECT_TRUE(out.Unsaved().empty());
	const EntityRemap identity;
	BinaryReader in(context, out.Bytes(), identity);
	Holder read {.spot = nullptr, .none = std::make_shared<const FakeSpot>()};
	Load(in, read);
	ASSERT_FALSE(in.Failed());
	ASSERT_NE(read.spot, nullptr);
	EXPECT_EQ(*read.spot, *holder.spot);
	EXPECT_NE(read.spot, holder.spot);
	EXPECT_EQ(read.none, nullptr);
}

namespace
{

/// A walk part of the way along its route, with a new destination that it searches for as it walks on
creature_route::Follower WalkSearchingAgain()
{
	using creature_route::Follower;
	Follower follower;
	follower.Reset({1000.0f, 1000.0f}, 2.0f);
	follower.GetObstacles().Add(1u, {1050.0f, 1000.0f}, 10.0f);
	follower.SetDestination({.point = {1100.0f, 1000.0f}, .arriveWithin = 1.0f, .mostCost = 5200.0f});
	while (follower.Update(16, false, 0.0f) == Follower::Outcome::None)
	{
	}
	follower.Advance(5.0f);
	follower.SetDestination({.point = {1100.0f, 1100.0f}, .arriveWithin = 1.0f, .mostCost = 5200.0f});
	return follower;
}

std::vector<uint8_t> FollowerBytes(const entt::meta_ctx& context, const creature_route::Follower& follower)
{
	BinaryWriter out(context);
	Save(out, follower);
	return std::move(out.Bytes());
}

} // namespace

TEST(WorldArchive, AWalkCarriesOnFromTheSavedSearch)
{
	using creature_route::Follower;
	entt::meta_ctx context;
	RegisterComponents(context);
	auto walking = WalkSearchingAgain();
	ASSERT_EQ(walking.GetState(), Follower::State::Replanning);
	ASSERT_GE(walking.GetSearchCount(), 2u);
	const auto bytes = FollowerBytes(context, walking);

	Follower loaded;
	const EntityRemap identity;
	BinaryReader in(context, bytes, identity);
	Load(in, loaded);
	ASSERT_FALSE(in.Failed());
	EXPECT_EQ(in.Remaining(), 0u);
	EXPECT_EQ(FollowerBytes(context, loaded), bytes);
	// The heading along the route comes back too: the next leg of the walk starts from it
	ASSERT_NE(walking.GetHeading(), GameHeading {});
	EXPECT_EQ(loaded.GetHeading(), walking.GetHeading());
	EXPECT_EQ(loaded.GetPosition(), walking.GetPosition());

	// The loaded walk and the walk that was not saved take the same steps to the same place
	auto first = Follower::Outcome::None;
	auto second = Follower::Outcome::None;
	for (int turn = 0; turn < 100 && first == Follower::Outcome::None; ++turn)
	{
		first = walking.Update(16, false, 0.0f);
		second = loaded.Update(16, false, 0.0f);
		ASSERT_EQ(first, second);
	}
	ASSERT_EQ(first, Follower::Outcome::Found);
	EXPECT_EQ(FollowerBytes(context, loaded), FollowerBytes(context, walking));
	for (int step = 0; step < 50; ++step)
	{
		const auto a = walking.Advance(3.0f);
		const auto b = loaded.Advance(3.0f);
		ASSERT_EQ(a.position, b.position);
		ASSERT_EQ(a.heading, b.heading);
	}
}

// The heading along the route and the circles that the obstacles leave out come back, so that a loaded walk starts
// its next leg as the walk that was not saved does
TEST(WorldArchive, AWalkKeepsItsHeadingAndTheCirclesItLeavesOut)
{
	using creature_route::Follower;
	entt::meta_ctx context;
	RegisterComponents(context);
	auto walking = WalkSearchingAgain();
	ASSERT_NE(walking.GetHeading(), GameHeading {});
	walking.GetObstacles().LeaveOutCirclesHolding({1200.0f, 1200.0f}, {1300.0f, 1300.0f});
	const auto bytes = FollowerBytes(context, walking);

	Follower loaded;
	const EntityRemap identity;
	BinaryReader in(context, bytes, identity);
	Load(in, loaded);
	ASSERT_FALSE(in.Failed());
	EXPECT_EQ(in.Remaining(), 0u);
	EXPECT_EQ(loaded.GetHeading(), walking.GetHeading());
	// A circle round a point that the walk leaves out does not go in, and other circles do
	EXPECT_FALSE(loaded.GetObstacles().Add(7u, {1200.0f, 1205.0f}, 10.0f));
	EXPECT_TRUE(loaded.GetObstacles().Add(8u, {1250.0f, 1205.0f}, 10.0f));
}

TEST(WorldArchive, ACreaturesWalkIsKeptWithItsComponent)
{
	entt::meta_ctx context;
	RegisterComponents(context);
	entt::registry original;
	const auto creature = original.create();
	auto& locomotion = original.emplace<ecs::components::CreatureLocomotion>(creature);
	locomotion.follower = WalkSearchingAgain();
	locomotion.routeWalker = creature_obstacles::Walker {.position = {1000.0f, 1000.0f},
	                                                     .height = 30.0f,
	                                                     .radius = 2.0f,
	                                                     .scriptControlled = false,
	                                                     .owner = PlayerNames::PLAYER_ONE};
	WorldSaveReport report;
	const auto bytes = SaveWorld(original, context, &report);
	for (const auto& value : report.unsavedValues)
	{
		EXPECT_EQ(value.place.find("CreatureLocomotion"), std::string::npos) << value.place << " (" << value.type << ")";
	}
	entt::registry loaded;
	ASSERT_TRUE(LoadWorld(loaded, context, bytes, WorldLoadMode::Exact).has_value());
	const auto& read = loaded.get<ecs::components::CreatureLocomotion>(creature);
	EXPECT_EQ(FollowerBytes(context, read.follower), FollowerBytes(context, locomotion.follower));
	ASSERT_TRUE(read.routeWalker.has_value());
	EXPECT_EQ(read.routeWalker->height, 30.0f);
	EXPECT_EQ(SaveWorld(loaded, context), bytes);
}

TEST(WorldArchive, ATownKeepsTheHarmOfTheLastAttackOfEachPlayer)
{
	entt::meta_ctx context;
	RegisterComponents(context);
	entt::registry original;
	const auto town = original.create();
	auto& record = original.emplace<ecs::components::TownAggression>(town).record;
	record.aggression.at(1) = 4.0f;
	record.lastTurns.at(1) = 77u;
	record.lastAggressor = PlayerNames::PLAYER_TWO;
	record.lastTurn = 77u;
	record.mercy = 2.5f;
	record.lastHarms.at(1) = {.burn = 3.0f, .crush = 1.0f, .hit = 0.5f};
	record.lastHarms.at(2) = {.burn = 0.0f, .crush = 6.0f, .hit = 0.0f};
	const auto bytes = SaveWorld(original, context);
	entt::registry loaded;
	ASSERT_TRUE(LoadWorld(loaded, context, bytes, WorldLoadMode::Exact).has_value());
	const auto& read = loaded.get<ecs::components::TownAggression>(town).record;
	EXPECT_EQ(read.lastHarms.at(1).burn, 3.0f);
	EXPECT_EQ(read.lastHarms.at(1).crush, 1.0f);
	EXPECT_EQ(read.lastHarms.at(1).hit, 0.5f);
	EXPECT_EQ(read.lastHarms.at(2).crush, 6.0f);
	EXPECT_EQ(read.lastHarms.at(3).burn, 0.0f);
	EXPECT_EQ(read.lastAggressor, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(read.mercy, 2.5f);
	EXPECT_EQ(SaveWorld(loaded, context), bytes);
}

TEST(WorldArchive, ARecordOfAnOlderBuildWithNoHarmsStillReadsItsOtherFields)
{
	entt::meta_ctx context;
	RegisterComponents(context);
	ecs::town_aggression::Record record;
	record.aggression.at(3) = 9.0f;
	record.lastTurn = 12u;
	record.protectionMultiplier = 0.5f;
	record.mercy = 1.5f;
	record.lastHarms.at(0) = {.burn = 1.0f, .crush = 2.0f, .hit = 3.0f};
	BinaryWriter whole(context);
	Save(whole, record);
	BinaryWriter harms(context);
	Save(harms, record.lastHarms);
	ASSERT_GT(whole.Bytes().size(), harms.Bytes().size());
	// The harms are the last bytes of the record, so an older record is the same bytes without them
	const auto older = std::span(whole.Bytes()).first(whole.Bytes().size() - harms.Bytes().size());
	ecs::town_aggression::Record loaded;
	const EntityRemap identity;
	BinaryReader in(context, older, identity);
	Load(in, loaded);
	EXPECT_EQ(loaded.aggression.at(3), 9.0f);
	EXPECT_EQ(loaded.lastTurn, 12u);
	EXPECT_EQ(loaded.protectionMultiplier, 0.5f);
	EXPECT_EQ(loaded.mercy, 1.5f);
	EXPECT_EQ(loaded.lastHarms.at(0).burn, 0.0f);
}

TEST(WorldArchive, SectionsKeepTheirNamesAndBytes)
{
	const std::vector<NamedSection> sections {
	    {.name = "fire", .bytes = {1, 2, 3}}, {.name = "empty", .bytes = {}}, {.name = "weather", .bytes = {9}}};
	const auto bytes = SaveSections(sections);
	const auto read = LoadSections(bytes);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(*read, sections);
	EXPECT_EQ(SaveSections(*read), bytes);
	// Bytes that stop short, or that have more after the sections, are damaged
	EXPECT_FALSE(LoadSections(std::span(bytes).first(bytes.size() - 1)).has_value());
	auto longer = bytes;
	longer.push_back(0);
	EXPECT_FALSE(LoadSections(longer).has_value());
}

TEST(WorldArchive, ASnapshotGivesTheSameBytesAsTheRegistryWhateverChangesAfterIt)
{
	entt::meta_ctx context;
	Register(context);
	entt::registry original;
	Populate(original);
	WorldSaveReport direct;
	const auto bytes = SaveWorld(original, context, &direct);
	const auto snapshot = SnapshotWorld(original, context);

	// A component that shares a value the game changes is encoded at once; the others are copies
	const auto storage = [&snapshot](std::string_view name) {
		return std::ranges::find(snapshot->storages, name, &WorldSnapshot::Storage::name);
	};
	ASSERT_NE(storage("FakeHidden"), snapshot->storages.end());
	EXPECT_FALSE(storage("FakeHidden")->copy.has_value());
	ASSERT_NE(storage("FakeWalker"), snapshot->storages.end());
	EXPECT_TRUE(storage("FakeWalker")->copy.has_value());

	// The writer encodes the snapshot on its own thread while the game changes and destroys what it copied
	auto encoded = std::async(std::launch::async, [&snapshot, &context]() {
		WorldSaveReport report;
		auto made = SaveWorld(*snapshot, context, &report);
		return std::make_pair(std::move(made), report);
	});
	for (auto&& [entity, walker] : original.view<FakeWalker>().each())
	{
		walker = {};
	}
	original.clear();
	const auto [fromSnapshot, report] = encoded.get();
	EXPECT_EQ(fromSnapshot, bytes);
	EXPECT_EQ(report.entities, direct.entities);
	EXPECT_EQ(report.components, direct.components);
	EXPECT_EQ(report.unregisteredComponents, direct.unregisteredComponents);
	EXPECT_EQ(report.unsavedValues, direct.unsavedValues);
}

// A save from the save room has the rooms of the temple as well as the land. It keeps every entity and component that
// a quick save of the same land keeps.
TEST(WorldArchive, ASaveWithTheTempleOpenKeepsEveryEntityOfAQuickSave)
{
	entt::meta_ctx context;
	Register(context);
	entt::registry land;
	Populate(land);
	for (int i = 0; i < 58; ++i)
	{
		land.emplace<FakeTag>(land.create());
	}
	const auto quick = SnapshotWorld(land, context);
	entt::registry fromQuick;
	ASSERT_TRUE(LoadWorld(fromQuick, context, SaveWorld(*quick, context), WorldLoadMode::Exact).has_value());

	// The player goes into the temple: the rooms are entities of the same registry
	for (int i = 0; i < 810; ++i)
	{
		land.emplace<FakeSpot>(land.create());
	}
	const auto inTemple = SnapshotWorld(land, context);
	entt::registry fromTemple;
	ASSERT_TRUE(LoadWorld(fromTemple, context, SaveWorld(*inTemple, context), WorldLoadMode::Exact).has_value());

	EXPECT_EQ(fromTemple.view<FakeTag>().size(), fromQuick.view<FakeTag>().size());
	EXPECT_EQ(fromTemple.view<FakeTag>().size(), land.view<FakeTag>().size());
	EXPECT_EQ(fromTemple.view<FakeWalker>().size(), fromQuick.view<FakeWalker>().size());
	EXPECT_EQ(fromTemple.view<FakeCounted>().size(), fromQuick.view<FakeCounted>().size());
	for (const auto entity : fromQuick.view<FakeTag>())
	{
		EXPECT_TRUE(fromTemple.valid(entity));
		EXPECT_TRUE(fromTemple.all_of<FakeTag>(entity));
	}
}

// A saved world keeps the ids of a sprite's textures, which name the same textures in the next run. The handles of the
// graphics card change from run to run, and a handle from an old run can name no texture.
TEST(WorldArchive, ASpriteKeepsTheIdsOfItsTextures)
{
	entt::meta_ctx context;
	RegisterComponents(context);
	entt::registry original;
	const auto glow = original.create();
	constexpr auto k_Sheet = entt::hashed_string("raw/ATMOS");
	constexpr auto k_SheetAlpha = entt::hashed_string("raw/ATMOSA");
	original.emplace<ecs::components::Sprite>(glow, ecs::components::Sprite {.texture = k_Sheet.value(),
	                                                                         .uvMin = glm::vec2(0.75f, 0.25f),
	                                                                         .uvExtent = glm::vec2(0.125f),
	                                                                         .tint = glm::vec4(1.0f),
	                                                                         .additive = true,
	                                                                         .facesCamera = true,
	                                                                         .alpha = k_SheetAlpha.value()});
	const auto lone = original.create();
	original.emplace<ecs::components::Sprite>(
	    lone, ecs::components::Sprite {
	              .texture = k_Sheet.value(), .uvMin = glm::vec2(0.0f), .uvExtent = glm::vec2(1.0f), .tint = glm::vec4(1.0f)});

	WorldSaveReport report;
	const auto bytes = SaveWorld(original, context, &report);
	EXPECT_TRUE(report.unsavedValues.empty());
	entt::registry loaded;
	ASSERT_TRUE(LoadWorld(loaded, context, bytes, WorldLoadMode::Exact).has_value());
	const auto& sprite = loaded.get<ecs::components::Sprite>(glow);
	EXPECT_EQ(sprite.texture, k_Sheet.value());
	ASSERT_TRUE(sprite.alpha.has_value());
	EXPECT_EQ(*sprite.alpha, k_SheetAlpha.value());
	EXPECT_EQ(graphics::sprite_textures::AlphaId(sprite), k_SheetAlpha.value());
	// A sprite with no alpha texture takes its alpha from its own texture
	const auto& alone = loaded.get<ecs::components::Sprite>(lone);
	EXPECT_FALSE(alone.alpha.has_value());
	EXPECT_EQ(graphics::sprite_textures::AlphaId(alone), k_Sheet.value());
}

namespace
{

/// A texture of a fake cache: the handle number that one run gave it
struct FakeRunTexture
{
	uint32_t handle {0};
};

/// The texture cache of one run, by the stable ids of the textures
using FakeTextureCache = std::unordered_map<entt::id_type, FakeRunTexture>;

const FakeRunTexture* FindIn(const FakeTextureCache& cache, entt::id_type id)
{
	const auto found = cache.find(id);
	return found != cache.end() ? &found->second : nullptr;
}

/// Whether a type is a pointer or a graphics handle, or holds one in a list, an option, a pair or a map
bool HoldsRunHandle(const entt::meta_type& type)
{
	if (!type)
	{
		return false;
	}
	if (type.is_pointer())
	{
		return true;
	}
	// The full name of the type also names the types in its lists, options, pairs and maps
	const std::regex runHandle(R"(\*|\b(bgfx|openblack::graphics)::\w*Handle\b)");
	const std::string name(type.info().name());
	return std::regex_search(name, runHandle);
}

/// The fields that the binary archive keeps and that hold a pointer or a graphics handle, as "Type.field"
std::vector<std::string> SavedRunHandles(const entt::meta_ctx& context)
{
	std::vector<std::string> found;
	for (auto&& [typeId, type] : entt::resolve(context))
	{
		for (auto&& [fieldId, data] : type.data())
		{
			const FieldInfo* info = data.custom();
			const bool kept = info != nullptr && (info->save != nullptr || info->saveFrom != nullptr);
			if (kept && HoldsRunHandle(data.type()))
			{
				found.push_back(ShortTypeName(type.info()) + "." + info->name);
			}
		}
	}
	return found;
}

/// A value that keeps graphics handles, registered past the checks of Reflect, as a careless registration could
struct FakeHandleHolder
{
	graphics::TextureHandle texture;
	std::optional<std::vector<graphics::ProgramHandle>> programs;
	int kept {0};
};

void SaveNothing(const entt::meta_any& /*value*/, BinaryWriter& /*out*/) {}

} // namespace

TEST(WorldArchive, ARunHandleIsAPointerOrAGraphicsHandle)
{
	static_assert(detail::k_IsRunHandle<int*>);
	static_assert(detail::k_IsRunHandle<const int*>);
	static_assert(detail::k_IsRunHandle<int FakeSpot::*>);
	static_assert(detail::k_IsRunHandle<graphics::TextureHandle>);
	static_assert(detail::k_IsRunHandle<const graphics::ProgramHandle>);
	static_assert(detail::k_IsRunHandle<bgfx::TextureHandle>);
	static_assert(detail::k_IsRunHandle<bgfx::FrameBufferHandle>);
	static_assert(!detail::k_IsRunHandle<entt::id_type>);
	static_assert(!detail::k_IsRunHandle<entt::entity>);
	static_assert(!detail::k_IsRunHandle<std::shared_ptr<int>>);
	static_assert(!detail::k_IsRunHandle<std::optional<entt::id_type>>);
	SUCCEED();
}

TEST(WorldArchive, NoSavedFieldOfTheGameHoldsAPointerOrAGraphicsHandle)
{
	// The next run gives its resources other numbers, so a saved handle names nothing or the wrong thing
	entt::meta_ctx context;
	RegisterComponents(context);
	const auto found = SavedRunHandles(context);
	EXPECT_TRUE(found.empty()) << found.front();
}

TEST(WorldArchive, TheCheckOfSavedFieldsFindsGraphicsHandles)
{
	entt::meta_ctx context;
	entt::meta_factory<FakeHandleHolder> factory {context};
	factory.data<&FakeHandleHolder::texture, entt::as_ref_t>(entt::hashed_string("texture"));
	factory.custom<FieldInfo>(FieldInfo {.name = "texture", .save = &SaveNothing});
	factory.data<&FakeHandleHolder::programs, entt::as_ref_t>(entt::hashed_string("programs"));
	factory.custom<FieldInfo>(FieldInfo {.name = "programs", .save = &SaveNothing});
	factory.data<&FakeHandleHolder::kept, entt::as_ref_t>(entt::hashed_string("kept"));
	factory.custom<FieldInfo>(FieldInfo {.name = "kept", .save = &SaveNothing});
	auto found = SavedRunHandles(context);
	std::ranges::sort(found);
	EXPECT_EQ(found, (std::vector<std::string> {"FakeHandleHolder.programs", "FakeHandleHolder.texture"}));
}

TEST(WorldArchive, ASpriteFindsItsTexturesInTheCacheOfTheNextRun)
{
	entt::meta_ctx context;
	RegisterComponents(context);
	constexpr auto k_Sheet = entt::hashed_string("raw/ATMOS");
	constexpr auto k_SheetAlpha = entt::hashed_string("raw/ATMOSA");
	constexpr auto k_Gone = entt::hashed_string("raw/GONE");
	// The run that saves gives the textures one set of handle numbers
	const FakeTextureCache saver {
	    {k_Sheet.value(), {.handle = 1146}}, {k_SheetAlpha.value(), {.handle = 1147}}, {k_Gone.value(), {.handle = 1148}}};
	entt::registry original;
	const auto glow = original.create();
	original.emplace<ecs::components::Sprite>(glow, ecs::components::Sprite {.texture = k_Sheet.value(),
	                                                                         .uvMin = glm::vec2(0.0f),
	                                                                         .uvExtent = glm::vec2(1.0f),
	                                                                         .tint = glm::vec4(1.0f),
	                                                                         .alpha = k_SheetAlpha.value()});
	const auto lost = original.create();
	original.emplace<ecs::components::Sprite>(
	    lost, ecs::components::Sprite {
	              .texture = k_Gone.value(), .uvMin = glm::vec2(0.0f), .uvExtent = glm::vec2(1.0f), .tint = glm::vec4(1.0f)});
	const auto before = graphics::sprite_textures::FindTextures(original.get<ecs::components::Sprite>(glow),
	                                                            [&saver](entt::id_type id) { return FindIn(saver, id); });
	ASSERT_TRUE(before.has_value());
	EXPECT_EQ(before->texture->handle, 1146u);
	const auto bytes = SaveWorld(original, context);

	// The run that loads gives the same textures other numbers, and does not have one of them
	const FakeTextureCache loader {{k_SheetAlpha.value(), {.handle = 3}}, {k_Sheet.value(), {.handle = 7}}};
	entt::registry loaded;
	ASSERT_TRUE(LoadWorld(loaded, context, bytes, WorldLoadMode::Exact).has_value());
	const auto find = [&loader](entt::id_type id) { return FindIn(loader, id); };
	const auto after = graphics::sprite_textures::FindTextures(loaded.get<ecs::components::Sprite>(glow), find);
	ASSERT_TRUE(after.has_value());
	EXPECT_EQ(after->texture->handle, 7u);
	EXPECT_EQ(after->alpha->handle, 3u);
	// A sprite whose texture the run does not have draws nothing
	EXPECT_FALSE(graphics::sprite_textures::FindTextures(loaded.get<ecs::components::Sprite>(lost), find).has_value());
}

TEST(WorldArchive, AnAudioEmitterKeepsNoSourceOfTheAudioDevice)
{
	entt::meta_ctx context;
	RegisterComponents(context);
	constexpr auto k_Sound = entt::hashed_string("G_Rustle");
	entt::registry original;
	const auto emitter = original.create();
	original.emplace<ecs::components::AudioEmitter>(emitter, ecs::components::AudioEmitter {
	                                                             .sourceId = 4242,
	                                                             .soundId = k_Sound.value(),
	                                                             .priority = 3,
	                                                         });
	WorldSaveReport report;
	const auto bytes = SaveWorld(original, context, &report);
	EXPECT_TRUE(report.unsavedValues.empty());
	entt::registry loaded;
	const auto result = LoadWorld(loaded, context, bytes, WorldLoadMode::Exact);
	ASSERT_TRUE(result.has_value());
	EXPECT_TRUE(result->report.unknownFields.empty());
	const auto& sound = loaded.get<ecs::components::AudioEmitter>(emitter);
	// The source number of the saved run does not come back
	EXPECT_EQ(sound.sourceId, 0u);
	EXPECT_EQ(sound.soundId, k_Sound.value());
	EXPECT_EQ(sound.priority, 3);
}

TEST(WorldArchive, AnOlderSaveOfAnAudioEmitterLoadsWithoutItsSource)
{
	// A build before the fix kept the source number of the audio device
	entt::meta_ctx older;
	Reflect<ecs::components::AudioEmitter>(older)
	    .Field<&ecs::components::AudioEmitter::sourceId>("sourceId")
	    .Field<&ecs::components::AudioEmitter::soundId>("soundId");
	constexpr auto k_Sound = entt::hashed_string("G_Rustle");
	entt::registry original;
	const auto emitter = original.create();
	original.emplace<ecs::components::AudioEmitter>(
	    emitter, ecs::components::AudioEmitter {.sourceId = 4242, .soundId = k_Sound.value()});
	const auto bytes = SaveWorld(original, older);

	entt::meta_ctx context;
	RegisterComponents(context);
	entt::registry loaded;
	const auto result = LoadWorld(loaded, context, bytes, WorldLoadMode::Exact);
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->report.unknownFields, std::vector<std::string> {"AudioEmitter.sourceId"});
	const auto& sound = loaded.get<ecs::components::AudioEmitter>(emitter);
	EXPECT_EQ(sound.sourceId, 0u);
	EXPECT_EQ(sound.soundId, k_Sound.value());
}

TEST(WorldArchive, AShorterSavedArrayFillsTheStartOfALongerArray)
{
	entt::meta_ctx context;
	Register(context);
	// A table for each of the 8 players of an old save loads into a table for more players
	const std::array<float, player_slots::k_LandSlots> plain {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
	const std::array<std::optional<uint32_t>, 2> listed {std::optional<uint32_t> {5}, std::nullopt};
	BinaryWriter out(context);
	Save(out, plain);
	Save(out, listed);

	const EntityRemap identity;
	BinaryReader in(context, out.Bytes(), identity);
	std::array<float, player_slots::k_MaxPlayers> morePlain {};
	std::array<std::optional<uint32_t>, 4> moreListed {};
	Load(in, morePlain);
	Load(in, moreListed);
	ASSERT_FALSE(in.Failed());
	EXPECT_EQ(in.Remaining(), 0u);
	for (size_t i = 0; i < morePlain.size(); ++i)
	{
		EXPECT_EQ(morePlain.at(i), i < plain.size() ? plain.at(i) : 0.0f) << "player " << i;
	}
	EXPECT_EQ(moreListed.at(0), std::optional<uint32_t> {5});
	EXPECT_FALSE(moreListed.at(1).has_value());
	EXPECT_FALSE(moreListed.at(2).has_value());
	EXPECT_FALSE(moreListed.at(3).has_value());
}

TEST(WorldArchive, ALongerSavedArrayFailsTheReader)
{
	entt::meta_ctx context;
	Register(context);
	const std::array<float, 8> plain {};
	BinaryWriter out(context);
	Save(out, plain);
	const EntityRemap identity;
	BinaryReader in(context, out.Bytes(), identity);
	std::array<float, 4> fewer {};
	Load(in, fewer);
	EXPECT_TRUE(in.Failed());
}

// A mark with no values has no order that the game reads: the hash of the world does not change with the order of the
// marked entities
TEST(WorldArchive, TheHashOfAMarkDoesNotFollowTheOrderOfItsEntities)
{
	entt::meta_ctx context;
	Register(context);
	const auto hashOf = [&context](bool reversed) {
		entt::registry registry;
		std::vector<entt::entity> made(3);
		registry.create(made.begin(), made.end());
		if (reversed)
		{
			std::ranges::reverse(made);
		}
		for (const auto entity : made)
		{
			registry.emplace<FakeTag>(entity);
		}
		const auto hashes = HashWorld(registry, context);
		const auto tag =
		    std::ranges::find(hashes.components, std::string("FakeTag"), [](const auto& part) { return part.first; });
		return tag != hashes.components.end() ? std::optional(tag->second) : std::nullopt;
	};
	ASSERT_TRUE(hashOf(false).has_value());
	EXPECT_EQ(hashOf(false), hashOf(true));
}

// With the fields asked for, each field of a component has its own hash, so a change of one field shows which it is
TEST(WorldArchive, TheHashOfEachFieldShowsWhichFieldChanged)
{
	entt::meta_ctx context;
	Register(context);
	entt::registry registry;
	const auto entity = registry.create();
	registry.emplace<FakeWalker>(entity, SampleWalker(entity, entity));
	const auto before = HashWorld(registry, context, {}, true);
	registry.get<FakeWalker>(entity).name = "Tom";
	const auto after = HashWorld(registry, context, {}, true);
	const auto fieldOf = [](const WorldHashes& hashes, std::string_view name) {
		const auto found = std::ranges::find(hashes.fields, std::string(name), [](const auto& part) { return part.first; });
		return found != hashes.fields.end() ? std::optional(found->second) : std::nullopt;
	};
	ASSERT_TRUE(fieldOf(before, "FakeWalker.name").has_value());
	EXPECT_NE(fieldOf(before, "FakeWalker.name"), fieldOf(after, "FakeWalker.name"));
	EXPECT_EQ(fieldOf(before, "FakeWalker.counts"), fieldOf(after, "FakeWalker.counts"));
	// Without the fields asked for, the hashes keep none
	EXPECT_TRUE(HashWorld(registry, context).fields.empty());
}

// A footpath that the game still looks for keeps its search in a world state. A loaded search goes on to the same route
// as the search that was saved.
TEST(WorldArchive, AFootpathSearchContinuesAfterALoad)
{
	namespace route = openblack::creature_route;
	entt::meta_ctx context;
	RegisterComponents(context);
	// One round rock stands in the way, in the cell of the middle of the line between the two ends
	const route::Obstacles::Gatherer gatherer = [](glm::ivec2 cell, route::Obstacles& into) {
		if (cell == glm::ivec2(30, 30))
		{
			into.Add(7u, {305.0f, 305.0f}, 6.0f);
		}
	};
	const glm::vec2 start {280.0f, 300.0f};
	const glm::vec2 goal {330.0f, 310.0f};
	ecs::components::FootpathFinder::Plan plan;
	plan.start = start;
	plan.goal = goal;
	plan.obstacles.ResetWithoutWalker(0.0f, gatherer);
	plan.obstacles.LeaveOutCirclesHolding(goal, start);
	plan.live = true;
	plan.search.emplace(plan.obstacles, route::Search::Start {.point = start},
	                    route::Search::Goal {.point = goal, .mostCost = 300.0f});
	plan.search->Step(plan.obstacles);

	entt::registry original;
	const auto finder = original.create();
	original.emplace<ecs::components::FootpathFinder>(finder, entt::entity {1}, entt::entity {2}, entt::entity {3}, plan);
	WorldSaveReport report;
	const auto bytes = SaveWorld(original, context, &report);
	EXPECT_TRUE(report.unsavedValues.empty());
	entt::registry loaded;
	ASSERT_TRUE(LoadWorld(loaded, context, bytes, WorldLoadMode::Exact).has_value());
	auto& back = loaded.get<ecs::components::FootpathFinder>(finder).plan;
	ASSERT_TRUE(back.has_value());
	// The save does not keep how the blocks fill, so the load gives it again
	EXPECT_FALSE(back->live);
	EXPECT_EQ(back->start, start);
	EXPECT_EQ(back->goal, goal);
	back->obstacles.SetGatherer(gatherer);
	back->obstacles.LeaveOutCirclesHolding(goal, start);

	const auto finish = [](ecs::components::FootpathFinder::Plan& searched) {
		for (int i = 0; i < 200 && searched.search->GetStatus() == route::Search::Status::Searching; ++i)
		{
			searched.search->Step(searched.obstacles);
		}
	};
	finish(plan);
	finish(*back);
	EXPECT_EQ(plan.search->GetStatus(), route::Search::Status::Found);
	ASSERT_EQ(back->search->GetStatus(), plan.search->GetStatus());
	EXPECT_EQ(back->obstacles.All().size(), plan.obstacles.All().size());
	ASSERT_EQ(back->search->GetBest().size(), plan.search->GetBest().size());
	for (size_t i = 0; i < plan.search->GetBest().size(); ++i)
	{
		EXPECT_EQ(back->search->GetBest()[i].to, plan.search->GetBest()[i].to);
		EXPECT_EQ(back->search->GetBest()[i].cost, plan.search->GetBest()[i].cost);
	}
}

TEST(WorldArchive, ASoundTagAndAnArenaKeepNoValuesOfTheRun)
{
	// The emitter of a sound tag is an entity of the audio of the run, and the game makes the tags new at a load, so
	// their count of turns starts again. The ring of an arena is made again after a load.
	entt::meta_ctx context;
	RegisterComponents(context);
	constexpr auto k_Sound = entt::hashed_string("G_Rustle");
	entt::registry original;
	const auto thing = original.create();
	original.emplace<ecs::components::SoundTag>(
	    thing,
	    ecs::components::SoundTag {
	        .sound = k_Sound.value(), .offset = {0.0f, 2.0f, 0.0f}, .active = true, .emitter = entt::entity {77}, .turns = 40});
	const auto arena = original.create();
	original.emplace<ecs::components::CreatureArena>(arena, ecs::components::CreatureArena {
	                                                            .place = {10, 20},
	                                                            .radius = 30.0f,
	                                                            .ring = std::make_shared<particles::LightSheet>(),
	                                                        });
	WorldSaveReport report;
	const auto bytes = SaveWorld(original, context, &report);
	EXPECT_TRUE(report.unsavedValues.empty());
	entt::registry loaded;
	const auto result = LoadWorld(loaded, context, bytes, WorldLoadMode::Exact);
	ASSERT_TRUE(result.has_value());
	const auto& tag = loaded.get<ecs::components::SoundTag>(thing);
	EXPECT_EQ(tag.emitter, entt::entity {entt::null});
	EXPECT_EQ(tag.turns, 0u);
	EXPECT_EQ(tag.sound, k_Sound.value());
	EXPECT_TRUE(tag.active);
	EXPECT_EQ(loaded.get<ecs::components::CreatureArena>(arena).ring, nullptr);
	EXPECT_FLOAT_EQ(loaded.get<ecs::components::CreatureArena>(arena).radius, 30.0f);
	// A save of the loaded world has the same bytes
	EXPECT_EQ(SaveWorld(loaded, context), bytes);
}

TEST(WorldArchive, AWriterGivesTheSameBytesForAValueThatItWroteBefore)
{
	entt::meta_ctx context;
	Register(context);
	const auto walker = SampleWalker(entt::entity {3}, entt::entity {8});
	FakeCounted counted;
	counted.counter.Add(12);

	// A fresh writer finds the plan of each type as it writes the values the first time
	BinaryWriter first(context);
	Save(first, walker);
	Save(first, counted);
	const auto firstBytes = first.Bytes();

	// The same writer writes them again by the plans that it kept
	BinaryWriter again(context);
	Save(again, walker);
	Save(again, counted);
	again.Clear();
	Save(again, walker);
	Save(again, counted);
	EXPECT_EQ(again.Bytes(), firstBytes);
	ASSERT_NE(again.FindPlan(entt::type_hash<FakeSpot>::value()), nullptr);
	EXPECT_EQ(again.FindPlan(entt::type_hash<FakeSpot>::value())->fields.size(), 3u);
	ASSERT_NE(again.FindPlan(entt::type_hash<FakeCounted>::value()), nullptr);
}

TEST(WorldArchive, AWriterNamesAValueThatItCannotKeepAlsoByThePlanOfItsType)
{
	entt::meta_ctx context;
	Register(context);
	BinaryWriter out(context);
	out.SetPlace("First", "hidden");
	Save(out, FakeHidden {.pointer = std::make_shared<int>(1), .kept = 4});
	ASSERT_NE(out.FindPlan(entt::type_hash<FakeHidden>::value()), nullptr);
	out.SetPlace("Second", "hidden");
	Save(out, FakeHidden {.pointer = std::make_shared<int>(2), .kept = 5});
	EXPECT_EQ(out.Unsaved().size(), 2u);
}

TEST(WorldArchive, TheBytesOfAWriterAreTheBytesWrittenUpToNow)
{
	entt::meta_ctx context;
	BinaryWriter out(context);
	out.WriteRaw(uint32_t {7});
	EXPECT_EQ(out.Size(), 4u);
	EXPECT_EQ(out.Bytes().size(), 4u);
	const auto length = out.BeginLength();
	out.Write(std::vector<uint8_t>(600, 9));
	out.EndLength(length);
	EXPECT_EQ(out.Size(), 608u);
	EXPECT_EQ(out.Written(4).size(), 604u);
	const auto& bytes = out.Bytes();
	ASSERT_EQ(bytes.size(), 608u);
	uint32_t written = 0;
	std::memcpy(&written, bytes.data() + length, sizeof(written));
	EXPECT_EQ(written, 600u);
	EXPECT_EQ(bytes.back(), 9);

	// A change that the caller made to the list stays, and the next bytes go after it
	out.Bytes().resize(2);
	out.WriteRaw(uint8_t {5});
	EXPECT_EQ(out.Bytes(), (std::vector<uint8_t> {7, 0, 5}));

	out.Clear();
	EXPECT_EQ(out.Size(), 0u);
	EXPECT_TRUE(out.Written().empty());
	out.WriteRaw(uint16_t {0x0102});
	EXPECT_EQ(out.Bytes(), (std::vector<uint8_t> {2, 1}));
}

TEST(WorldArchive, AListOfEnumerationsIsTheBytesOfEachValueInTurn)
{
	entt::meta_ctx context;
	const std::vector<entt::entity> entities {entt::entity {1}, entt::null, entt::entity {300}};
	const std::array<PlayerNames, 2> players {PlayerNames::PLAYER_TWO, PlayerNames::NEUTRAL};
	BinaryWriter lists(context);
	Save(lists, entities);
	Save(lists, players);

	BinaryWriter oneByOne(context);
	oneByOne.WriteSize(entities.size());
	for (const auto entity : entities)
	{
		oneByOne.WriteRaw(entity);
	}
	oneByOne.WriteSize(players.size());
	for (const auto player : players)
	{
		oneByOne.WriteRaw(player);
	}
	EXPECT_EQ(lists.Bytes(), oneByOne.Bytes());
}
