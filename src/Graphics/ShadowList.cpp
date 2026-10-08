/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ShadowList.h"

#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <filesystem>
#include <optional>
#include <string>

#include <LNDFile.h>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>
#include <stb_image_write.h>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "Debug/DebugEnv.h"
#include "ECS/Abodes.h"
#include "ECS/Animations.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/DynamicShadow.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/NotDrawn.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/Villager.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "Graphics/Argb4444.h"
#include "Graphics/Haze.h"
#include "Graphics/ModelLight.h"
#include "Locator.h"
#include "Particles/Creators/Mesh.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;
using namespace openblack::graphics::shadow_list;

namespace
{
/// The hand's texture size and density (k_HandShadowAsOriginal): the original's 32 x 32 with half rows; openblack's
/// old 64 x 64 at full density with the flag off
constexpr int k_HandTexels = k_HandShadowAsOriginal ? shadow_math::k_Texels : 64;
constexpr bool k_HandHalfRows = k_HandShadowAsOriginal;
constexpr bool k_HandHoldsInShadow = k_HandShadowAsOriginal;

/// A physics object gets a projected shadow when its shadow is chroma-keyed (IsChroma), when it casts a shadow on the
/// texture or when it is animated: the casters of a static shadow (RenderingSystem's CastsStaticShadow) and the
/// villagers and animals
bool IsChroma(const ecs::Registry& registry, entt::entity entity);

bool CastsPhysicsShadow(const ecs::Registry& registry, entt::entity entity)
{
	using namespace ecs::components;
	// building fragments: their shadow-on-texture flag is cleared
	if (registry.AnyOf<Fragment>(entity))
	{
		return false;
	}
	// the shadow-on-texture flag is also off for what is not drawn and for an Abode not built yet: the same rule as
	// CastsStaticShadow's
	if (!ecs::abodes::CastsShadowOnTexture(entity))
	{
		return false;
	}
	// chroma first: the trees, dead trees and the hand's food pot
	if (IsChroma(registry, entity) || registry.AnyOf<Villager, Animal>(entity))
	{
		return true;
	}
	return registry.AnyOf<Fixed, MobileStatic, MobileObject, Tree, Abode, Feature, BigForest>(entity) &&
	       !registry.AnyOf<Pot, AnimatedStatic, DeadTree, Field, Creature, Hand, Alpha, TempleInteriorPart>(entity);
}

/// The instance matrix as a plain affine matrix: the columns' w carry other data (docs/bw1-notes/openblack-internals.md)
glm::mat4 InstanceMatrix(const glm::mat4& instance)
{
	glm::mat4 matrix = instance;
	matrix[0].w = 0.0f;
	matrix[1].w = 0.0f;
	matrix[2].w = 0.0f;
	matrix[3].w = 1.0f;
	return matrix;
}

/// Whether the object's shadow is chroma-keyed (drawn from its texture's alpha): set by trees, big forests, dead trees
/// (a felled tree keeps its dead tree here) and the pot of info 12 (HandFood); field crops clear it
bool IsChroma(const ecs::Registry& registry, entt::entity entity)
{
	using namespace ecs::components;
	if (registry.AnyOf<Tree, BigForest, DeadTree>(entity))
	{
		return true;
	}
	const auto* pot = registry.TryGet<const Pot>(entity);
	return pot != nullptr && pot->type == PotInfo::HandFood;
}

/// The chroma shadow drawn textured into the 16-bit target: every sub-mesh with the LOD 0 bit, not the boned ones,
/// each primitive with the 64 x 64 map of its texture through the object's matrix (no bones)
void RenderChroma(const L3DMesh& mesh, const glm::mat4& matrix, const shadow_math::Projection& projection,
                  const shadow_math::Box& box, std::vector<uint16_t>& target, int side)
{
	for (const auto& subMesh : mesh.GetSubMeshes())
	{
		if (subMesh->IsPhysics() || (subMesh->GetFlags().lodMask & 1) != 1 || subMesh->GetFlags().hasBones)
		{
			continue;
		}
		const auto& local = subMesh->GetSkinLocalPositions();
		const auto& uvs = subMesh->GetCollisionUVs();
		const auto& collision = subMesh->GetCollisionIndices();
		std::vector<glm::vec4> table(local.size()); // the projected vertices
		for (size_t i = 0; i < local.size(); ++i)
		{
			table[i] = shadow_math::ChromaVertex(projection, box, matrix, local[i], i < uvs.size() ? uvs[i] : glm::vec2(0.0f));
		}
		const auto& primitives = subMesh->GetPrimitives();
		const auto& ranges = subMesh->GetCollisionRanges();
		for (size_t p = 0; p < primitives.size() && p < ranges.size(); ++p)
		{
			// (inferred) a primitive whose skin is not the mesh's own (a shared texture) has no map here: skipped
			const auto* map = mesh.GetShadowAlphaMap(primitives[p].skinID);
			if (map == nullptr)
			{
				continue;
			}
			const auto end = std::min<size_t>(collision.size(), size_t {ranges[p].first} + ranges[p].second);
			for (size_t i = ranges[p].first; i + 2 < end; i += 3) // its triangles
			{
				if (collision[i] >= table.size() || collision[i + 1] >= table.size() || collision[i + 2] >= table.size())
				{
					continue; // (port guard)
				}
				shadow_math::ChromaTriangle({table[collision[i]], table[collision[i + 1]], table[collision[i + 2]]}, *map,
				                            target, side);
			}
		}
	}
}

/// One caster's part of the silhouette: its projected points and its primitives' triangles into them
struct Silhouette
{
	std::vector<glm::vec2> points;
	struct Primitive
	{
		size_t first;
		size_t count;
		bool bothFaces;
		bool halfRows;
	};
	std::vector<uint16_t> indices;
	std::vector<Primitive> primitives;
};

/// The caster or the held object: every sub-mesh with the LOD 0 bit projected, each vertex by its bone (the skinned
/// branch) or the object's matrix
void ProjectCaster(const L3DMesh& mesh, const glm::mat4& instance, const glm::mat4* bones, size_t boneCount,
                   const shadow_math::Projection& projection, bool halfRows, shadow_math::Box& box, Silhouette& out)
{
	for (const auto& subMesh : mesh.GetSubMeshes())
	{
		if (subMesh->IsPhysics() || (subMesh->GetFlags().lodMask & 1) != 1)
		{
			continue;
		}
		const auto& local = subMesh->GetSkinLocalPositions();
		const auto& skin = subMesh->GetSkinBones();
		const auto& collision = subMesh->GetCollisionIndices();
		const size_t base = out.points.size();
		if (base + local.size() > 0xFFFF)
		{
			continue; // (port guard) the raster's 16-bit indices
		}
		for (size_t i = 0; i < local.size(); ++i)
		{
			const auto bone = i < skin.size() && skin[i] < boneCount ? skin[i] : 0;
			const auto matrix = bones != nullptr && boneCount > 0 ? instance * bones[bone] : instance;
			out.points.push_back(shadow_math::Project(projection, matrix, local[i], box));
		}
		// the file's triangles of each primitive (GetCollisionRanges)
		const auto& primitives = subMesh->GetPrimitives();
		const auto& ranges = subMesh->GetCollisionRanges();
		for (size_t p = 0; p < primitives.size() && p < ranges.size(); ++p)
		{
			const size_t first = out.indices.size();
			const auto end = std::min<size_t>(collision.size(), size_t {ranges[p].first} + ranges[p].second);
			for (size_t i = ranges[p].first; i < end; ++i)
			{
				out.indices.push_back(static_cast<uint16_t>(base + collision[i]));
			}
			// both faces with a two-sided material or a mist caster (no mist gets a projected shadow)
			out.primitives.push_back({first, out.indices.size() - first, primitives[p].twoSided, halfRows});
		}
	}
}

void BuildBlocks(const LandIslandInterface& island, const FrameInputs& inputs, Blocks& blocks)
{
	blocks.states.fill({});
	for (const auto& block : island.GetBlocks())
	{
		const auto mapPosition = block.GetMapPosition();
		const int bx = static_cast<int>(mapPosition.x / shadow_math::k_BlockSize);
		const int bz = static_cast<int>(mapPosition.y / shadow_math::k_BlockSize);
		if (bx < 0 || bz < 0 || bx >= Blocks::k_Side || bz >= Blocks::k_Side)
		{
			continue; // (port guard)
		}
		const auto& lnd = block.GetLndBlock();
		const float height = lnd ? static_cast<float>(static_cast<int32_t>(lnd->highestAltitude)) : 0.0f;
		const auto corners = haze::BlockCorners(mapPosition, height, inputs.landRef);
		// the centre: x, z + 80; y 0 with LandRef, else h 0.67 0.5
		const glm::vec3 centre(mapPosition.x + 80.0f, inputs.landRef ? 0.0f : height * 0.67f * 0.5f, mapPosition.y + 80.0f);
		const float dx = centre.x - inputs.camera.x;
		const float dy = centre.y - inputs.camera.y;
		const float dz = centre.z - inputs.camera.z;
		shadow_math::BlockState state {
		    .exists = true,
		    .visible = shadow_math::BlockVisible(corners, inputs.worldToClipping, inputs.nearW),
		};
		// (inferred: no visible effect) the original keeps an invisible block's old distance; here every block's is fresh
		state.distance = std::sqrt(dz * dz + dy * dy + dx * dx);
		blocks.states[static_cast<size_t>(bx * Blocks::k_Side + bz)] = state;
	}
}

void Upload(ShadowInfo& shadow)
{
	const auto side = static_cast<uint16_t>(shadow.texels);
	if (!shadow.texture.IsValid())
	{
		// cleared to 0; no tiling (CLAMP)
		shadow.texture.Reset(bgfx::createTexture2D(side, side, false, 1, bgfx::TextureFormat::R8,
		                                           BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP, nullptr));
		bgfx::setName(shadow.texture.Get(), "ShadowInfo");
	}
	const auto* memory = bgfx::alloc(static_cast<uint32_t>(side) * side);
	for (size_t i = 0; i < shadow.texels16.size() && i < memory->size; ++i)
	{
		memory->data[i] = argb4444::Expand(shadow.texels16[i]); // the ARGB4444 alpha nibble as D3D samples it
	}
	bgfx::updateTexture2D(shadow.texture.Get(), 0, 0, 0, 0, side, side, memory);
}

/// OPENBLACK_DUMP_SHADOWS=<dir>: each texture x 8 as a PNG, once per 300 frames
void Dump(const ShadowInfo& shadow, int frame, size_t index)
{
	const char* dir = std::getenv("OPENBLACK_DUMP_SHADOWS");
	if (dir == nullptr || frame % 300 != 0)
	{
		return;
	}
	constexpr int k_Zoom = 8;
	const int side = shadow.texels * k_Zoom;
	std::vector<uint8_t> pixels(static_cast<size_t>(side * side));
	for (int y = 0; y < side; ++y)
	{
		for (int x = 0; x < side; ++x)
		{
			pixels[static_cast<size_t>(y * side + x)] =
			    argb4444::Expand(shadow.texels16[static_cast<size_t>((y / k_Zoom) * shadow.texels + x / k_Zoom)]);
		}
	}
	const auto path = std::filesystem::path(dir) / ("shadow_" + std::to_string(frame) + "_" + std::to_string(index) + "_" +
	                                                std::to_string(static_cast<uint32_t>(shadow.caster)) + ".png");
	stbi_write_png(path.string().c_str(), side, side, 1, pixels.data(), side);
}
} // namespace

List::List() = default;

List::~List()
{
	Clear();
}

ShadowInfo& List::Add(entt::entity caster, Update update, LightKind light, bool onObjects, bool halfRows, int texels)
{
	ShadowInfo shadow {
	    .caster = caster,
	    .update = update,
	    .light = light,
	    .emitter = update == Update::Generic,
	    .onObjects = onObjects,
	    .halfRows = halfRows,
	    .texels = texels,
	};
	shadow.texels16.assign(static_cast<size_t>(texels * texels), 0);
	return _shadows.emplace_front(std::move(shadow)); // the new one becomes the head
}

void List::Remove(entt::entity caster)
{
	// the entry's texture goes with it
	_shadows.remove_if([caster](const ShadowInfo& shadow) { return shadow.caster == caster; });
}

void List::Clear()
{
	_shadows.clear(); // each entry's texture goes with it
}

void List::Frame(const FrameInputs& inputs)
{
	++_frame;
	if (!Locator::terrainSystem::has_value() || !Locator::rendereringSystem::has_value() ||
	    !Locator::entitiesRegistry::has_value())
	{
		Clear();
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& renderCtx = Locator::rendereringSystem::value().GetContext();
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto& island = Locator::terrainSystem::value();

	// ---- the producers ----
	for (auto& shadow : _shadows)
	{
		shadow.seen = false;
	}
	const auto find = [this](entt::entity caster) -> ShadowInfo* {
		for (auto& shadow : _shadows)
		{
			if (shadow.caster == caster)
			{
				return &shadow;
			}
		}
		return nullptr;
	};
	const auto want = [&](entt::entity caster, Update update, LightKind light, bool onObjects, bool halfRows,
	                      int texels) -> ShadowInfo& {
		auto* shadow = find(caster);
		if (shadow == nullptr || shadow->light != light || shadow->texels != texels)
		{
			Remove(caster);
			shadow = &Add(caster, update, light, onObjects, halfRows, texels);
		}
		shadow->onObjects = onObjects;
		shadow->seen = true;
		return *shadow;
	};
	// the hand: its dynamic shadow is always on in the original's data; it is drawn over the objects too
	if (Locator::handSystem::has_value())
	{
		const auto hand = Locator::handSystem::value().GetPlayerHands()[0];
		// a hidden hand casts no shadow
		if (registry.Valid(hand) && !registry.AllOf<ecs::components::NotDrawn>(hand))
		{
			auto& shadow = want(hand, Update::Complex, LightKind::Hand, true, k_HandHalfRows, k_HandTexels);
			// the held object, when it is drawn in the hand: the seeds that are only an effect in the hand have no mesh
			// drawn, so no instance
			// the render hand's object, from the press; its own dynamic shadow is off while held, back on when thrown
			const auto held = Locator::handSystem::value().GetRenderHandObject();
			shadow.held =
			    k_HandHoldsInShadow && held.has_value() && renderCtx.entityInstances.contains(*held) ? *held : entt::null;
		}
	}
	// the creatures: complex casters, as the hand, from the same constructor of the original; drawn over the objects too
	// (inferred: as the hand's). Through the const registry, so a land without a creature gains no storage
	registry.Each<const ecs::components::Creature>([&](entt::entity entity, const ecs::components::Creature& /*creature*/) {
		if (renderCtx.entityInstances.contains(entity) && !registry.AllOf<ecs::components::NotDrawn>(entity))
		{
			want(entity, Update::Complex, LightKind::Creature, true, false, shadow_math::k_Texels);
		}
	});
	// the flying physics objects (the resting proxies are skipped)
	ecs::physics::PhysicsObjects::ForEach([&](const ecs::physics::PhysicsObject& object) {
		if (object.body.resting || !ecs::IsAvailable(object.entity) || !CastsPhysicsShadow(registry, object.entity))
		{
			return;
		}
		// not over the objects, vertical light
		want(object.entity, Update::Generic, LightKind::Vertical, false, false, shadow_math::k_Texels);
	});
	// the objects with a holder of their own (the launched boat: the sun's light, drawn over the objects)
	registry.Each<const ecs::components::DynamicShadow>(
	    [&](entt::entity entity, const ecs::components::DynamicShadow& dynamic) {
		    want(entity, Update::Generic, dynamic.useSun ? LightKind::Sun : LightKind::Vertical, dynamic.onObjects, false,
		         shadow_math::k_Texels);
	    },
	    entt::exclude<ecs::components::Unavailable>);
	// the particle mesh atoms with CastHumanShadow: made with the particle (not over the objects, vertical light), its
	// object given by the atom's draw and taken out with the particle; the atoms Collect met this frame
	for (const auto& atom : psys::mesh_atoms::HumanShadows())
	{
		auto found = std::ranges::find(_shadows, static_cast<const void*>(atom.atom), &ShadowInfo::psysAtom);
		ShadowInfo* shadow = found != _shadows.end() ? &*found : nullptr;
		if (shadow == nullptr)
		{
			shadow = &Add(entt::null, Update::Generic, LightKind::Vertical, false, false, shadow_math::k_Texels);
			shadow->psysAtom = atom.atom;
		}
		shadow->particleMesh = atom.meshId;
		shadow->particleMatrix = atom.model;
		shadow->particleScale = atom.scale;
		shadow->seen = true;
	}
	for (auto it = _shadows.begin(); it != _shadows.end();)
	{
		if (!it->seen)
		{
			it = _shadows.erase(it); // its texture goes with it
		}
		else
		{
			++it;
		}
	}
	if (_shadows.empty())
	{
		return;
	}

	// ---- the updates ----
	BuildBlocks(island, inputs, _blocks);
	const shadow_math::BlockAt blockAt = [this](int x, int z) { return _blocks.At(x, z); };
	const int cellLimit = static_cast<int>(island.GetCellsPerSide()) - 1; // 511 in the original's 512 cells
	const auto poses = ecs::PosesByInstance(renderCtx);
	const bool trace = debug_env::ShadowTrace() && _frame % 60 == 0;
	size_t index = 0;
	for (auto& shadow : _shadows)
	{
		++index;
		// a particle atom's shadow is updated with its object, never an entity's
		const bool atomCaster = shadow.psysAtom != nullptr;
		const auto instance = atomCaster ? renderCtx.entityInstances.end() : renderCtx.entityInstances.find(shadow.caster);
		if (atomCaster ? !meshes.Contains(shadow.particleMesh)
		               : instance == renderCtx.entityInstances.end() || !meshes.Contains(instance->second.meshId) ||
		                     instance->second.index >= renderCtx.instanceUniforms.size())
		{
			shadow.active = false; // (inferred) not drawn this frame: like a hidden object in the complex update
			if (trace)
			{
				SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "shadow {} caster {}: not drawn", index,
				                   static_cast<uint32_t>(shadow.caster));
			}
			continue;
		}
		shadow.active = true;
		const auto meshId = atomCaster ? shadow.particleMesh : instance->second.meshId;
		const auto mesh = meshes.Handle(meshId);
		const auto matrix =
		    atomCaster ? shadow.particleMatrix : InstanceMatrix(renderCtx.instanceUniforms[instance->second.index]);
		const glm::vec3 position(matrix[3]); // the drawn (interpolated) matrix
		const auto* transform = atomCaster ? nullptr : registry.TryGet<const ecs::components::Transform>(shadow.caster);
		// the object's scale (a particle atom's: its drawn scale)
		const float scale = atomCaster ? shadow.particleScale : transform != nullptr ? transform->scale.x : 1.0f;
		const float radius = ecs::object::MeshHalfDiagonal(meshId); // the mesh's radius
		const float ground = island.GetHeightAt(glm::vec2(position.x, position.z));

		// the generic / complex update's fade and alpha
		const float fade = shadow_math::Fade(position, ground, inputs.camera, scale, radius, blockAt, cellLimit);
		shadow.alpha = shadow.update == Update::Generic ? shadow_math::AlphaGeneric(fade, shadow.baseAlpha)
		                                                : shadow_math::AlphaComplex(fade, shadow.baseAlpha);
		if (shadow.alpha == 0)
		{
			if (trace)
			{
				SPDLOG_LOGGER_INFO(
				    spdlog::get("graphics"),
				    "shadow {} caster {}: alpha 0, at ({:.1f}, {:.1f}, {:.1f}) ground {:.1f} scale {:.2f} radius {:.2f}", index,
				    static_cast<uint32_t>(shadow.caster), position.x, position.y, position.z, ground, scale, radius);
			}
			continue;
		}
		glm::vec3 light {0.0f}; // every LightKind sets it below (MSVC C4701 does not see the switch is complete)
		switch (shadow.light)
		{
		case LightKind::Vertical:
			light = shadow_math::LightGeneric(position, false);
			break;
		case LightKind::Sun:
			light = shadow_math::LightGeneric(position, true);
			break;
		case LightKind::Hand:
			light = shadow_math::LightHand(position);
			break;
		case LightKind::Creature:
			light = shadow_math::LightCreature(position, model_light::Light(), radius, scale);
			break;
		}
		shadow.projection = shadow_math::MakeProjection(position, light);
		if (shadow.light == LightKind::Hand && !k_HandShadowAsOriginal)
		{
			// k_HandShadowAsOriginal off: the look of the hand's shadow before the list, to compare: projected from the
			// light onto the ground under the hand, s = (ground - Ly) / (y - Ly) (the old vs_dynamic_shadow_instanced).
			// The projection's t = -Ly / (h - Ly) with h = y - base gives the same with the base at the ground and the
			// light's y taken from it. The original's base is the hand's own y.
			shadow.projection.baseY = ground;
			shadow.projection.light.y = light.y - ground;
		}
		shadow.box = {};

		// the caster's bones: the hand's, a posed model's, or the mesh's rest pose
		const glm::mat4* bones = nullptr;
		size_t boneCount = 0;
		// a particle atom's object is a static object: it has no pose, so its vertices take the matrix alone
		if (mesh->IsBoned() && !atomCaster)
		{
			const std::vector<glm::mat4>* handBones = nullptr;
			if (shadow.light == LightKind::Hand && Locator::handSystem::has_value())
			{
				handBones = Locator::handSystem::value().GetBoneMatrices();
			}
			if (handBones != nullptr && handBones->size() == mesh->GetBoneMatrices().size())
			{
				bones = handBones->data();
				boneCount = handBones->size();
			}
			else
			{
				bones = mesh->GetBoneMatrices().data();
				auto count = static_cast<uint8_t>(std::min<size_t>(255, mesh->GetBoneMatrices().size()));
				ecs::UsePose(poses, instance->second.index, *mesh, bones, count);
				boneCount = count;
			}
		}
		Silhouette silhouette;
		ProjectCaster(*mesh, matrix, bones, boneCount, shadow.projection, shadow.halfRows, shadow.box, silhouette);
		const size_t casterPrimitives = silhouette.primitives.size();

		// the held object: its own base y, its pose, the same light and box, full density
		const L3DMesh* heldMesh = nullptr;
		glm::mat4 heldMatrix(1.0f);
		shadow_math::Projection heldProjection = shadow.projection;
		if (shadow.held != entt::null)
		{
			const auto heldInstance = renderCtx.entityInstances.find(shadow.held);
			if (heldInstance != renderCtx.entityInstances.end() && meshes.Contains(heldInstance->second.meshId) &&
			    heldInstance->second.index < renderCtx.instanceUniforms.size())
			{
				heldMesh = meshes.Handle(heldInstance->second.meshId).operator->();
				heldMatrix = InstanceMatrix(renderCtx.instanceUniforms[heldInstance->second.index]);
				heldProjection.baseY = heldMatrix[3].y;
				const glm::mat4* heldBones = nullptr;
				uint8_t heldCount = 0;
				if (heldMesh->IsBoned())
				{
					heldBones = heldMesh->GetBoneMatrices().data();
					heldCount = static_cast<uint8_t>(std::min<size_t>(255, heldMesh->GetBoneMatrices().size()));
					ecs::UsePose(poses, heldInstance->second.index, *heldMesh, heldBones, heldCount);
				}
				ProjectCaster(*heldMesh, heldMatrix, heldBones, heldCount, heldProjection, false, shadow.box, silhouette);
			}
		}
		if (silhouette.points.empty() || !(shadow.box.x1 > shadow.box.x0) || !(shadow.box.z1 > shadow.box.z0))
		{
			shadow.alpha = 0; // (port guard) nothing to divide the grid by
			continue;
		}

		// the grid, then a chroma caster drawn textured into a 16-bit render (its raster skipped) and its held object
		// dropped, else the raster; the held object the same way; the resolve, the chroma blur and the baked fade
		shadow_math::ToGrid(shadow.box, silhouette.points, shadow.texels);
		shadow_math::Coverage coverage(shadow.texels);
		const auto raster = [&](size_t first, size_t last) {
			for (size_t p = first; p < last; ++p)
			{
				const auto& primitive = silhouette.primitives[p];
				shadow_math::RasterTriangles(
				    silhouette.points, std::span<const uint16_t>(silhouette.indices).subspan(primitive.first, primitive.count),
				    primitive.bothFaces, primitive.halfRows, coverage);
			}
		};
		std::vector<uint16_t> rendered; // the 16-bit render target, cleared
		// a particle atom's object is never chroma
		const bool chroma = !atomCaster && IsChroma(registry, shadow.caster);
		if (chroma)
		{
			rendered.assign(static_cast<size_t>(shadow.texels) * static_cast<size_t>(shadow.texels), 0);
			RenderChroma(*mesh, matrix, shadow.projection, shadow.box, rendered, shadow.texels);
			heldMesh = nullptr;
		}
		else
		{
			raster(0, casterPrimitives);
		}
		if (heldMesh != nullptr)
		{
			if (IsChroma(registry, shadow.held))
			{
				rendered.resize(static_cast<size_t>(shadow.texels) * static_cast<size_t>(shadow.texels), 0);
				// with the shadow's own base, not the held object's: the textured draw reads the shadow's base y
				RenderChroma(*heldMesh, heldMatrix, shadow.projection, shadow.box, rendered, shadow.texels);
			}
			else
			{
				raster(casterPrimitives, silhouette.primitives.size());
			}
		}
		shadow_math::Resolve(coverage, shadow.texels16);
		if (!rendered.empty())
		{
			shadow_math::ChromaFilter(rendered, shadow.texels16);
		}
		shadow_math::BakeAlpha(shadow.texels16, shadow.alpha);

		// the land draw's t': H = the caster's altitude for an emitter, else the caster's y
		shadow.landT = shadow_math::LandProjectionFactor(shadow.projection.baseY, light.y,
		                                                 shadow.emitter ? ground : shadow.projection.baseY);
		Upload(shadow);
		Dump(shadow, _frame, index);
		if (trace)
		{
			SPDLOG_LOGGER_INFO(
			    spdlog::get("graphics"),
			    "shadow {} caster {} light {} alpha {} fade {:.1f} box ({:.2f}, {:.2f})..({:.2f}, {:.2f}) kMin {:.1f} "
			    "t' {:.5f} max n {} points {}",
			    index, static_cast<uint32_t>(shadow.caster), static_cast<int>(shadow.light), shadow.alpha, fade, shadow.box.x0,
			    shadow.box.z0, shadow.box.x1, shadow.box.z1, shadow.box.kMin, shadow.landT,
			    *std::max_element(shadow.texels16.begin(), shadow.texels16.end()), silhouette.points.size());
		}
	}
}
