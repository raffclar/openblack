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
#include <string>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "ECS/SuperVillagerEyes.h"

namespace openblack::graphics
{
struct SuperVillagerFrame;
}

/// The intro family's high detail models. SET_HIGH_GRAPHICS_DETAIL (op 290) makes a villager a SuperVillager (an
/// engine-side list): drawn by its own path with the HD mesh of Data\MISC\Intro, a 300 ms clip cross-fade and a second
/// yaw smoothing, and mesh eyes (ECS/SuperVillagerEyes.h). THING_JC_SPECIAL (op 349) sets its flags.
///
/// openblack draws the body with the villager entity itself (RenderingSystem, the swapped Mesh); the fade and the yaw
/// stage are parameters of the villager's smooth drawing (components::SkeletalAnimation::crossFade*, DrawPosition::
/// follow*, ECS/Animations.h and ECS/MobileDrawing.h), set here every frame.
///
/// They live only while a script holds the wide screen (the landscape's draw releases them otherwise).
namespace openblack::ecs::components
{
struct SuperVillager
{
	/// the eye type of op 290 (0 man, 1 woman or the animal trainer, 2 boy; -1 another mesh)
	int32_t eyeType {-1};
	/// bit 1 drawn at the intro hand's grip (feature 7), bit 2 no smoothing and no fade (8, 9, 16, 17)
	uint32_t flags {0};
	/// the HD mesh (0: none, the thing's own high mesh is drawn)
	entt::id_type hdMesh {0};
	/// the meshes put back by Release. openblack draws one: the drawn Mesh id (or the hidden one)
	entt::id_type savedMesh {0};
	/// the mesh eyes
	std::optional<super_villager::eyes::Eyes> eyes;
	/// this frame, the drawn mesh's box passed the on-screen test. Only then the fade, the yaw stage and the eyes run
	/// (their Random draws included)
	bool onScreen {true};
};
} // namespace openblack::ecs::components

namespace openblack::ecs::super_villager
{
constexpr uint32_t k_FollowHand = 1; ///< flags bit 1
constexpr uint32_t k_Snap = 2;       ///< flags bit 2

/// SetHighGraphicsDetail after its POPs: on, the HD mesh of the thing's high mesh (MeshPack 501 / 498 / 439 / 420 ->
/// nors_man / nors_woman / nors_boy / sable) and the thing joins the list; off, Release
void SetHighGraphicsDetail(entt::entity thing, bool on);
/// ThingJCSpecial after its POPs (object, feature, bool)
void ThingJcSpecial(entt::entity thing, int32_t feature, bool on);

/// False when the thing is not one. The eyes go whatever the thing's state (they are deleted unconditionally): the
/// list entry keeps their ids, so a thing destroyed while it is a SuperVillager (its component gone with it) leaves
/// nothing drawn
bool Release(entt::entity thing);
/// Every one Released and the list emptied (a map cleared, a script reboot, the script camera released, the landscape's
/// draw without a wide screen)
void ReleaseAll();
/// The list order: the newest first
[[nodiscard]] std::vector<entt::entity> List();

/// Once a frame before ECS/MobileDrawing: the landscape's release (no script wide screen: all go; a thing gone: it
/// goes), the swim rings (a swimmer, one clock for all, Random(0, 2 pi) every 1000 ms, ecs::AddWaterRing), the
/// on-screen test (SuperVillager::onScreen: region_on_screen::SphereOnScreen) and this frame's parameters of the
/// villagers' smooth drawing (frozen off screen)
void Update();
/// Once a frame right after ECS/MobileDrawing, before the poses and the carried props: the feature-7 position of the
/// driver loop (after the villagers are drawn)
void FollowHand();
/// Once a frame after the poses and after the fish shoals: each one on screen drawn, in list order; the others' eyes
/// hidden
void Draw(int32_t milliseconds);
/// What the draw needs of the list (Graphics/SuperVillagerFrame.h), filled once a frame before DrawScene (Game::Run,
/// next to FillOverlayFrame): the renderer reads only that copy, never the registry or the list.
/// - litByDefaultSun: every SuperVillager draw (the body and the eyes) is lit by the default sun
///   (model_light::k_DefaultSun), not by the frame's light: the HD
///   meshes of the list and the eye meshes (the renderer's draws of those meshes take model_light::ScopedLight).
///   (approximate) a SuperVillager drawn with its own high mesh (no HD file) shares it with the other villagers: that
///   mesh keeps the frame light;
/// - swimmers, in list order: the ones animated "M_P_Swim2", which the landscape's draw
///   cuts under the water.
/// The containers keep their capacity across frames; with no SuperVillager both are just cleared
void FillFrame(graphics::SuperVillagerFrame& out);

/// The intro hand's grip while the intro hand is shown (nullopt else): Game.cpp sets it once a frame, before
/// Update, from ecs::intro_special::Grip()
void SetIntroHandGrip(std::optional<glm::vec3> grip);

/// What an HD mesh file gives, loaded once into the mesh manager: op 290's HD bodies and the
/// intro hand (ecs/IntroSpecial.h)
struct HdModel
{
	entt::id_type mesh {0};
	/// the bone block and the skin, for the eyes and the hand's grip
	std::optional<eyes::HostModel> host;
};
/// Data\MISC\<relative>.l3d as the mesh "misc/<relative>" (once; null when the file cannot be read)
const HdModel* LoadHdModel(const std::string& relative, const std::string& name);
/// LoadHdModel's read, uncached: its mesh goes to the mesh cache; no mesh (0) when the file cannot be read
[[nodiscard]] HdModel ReadHdModel(const std::string& relative, const std::string& name);

namespace testing
{
/// (openblack) a SuperVillager on `thing` with six bare eye entities (no files, no meshes) of that type, linked at the
/// head of the list as SetHighGraphicsDetail does: for the tests of the list's bookkeeping
void Adopt(entt::entity thing, int32_t eyeType);
} // namespace testing

} // namespace openblack::ecs::super_villager
