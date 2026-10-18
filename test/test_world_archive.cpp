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

#include <array>
#include <bitset>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <entt/entity/registry.hpp>
#include <entt/meta/context.hpp>
#include <entt/meta/resolve.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Common/GameHeading.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/Reflection/WorldArchive.h"

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
