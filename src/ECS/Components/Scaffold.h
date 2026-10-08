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

#include "3D/MapCoords.h"
#include "Enums.h"

namespace openblack
{
struct GAbodeInfo;
}

namespace openblack::ecs::components
{

/// One of the scaffold's two planned abodes (the current one and the candidate), made at the scaffold's position with
/// scale 1.0: a plan in no town list (Town::plannedAbodes never holds it). Only what the scaffold reads of it is kept.
/// (pending) how the original creates and initialises it is not read: its initialisation with a town is taken as "sets
/// its town"
struct ScaffoldPlan
{
	/// The GAbodeInfo the plan offers (null = no building: the scaffold has no plan)
	const GAbodeInfo* info {nullptr};
	/// The scaffold's position when the plan is made and when it tries to build it (the town's new planned building
	/// does not write it)
	map_coords::MapCoords position {};
	/// The Y angle (radians)
	float yAngle {0.0f};
	/// The plan's scale
	float scale {1.0f};
	/// False once it was deleted (the scaffold still points at it until it sets another one, as in the original)
	bool available {true};
};

/// A scaffold (a mobile object), its own state. ecs::scaffolds is its only writer.
/// The value (how many scaffolds it stands for) is the object's scale, the Transform's: value =
/// truncate((scale - 0.5f) * 5.0f + 0.5f), and setting value n sets scale = float(n) * 0.2f + 0.5f
/// (scaffolds::GetValue / SetValue). Not kept: a "next" link (always 0 here; the workshop update reads it: workshops'
/// business)
struct Scaffold
{
	// ---- flags (uint16) -------------------------------------------------------------------------------------------
	/// bit 0: the cross-fade phase (the phantom building's update, step 3 of the in-hand update)
	static constexpr uint16_t k_FadePhase = 0x1;
	/// bit 1: the hover test was done this hand turn
	static constexpr uint16_t k_HoverDone = 0x2;
	/// bits 2..5: the "no building here" feedback counter (the in-hand update)
	static constexpr uint16_t k_FeedbackMask = 0x3C;
	static constexpr uint16_t k_FeedbackStep = 0x4;
	/// bits 6..7: the workshop slot (set when the workshop finishes the scaffold, read by the tap split)
	static constexpr uint16_t k_SlotMask = 0xC0;
	static constexpr uint16_t k_SlotShift = 6;
	/// bit 8: the plan founds a new town (the plan choice's town-less branch; read when building)
	static constexpr uint16_t k_FoundsTown = 0x100;
	/// bit 9: no phantom rotation (the in-hand update). (pending) its setter: no write found in the original's
	/// scaffold code
	static constexpr uint16_t k_NoPhantomRotation = 0x200;
	/// bit 10: dropped gently (when it leaves the hand: landed and not "don't replant"); cleared when its physics ends
	static constexpr uint16_t k_DroppedGently = 0x400;
	/// bit 11: SET_SCAFFOLD_PROPERTIES' destroy: destroys the things in the way when it leaves the hand, and the `force`
	/// of building / choosing the plan ((flags >> 11) & 1)
	static constexpr uint16_t k_Destroy = 0x800;

	/// the cross-fade timer start and its step per hand turn
	static constexpr int32_t k_FadeStart = 400;
	static constexpr int32_t k_FadeStep = 100;
	/// `limit`: "any building"
	static constexpr int32_t k_AnyAbode = -1;

	/// The owner workshop (removing it from the workshop checks that it is one), or null
	entt::entity owner {entt::null};
	/// The BuildingSite it belongs to (set when the site adds it), or null
	entt::entity site {entt::null};
	/// The current plan (what the phantom shows). nullopt = a null pointer
	std::optional<ScaffoldPlan> plan;
	/// The candidate plan (the plan choice writes its info / angle / scale). nullopt = a null pointer
	std::optional<ScaffoldPlan> candidate;
	/// The phantom building's 3D object. Only its mesh is kept (set with the plan or the phantom's position, read by
	/// the in-hand update); 0 = no mesh. (not ported) the phantom's draw (RenderingSystem.cpp:180)
	entt::id_type phantomMesh {0};
	/// The cross-fade timer (400, -100 per hand turn)
	int32_t fade {0};
	/// The player who holds / dropped it (set when put in the hand; cleared when its physics ends)
	std::optional<PlayerNames> holder;
	/// The phantom's Y angle (radians) and its scale
	float phantomAngle {0.0f};
	float phantomScale {0.0f};
	/// The flags above (the slot in bits 6..7)
	uint16_t flags {0};
	/// Its town: the creation's argument 3, read by the combined-town lookup
	entt::entity town {entt::null};
	/// The only ABODE_NUMBER it may offer (SET_SCAFFOLD_PROPERTIES' type), -1 = any
	int32_t limit {k_AnyAbode};
	/// The game turn when it was placed (its physics ended, or a forced build); 0 while held and for a new one
	uint32_t placedTurn {0};
	/// (approximate) the interface that last dropped it is the local one (when building, the plant sound is 2D then):
	/// openblack has one local interface, so "the local hand let it go last" (scaffolds::OnLeftHand with PLAYER_ONE)
	bool lastDroppedByLocalHand {false};
};

} // namespace openblack::ecs::components
