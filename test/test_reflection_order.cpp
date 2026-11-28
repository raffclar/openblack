/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstdlib>

#include <array>
#include <deque>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <entt/meta/context.hpp>
#include <entt/meta/meta.hpp>
#include <entt/meta/resolve.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureObjectBeliefs.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Reflection/ComponentReflection.h"

using namespace openblack::ecs::reflection;

namespace
{

/// Every registered type in the order of registration, with each of its fields in order and what is kept with them
std::string DumpRegistration(const entt::meta_ctx& context)
{
	std::ostringstream out;
	for (auto&& [id, type] : entt::resolve(context))
	{
		out << type.info().name();
		if (const ComponentInfo* component = type.custom(); component != nullptr)
		{
			out << " component storage=" << (component->storage != nullptr) << " copy=" << (component->copy != nullptr)
			    << " codec=" << (component->codec.save != nullptr);
		}
		if (const ValueInfo* value = type.custom(); value != nullptr)
		{
			out << " value unwrap=" << (value->unwrap != nullptr) << " engage=" << (value->engage != nullptr)
			    << " codec=" << (value->codec.save != nullptr);
		}
		out << '\n';
		for (auto&& [fieldId, data] : type.data())
		{
			out << "  " << fieldId;
			if (const FieldInfo* field = data.custom(); field != nullptr)
			{
				out << ' ' << field->name << " refers=" << (field->refersTo != nullptr) << " save=" << (field->save != nullptr)
				    << " saveFrom=" << (field->saveFrom != nullptr) << " load=" << (field->load != nullptr)
				    << " levels=" << field->indexNames.size();
				for (const auto& names : field->indexNames)
				{
					out << " [";
					for (const auto& name : names)
					{
						out << name << ',';
					}
					out << ']';
				}
			}
			out << '\n';
		}
	}
	return out.str();
}

} // namespace

TEST(ReflectionOrder, RegistrationIsTheSameEveryTime)
{
	entt::meta_ctx first;
	entt::meta_ctx second;
	RegisterComponents(first);
	RegisterComponents(second);
	const auto dump = DumpRegistration(first);
	EXPECT_FALSE(dump.empty());
	EXPECT_EQ(dump, DumpRegistration(second));
}

TEST(ReflectionOrder, EveryComponentHasFieldsWithOwnNames)
{
	entt::meta_ctx context;
	RegisterComponents(context);
	for (auto&& [id, type] : entt::resolve(context))
	{
		std::set<std::string> names;
		for (auto&& [fieldId, data] : type.data())
		{
			if (const FieldInfo* field = data.custom(); field != nullptr)
			{
				EXPECT_TRUE(names.insert(field->name).second) << type.info().name() << "." << field->name;
			}
		}
	}
}

// With OPENBLACK_REFLECTION_DUMP set to a path, writes the dump there, so that two builds (before and after a change
// to how the registration is split into files) can be compared with a plain text diff
TEST(ReflectionOrder, WritesTheDumpWhenAsked)
{
	const char* path = std::getenv("OPENBLACK_REFLECTION_DUMP");
	if (path == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_REFLECTION_DUMP is not set";
	}
	entt::meta_ctx context;
	RegisterComponents(context);
	std::ofstream file(path, std::ios::binary);
	file << DumpRegistration(context);
	EXPECT_TRUE(file.good());
}

namespace
{

/// Saves a value through its registered fields and through the values that it gives to a visitor, and loads each save
/// back the other way. The two saves must give the same bytes, and each type must give each registered field once.
template <typename Type>
void ExpectVisitedSaveIsTheFieldSave(const entt::meta_ctx& context, const Type& value)
{
	SCOPED_TRACE(entt::type_id<Type>().name());
	size_t visited = 0;
	Type::VisitSaved(value, [&visited](const auto&... fields) { visited = sizeof...(fields); });
	const auto type = entt::resolve<Type>(context);
	ASSERT_TRUE(type);
	EXPECT_EQ(visited, static_cast<size_t>(std::distance(type.data().begin(), type.data().end())));

	// The first save through the fields makes the plan of the type, and the second save uses the plan
	BinaryWriter byFields(context);
	for (int pass = 0; pass < 2; ++pass)
	{
		byFields.Clear();
		SaveRegistered(byFields, entt::meta_any {context, std::in_place_type<const Type&>, value});
		BinaryWriter byVisitor(context);
		Save(byVisitor, value);
		ASSERT_EQ(byFields.Bytes(), byVisitor.Bytes());

		const EntityRemap identity;
		Type loadedByFields {};
		BinaryReader fieldReader(context, byVisitor.Bytes(), identity);
		entt::meta_any reference {context, std::in_place_type<Type&>, loadedByFields};
		LoadRegistered(fieldReader, reference);
		Type loadedByVisitor {};
		BinaryReader visitReader(context, byFields.Bytes(), identity);
		Load(visitReader, loadedByVisitor);
		ASSERT_FALSE(fieldReader.Failed());
		ASSERT_FALSE(visitReader.Failed());
		BinaryWriter again(context);
		Save(again, loadedByFields);
		Save(again, loadedByVisitor);
		BinaryWriter twice(context);
		Save(twice, value);
		Save(twice, value);
		EXPECT_EQ(again.Bytes(), twice.Bytes());
	}
}

} // namespace

TEST(ReflectionOrder, TheBeliefsOfACreatureSaveTheSameBytesThroughTheVisitorAsThroughTheFields)
{
	namespace beliefs = openblack::creature_object_beliefs;
	entt::meta_ctx context;
	RegisterComponents(context);

	const openblack::map_coords::MapCoords place {.x = 0x12345, .z = -0x6789A, .altitude = 3.5f};
	openblack::creature_tree::Belief attributes {.type = 7, .values = {}, .x = 0x12345, .z = -0x6789A};
	attributes.values.at(0) = 3;
	attributes.values.at(4) = 0xFFFFFFFF;
	beliefs::Belief belief {
	    .thing = entt::entity {42},
	    .kind = 4,
	    .place = place,
	    .inspected = 100,
	    .partOf = entt::entity {9},
	    .order = 0x0102030405060708,
	    .blockedTurn = std::nullopt,
	    .attributes = attributes,
	    .sourcesTurn = 77,
	};
	beliefs::Belief bare {.thing = entt::entity {5},
	                      .kind = 2,
	                      .place = {},
	                      .inspected = std::nullopt,
	                      .partOf = std::nullopt,
	                      .order = 1,
	                      .blockedTurn = 12,
	                      .attributes = std::nullopt,
	                      .sourcesTurn = 0};
	const beliefs::BeliefPlace beliefPlace {.kind = 4, .slot = 3};
	const beliefs::BeliefPool pool {.slots = {std::nullopt, belief, bare, std::nullopt}, .used = 2, .oldest = 1};
	beliefs::StoreContents contents {};
	contents.pools.at(4) = pool;
	contents.pools.at(2).slots = {bare};
	contents.index = {{entt::entity {5}, {.kind = 2, .slot = 0}}, {entt::entity {42}, beliefPlace}};
	contents.made = 9;

	ExpectVisitedSaveIsTheFieldSave(context, place);
	ExpectVisitedSaveIsTheFieldSave(context, attributes);
	ExpectVisitedSaveIsTheFieldSave(context, openblack::creature_tree::Belief {});
	ExpectVisitedSaveIsTheFieldSave(context, belief);
	ExpectVisitedSaveIsTheFieldSave(context, bare);
	ExpectVisitedSaveIsTheFieldSave(context, beliefPlace);
	ExpectVisitedSaveIsTheFieldSave(context, pool);
	ExpectVisitedSaveIsTheFieldSave(context, beliefs::BeliefPool {});
	ExpectVisitedSaveIsTheFieldSave(context, contents);
	ExpectVisitedSaveIsTheFieldSave(context, beliefs::StoreContents {});
	ExpectVisitedSaveIsTheFieldSave(context, beliefs::IndexEntry {.thing = entt::entity {42}, .place = beliefPlace});

	// The index of the contents writes the same bytes as a map from each thing to its place
	const std::map<entt::entity, beliefs::BeliefPlace> asMap {{entt::entity {5}, {.kind = 2, .slot = 0}},
	                                                          {entt::entity {42}, beliefPlace}};
	BinaryWriter mapBytes(context);
	Save(mapBytes, asMap);
	BinaryWriter indexBytes(context);
	Save(indexBytes, contents.index);
	EXPECT_EQ(mapBytes.Bytes(), indexBytes.Bytes());
}

namespace
{

enum class Shade : uint8_t
{
	Light = 3,
	Dark = 200,
};

/// The same values, once in a type that saves flat and once in a type that saves each value the general way
template <bool Flat>
struct Mixed
{
	bool flag {false};
	int8_t small {0};
	size_t count {0};
	std::optional<bool> maybeFlag;
	std::optional<size_t> maybeCount;
	std::array<std::optional<uint16_t>, 3> slots {};
	std::array<bool, 2> flags {};
	Shade shade {Shade::Light};
	double number {0.0};

	template <typename Self, typename Visitor>
	static void VisitSaved(Self& self, Visitor&& visit)
	{
		visit(self.flag, self.small, self.count, self.maybeFlag, self.maybeCount, self.slots, self.flags, self.shade,
		      self.number);
	}
	static constexpr bool k_SavesFlat = Flat;
};

template <bool Flat>
struct MixedLists
{
	std::vector<Mixed<Flat>> list;
	std::deque<std::optional<Mixed<Flat>>> maybeList;
	std::map<int16_t, Mixed<Flat>> byNumber;
	std::optional<Mixed<Flat>> maybe;
	std::array<Mixed<Flat>, 2> pair {};

	template <typename Self, typename Visitor>
	static void VisitSaved(Self& self, Visitor&& visit)
	{
		visit(self.list, self.maybeList, self.byNumber, self.maybe, self.pair);
	}
};

template <bool Flat>
MixedLists<Flat> MakeMixedLists()
{
	Mixed<Flat> first {.flag = true,
	                   .small = -5,
	                   .count = 0x1122334455,
	                   .maybeFlag = false,
	                   .maybeCount = std::nullopt,
	                   .slots = {std::nullopt, uint16_t {7}, std::nullopt},
	                   .flags = {true, false},
	                   .shade = Shade::Dark,
	                   .number = -0.25};
	Mixed<Flat> second {};
	second.maybeCount = 9;
	MixedLists<Flat> lists;
	lists.list = {first, second, first};
	lists.maybeList = {std::nullopt, second};
	lists.byNumber = {{-3, second}, {12, first}};
	lists.maybe = first;
	lists.pair = {second, first};
	return lists;
}

} // namespace

TEST(ReflectionOrder, ATypeThatSavesFlatGivesTheSameBytesAsTheGeneralSave)
{
	static_assert(detail::SavesFlat<Mixed<true>>());
	static_assert(!detail::SavesFlat<Mixed<false>>());
	static_assert(detail::k_SavesAsOneBlock<std::optional<Mixed<true>>>);
	const entt::meta_ctx context;
	BinaryWriter flat(context);
	Save(flat, MakeMixedLists<true>());
	BinaryWriter general(context);
	Save(general, MakeMixedLists<false>());
	EXPECT_EQ(flat.Bytes(), general.Bytes());

	const EntityRemap identity;
	BinaryReader reader(context, flat.Bytes(), identity);
	MixedLists<true> loaded;
	Load(reader, loaded);
	ASSERT_FALSE(reader.Failed());
	BinaryWriter again(context);
	Save(again, loaded);
	EXPECT_EQ(again.Bytes(), flat.Bytes());
}
