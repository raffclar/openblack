/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DesignedScenery.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>

#include <exception>
#include <unordered_set>

#include <entt/entity/entity.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/FrameAnim.h"
#include "3D/LandIslandInterface.h"
#include "3D/ObjectMatrix.h"
#include "Audio/Device/Sound.h"
#include "Audio/Services/SoundTags.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/MapScriptSystemInterface.h"
#include "ECS/Systems/WorldEffectsInterface.h"
#include "ECS/WaterRings.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// What this module keeps between calls (Locator::worldEffects)
struct DesignedSceneryState
{
	// the land the scenery was made for, 74 at start (no land)
	int32_t lastLand {74};
	// the objects (the waterfall or the ark; the dinosaur). Their meshes stay loaded in the mesh manager instead of
	// being released
	entt::entity object {entt::null};
	entt::entity object2 {entt::null};
	// the SoundTag on its script marker (the marker is only its point here)
	audio::sound_tags::TagId tag {audio::sound_tags::k_NoTag};
	// the waterfall's texture V offset and its ring timer (seconds of game time)
	float v {0.0f};
	float ringTimer {0.0f};
	// the misc meshes that failed to load (not tried again)
	std::unordered_set<entt::id_type> failedMeshes {};
};

DesignedSceneryState& DesignedSceneryData()
{
	return openblack::Locator::worldEffects::value().Get<DesignedSceneryState>();
}

// Land 3: the waterfall's position, angle and scale
constexpr glm::vec3 k_WaterfallPos {3059.23f, 0.0f, 3145.33f};
constexpr float k_WaterfallAngle = 4.7f;
constexpr float k_WaterfallScale = 1.0f;
// one ring per 0.7 s; its point, growth, rate and alpha
constexpr float k_RingPeriod = 0.7f;
constexpr glm::vec3 k_RingPos {3018.8f, 0.2f, 3130.15f};
constexpr float k_RingGrowth = 30.0f;
constexpr float k_RingRate = 0.3f;
constexpr uint32_t k_RingAlpha = 0x80;
// Land 4: the ark's and the dinosaur's positions, angles and scales
constexpr glm::vec2 k_ArkPos {3538.0f, 2129.0f};
constexpr float k_ArkAngle = 8.9728f;
constexpr float k_ArkScale = 1.1f;
constexpr glm::vec2 k_DinosaurPos {2690.0f, 2590.0f};
constexpr float k_DinosaurAngle = 1.57f;
constexpr float k_DinosaurScale = 1.0f;

bool Trace()
{
	static const bool k_Trace = std::getenv("OPENBLACK_SCENERY_TRACE") != nullptr;
	return k_Trace;
}

/// Data\\MISC\\<file>.l3d: loaded once into the mesh manager as "misc/<file>"
entt::id_type LoadMiscMesh(const char* file)
{
	const auto id = resources::HashIdentifier(fmt::format("misc/{}", file));
	auto& meshes = Locator::resources::value().GetMeshes();
	// a file that failed is not tried again every frame
	if (DesignedSceneryData().failedMeshes.contains(id))
	{
		return 0;
	}
	if (!meshes.Contains(id))
	{
		try
		{
			const auto path = Locator::filesystem::value().GetPath<filesystem::Path::Misc>() / fmt::format("{}.l3d", file);
			meshes.Load(id, resources::L3DLoader::FromDiskTag {}, path);
		}
		catch (const std::exception& e)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Designed scenery: cannot load {}.l3d: {}", file, e.what());
			DesignedSceneryData().failedMeshes.insert(id);
			return 0;
		}
	}
	return id;
}

/// A bare object with the mesh, dynamic lighting and the position (pos, angle, scale). The objects' LOD distance factor
/// k (1 for the waterfall, 10 for the ark and the dinosaur) scales D = min((k + 1) x two engine constants, 100000),
/// LOD 1 / 2 / 3 / 4 below 23.33 D / 66.67 D / 86.67 D / beyond the last distance, and a submesh is drawn when bits
/// 29-31 of its flags hold the LOD. Every submesh of the three meshes has 0xE0000800 (all the LODs), so k changes
/// nothing on screen and is not kept.
entt::entity MakeObject(const char* file, const glm::vec3& position, float angle, float scale)
{
	const auto mesh = LoadMiscMesh(file);
	if (mesh == 0)
	{
		return entt::null;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, position, affine::AngleY(angle), glm::vec3(scale));
	registry.Assign<Mesh>(entity, mesh, static_cast<int8_t>(0), static_cast<int8_t>(0));
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Designed scenery: {} at ({:.2f}, {:.2f}, {:.2f}) angle {} scale {}", file,
		                   position.x, position.y, position.z, angle, scale);
	}
	return entity;
}

float Altitude(const glm::vec2& at)
{
	return Locator::terrainSystem::value().GetHeightAt(at);
}

/// A script marker and its sound tag (12 G_WaterFlow, false, mode 2, loops -1, 0, 3D, InGame, 0). The marker is made
/// from MapCoords (y kept as y - altitude), so it sounds at y = 0.
audio::sound_tags::TagId MakeWaterFlowTag(const glm::vec3& point)
{
	audio::sound_tags::TagDesc desc {
	    .sample = static_cast<entt::id_type>(audio::SoundId::G_WaterFlow),
	    .point = point,
	};
	return audio::sound_tags::Create(desc);
}

void DestroyObject(entt::entity& entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (entity != entt::null && registry.Valid(entity))
	{
		registry.Destroy(entity);
	}
	entity = entt::null;
}

void ProcessWaterfall(float seconds)
{
	auto& state = DesignedSceneryData();
	auto& registry = Locator::entitiesRegistry::value();
	if (state.object == entt::null)
	{
		// a static object
		state.object = MakeObject("waterfall3", k_WaterfallPos, k_WaterfallAngle, k_WaterfallScale);
		if (state.object == entt::null)
		{
			return;
		}
		registry.Assign<UvScroll>(state.object);
		state.tag = MakeWaterFlowTag(k_WaterfallPos);
	}
	// V = frac(V - 0.5 dt) (truncated, so it stays in -1..0), then the object's UV offset (0, V). The static draw
	// passes the offsets on, and the triangle submit adds them to the UVs unless the material's flags byte has bit
	// 0x10: the rock of waterfall3.l3d (submesh 0, Textured, flags 0x14) stays still and only the water (submesh 1,
	// TexturedChroma, 0x04) flows (L3DSubMesh::Primitive::uvOffset).
	// (frame_anim::WaterfallScroll)
	const float v = graphics::frame_anim::WaterfallScroll(state.v, seconds);
	if (auto* scroll = registry.TryGet<UvScroll>(state.object); scroll != nullptr)
	{
		scroll->v = v;
	}
	// a ring at the foot once the timer passes 0.7 s (the timer restarts even when the pool is full)
	state.ringTimer += seconds;
	if (state.ringTimer > k_RingPeriod)
	{
		state.ringTimer = 0.0f;
		const ecs::WaterRing ring {.position = k_RingPos,
		                           .age = 0,
		                           .growth = k_RingGrowth,
		                           .angle = 0.0f,
		                           .aspect = 1.0f,
		                           .rate = k_RingRate,
		                           .cell = 0x30,
		                           // (0x80 << 24) | the rgb of the light table[255]. The drift field is not written (it keeps
		                           // the slot's last value); openblack's rings have no slots nor drift, so none.
		                           .argb = (k_RingAlpha << 24u) | 0x00FFFFFFu,
		                           .seaLight = true};
		ecs::AddWaterRing(ring);
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Designed scenery: waterfall ring, V {:.4f}, {} rings", state.v,
			                   ecs::GetWaterRings().size());
		}
	}
}

void ProcessLand4()
{
	auto& state = DesignedSceneryData();
	if (state.object == entt::null)
	{
		// a morphable object
		state.object = MakeObject("arche", glm::vec3(k_ArkPos.x, Altitude(k_ArkPos), k_ArkPos.y), k_ArkAngle, k_ArkScale);
		if (state.object != entt::null)
		{
			// a morphable object: it melts into the terrain
			Locator::entitiesRegistry::value().Assign<MorphWithTerrain>(state.object);
			state.tag = MakeWaterFlowTag(glm::vec3(k_ArkPos.x, 0.0f, k_ArkPos.y));
		}
	}
	if (state.object2 == entt::null)
	{
		// its landscape footprint: dinosaur.l3d has ContainsLandscapeFeature, so the footprint pass
		// draws it like any placed mesh's (Renderer::DrawFootprintPass)
		state.object2 = MakeObject("dinosaur", glm::vec3(k_DinosaurPos.x, Altitude(k_DinosaurPos), k_DinosaurPos.y),
		                           k_DinosaurAngle, k_DinosaurScale);
		if (state.object2 != entt::null)
		{
			// a morphable object too: it melts into the terrain
			Locator::entitiesRegistry::value().Assign<MorphWithTerrain>(state.object2);
		}
	}
}
} // namespace

void ecs::designed_scenery::Update(float gameMilliseconds)
{
	auto& state = DesignedSceneryData();
	if (!Locator::entitiesRegistry::has_value() || !Locator::resources::has_value() || !Locator::terrainSystem::has_value() ||
	    !Locator::mapScriptSystem::has_value())
	{
		return;
	}
	const int32_t land = Locator::mapScriptSystem::value().Globals().landNumber;
	if (land != state.lastLand)
	{
		// a new land deletes what the last one had
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Designed scenery: land {} -> {}", state.lastLand, land);
		}
		state.lastLand = land;
		DestroyObject(state.object);
		DestroyObject(state.object2);
		audio::sound_tags::Delete(state.tag);
		state.tag = audio::sound_tags::k_NoTag;
	}
	if (land == 3)
	{
		// the game time increment * 0.001
		ProcessWaterfall(gameMilliseconds * 0.001f);
	}
	else if (land == 4)
	{
		ProcessLand4();
	}
}

void ecs::designed_scenery::OnLoadMap()
{
	auto& state = DesignedSceneryData();
	state.object = entt::null;
	state.object2 = entt::null;
	state.tag = audio::sound_tags::k_NoTag;
}
