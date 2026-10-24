/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdlib>

#include <fstream>
#include <set>
#include <sstream>
#include <string>

#include <entt/meta/context.hpp>
#include <entt/meta/meta.hpp>
#include <entt/meta/resolve.hpp>
#include <gtest/gtest.h>

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
