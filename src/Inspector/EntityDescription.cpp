/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "EntityDescription.h"

#include <cstring>

#include <algorithm>
#include <array>

#include <fmt/format.h>

#include "3D/PhysicsDrawMatrix.h"
#include "ComponentReflection.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"

using namespace openblack;
using namespace openblack::inspector;
using namespace openblack::ecs::components;

namespace
{

/// The name the info table gives a type, such as "BEECH", or its number when there is no table or no such row
template <typename Table, typename Type>
std::string TypeName(const Table* table, Type type)
{
	const auto index = static_cast<size_t>(type);
	if (table != nullptr && index < table->size())
	{
		const auto& text = (*table)[index].debugString;
		const auto length = strnlen(text.data(), text.size());
		if (length != 0)
		{
			return {text.data(), length};
		}
	}
	return std::to_string(index);
}

template <typename Member>
const Member* Table(const InfoConstants* info, Member InfoConstants::* member)
{
	return info == nullptr ? nullptr : &(info->*member);
}

} // namespace

uint32_t openblack::inspector::ToId(entt::entity entity)
{
	return entt::to_integral(entity);
}

std::optional<entt::entity> openblack::inspector::FromId(const Json& id)
{
	if (!id.is_number_integer() && !id.is_number_unsigned())
	{
		return std::nullopt;
	}
	const auto number = id.get<int64_t>();
	if (number < 0 || number > static_cast<int64_t>(UINT32_MAX))
	{
		return std::nullopt;
	}
	return static_cast<entt::entity>(static_cast<uint32_t>(number));
}

EntityDescription openblack::inspector::Describe(const ecs::Registry& registry, entt::entity entity, const InfoConstants* info)
{
	EntityDescription description;
	if (const auto* transform = registry.TryGet<Transform>(entity); transform != nullptr)
	{
		description.position = DrawnPosition(registry, entity);
		if (description.position != transform->position)
		{
			description.standing = transform->position;
		}
	}
	const auto set = [&description](std::string kind, std::string label) {
		description.kind = std::move(kind);
		description.label = std::move(label);
	};
	if (const auto* creature = registry.TryGet<Creature>(entity); creature != nullptr)
	{
		set("Creature", fmt::format("Creature: species {}, player {}", static_cast<int>(creature->species),
		                            static_cast<int>(creature->owner)));
	}
	else if (const auto* villager = registry.TryGet<Villager>(entity); villager != nullptr)
	{
		const auto stage = static_cast<size_t>(villager->lifeStage);
		const auto sex = static_cast<size_t>(villager->sex);
		set("Villager", fmt::format("Villager: {} {}, number {}",
		                            stage < Villager::k_LifeStageStrs.size() ? Villager::k_LifeStageStrs.at(stage) : "?",
		                            sex < Villager::k_SexStrs.size() ? Villager::k_SexStrs.at(sex) : "?",
		                            static_cast<int>(villager->number)));
	}
	else if (const auto* abode = registry.TryGet<Abode>(entity); abode != nullptr)
	{
		set("Abode", "Abode: " + TypeName(Table(info, &InfoConstants::abode), abode->info));
	}
	else if (const auto* tree = registry.TryGet<Tree>(entity); tree != nullptr)
	{
		set("Tree", "Tree: " + TypeName(Table(info, &InfoConstants::tree), tree->type));
	}
	else if (const auto* animal = registry.TryGet<Animal>(entity); animal != nullptr)
	{
		set("Animal", "Animal: " + TypeName(Table(info, &InfoConstants::animal), animal->type));
	}
	else if (const auto* field = registry.TryGet<Field>(entity); field != nullptr)
	{
		set("Field", "Field: " + TypeName(Table(info, &InfoConstants::fieldType), field->type));
	}
	else if (const auto* feature = registry.TryGet<Feature>(entity); feature != nullptr)
	{
		set("Feature", "Feature: " + TypeName(Table(info, &InfoConstants::feature), feature->type));
	}
	else if (const auto* mobileStatic = registry.TryGet<MobileStatic>(entity); mobileStatic != nullptr)
	{
		set("MobileStatic", "MobileStatic: " + TypeName(Table(info, &InfoConstants::mobileStatic), mobileStatic->type));
	}
	else if (const auto* mobileObject = registry.TryGet<MobileObject>(entity); mobileObject != nullptr)
	{
		set("MobileObject", "MobileObject: " + TypeName(Table(info, &InfoConstants::mobileObject), mobileObject->type));
	}
	else if (const auto* town = registry.TryGet<Town>(entity); town != nullptr)
	{
		set("Town", fmt::format("Town {}, player {}", town->id, static_cast<int>(town->owner)));
	}
	else if (const auto* temple = registry.TryGet<Temple>(entity); temple != nullptr)
	{
		set("Temple", fmt::format("Temple of player {}", static_cast<int>(temple->owner)));
	}
	else if (const auto* player = registry.TryGet<Player>(entity); player != nullptr)
	{
		set("Player", fmt::format("Player {}", static_cast<int>(player->name)));
	}
	else if (registry.AllOf<Fire>(entity))
	{
		set("Fire", "Fire");
	}
	else
	{
		// Anything else is known by its first component other than its place
		const auto names = ComponentNames(registry, entity);
		const auto first = std::ranges::find_if(names, [](const auto& name) { return name != "Transform"; });
		const auto kind = first != names.end() ? *first : (names.empty() ? std::string("Entity") : names.front());
		set(kind, kind);
	}
	return description;
}

std::optional<glm::vec3> openblack::inspector::DrawnPosition(const ecs::Registry& registry, entt::entity entity)
{
	const auto* transform = registry.TryGet<const Transform>(entity);
	if (transform == nullptr)
	{
		return std::nullopt;
	}
	return physics_draw::Position(transform->position, registry.TryGet<const PhysicsDrawPose>(entity));
}

Json openblack::inspector::ToListItem(entt::entity entity, const EntityDescription& description)
{
	Json item = {{"id", ToId(entity)}, {"kind", description.kind}, {"label", description.label}};
	if (description.position.has_value())
	{
		const auto& p = *description.position;
		item["position"] = {p.x, p.y, p.z};
	}
	if (description.standing.has_value())
	{
		const auto& p = *description.standing;
		item["transform_position"] = {p.x, p.y, p.z};
	}
	return item;
}

std::vector<std::string> openblack::inspector::ComponentNames(const ecs::Registry& registry, entt::entity entity)
{
	std::vector<std::string> names;
	for (const auto& [id, storage] : registry.Underlying().storage())
	{
		if (reflection::StorageType(storage) == entt::type_id<entt::entity>() || !storage.contains(entity))
		{
			continue;
		}
		names.push_back(reflection::ShortTypeName(reflection::StorageType(storage)));
	}
	return names;
}

Json openblack::inspector::ComponentsToJson(const ecs::Registry& registry, const entt::meta_ctx& context, entt::entity entity,
                                            const std::vector<std::string>& names)
{
	Json components = Json::object();
	for (const auto& [id, storage] : registry.Underlying().storage())
	{
		if (reflection::StorageType(storage) == entt::type_id<entt::entity>() || !storage.contains(entity))
		{
			continue;
		}
		auto name = reflection::ShortTypeName(reflection::StorageType(storage));
		if (!names.empty() && std::ranges::find(names, name) == names.end())
		{
			continue;
		}
		// Empty components (tags) hold no value, and are null
		components[name] = reflection::ComponentToJson(context, reflection::StorageType(storage), storage.value(entity));
	}
	return components;
}
