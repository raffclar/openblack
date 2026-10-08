/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SuperVillager.h"

#include <algorithm>
#include <bit>
#include <exception>
#include <map>
#include <stdexcept>
#include <string>

#include <L3DFile.h>
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>

#include "3D/AffineMatrix.h"
#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "Camera/FieldOfView.h"
#include "Common/GameRandom.h"
#include "ECS/Components/DrawPosition.h"
#include "ECS/Components/DynamicShadow.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/IntroSpecial.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Registry.h"
#include "ECS/Systems/WorldEffectsInterface.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/VillagerSpeed.h"
#include "ECS/WaterRings.h"
#include "FileSystem/FileSystemInterface.h"
#include "GameClock.h"
#include "Graphics/RegionOnScreen.h"
#include "Graphics/SuperVillagerFrame.h"
#include "Help/HelpSystem.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::ecs::super_villager
{
using namespace components;

namespace
{
constexpr int32_t k_FadeMs = 300; ///< the clip cross-fade's length
/// The drawn yaw's turn rate, 5 pi / 4 rad/s
constexpr float k_TurnRate = std::bit_cast<float>(0x407B53D2u);

/// One super villager in the list: the thing and the ids of its eyes' six objects, kept here so that
/// Release can delete them even when the thing (and its SuperVillager component) is gone
struct Entry
{
	entt::entity thing {entt::null};
	eyes::Objects eyes {entt::null, entt::null, entt::null, entt::null, entt::null, entt::null};
};
/// What this module keeps between calls (Locator::worldEffects)
struct SuperVillagerState
{
	/// The super villagers, newest first: a new one is linked at the head
	std::vector<Entry> list {};
	/// The eyes' shared objects, never reset, as in the original
	eyes::Shared shared {};
	/// The grip point while the intro's special plays: set each frame from ECS/IntroSpecial (Game.cpp)
	std::optional<glm::vec3> handGrip {};
	/// The swim rings' clock, one for every swimmer, read and written only here
	int32_t swimRingMs {0};
};

SuperVillagerState& SuperVillagerData()
{
	return openblack::Locator::worldEffects::value().Get<SuperVillagerState>();
}
constexpr int32_t k_SwimRingEveryMs = 1000;                  ///< a swim ring every second
constexpr float k_TwoPi = std::bit_cast<float>(0x40C90FDBu); ///< the ring's random turn goes up to 2 pi
/// The swim ring is made under bone 5 (its x and z in the bone buffer)
constexpr size_t k_SwimRingBone = 5;

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

} // namespace

HdModel ReadHdModel(const std::string& relative, const std::string& name)
{
	try
	{
		auto& fileSystem = Locator::filesystem::value();
		const auto& bytes = resources::LoadBlob(Locator::resources::value().GetBlobs(),
		                                        fileSystem.GetPath<filesystem::Path::Misc>() / (relative + ".l3d"));
		l3d::L3DFile file;
		if (file.Open(bytes) != l3d::L3DResult::Success)
		{
			throw std::runtime_error("not an L3D");
		}
		HdModel model;
		model.mesh = resources::HashIdentifier("misc/" + relative);
		auto& meshes = Locator::resources::value().GetMeshes();
		if (!meshes.Contains(model.mesh))
		{
			// (pending: engine::gpu::Submit or preload) the mesh's bgfx buffers are made here, from op 290
			// during the turn (game logic), not before the game or through the GPU queue
			meshes.Load(model.mesh, resources::L3DLoader::FromBufferTag {}, name, bytes);
		}
		const bool hasEBone =
		    (static_cast<uint32_t>(file.GetHeader().flags) & static_cast<uint32_t>(l3d::L3DMeshFlags::ContainsEBone)) != 0;
		if (const auto& eBone = file.GetEBone(); hasEBone && eBone.has_value())
		{
			eyes::HostModel host {
			    .name = name,
			    .matrices = {eBone->matrices[0], eBone->matrices[1]},
			    .bones = {eBone->bones[0], eBone->bones[1]},
			};
			if (eyes::l3d_patch::FirstSkinId(bytes, host.skinId))
			{
				host.skin = eyes::l3d_patch::FindSkin(bytes, host.skinId);
			}
			model.host = std::move(host);
		}
		return model;
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "SuperVillager: cannot load {}: {}", relative, e.what());
		return {};
	}
}

const HdModel* LoadHdModel(const std::string& relative, const std::string& name)
{
	// loaded once into the HD model cache; one that cannot be read is cached with no mesh, so it is tried once
	auto& models = Locator::resources::value().GetHdModels();
	const auto id = entt::hashed_string(("hdmodel/" + relative).c_str()).value();
	if (!models.Contains(id))
	{
		models.Load(id, resources::HdModelLoader::FromDiskTag {}, relative, name);
	}
	const auto& model = *models.Handle(id);
	return model.mesh != 0 ? &model : nullptr;
}

namespace
{
/// The high detail mesh the op compares: the info.dat high mesh, the child one for a child (as a villager's age
/// sets it). (approximate) only villagers: other things keep their mesh
std::optional<MeshId> HighMesh(entt::entity thing)
{
	const bool isVillager = Entities().AllOf<Villager>(thing);
	const auto* info = isVillager ? VillagerInfoOf(thing) : nullptr;
	if (info == nullptr)
	{
		return std::nullopt;
	}
	return villager::IsChild(thing) ? info->childMeshHigh : info->highDetail;
}

/// openblack's one drawn mesh, or the one kept while the villager is hidden (SkeletalAnimation::hiddenMesh)
entt::id_type* DrawnMesh(entt::entity thing)
{
	auto& registry = Entities();
	if (auto* mesh = registry.TryGet<Mesh>(thing); mesh != nullptr)
	{
		return &mesh->id;
	}
	if (auto* animation = registry.TryGet<SkeletalAnimation>(thing); animation != nullptr && animation->hiddenMesh != 0)
	{
		return &animation->hiddenMesh;
	}
	return nullptr;
}

/// The release's part on the drawing object: the cross-fade state cleared, and the smooth-drawing parameters off
void ClearSmoothing(entt::entity thing)
{
	auto& registry = Entities();
	if (auto* animation = registry.TryGet<SkeletalAnimation>(thing); animation != nullptr)
	{
		animation->crossFadeMs = 0;
		animation->crossFadeFrozen = false;
		animation->crossFade = {};
		animation->drawnPose.clear();
	}
	if (auto* draw = registry.TryGet<DrawPosition>(thing); draw != nullptr)
	{
		draw->followRate = 0.0f;
		draw->followSnap = false;
		draw->followTurn = true;
		draw->followFrozen = false;
		draw->hasFollowYaw = false;
		draw->followDrawnTurn = 0.0f;
	}
}

/// Makes the thing a super villager: nothing when it is one already (its HD mesh stays loaded, as the original leaks it)
void Create(entt::entity thing, int32_t eyeType, const HdModel* model, std::optional<MeshId> high)
{
	auto& state = SuperVillagerData();
	auto& registry = Entities();
	if (registry.AllOf<SuperVillager>(thing))
	{
		return;
	}
	auto& super = registry.Assign<SuperVillager>(thing);
	state.list.insert(state.list.begin(), Entry {thing}); // linked at the head
	super.eyeType = eyeType;
	// Its temporary shadow: a projected shadow lit by the fixed sun, falling on the land only, updated once a frame by
	// the landscape draw (the op then marks it so that the shadows' own update skips it). Making it also turns off the
	// thing's ground blobs (SuperVillagerFrame::notHumanShadowed) and its fade with the distance (openblack fades no
	// object with the distance yet; a port of it must leave the SuperVillagers out). A villager has
	// no DynamicShadow of its own (only the launched boat does). (approximate) made by graphics::shadow_list's
	// DynamicShadow producer with the boat, not from its own list inside the landscape draw: the order of the
	// projected shadows can differ from the original's where they overlap
	registry.Assign<DynamicShadow>(thing, DynamicShadow {.onObjects = false, .useSun = true});
	// The smoothed yaw starts at the yaw the object was drawn with last (in the drawing object's convention:
	// ECS/MobileDrawing's drawn yaw + 90 degrees). (openblack) not drawn yet (no
	// drawn yaw): UpdateMobileDrawing takes it on its first followed frame
	if (auto* draw = registry.TryGet<DrawPosition>(thing); draw != nullptr && draw->hasYaw)
	{
		draw->followYaw = draw->yaw + glm::half_pi<float>();
		draw->hasFollowYaw = true;
	}
	// The three meshes are saved, then the HD mesh, if any, goes into the high detail slot. The
	// SuperVillager draws the high slot: the HD mesh, or the thing's own high mesh
	super.hdMesh = model != nullptr ? model->mesh : 0;
	const entt::id_type drawn = super.hdMesh != 0 ? super.hdMesh : high.has_value() ? resources::HashIdentifier(*high) : 0;
	if (auto* mesh = DrawnMesh(thing); mesh != nullptr && drawn != 0)
	{
		super.savedMesh = *mesh;
		*mesh = drawn;
	}
	// Eyes when the high slot's mesh has an EBone block and the type is not -1
	if (model != nullptr && model->host.has_value() && eyeType != -1)
	{
		super.eyes = eyes::Create(thing, eyeType, *model->host);
		state.list.front().eyes = super.eyes->objects;
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "SuperVillager: {} (eye type {}, {})", static_cast<uint32_t>(thing), eyeType,
	                   model != nullptr ? "HD mesh" : "its high mesh");
}

/// The release's part on a thing still alive (the eyes and the list entry are the caller's)
void Restore(entt::entity thing)
{
	auto& registry = Entities();
	auto* super = registry.TryGet<SuperVillager>(thing);
	if (super == nullptr)
	{
		return;
	}
	// the three meshes back
	if (auto* mesh = DrawnMesh(thing); mesh != nullptr && super->savedMesh != 0)
	{
		*mesh = super->savedMesh;
	}
	// the temporary shadow freed and the two flags it saved given back (the ground blobs and the fade with the
	// distance)
	registry.Remove<DynamicShadow>(thing);
	ClearSmoothing(thing);
	registry.Remove<SuperVillager>(thing);
}

/// Releases one entry already out of the list: the eyes are deleted whatever the
/// thing's state; the rest only on a thing still alive
void ReleaseEntry(Entry& entry)
{
	eyes::Destroy(entry.eyes);
	if (Locator::entitiesRegistry::has_value() && Entities().Valid(entry.thing))
	{
		Restore(entry.thing);
	}
}

/// The on-screen test: the drawn mesh's bounding sphere under the drawing object, through
/// graphics::region_on_screen::SphereOnScreen, the faithful port of the original's region test.
/// centre = the box's centre ((min + max) / 2 of the rest-posed vertices of every submesh =
/// AxisAlignedBoundingBox::Center) through the object's matrix; radius = the object's scale x the box's half diagonal
/// (|max - min| / 2); origin = the matrix's translation. (approximate) the drawn matrix of the frame before (Update runs
/// before ECS/MobileDrawing; the original tests the matrix set by this frame's villager draw); the original test's side
/// effects (the last on-screen flag, selected box and distance) and its other path are not ported
bool RegionOnScreen(entt::entity thing)
{
	auto& registry = Entities();
	const auto* mesh = DrawnMesh(thing);
	field_of_view::View view;
	if (mesh == nullptr || !registry.AllOf<Transform>(thing) || !Locator::resources::has_value() ||
	    !field_of_view::CurrentView(view))
	{
		return true;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(*mesh))
	{
		return true;
	}
	const auto box = meshes.Handle(*mesh)->GetBoundingBox();
	const auto model = DrawnModel(registry, thing);
	const auto& scale = registry.Get<const Transform>(thing).scale;
	// the box centre in the original's order; (approximate) the cells are DrawnModel's
	const glm::vec3 centre = affine::BoxCentreThroughObject(affine::FromModel(model), box.Center());
	const float radius = std::max({scale.x, scale.y, scale.z}) * (glm::length(box.Size()) * 0.5f);
	return graphics::region_on_screen::SphereOnScreen(view, centre, radius, glm::vec3(model[3]));
}

/// For each swimmer, after its under-water draw (off screen too: the on-screen test comes later): the shared clock +=
/// the frame's game time; past 1000 ms (signed compare) it restarts at 0 (not -1000) and a ring is made at (x, 0.05, z)
/// of bone 5 in the bone buffer, turned by Random(0, 2 pi) (the CRT stream, drawn before the pool is searched: a full
/// pool still takes the draw). Ring = age 0, growth 1, the angle, an unnamed field 1, aspect 1, rate 1, cell 0x30,
/// colour 0xFFFFFFFF (white, not the land light); one field left as the slot had it. (approximate) the buffer holds the
/// last pose made before this point of the frame (the body is posed later): here the swimmer's own pose, drawn last
/// frame
void SwimRing(const Registry& registry, entt::entity thing, const SkeletalAnimation& animation)
{
	auto& state = SuperVillagerData();
	state.swimRingMs += static_cast<int32_t>(game_clock::FrameGameMs()); // the frame's game ms
	if (state.swimRingMs <= k_SwimRingEveryMs)
	{
		return;
	}
	state.swimRingMs = 0;
	const auto model = DrawnModel(registry, thing);
	const glm::vec3 bone =
	    animation.pose.size() > k_SwimRingBone ? glm::vec3(model * animation.pose[k_SwimRingBone][3]) : glm::vec3(model[3]);
	const float angle = game_random::crt::Random(0.0f, k_TwoPi);
	const WaterRing ring {.position = glm::vec3(bone.x, 0.05f, bone.z), // 0x3D4CCCCD
	                      .age = 0,
	                      .growth = 1.0f,
	                      .angle = angle,
	                      .aspect = 1.0f,
	                      .rate = 1.0f,
	                      .cell = 0x30,
	                      .argb = 0xFFFFFFFFu};
	AddWaterRing(ring); // 1024 slots; full: no ring, the clock and the draw already spent
}

/// Sets the thing's heading through villager::SetYAngle, the one copy (ECS/Villager/VillagerScript.h)
void SetYAngle(entt::entity thing, float angle)
{
	auto& registry = Entities();
	auto* wallHug = registry.TryGet<WallHug>(thing);
	auto* transform = registry.TryGet<Transform>(thing);
	if (wallHug != nullptr && transform != nullptr)
	{
		villager::SetYAngle(*transform, *wallHug, angle);
	}
}

/// The thing's heading: WallHug::yAngle here
float GetYAngle(entt::entity thing)
{
	const auto* wallHug = Entities().TryGet<const WallHug>(thing);
	return wallHug != nullptr ? wallHug->yAngle : 0.0f;
}
} // namespace

void SetHighGraphicsDetail(entt::entity thing, bool on)
{
	if (!on)
	{
		// off: the normal draw back, the landscape draw list rebuilt, then the release
		Release(thing);
		return;
	}
	// on: the thing's normal draw is turned off (here the entity is the body itself)
	// the high slot against MeshPack[501], [498], [439], [420] in that order
	const auto high = HighMesh(thing);
	int32_t eyeType = -1;
	const HdModel* model = nullptr;
	if (high == MeshId::PersonNorseMaleA1)
	{
		eyeType = 0;
		model = LoadHdModel("Intro/nors_man", "nors_man"); // the father
	}
	else if (high == MeshId::PersonNorseFemaleA1)
	{
		eyeType = 1;
		model = LoadHdModel("Intro/nors_woman", "nors_woman"); // the mother
	}
	else if (high == MeshId::PersonBoyWhite1)
	{
		eyeType = 2;
		model = LoadHdModel("Intro/nors_boy", "nors_boy"); // the son
	}
	else if (high == MeshId::PersonAnimalTrainer)
	{
		eyeType = 1;
		model = LoadHdModel("sable", "sable"); // the creature trainer (no EBone block: no eyes)
	}
	Create(thing, eyeType, model, high);
	// the op then marks the shadow as updated by the landscape draw and rebuilds the landscape draw list (openblack has no
	// such list)
}

void ThingJcSpecial(entt::entity thing, int32_t feature, bool on)
{
	auto& registry = Entities();
	// the thing must be one of the list, except for feature 19
	auto* super = registry.TryGet<SuperVillager>(thing);
	if (super == nullptr && feature != 19)
	{
		return;
	}
	switch (feature) // a jump table over features 7..19
	{
	case 7: // drawn at the intro hand's grip (by the landscape draw)
		super->flags |= k_FollowHand;
		break;
	case 8: // on, the smoothing and the fade back; off, none
		super->flags = on ? super->flags & ~k_Snap : super->flags | k_Snap;
		break;
	case 9: // SetYAngle(-GetYAngle()) (the heading mirrored, not turned round)
		SetYAngle(thing, -GetYAngle(thing));
		super->flags |= k_Snap;
		break;
	case 16: // + pi/2
		SetYAngle(thing, GetYAngle(thing) + glm::half_pi<float>());
		super->flags |= k_Snap;
		break;
	case 17: // - pi/2
		SetYAngle(thing, GetYAngle(thing) - glm::half_pi<float>());
		super->flags |= k_Snap;
		break;
	case 18: // the intro's special released (ecs/IntroSpecial.h; its on flag stays, so does the grip)
		intro_special::ReleaseAll();
		break;
	case 19: // the thing's fade with the distance turned off. The original fades such an object out with the
	         // distance (its alpha = 255 x the part left before the last distance, then drawn with the global alpha),
	         // so the thing is no longer faded by the distance. Nothing to port while
	         // openblack fades no object with the distance; a port of that fade must leave this thing out
		break;
	default: // 10..15 and outside 7..19: nothing
		break;
	}
}

bool Release(entt::entity thing)
{
	auto& state = SuperVillagerData();
	// the entry whose drawing object it is, if any
	const auto it = std::ranges::find(state.list, thing, &Entry::thing);
	if (it == state.list.end())
	{
		return false;
	}
	auto entry = *it;
	state.list.erase(it);
	ReleaseEntry(entry);
	return true;
}

void ReleaseAll()
{
	auto& state = SuperVillagerData();
	auto all = std::move(state.list);
	state.list.clear();
	for (auto& entry : all)
	{
		ReleaseEntry(entry);
	}
}

std::vector<entt::entity> List()
{
	auto& state = SuperVillagerData();
	std::vector<entt::entity> things;
	things.reserve(state.list.size());
	for (const auto& entry : state.list)
	{
		things.push_back(entry.thing);
	}
	return things;
}

void Update()
{
	if (SuperVillagerData().list.empty())
	{
		return;
	}
	// without a script's wide screen every one goes, every frame
	const auto* helpSystem = help::Get();
	if (helpSystem == nullptr || !helpSystem->IsScriptWideScreen())
	{
		ReleaseAll();
		return;
	}
	auto& registry = Entities();
	for (const auto thing : List())
	{
		// the thing gone (no longer available; (inferred) a destroyed entity here):
		// Release, its eyes deleted with it
		if (!registry.Valid(thing) || !registry.AllOf<SuperVillager>(thing))
		{
			Release(thing);
			continue;
		}
		auto& super = registry.Get<SuperVillager>(thing);
		const bool snap = (super.flags & k_Snap) != 0;
		// the clip "M_P_Swim2" -> the cut draw (renderer) and the swim ring
		if (const auto* animation = registry.TryGet<const SkeletalAnimation>(thing);
		    animation != nullptr && animation->hasClip && animation->clipIndex == static_cast<int32_t>(AnimId::PSwim2))
		{
			SwimRing(registry, thing, *animation);
		}
		// off screen the smooth drawing does not run at all this frame
		super.onScreen = RegionOnScreen(thing);
		// the 300 ms fade, not drawn with bit 2 (DrawPosition::followSnap,
		// the one field of the bit for both stages)
		if (auto* animation = registry.TryGet<SkeletalAnimation>(thing); animation != nullptr)
		{
			animation->crossFadeMs = k_FadeMs;
			animation->crossFadeFrozen = !super.onScreen;
		}
		// the yaw over the villager's drawn one, not with bit 2; the swim branch
		// poses with the object's own matrix, unturned. (approximate) the swim test on the clip set last
		// frame, and a fade while swimming also turns it in the original (the fade's bit 2 test comes first)
		if (auto* draw = registry.TryGet<DrawPosition>(thing); draw != nullptr)
		{
			const auto* animation = registry.TryGet<const SkeletalAnimation>(thing);
			draw->followRate = k_TurnRate;
			draw->followSnap = snap;
			draw->followFrozen = !super.onScreen;
			draw->followTurn = animation == nullptr || animation->clipIndex != static_cast<int32_t>(AnimId::PSwim2);
		}
	}
}

void FollowHand()
{
	auto& superVillagerState = SuperVillagerData();
	if (superVillagerState.list.empty() || !superVillagerState.handGrip.has_value())
	{
		return;
	}
	auto& registry = Entities();
	for (const auto thing : List())
	{
		const auto* super = registry.Valid(thing) ? registry.TryGet<const SuperVillager>(thing) : nullptr;
		if (super == nullptr || (super->flags & k_FollowHand) == 0)
		{
			continue;
		}
		// the landscape draw, with bit 1 and the intro's special on: the drawing object at the grip with its yaw and
		// scale and no slope shear; the grip is the intro hand's (intro_special::Grip).
		// The super villager draw's own move to the origin, under a flag that is never set, is dead code.
		// This overwrites the DrawPosition written by UpdateMobileDrawing.
		// Only the DrawPosition (the drawing object's matrix): no Transform, no game state (SetDirty only re-reads the
		// draw instances)
		if (auto* draw = registry.TryGet<DrawPosition>(thing); draw != nullptr)
		{
			draw->position = *superVillagerState.handGrip;
			draw->shearX = 0.0f;
			draw->shearZ = 0.0f;
			registry.SetDirty();
		}
	}
}

void Draw(int32_t milliseconds)
{
	auto& registry = Entities();
	for (const auto thing : List())
	{
		// a thing destroyed since Update: released now, so its eyes are not drawn this frame
		if (!registry.Valid(thing) || !registry.AllOf<SuperVillager>(thing))
		{
			Release(thing);
			continue;
		}
		auto& super = registry.Get<SuperVillager>(thing);
		if (!super.eyes.has_value())
		{
			continue;
		}
		// off screen no smooth drawing, so no eyes update (no draws of the CRT stream, no timers moved)
		if (!super.onScreen)
		{
			eyes::Hide(*super.eyes);
			continue;
		}
		// the eyes after the body
		eyes::Update(*super.eyes, SuperVillagerData().shared, thing, milliseconds);
	}
}

void FillFrame(graphics::SuperVillagerFrame& out)
{
	auto& state = SuperVillagerData();
	out.litByDefaultSun.clear();
	out.swimmers.clear();
	out.notHumanShadowed.clear();
	if (state.list.empty() || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Entities();
	for (const auto& entry : state.list)
	{
		const bool valid = registry.Valid(entry.thing);
		if (const auto* super = valid ? registry.TryGet<const SuperVillager>(entry.thing) : nullptr;
		    super != nullptr && super->hdMesh != 0)
		{
			out.litByDefaultSun.insert(super->hdMesh);
		}
		if (valid && registry.AllOf<SuperVillager>(entry.thing))
		{
			out.notHumanShadowed.push_back(entry.thing); // no ground blobs while the temporary shadow lasts
		}
		for (const auto object : entry.eyes)
		{
			if (const auto* eye =
			        object != entt::null && registry.Valid(object) ? registry.TryGet<const SuperVillagerEye>(object) : nullptr;
			    eye != nullptr && eye->mesh != 0)
			{
				out.litByDefaultSun.insert(eye->mesh);
			}
		}
		// the swimmers: the clip "M_P_Swim2"
		const auto* animation = valid ? registry.TryGet<const SkeletalAnimation>(entry.thing) : nullptr;
		if (animation != nullptr && animation->hasClip && animation->clipIndex == static_cast<int32_t>(AnimId::PSwim2))
		{
			out.swimmers.push_back(entry.thing);
		}
	}
}

void SetIntroHandGrip(std::optional<glm::vec3> grip)
{
	SuperVillagerData().handGrip = grip;
}

void testing::Adopt(entt::entity thing, int32_t eyeType)
{
	auto& state = SuperVillagerData();
	auto& registry = Entities();
	if (registry.AllOf<SuperVillager>(thing))
	{
		return;
	}
	auto& super = registry.Assign<SuperVillager>(thing);
	super.eyeType = eyeType;
	super.eyes = eyes::Create(thing, eyeType, eyes::HostModel {}, std::array<entt::id_type, 5> {});
	state.list.insert(state.list.begin(), Entry {thing, super.eyes->objects}); // linked at the head
}

} // namespace openblack::ecs::super_villager
