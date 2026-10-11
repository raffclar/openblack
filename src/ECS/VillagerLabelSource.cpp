/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerLabelSource.h"

#include <cstring>

#include <optional>
#include <string>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "Camera/Camera.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerLabel.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/VillagerAge.h"
#include "Gui/TextDatabase.h"
#include "InfoConstants.h"
#include "Locator.h"

namespace openblack::ecs::villager_label_source
{

namespace
{
namespace labels = gui::villager_labels;

/// The help texts the details are written with
constexpr uint32_t k_LifeText = 6111;
constexpr uint32_t k_FoodText = 6112;
constexpr uint32_t k_AgeText = 6113;
constexpr uint32_t k_PoisonedText = 6114;
constexpr uint32_t k_PregnantText = 6115;
/// The format of a state that has no text of its own
constexpr uint32_t k_DebugDetailsText = 6709;

/// Text from the tables, a byte a character
std::u16string FromTable(const char* text, size_t size)
{
	const auto length = strnlen(text, size);
	std::u16string wide(length, u'\0');
	for (size_t i = 0; i < length; ++i)
	{
		wide[i] = static_cast<char16_t>(static_cast<unsigned char>(text[i]));
	}
	return wide;
}

/// The colour of the player owning a town, none for a town of no player's or no town
std::optional<glm::u8vec3> OwnerColour(const Registry& registry, entt::entity town)
{
	if (town == entt::null || !registry.Valid(town))
	{
		return std::nullopt;
	}
	const auto* owned = registry.TryGet<const components::Town>(town);
	if (owned == nullptr || owned->owner == PlayerNames::NEUTRAL)
	{
		return std::nullopt;
	}
	const auto land = registry.Context().mapScriptGlobals.landNumber;
	const auto index = labels::PlayerColourIndex(static_cast<uint8_t>(owned->owner), land);
	const auto argb = components::Player::k_Colours.at(index & 7u);
	return glm::u8vec3((argb >> 16u) & 0xFFu, (argb >> 8u) & 0xFFu, argb & 0xFFu);
}
} // namespace

std::vector<labels::Request> Gather(const Frame& frame)
{
	using namespace components;
	std::vector<labels::Request> requests;
	if (frame.texts == nullptr || !Locator::camera::has_value() || !Locator::infoConstants::has_value() ||
	    !Locator::livingActionSystem::has_value() || frame.screen.x <= 0 || frame.screen.y <= 0)
	{
		return requests;
	}
	// No label is made while the cinema bars are in
	if (Locator::cinematicDirectorSystem::has_value() && Locator::cinematicDirectorSystem::value().IsWideScreenOn())
	{
		return requests;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& context = registry.Context();
	const auto& camera = Locator::camera::value();
	const auto& info = Locator::infoConstants::value();
	const auto& living = Locator::livingActionSystem::value();
	const auto& texts = *frame.texts;
	const auto eye = camera.GetOrigin();
	const auto viewport = glm::vec4(0.0f, 0.0f, glm::vec2(frame.screen));
	const labels::DetailWords words {.age = texts.GetHelpText(k_AgeText).text,
	                                 .life = texts.GetHelpText(k_LifeText).text,
	                                 .food = texts.GetHelpText(k_FoodText).text,
	                                 .pregnant = texts.GetHelpText(k_PregnantText).text,
	                                 .poisoned = texts.GetHelpText(k_PoisonedText).text};

	registry.Each<const Villager, const Transform, const LivingAction>(
	    [&](entt::entity entity, const Villager& villager, const Transform& transform, const LivingAction& action) {
		    const auto* named = registry.TryGet<const NamedVillager>(entity);
		    const auto choice = labels::Choose({
		        .named = named != nullptr,
		        .nameAllowed = !registry.AnyOf<InScript, NameHidden>(entity),
		        // Named villagers' speech bubbles aren't made yet
		        .speaking = false,
		        .detailsForAll = context.villagerDetailsShown,
		        .detailsForThis = registry.AllOf<DetailsShown>(entity),
		        .nameFade = frame.nameFade,
		    });
		    if (choice.kind == labels::Kind::None)
		    {
			    return;
		    }
		    // The point over its head, in the whole pixel it falls in on the screen
		    const auto point = transform.position + glm::vec3(0.0f, labels::k_HeadHeight, 0.0f);
		    glm::vec3 screen;
		    if (!camera.ProjectWorldToScreen(point, viewport, screen))
		    {
			    return;
		    }
		    const glm::ivec2 pixel {static_cast<int>(screen.x), static_cast<int>(screen.y)};
		    if (pixel.x < 0 || pixel.y < 0 || pixel.x >= frame.screen.x || pixel.y >= frame.screen.y)
		    {
			    return;
		    }
		    std::optional<std::u16string> name;
		    if (named != nullptr && choice.withName)
		    {
			    const auto& row = info.specialVillager.at(named->row);
			    name = FromTable(row.name.data(), row.name.size());
		    }
		    const auto owner = OwnerColour(registry, villager.town);
		    labels::Request request {.pixel = pixel, .distance = glm::distance(eye, point), .size = choice.size};
		    if (choice.kind == labels::Kind::Name)
		    {
			    request.text = *name;
			    request.colour = labels::LabelColour(owner, false);
		    }
		    else
		    {
			    const auto state = living.VillagerGetFinalState(action);
			    const auto& stateInfo = info.villagerStateTable.at(static_cast<size_t>(state));
			    std::optional<std::u16string_view> shownName;
			    if (name.has_value())
			    {
				    shownName = *name;
			    }
			    labels::Details details {.name = shownName,
			                             .age = villager_age::AgeNow(villager),
			                             .life = villager.life,
			                             .food = villager.food,
			                             // Pregnancy isn't simulated yet
			                             .pregnant = false,
			                             .poisoned = registry.AllOf<Poisoned>(entity)};
			    if (stateInfo.helpText != 0)
			    {
				    details.doing = texts.GetHelpText(stateInfo.helpText).text;
				    request.text = labels::DetailsText(details, words);
			    }
			    else
			    {
				    const auto stateName = FromTable(stateInfo.name.data(), stateInfo.name.size());
				    details.doing = stateName;
				    request.text = labels::DebugDetailsText(details, texts.GetHelpText(k_DebugDetailsText).text);
			    }
			    request.colour = labels::LabelColour(owner, villager.sex == Villager::Sex::FEMALE);
		    }
		    requests.push_back(std::move(request));
	    });
	return requests;
}

} // namespace openblack::ecs::villager_label_source
