/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ComponentReflection.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <entt/meta/container.hpp>
#include <entt/meta/meta.hpp>
#include <entt/meta/resolve.hpp>

using namespace openblack::ecs;

std::string reflection::ShortTypeName(const entt::type_info& info)
{
	std::string_view name = info.name();
	for (const std::string_view prefix : {"struct ", "class ", "enum "})
	{
		if (name.starts_with(prefix))
		{
			name.remove_prefix(prefix.size());
		}
	}
	// The namespaces go, but not those of a template's arguments
	const auto open = name.find('<');
	const auto head = name.substr(0, open);
	const auto colons = head.rfind("::");
	if (colons != std::string_view::npos)
	{
		name.remove_prefix(colons + 2);
	}
	return std::string(name);
}

namespace
{

/// The most elements of a list looked through for references
constexpr size_t k_MostElementsSearched = 4096;

void FindReferences(const entt::meta_any& any, entt::entity target, const std::string& path, int depth,
                    std::vector<std::string>& found)
{
	if (!any || depth > reflection::k_DeepestNesting)
	{
		return;
	}
	const auto type = any.type();
	if (type.data().begin() != type.data().end())
	{
		for (const auto& [id, data] : type.data())
		{
			const reflection::FieldInfo* info = data.custom();
			const auto name = info != nullptr ? info->name : std::to_string(id);
			const auto fieldPath = path.empty() ? name : path + "." + name;
			const auto value = data.get(any);
			if (info != nullptr && info->refersTo != nullptr)
			{
				if (info->refersTo(value, target))
				{
					found.push_back(fieldPath);
				}
				continue;
			}
			FindReferences(value, target, fieldPath, depth + 1, found);
		}
		return;
	}
	if (auto sequence = any.as_sequence_container(); sequence)
	{
		size_t index = 0;
		for (auto element : sequence)
		{
			if (index == k_MostElementsSearched)
			{
				break;
			}
			FindReferences(element, target, path + "." + std::to_string(index++), depth + 1, found);
		}
	}
}

} // namespace

std::vector<std::string> reflection::References(const entt::meta_ctx& context, const entt::type_info& info,
                                                const void* component, entt::entity target)
{
	std::vector<std::string> found;
	const auto type = entt::resolve(context, info);
	if (!type || component == nullptr)
	{
		return found;
	}
	FindReferences(type.from_void(component), target, {}, 0, found);
	return found;
}

bool reflection::IsReflected(const entt::meta_ctx& context, const entt::type_info& info)
{
	const auto type = entt::resolve(context, info);
	return type && type.data().begin() != type.data().end();
}

entt::sparse_set* reflection::FindStorage(entt::registry& registry, const entt::meta_ctx& context, std::string_view name)
{
	for (auto&& [id, storage] : registry.storage())
	{
		if (ShortTypeName(StorageType(storage)) == name)
		{
			return &storage;
		}
	}
	for (auto&& [id, type] : entt::resolve(context))
	{
		const ComponentInfo* info = type.custom();
		if (info != nullptr && info->storage != nullptr && ShortTypeName(type.info()) == name)
		{
			return &info->storage(registry);
		}
	}
	return nullptr;
}

const entt::sparse_set* reflection::FindStorage(const entt::registry& registry, std::string_view name)
{
	for (const auto& [id, storage] : registry.storage())
	{
		if (ShortTypeName(StorageType(storage)) == name)
		{
			return &storage;
		}
	}
	return nullptr;
}

void reflection::RegisterComponents(entt::meta_ctx& context)
{
	RegisterComponentFields(context);
	RegisterHandWrittenFields(context);
}

namespace
{

/// The codec of a whole type, when the type has one
const reflection::TypeCodec* CodecOf(const entt::meta_type& type)
{
	if (const reflection::ComponentInfo* component = type.custom(); component != nullptr && component->codec.save != nullptr)
	{
		return &component->codec;
	}
	if (const reflection::ValueInfo* value = type.custom(); value != nullptr && value->codec.save != nullptr)
	{
		return &value->codec;
	}
	return nullptr;
}

/// Whether the binary archive keeps the field
bool Kept(const reflection::FieldInfo* info)
{
	return info != nullptr && info->save != nullptr && info->load != nullptr;
}

} // namespace

bool reflection::IsRegistered(const entt::meta_ctx& context, const entt::type_info& info)
{
	const auto type = entt::resolve(context, info);
	return type && (CodecOf(type) != nullptr || type.data().begin() != type.data().end());
}

void reflection::SaveRegistered(BinaryWriter& out, const entt::meta_any& value)
{
	const auto type = entt::resolve(out.Context(), value.type().info());
	if (!type)
	{
		out.Unsupported(value.type().info().name());
		return;
	}
	if (const auto* codec = CodecOf(type); codec != nullptr)
	{
		codec->save(value.base().data(), out);
		return;
	}
	for (const auto& [id, data] : type.data())
	{
		const FieldInfo* info = data.custom();
		if (Kept(info) && info->saveFrom != nullptr)
		{
			info->saveFrom(value.base().data(), out);
		}
		else if (Kept(info))
		{
			info->save(data.get(value), out);
		}
		else if (info != nullptr)
		{
			// A value that only a function reads cannot come back: the list of unsaved values names it
			out.Unsupported(info->name);
		}
	}
}

void reflection::LoadRegistered(BinaryReader& in, entt::meta_any& value)
{
	const auto type = entt::resolve(in.Context(), value.type().info());
	if (!type)
	{
		return;
	}
	if (const auto* codec = CodecOf(type); codec != nullptr)
	{
		// A reference to a constant value cannot take what the archive reads
		if (value.base().policy() == entt::any_policy::cref)
		{
			in.Fail();
			return;
		}
		codec->load(const_cast<void*>(value.base().data()), in);
		return;
	}
	for (const auto& [id, data] : type.data())
	{
		const FieldInfo* info = data.custom();
		if (Kept(info) && !in.Failed())
		{
			info->load(data, value, in);
		}
	}
}
