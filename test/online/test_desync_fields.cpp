/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <WorldFile.h>
#include <gtest/gtest.h>

#include "ECS/SyncedState.h"
#include "Server/DesyncFields.h"

using namespace openblack;
using namespace openblack::server::desync_fields;

namespace
{

/// The bytes of an entities block, as the save writes it, for one component with fields and rows
class EntitiesBlock
{
public:
	void Number(std::vector<uint8_t>& out, uint32_t value)
	{
		for (size_t i = 0; i < sizeof(uint32_t); ++i)
		{
			out.push_back(static_cast<uint8_t>(value >> (8 * i)));
		}
	}

	void Name(std::vector<uint8_t>& out, std::string_view name)
	{
		Number(out, static_cast<uint32_t>(name.size()));
		out.insert(out.end(), name.begin(), name.end());
	}

	/// A component with its fields, and one row of each entity with one byte for each field
	void Component(std::string_view name, const std::vector<std::string>& fields,
	               const std::vector<std::pair<uint32_t, std::vector<uint8_t>>>& rows)
	{
		std::vector<uint8_t> block;
		Number(block, static_cast<uint32_t>(fields.size()));
		for (const auto& field : fields)
		{
			Name(block, field);
		}
		Number(block, static_cast<uint32_t>(rows.size()));
		for (const auto& [entity, values] : rows)
		{
			Number(block, entity);
			for (const auto value : values)
			{
				Number(block, 1);
				block.push_back(value);
			}
		}
		Name(_components, name);
		Number(_components, static_cast<uint32_t>(block.size()));
		_components.insert(_components.end(), block.begin(), block.end());
		++_count;
	}

	/// The saved world with the block
	[[nodiscard]] std::vector<uint8_t> World()
	{
		std::vector<uint8_t> block {'O', 'B', 'E', 'W'};
		Number(block, 1);
		// Two entities, both in use
		Number(block, 2);
		Number(block, 2);
		Number(block, 7);
		Number(block, 9);
		Number(block, _count);
		block.insert(block.end(), _components.begin(), _components.end());
		gamesave::WorldFile file;
		file.chunks.push_back({.tag = gamesave::k_EntitiesTag, .bytes = std::move(block)});
		return gamesave::Write(file);
	}

private:
	std::vector<uint8_t> _components;
	uint32_t _count {0};
};

const std::vector<std::string> k_Fields {"alignment", "fatness", "size"};

std::vector<uint8_t> WorldWith(std::vector<uint8_t> creature7, std::vector<uint8_t> creature9)
{
	EntitiesBlock block;
	block.Component("Transform", {"position"}, {{7, {1}}, {9, {2}}});
	block.Component("Creature", k_Fields, {{7, std::move(creature7)}, {9, std::move(creature9)}});
	return block.World();
}

} // namespace

TEST(DesyncFields, TheFieldsThatDifferAreNamedForEachEntity)
{
	const auto server = WorldWith({1, 2, 3}, {4, 5, 6});
	const auto player = WorldWith({9, 2, 8}, {4, 5, 6});
	const std::array<uint32_t, 2> entities {7, 9};
	const auto fields = DifferentFields(server, player, "Creature", entities, {});
	ASSERT_TRUE(fields.has_value());
	ASSERT_EQ(fields->size(), 1u);
	EXPECT_EQ(fields->front(), (EntityFields {.entity = 7, .fields = {"alignment", "size"}}));
	EXPECT_EQ(Describe(*fields), "entity 7: alignment, size");
	EXPECT_EQ(FirstField("Creature", *fields), "Creature.alignment");
}

TEST(DesyncFields, AFieldThatOnlyShowsTheWorldIsLeftOut)
{
	const auto server = WorldWith({1, 2, 3}, {4, 5, 6});
	const auto player = WorldWith({9, 2, 3}, {4, 5, 6});
	const std::array<uint32_t, 1> entities {7};
	const std::array presentation {
	    synced_state::PresentationPart {.component = "Creature", .field = "alignment", .marker = ""}};
	const auto fields = DifferentFields(server, player, "Creature", entities, presentation);
	ASSERT_TRUE(fields.has_value());
	EXPECT_TRUE(fields->empty());
	EXPECT_EQ(Describe(*fields), "no field differs");
	EXPECT_FALSE(FirstField("Creature", *fields).has_value());
}

TEST(DesyncFields, OnlyTheAskedEntitiesAndComponentAreCompared)
{
	const auto server = WorldWith({1, 2, 3}, {4, 5, 6});
	const auto player = WorldWith({1, 2, 3}, {0, 5, 6});
	const std::array<uint32_t, 1> entities {7};
	const auto fields = DifferentFields(server, player, "Creature", entities, {});
	ASSERT_TRUE(fields.has_value());
	EXPECT_TRUE(fields->empty());
	// A component that the world does not have gives no field
	const std::array<uint32_t, 1> other {9};
	const auto none = DifferentFields(server, player, "Villager", other, {});
	ASSERT_TRUE(none.has_value());
	EXPECT_TRUE(none->empty());
}

TEST(DesyncFields, AWorldThatDoesNotReadGivesNone)
{
	const auto server = WorldWith({1, 2, 3}, {4, 5, 6});
	const std::vector<uint8_t> broken {'O', 'B', 'S', 'V', 1};
	const std::array<uint32_t, 1> entities {7};
	EXPECT_FALSE(DifferentFields(server, broken, "Creature", entities, {}).has_value());
	auto cut = server;
	cut.resize(cut.size() - 3);
	EXPECT_FALSE(DifferentFields(server, cut, "Creature", entities, {}).has_value());
}
