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

#include <optional>

#include <entt/entity/entity.hpp>

/// Which of the land's objects are drawn in less detail the further into the view they stand (see
/// graphics::mesh_detail), and which of those fade out and then go in the distance. Every object on the land is, by its
/// size: buildings, trees, features, fields, piles, animals, villagers. The creatures, the hand and the temple are always
/// drawn whole. Some things never fade: whatever a script made or keeps in view, the birds, the village centre, the ball,
/// the physical shield, a script's highlight, the vortex, and anything moving in the physics while it does.
#include "ECS/Registry.h"

namespace openblack::ecs::components
{
struct Animal;
struct Ball;
struct CarriedByTornado;
struct CreatureMorph;
struct DetailMeshes;
struct DrawImportance;
struct DrawnIntoVortex;
struct Hand;
struct HeldByCreature;
struct HighDetail;
struct InHand;
struct InPhysics;
struct IntroHand;
struct MagicShield;
struct NeverFades;
struct ScriptHighlight;
struct SeeThrough;
struct Temple;
struct TempleInteriorPart;
struct Translucent;
struct Vortex;
} // namespace openblack::ecs::components

namespace openblack::ecs::object_detail
{

/// Whether an object is drawn by the distance bands at all: what it is decides it, so this only changes when the
/// object's draw layout does (see k_ChangesDrawLayout)
[[nodiscard]] bool Banded(const Registry& registry, entt::entity entity);

/// Whether an object drawn by the bands is drawn by them this frame: not while a hand or a creature holds it, which
/// draws it whole, nor while a script draws it in high detail
[[nodiscard]] bool BandedNow(const Registry& registry, entt::entity entity);

/// Whether a mark on the land is drawn by the bands this frame: a mark that fades by its own alpha is drawn whole until
/// it has gone
[[nodiscard]] constexpr bool MarkBandedNow(std::optional<uint8_t> alpha)
{
	return !alpha.has_value();
}

/// Whether an object drawn by the bands fades out and then goes beyond them; one that doesn't is drawn as its low mesh
/// however far it is. `townCentre` says it is a village centre.
[[nodiscard]] bool Disappears(const Registry& registry, entt::entity entity, bool townCentre);

/// The importance of a thing that a vortex carries to the next land. The thing then keeps its high meshes further off.
inline constexpr float k_CarriedByVortexImportance = 10.0f;
/// The importance of a dead tree
inline constexpr float k_DeadTreeImportance = 1.0f;

/// How important it is to keep a thing in detail: its distance bands reach further by this much and one. A thing that a
/// vortex carries to the next land has the vortex importance. Another thing has the importance it got when it was made:
/// a villager's comes from its age. A thing with no importance has 0.
[[nodiscard]] float Importance(const Registry& registry, entt::entity entity);

/// The same questions for many things in one walk. It finds the stores of the components one time and gives the same
/// answers as the functions above while those stores exist. Make it again when the walk can make the first component of
/// one of these kinds.
class Stores
{
public:
	explicit Stores(const Registry& registry);
	[[nodiscard]] bool Banded(entt::entity entity) const;
	[[nodiscard]] bool BandedNow(entt::entity entity) const;
	[[nodiscard]] bool Disappears(entt::entity entity, bool townCentre) const;
	[[nodiscard]] float Importance(entt::entity entity) const;

private:
	[[nodiscard]] bool CarriedByVortex(entt::entity entity) const;

	ComponentLookup<components::CreatureMorph> _creatureMorph;
	ComponentLookup<components::Hand> _hand;
	ComponentLookup<components::IntroHand> _introHand;
	ComponentLookup<components::Temple> _temple;
	ComponentLookup<components::TempleInteriorPart> _templeInteriorPart;
	ComponentLookup<components::Translucent> _translucent;
	ComponentLookup<components::InHand> _inHand;
	ComponentLookup<components::HeldByCreature> _heldByCreature;
	ComponentLookup<components::HighDetail> _highDetail;
	ComponentLookup<components::SeeThrough> _seeThrough;
	ComponentLookup<components::NeverFades> _neverFades;
	ComponentLookup<components::InPhysics> _inPhysics;
	ComponentLookup<components::Ball> _ball;
	ComponentLookup<components::Vortex> _vortex;
	ComponentLookup<components::ScriptHighlight> _scriptHighlight;
	ComponentLookup<components::DrawnIntoVortex> _drawnIntoVortex;
	ComponentLookup<components::CarriedByTornado> _carriedByTornado;
	ComponentLookup<components::MagicShield> _magicShield;
	ComponentLookup<components::Animal> _animal;
	ComponentLookup<components::DetailMeshes> _detailMeshes;
	ComponentLookup<components::DrawImportance> _drawImportance;
};

} // namespace openblack::ecs::object_detail
