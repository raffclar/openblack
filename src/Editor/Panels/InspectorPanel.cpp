/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "InspectorPanel.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <functional>
#include <string>
#include <string_view>

#include <fmt/format.h>
#include <glm/trigonometric.hpp>
#include <imgui.h>

#include "Debug/CreatureSpawner.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/Systems/EditorSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/VillagerAge.h"
#include "Editor/EditorActions.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorEntities.h"
#include "Editor/EditorMath.h"
#include "Editor/EditorStyle.h"
#include "Locator.h"

namespace openblack::editor
{

using namespace ecs::components;
using CameraMode = ecs::systems::EditorSystemInterface::CameraMode;

namespace
{
template <typename Enum, size_t N>
bool EnumCombo(const char* label, Enum& value, const std::array<std::string_view, N>& names)
{
	const auto index = static_cast<size_t>(value);
	bool changed = false;
	if (ImGui::BeginCombo(label, index < names.size() ? names.at(index).data() : "?"))
	{
		for (size_t i = 0; i < names.size(); ++i)
		{
			if (ImGui::Selectable(names.at(i).data(), i == index))
			{
				value = static_cast<Enum>(i);
				changed = true;
			}
		}
		ImGui::EndCombo();
	}
	return changed;
}

/// A component's name and a line about it, if the thing has it
struct ComponentView
{
	const char* name;
	std::function<std::optional<std::string>(const ecs::Registry&, entt::entity)> describe;
};

template <typename Component>
ComponentView View(const char* name, std::function<std::string(const Component&)> describe = {})
{
	return {name, [describe](const ecs::Registry& registry, entt::entity entity) -> std::optional<std::string> {
		        const auto* component = registry.TryGet<Component>(entity);
		        if (component == nullptr)
		        {
			        return std::nullopt;
		        }
		        return describe ? describe(*component) : std::string();
	        }};
}

const std::vector<ComponentView>& ComponentViews()
{
	static const std::vector<ComponentView> k_Views {
	    View<Mesh>("Mesh", [](const Mesh& mesh) { return fmt::format("mesh {:08x}, submesh {}", mesh.id, mesh.submeshId); }),
	    View<Fixed>("Fixed",
	                [](const Fixed& fixed) {
		                return fmt::format("obstacle of radius {:.1f} at {:.0f}, {:.0f}", fixed.boundingRadius,
		                                   fixed.boundingCenter.x, fixed.boundingCenter.y);
	                }),
	    View<Mobile>("Mobile"),
	    View<Creature>("Creature",
	                   [](const Creature& creature) {
		                   return fmt::format("{}, alignment {:+.2f}, size {:.2f}", SpeciesName(creature.species),
		                                      creature.alignment, creature.size);
	                   }),
	    View<CreatureLocomotion>("Creature movement",
	                             [](const CreatureLocomotion& locomotion) {
		                             return fmt::format("speed {:.1f}, heading {:.0f} degrees", locomotion.speed,
		                                                glm::degrees(locomotion.heading.Radians()));
	                             }),
	    View<CreatureMindState>("Creature mind",
	                            [](const CreatureMindState& mind) {
		                            return fmt::format("{}grown up to stage {}", mind.paused ? "paused, " : "",
		                                               mind.developmentPhase);
	                            }),
	    View<Villager>("Villager",
	                   [](const Villager& villager) {
		                   return fmt::format("age {}, life {:.2f}, food {:.2f}", ecs::villager_age::AgeNow(villager),
		                                      villager.life, villager.food);
	                   }),
	    View<LivingAction>(
	        "Living action",
	        [](const LivingAction& action) { return fmt::format("{} turns in its state", action.turnsSinceStateChange); }),
	    View<Abode>("Building",
	                [](const Abode& abode) {
		                return fmt::format("town {}, food {}, wood {}, {} living there", abode.townId, abode.foodAmount,
		                                   abode.woodAmount, abode.inhabitants.size());
	                }),
	    View<StoragePit>("Storage pit"),
	    View<Town>("Town",
	               [](const Town& town) {
		               return fmt::format("id {}, player {}, {} homeless", town.id, static_cast<int>(town.owner) + 1,
		                                  town.homelessVillagers.size());
	               }),
	    View<WorshipSite>("Worship site",
	                      [](const WorshipSite& site) {
		                      return fmt::format("player {}, place {}, {} towns, {} worshippers",
		                                         static_cast<int>(site.player) + 1, site.place, site.towns.size(),
		                                         site.worshippers.size());
	                      }),
	    View<Tree>("Tree",
	               [](const Tree& tree) {
		               return fmt::format("type {}, grows to {:.2f}", static_cast<int>(tree.type), tree.maxSize);
	               }),
	    View<Feature>("Feature", [](const Feature& feature) { return fmt::format("type {}", static_cast<int>(feature.type)); }),
	    View<MobileObject>("Mobile object",
	                       [](const MobileObject& object) { return fmt::format("type {}", static_cast<int>(object.type)); }),
	    View<MobileStatic>("Mobile static",
	                       [](const MobileStatic& object) { return fmt::format("type {}", static_cast<int>(object.type)); }),
	    View<Field>("Field", [](const Field& field) { return fmt::format("town {}", field.town); }),
	    View<Pot>("Store", [](const Pot& pot) { return fmt::format("{} of {}", pot.amount, pot.maxAmount); }),
	};
	return k_Views;
}
} // namespace

void InspectorPanel::Draw(EditorContext& context) noexcept
{
	const auto selected = context.system.GetSelection().Get();
	if (!selected.has_value() || !context.registry.Valid(*selected))
	{
		ImGui::TextDisabled("Nothing picked");
		ImGui::TextWrapped("Click something on the land or in the outliner to pick it.");
		return;
	}
	const auto entity = *selected;
	DrawHeader(context, entity);
	if (!context.registry.Valid(entity))
	{
		return;
	}
	DrawTransform(context, entity);
	if (context.registry.AllOf<Creature>(entity))
	{
		if (ImGui::CollapsingHeader("Creature", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::PushID("Creature");
			context.spawner.DrawCreature(entity);
			ImGui::PopID();
		}
		if (ImGui::CollapsingHeader("Every creature"))
		{
			context.spawner.DrawSharedSettings();
		}
	}
	if (context.registry.AllOf<Villager, LivingAction>(entity))
	{
		DrawVillager(context, entity);
	}
	DrawComponents(context, entity);
}

void InspectorPanel::DrawHeader(EditorContext& context, entt::entity entity) noexcept
{
	ImGui::TextColored(style::k_Accent, "%s", LabelOf(context.registry, context.names, entity).c_str());
	ImGui::TextColored(style::k_Muted, "%s, entity %u", Name(KindOf(context.registry, entity)).data(),
	                   entt::to_integral(entity));
	if (ImGui::Button("View"))
	{
		context.system.FrameSelection();
	}
	ImGui::SetItemTooltip("Bring it into view (Home)");
	ImGui::SameLine();
	const auto mode = context.system.GetCameraMode();
	if (ImGui::Button(mode == CameraMode::Orbit ? "Stop orbiting" : "Orbit"))
	{
		context.system.SetCameraMode(mode == CameraMode::Orbit ? CameraMode::Free : CameraMode::Orbit);
	}
	ImGui::SameLine();
	if (ImGui::Button(mode == CameraMode::Follow ? "Stop following" : "Follow"))
	{
		context.system.SetCameraMode(mode == CameraMode::Follow ? CameraMode::Free : CameraMode::Follow);
	}
	ImGui::SameLine();
	const auto& selection = context.system.GetSelection();
	const auto picks = selection.Count();
	ImGui::BeginDisabled(std::ranges::none_of(selection.All(), [&context](entt::entity picked) {
		return context.registry.Valid(picked) && ItemOf(context.registry, picked).has_value();
	}));
	if (ImGui::Button("Duplicate"))
	{
		DuplicatePicked(context);
	}
	ImGui::EndDisabled();
	ImGui::SetItemTooltip(picks > 1 ? "A copy beside each picked thing (Ctrl+D)" : "A copy beside it (Ctrl+D)");
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.20f, 0.20f, 1.0f));
	if (ImGui::Button("Delete"))
	{
		DeletePicked(context);
	}
	ImGui::PopStyleColor();
	ImGui::SetItemTooltip(picks > 1 ? "Remove all the picked things from the land (Delete). Undo makes them again "
	                                  "as the palette makes them."
	                                : "Remove it from the land (Delete). Undo makes it again as the palette makes it.");
	if (picks > 1)
	{
		ImGui::TextColored(style::k_Muted, "%zu things picked: the sections below show the main pick", picks);
	}
}

void InspectorPanel::DrawTransform(EditorContext& context, entt::entity entity) noexcept
{
	auto* transform = context.registry.TryGet<Transform>(entity);
	if (transform == nullptr || !ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}
	const std::array one {entity};
	// Each change of a field becomes one step of the history when the field lets go of the mouse or the keys. A field
	// can change the pose in the frame that it starts, so the pose from before the field is the one to keep.
	PoseEdit before;
	before.Begin(context.registry, one);
	const auto track = [&](std::string_view action) {
		if (ImGui::IsItemActivated())
		{
			_edit = before;
		}
		if (ImGui::IsItemDeactivated() && _edit.Active())
		{
			_edit.Finish(context, action);
		}
		before.Begin(context.registry, one);
	};
	auto position = transform->position;
	if (ImGui::DragFloat3("Position", &position.x, 0.25f, 0.0f, 0.0f, "%.1f"))
	{
		MoveTo(entity, position);
	}
	track("Move");
	ImGui::SameLine();
	if (ImGui::SmallButton("Ground"))
	{
		_edit.Begin(context.registry, one);
		MoveTo(entity, {position.x, LandHeight({position.x, position.z}), position.z});
		_edit.Finish(context, "Ground");
		before.Begin(context.registry, one);
	}
	ImGui::SetItemTooltip("Puts it on the land");
	transform = context.registry.TryGet<Transform>(entity);
	if (transform == nullptr)
	{
		return;
	}
	auto yaw = glm::degrees(YawOf(transform->rotation));
	if (ImGui::SliderFloat("Turn", &yaw, -180.0f, 180.0f, "%.0f degrees"))
	{
		Turn(entity, glm::radians(yaw) - YawOf(transform->rotation));
	}
	track("Turn");
	auto scale = transform->scale.x;
	if (ImGui::DragFloat("Scale", &scale, 0.01f, 0.05f, 50.0f, "%.2f"))
	{
		const auto ratio = scale / std::max(transform->scale.x, 0.0001f);
		transform->scale *= ratio;
		context.registry.SetDirty();
	}
	track("Scale");
	if (const auto bounds = WorldBoundsOf(context.registry, entity))
	{
		const auto size = bounds->Size();
		ImGui::TextColored(style::k_Muted, "%.1f wide, %.1f tall, %.1f deep", static_cast<double>(size.x),
		                   static_cast<double>(size.y), static_cast<double>(size.z));
	}
}

void InspectorPanel::DrawVillager(EditorContext& context, entt::entity entity) noexcept
{
	if (!ImGui::CollapsingHeader("Villager", ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}
	auto& villager = context.registry.Get<Villager>(entity);
	auto& action = context.registry.Get<LivingAction>(entity);
	if (villager.abode == entt::null)
	{
		ImGui::TextColored(style::k_Warning, "Homeless");
	}
	ImGui::SliderFloat("Life", &villager.life, 0.0f, 1.0f);
	if (auto age = ecs::villager_age::AgeNow(villager); ImGui::InputScalar("Age", ImGuiDataType_U32, &age))
	{
		ecs::villager_age::SetBirthTurnForAge(villager, age);
	}
	ImGui::InputFloat("Food", &villager.food);
	EnumCombo("Life stage", villager.lifeStage, Villager::k_LifeStageStrs);
	EnumCombo("Sex", villager.sex, Villager::k_SexStrs);
	EnumCombo("Tribe", villager.tribe, k_TribeStrs);
	EnumCombo("Role", villager.number, k_VillagerNumberStrs);
	if (!Locator::livingActionSystem::has_value())
	{
		return;
	}
	auto& actions = Locator::livingActionSystem::value();
	ImGui::SeparatorText("States");
	for (size_t index = 0; index < LivingAction::k_IndexStrings.size(); ++index)
	{
		const auto which = static_cast<LivingAction::Index>(index);
		auto state = actions.VillagerGetState(action, which);
		if (EnumCombo(LivingAction::k_IndexStrings.at(index).data(), state, k_VillagerStateStrings))
		{
			actions.VillagerSetState(action, which, state, true);
		}
	}
	ImGui::Text("%u turns in the state, %u until it changes", static_cast<unsigned>(action.turnsSinceStateChange),
	            static_cast<unsigned>(action.turnsUntilStateChange));
}

void InspectorPanel::DrawComponents(EditorContext& context, entt::entity entity) noexcept
{
	if (!ImGui::CollapsingHeader("Components"))
	{
		return;
	}
	if (!ImGui::BeginTable("Components", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
	{
		return;
	}
	for (const auto& view : ComponentViews())
	{
		if (const auto line = view.describe(context.registry, entity))
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(view.name);
			ImGui::TableNextColumn();
			ImGui::TextColored(style::k_Muted, "%s", line->c_str());
		}
	}
	ImGui::EndTable();
}

} // namespace openblack::editor
