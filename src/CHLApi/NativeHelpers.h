/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <LHVMTypes.h>
#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "CHLApi.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "Enums.h"
#include "ScriptHeaders/ScriptEnums.h"

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::components
{
struct HandClicked;
struct Storm;
struct Town;
} // namespace openblack::ecs::components

namespace openblack::ecs::systems
{
class ScriptMusicSystemInterface;
}

namespace openblack::help::spirits
{
class AdvisorSpiritController;
}

namespace openblack::script::switches
{
struct SwitchChanges;
}

namespace openblack::chlapi
{

// The helpers that the natives of more than one file use

// In CHLApi.cpp
void NotImplemented(std::optional<int32_t> detail = std::nullopt);
uint32_t CurrentTask();
lhvm::ScriptType TaskType(uint32_t task);
void ScriptMessage(std::string_view message);
void ObjectNoLongerValid();
bool PlayerInsideTemple();
void ApplySwitchChanges(const script::switches::SwitchChanges& changes);
glm::vec3 PopVec();
void PushVec(const glm::vec3& vec);
std::string PopString();
VMValue Pop(DataType& type);
VMValue Pop();
float Popf();
entt::entity PopObject();
bool IsDirectableVillager(entt::entity object);
void Pushv(float value);
void Pushb(bool value);
void RemakeDrawList();
std::optional<ecs::systems::HandSystemInterface::ReportedView> ScriptView();
bool IsWorldObject(entt::entity object);
bool IsLivingThing(entt::entity object);
float LivingAngleOf(entt::entity object, const ecs::components::Transform& transform);
void TurnLivingThing(entt::entity object, ecs::components::Transform& transform, float angle);
void TurnPlacedThing(entt::entity object, const glm::vec3& angles, bool lean);
float ScriptHeightOf(entt::entity object);
bool IsScriptContainer(const ecs::Registry& registry, entt::entity thing);
std::vector<entt::entity> ContainerMembers(const ecs::Registry& registry, entt::entity container);
PlayerNames ScriptPlayerName(int32_t scriptPlayer);

// In CHLApi/CameraNatives.cpp
void CountCameraOrder();

} // namespace openblack::chlapi
