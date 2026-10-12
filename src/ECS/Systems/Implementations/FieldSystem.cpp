/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "FieldSystem.h"

#include <algorithm>

#include <glm/gtx/vec_swizzle.hpp>

#include "Common/GUtilsDistance.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/VillagerWorkRules.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

namespace
{

/// How near a field is to a farmer counts for less the further it is, out to this many metres
constexpr float k_FieldReach = 300.0f;

ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

field_crop::Type TypeOf(FieldTypeInfo type)
{
	const auto& info = Locator::infoConstants::value().fieldType.at(static_cast<size_t>(type));
	return {
	    .ageGrowth = info.ageGrowth,
	    .ageRipe = info.ageRecolt,
	    .timesToSow = info.timesToSow,
	    .totalFood = info.totalFoodInField,
	    .sunWhenGrowing = info.effectSunWhenGrowing,
	    .sunWhenRipening = info.effectSunWhenRipening,
	    .rainWhenGrowing = info.effectRainWhenGrowing,
	    .rainWhenRipening = info.effectRainWhenRipening,
	};
}

/// The land's alignment at a place: each player's influence there times their alignment
float LandAlignment(const glm::vec3& position)
{
	if (!Locator::influenceSystem::has_value() || !Locator::alignmentSystem::has_value())
	{
		return 0.0f;
	}
	const auto& influence = Locator::influenceSystem::value();
	const auto& alignment = Locator::alignmentSystem::value();
	float sum = 0.0f;
	for (size_t i = 0; i < static_cast<size_t>(PlayerNames::_COUNT); ++i)
	{
		const auto player = static_cast<PlayerNames>(i);
		sum += alignment.GetPlayerAlignment(player) * influence.PlayerInfluence(player, position);
	}
	return field_crop::ClampAlignment(sum);
}

bool IsRaining(const glm::vec3& position)
{
	return Locator::weatherSystem::has_value() && Locator::weatherSystem::value().GetWeather(position).rain > 0;
}

} // namespace

void FieldSystem::ProcessTurn(uint32_t turn)
{
	Locator::entitiesRegistry::value().Each<Field, const Transform>([turn](Field& field, const Transform& transform) {
		if ((turn + field.growthTurn) % field_crop::k_TurnsPerGrowth != 0)
		{
			return;
		}
		field_crop::Grow(field.crop, TypeOf(field.type), LandAlignment(transform.position), IsRaining(transform.position));
	});
}

void FieldSystem::Update(std::chrono::duration<float, std::milli> gameTime)
{
	const float seconds = std::chrono::duration<float>(gameTime).count();
	Locator::entitiesRegistry::value().Each<Field>([this, seconds](Field& field) {
		if (const auto look = GetLook(field); look.has_value())
		{
			field.height = field_crop::Advance(field.height, look->height, seconds);
		}
	});
}

std::optional<field_crop::Look> FieldSystem::GetLook(const Field& field) const
{
	const auto leastFood =
	    Locator::infoConstants::value().pot.at(static_cast<size_t>(PotInfo::HandFood)).amountPickedUpInitially;
	return field_crop::LookOf(field.crop, TypeOf(field.type), leastFood);
}

field_crop::Activity FieldSystem::GetActivity(entt::entity field) const
{
	const auto& data = Entities().Get<const Field>(field);
	return field_crop::ActivityOf(data.crop, TypeOf(data.type));
}

bool FieldSystem::IsRipe(entt::entity field) const
{
	const auto& data = Entities().Get<const Field>(field);
	return field_crop::IsRipe(data.crop, TypeOf(data.type));
}

std::optional<FieldSystemInterface::FieldChoice> FieldSystem::BestFieldFor(entt::entity town, const glm::vec3& position) const
{
	const auto* record = Entities().TryGet<const Town>(town);
	if (record == nullptr)
	{
		return std::nullopt;
	}
	const auto& types = Locator::infoConstants::value().fieldType;
	const auto here = glm::xz(position);
	// The newest of equals wins, as fields are weighed newest first
	std::optional<FieldChoice> best;
	float bestScore = 0.0f;
	Entities().Each<const Field, const Transform>([&](entt::entity entity, const Field& field, const Transform& at) {
		if (field.town != static_cast<int>(record->id))
		{
			return;
		}
		const auto& type = types.at(static_cast<size_t>(field.type));
		const float desire = ecs::villager_work::DesireToBeFarmed({.timesSown = field.crop.timesSown,
		                                                           .timesToSow = type.timesToSow,
		                                                           .age = field.crop.age,
		                                                           .ageGrowth = type.ageGrowth,
		                                                           .ageRecolt = type.ageRecolt,
		                                                           .farmers = static_cast<uint32_t>(field.farmers.size()),
		                                                           .maxFarmers = type.maxFarmerInFarm});
		const float score =
		    gutils::GetDistanceModifier(gutils::GetDistanceInMetres(glm::xz(at.position), here), k_FieldReach) * desire;
		if (score > bestScore)
		{
			bestScore = score;
			best = FieldChoice {.field = entity, .score = score};
		}
	});
	return best;
}

void FieldSystem::AddFarmer(entt::entity field, entt::entity villager)
{
	auto& farmers = Entities().Get<Field>(field).farmers;
	if (std::ranges::find(farmers, villager) == farmers.end())
	{
		farmers.push_back(villager);
	}
}

void FieldSystem::RemoveFarmer(entt::entity field, entt::entity villager)
{
	std::erase(Entities().Get<Field>(field).farmers, villager);
}

bool FieldSystem::PlantCrop(entt::entity field)
{
	auto& data = Entities().Get<Field>(field);
	return field_crop::PlantCrop(data.crop, TypeOf(data.type));
}

uint32_t FieldSystem::RemoveFood(entt::entity field, float amount)
{
	auto& data = Entities().Get<Field>(field);
	const auto& info = Locator::infoConstants::value().fieldType.at(static_cast<size_t>(data.type));
	const auto harvest = field_crop::RemoveFood(data.crop, TypeOf(data.type), info.ratioBeforeRipe, amount);
	if (harvest.emptied)
	{
		// An emptied field's fire goes out, and its town looks again at what it wants
		if (Locator::fireSystem::has_value())
		{
			Locator::fireSystem::value().SetTemperature(field, 0.0f, entt::null);
		}
		const auto& towns = Entities().Context().towns;
		if (const auto town = towns.find(static_cast<uint32_t>(data.town));
		    data.town >= 0 && town != towns.end() && Locator::townSystem::has_value())
		{
			Locator::townSystem::value().MarkDesiresChanged(town->second);
		}
	}
	return harvest.given;
}
