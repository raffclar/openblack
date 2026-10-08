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

#include <entt/entity/entity.hpp>

// Object life in 0..1: villagers keep it in Villager::life, rocks and animals in components::Life, the
// rest have none yet (1). Used by the physics impacts, and by the miracles' effects (heal, fire, lightning, worship).
// Poison (components::Poisoned) is a life effect too, so it lives here: only the heal miracle cures it (the spell's
// default effect).

namespace openblack::ecs::life
{
/// The object's life (1 without a Life component)
[[nodiscard]] float LifeOf(entt::entity entity);

/// The villager's or the object's life set
void SetLife(entt::entity entity, float life);

/// Life - amount (the same for villagers and animals), or 0 when the amount is more
/// than the life. Returns the new life. It doesn't kill: that is the caller's (the dying states are not ported).
float ReduceLife(entt::entity entity, float amount);

/// Life + amount, up to 1 (the same for villagers). Returns the new life.
float IncreaseLife(entt::entity entity, float amount);

/// A villager's or an animal's death. TODO(physics): the corpse and the death states; the object goes.
void Kill(entt::entity entity, const char* reason);

// ---- poison --------------------------------------------------------------------------------------------------------

/// The living's poisoned bit, here components::Poisoned
[[nodiscard]] bool IsPoisoned(entt::entity entity);

/// The living's poisoned bit (a pot's own flag is components::Pot's).
/// Only a Living (villager or animal) can be poisoned.
void SetPoisoned(entt::entity entity, bool poisoned);

/// The two ways a villager gets poisoned in the original, both when the food *reaches it*, not when it is eaten:
/// a resource added to the villager (FOOD given with the poisoned argument, e.g. a poisoned pile put in its hands) and
/// a resource it takes from an object that is poisoned (a poisoned pot or storage pit). The eating side (the states,
/// the eat animation 0xD4 of eating food, at home or not) belongs to the villagers' code: call this from the place
/// where the poisoned food reaches the villager.
void TakePoisonedResource(entt::entity living);

/// The villager's hunger check: the life a villager loses on every periodic check when it is hungry *or* poisoned.
/// The amount is max(1 - food / hungryForFood, 1) x hungerToLifeMultiplier: it keeps the *bigger* of the two, so with
/// food clamped to >= 0 the food term is dead code and the loss is just hungerToLifeMultiplier.
[[nodiscard]] float HungerLifeLoss(float food, float hungryForFood, float hungerToLifeMultiplier);

/// The poison's harm over time, the half of the hunger check that does not need the hunger states: a poisoned
/// villager takes HungerLifeLoss with its own GVillagerInfo. Returns the life lost (0 when it is not poisoned).
/// Sleeping also skips the sleep's life recovery while it is poisoned (that state is not ported).
float ProcessPoison(entt::entity villager);

/// The poisoned tint (the draw of a Living and a villager): a poisoned Living with no specular colour of its own
/// (components::SpecularColour, which the heal chakra uses and which wins over the tint) is drawn with this diffuse,
/// ARGB (despite the original's name for it, it goes in the diffuse slot, the same one the white 0xFFFFFFFF uses).
constexpr uint32_t k_PoisonDiffuse = 0xFFE8FFDDU;
/// ... and this specular, added to the object's specular channel by channel with saturation. RenderingSystem
/// (DrawColoursOf) packs the pair for vs_object, the villagers' and the poisoned pots'.
constexpr uint32_t k_PoisonSpecular = 0xFF001000U;
} // namespace openblack::ecs::life
