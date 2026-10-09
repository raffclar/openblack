/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ComponentReflection.h"

#include <cstdint>

#include <optional>
#include <string>

#include <entt/entity/entity.hpp>
#include <entt/meta/container.hpp>
#include <entt/meta/meta.hpp>
#include <entt/meta/resolve.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"

using namespace openblack::inspector;
using namespace openblack::ecs::components;

namespace
{

/// Nested structures are written this deep, and containers only this many of their first elements
constexpr int k_DeepestNesting = 3;
constexpr size_t k_MostElements = 16;

template <typename Vector>
Json VectorToJson(const Vector& vector)
{
	Json array = Json::array();
	for (typename Vector::length_type i = 0; i < Vector::length(); ++i)
	{
		array.push_back(vector[i]);
	}
	return array;
}

/// A value of a type that JSON holds directly, none for any other
std::optional<Json> LeafToJson(const entt::meta_any& any)
{
	if (const auto* value = any.try_cast<const bool>(); value != nullptr)
	{
		return Json(*value);
	}
	if (const auto* value = any.try_cast<const float>(); value != nullptr)
	{
		return Json(*value);
	}
	if (const auto* value = any.try_cast<const double>(); value != nullptr)
	{
		return Json(*value);
	}
	if (const auto* value = any.try_cast<const int>(); value != nullptr)
	{
		return Json(*value);
	}
	if (const auto* value = any.try_cast<const uint32_t>(); value != nullptr)
	{
		return Json(*value);
	}
	if (const auto* value = any.try_cast<const int8_t>(); value != nullptr)
	{
		return Json(*value);
	}
	if (const auto* value = any.try_cast<const uint8_t>(); value != nullptr)
	{
		return Json(*value);
	}
	if (const auto* value = any.try_cast<const int16_t>(); value != nullptr)
	{
		return Json(*value);
	}
	if (const auto* value = any.try_cast<const uint16_t>(); value != nullptr)
	{
		return Json(*value);
	}
	if (const auto* value = any.try_cast<const int64_t>(); value != nullptr)
	{
		return Json(*value);
	}
	if (const auto* value = any.try_cast<const uint64_t>(); value != nullptr)
	{
		return Json(*value);
	}
	if (const auto* value = any.try_cast<const std::string>(); value != nullptr)
	{
		return Json(*value);
	}
	if (const auto* value = any.try_cast<const entt::entity>(); value != nullptr)
	{
		return *value == entt::null ? Json(nullptr) : Json(entt::to_integral(*value));
	}
	if (const auto* value = any.try_cast<const glm::vec2>(); value != nullptr)
	{
		return VectorToJson(*value);
	}
	if (const auto* value = any.try_cast<const glm::vec3>(); value != nullptr)
	{
		return VectorToJson(*value);
	}
	if (const auto* value = any.try_cast<const glm::vec4>(); value != nullptr)
	{
		return VectorToJson(*value);
	}
	if (const auto* value = any.try_cast<const glm::mat3>(); value != nullptr)
	{
		return Json {VectorToJson((*value)[0]), VectorToJson((*value)[1]), VectorToJson((*value)[2])};
	}
	return std::nullopt;
}

Json AnyToJson(const entt::meta_any& any, int depth);

Json FieldsToJson(const entt::meta_any& any, int depth)
{
	Json object = Json::object();
	for (const auto& [id, data] : any.type().data())
	{
		const reflection::FieldName* name = data.custom();
		const auto key = name != nullptr ? name->name : std::to_string(id);
		object[key] = AnyToJson(data.get(any), depth + 1);
	}
	return object;
}

Json AnyToJson(const entt::meta_any& any, int depth)
{
	if (!any)
	{
		return nullptr;
	}
	if (auto leaf = LeafToJson(any); leaf.has_value())
	{
		return *std::move(leaf);
	}
	const auto type = any.type();
	if (type.is_enum())
	{
		// An enumeration is written as its number
		if (const auto number = any.allow_cast<int64_t>(); number)
		{
			return number.cast<int64_t>();
		}
	}
	if (depth >= k_DeepestNesting)
	{
		return "...";
	}
	if (type.data().begin() != type.data().end())
	{
		return FieldsToJson(any, depth);
	}
	if (auto sequence = any.as_sequence_container(); sequence)
	{
		Json array = Json::array();
		size_t count = 0;
		for (auto element : sequence)
		{
			if (count++ == k_MostElements)
			{
				break;
			}
			array.push_back(AnyToJson(element, depth + 1));
		}
		if (sequence.size() <= k_MostElements)
		{
			return array;
		}
		return {{"size", sequence.size()}, {"first", std::move(array)}};
	}
	if (auto associative = any.as_associative_container(); associative)
	{
		return {{"size", associative.size()}};
	}
	// A value of a type that isn't registered: its name, to say what is there
	return "<" + reflection::ShortTypeName(type.info()) + ">";
}

} // namespace

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

bool reflection::IsReflected(const entt::meta_ctx& context, const entt::type_info& info)
{
	const auto type = entt::resolve(context, info);
	return type && type.data().begin() != type.data().end();
}

Json reflection::ComponentToJson(const entt::meta_ctx& context, const entt::type_info& info, const void* component)
{
	const auto type = entt::resolve(context, info);
	if (!type || type.data().begin() == type.data().end() || component == nullptr)
	{
		return nullptr;
	}
	return FieldsToJson(type.from_void(component), 0);
}

void reflection::RegisterComponents(entt::meta_ctx& context)
{
	Reflect<Transform>(context)
	    .Field<&Transform::position>("position")
	    .Field<&Transform::rotation>("rotation")
	    .Field<&Transform::scale>("scale");
	Reflect<Fixed>(context).Field<&Fixed::boundingCenter>("boundingCenter").Field<&Fixed::boundingRadius>("boundingRadius");
	Reflect<Villager>(context)
	    .Field<&Villager::life>("life")
	    .Field<&Villager::birthTurn>("birthTurn")
	    .Field<&Villager::food>("food")
	    .Field<&Villager::lifeStage>("lifeStage")
	    .Field<&Villager::sex>("sex")
	    .Field<&Villager::tribe>("tribe")
	    .Field<&Villager::number>("number")
	    .Field<&Villager::task>("task")
	    .Field<&Villager::town>("town")
	    .Field<&Villager::abode>("abode")
	    .Field<&Villager::carried>("carried");
	Reflect<Abode>(context)
	    .Field<&Abode::type>("type")
	    .Field<&Abode::townId>("townId")
	    .Field<&Abode::foodAmount>("foodAmount")
	    .Field<&Abode::woodAmount>("woodAmount")
	    .Field<&Abode::inhabitants>("inhabitants")
	    .Field<&Abode::presentAtHome>("presentAtHome")
	    .Field<&Abode::info>("info");
	Reflect<Tree>(context).Field<&Tree::type>("type").Field<&Tree::maxSize>("maxSize").Field<&Tree::turnsToGrowth>(
	    "turnsToGrowth");
	Reflect<Feature>(context).Field<&Feature::type>("type");
	Reflect<Field>(context).Field<&Field::town>("town").Field<&Field::type>("type").Field<&Field::growthTurn>("growthTurn");
	Reflect<Animal>(context)
	    .Field<&Animal::type>("type")
	    .Field<&Animal::owner>("owner")
	    .Field<&Animal::flock>("flock")
	    .Field<&Animal::state>("state")
	    .Field<&Animal::turnsInState>("turnsInState")
	    .Field<&Animal::hunger>("hunger")
	    .Field<&Animal::position>("position")
	    .Field<&Animal::heading>("heading")
	    .Field<&Animal::life>("life");
	Reflect<Creature>(context)
	    .Field<&Creature::owner>("owner")
	    .Field<&Creature::leashable>("leashable")
	    .Field<&Creature::species>("species")
	    .Field<&Creature::alignment>("alignment")
	    .Field<&Creature::fatness>("fatness")
	    .Field<&Creature::strength>("strength")
	    .Field<&Creature::size>("size")
	    .Field<&Creature::objectsDestroyed>("objectsDestroyed")
	    .Field<&Creature::canDie>("canDie");
	Reflect<Town>(context)
	    .Field<&Town::id>("id")
	    .Field<&Town::owner>("owner")
	    .Field<&Town::uninhabitable>("uninhabitable")
	    .Field<&Town::homelessVillagers>("homelessVillagers")
	    .Field<&Town::abodes>("abodes")
	    .Field<&Town::playthings>("playthings")
	    .Field<&Town::emergencyTurn>("emergencyTurn");
	Reflect<Player>(context)
	    .Field<&Player::name>("name")
	    .Field<&Player::windResistance>("windResistance")
	    .Field<&Player::villagersLost>("villagersLost")
	    .Field<&Player::villagersKilled>("villagersKilled")
	    .Field<&Player::sacrifices>("sacrifices");
	Reflect<Temple>(context)
	    .Field<&Temple::owner>("owner")
	    .Field<&Temple::lastHitTurn>("lastHitTurn")
	    .Field<&Temple::beamTarget>("beamTarget")
	    .Field<&Temple::destroying>("destroying");
	Reflect<Fire>(context)
	    .Field<&Fire::source>("source")
	    .Field<&Fire::player>("player")
	    .Field<&Fire::root>("root")
	    .Field<&Fire::createdTurn>("createdTurn")
	    .Field<&Fire::reaction>("reaction");
	Reflect<MobileStatic>(context).Field<&MobileStatic::type>("type");
	Reflect<MobileObject>(context).Field<&MobileObject::type>("type");
}
