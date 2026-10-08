/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureDraw.h"

#include <cmath>

#include <limits>

#include <glm/common.hpp>

#include "Creature/CreatureHair.h"
#include "Creature/CreatureMorph.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Components/Mesh.h"
#include "Graphics/ArgbColour.h"

using namespace openblack;
using namespace openblack::graphics;
using namespace openblack::ecs::components;

namespace
{
/// A channel, 0 to 1, as a byte
uint32_t ChannelByte(float channel)
{
	return static_cast<uint32_t>(std::lround(std::clamp(channel, 0.0f, 1.0f) * 255.0f));
}

/// An opaque colour, each channel 0 to 255, as 0xAARRGGBB
uint32_t OpaqueArgb(const glm::ivec3& colour)
{
	const auto byte = [](int channel) { return static_cast<uint32_t>(std::clamp(channel, 0, 255)); };
	return 0xFF000000u | (byte(colour.r) << 16u) | (byte(colour.g) << 8u) | byte(colour.b);
}

/// One strand's ribbon added to a strip, two triangles between each pair of its points. False when the strip has no
/// room left for it.
bool AddStrand(creature_draw::Strip& strip, std::span<const glm::vec3> points, const glm::vec3& eye, float halfWidth,
               uint32_t abgr)
{
	if (points.size() < 2)
	{
		return true;
	}
	const size_t first = strip.vertices.size();
	const size_t count = points.size() * 2;
	if (first + count > std::numeric_limits<uint16_t>::max())
	{
		return false;
	}
	std::vector<creature_hair::RibbonVertex> corners(count);
	creature_hair::BuildRibbon(points, eye, halfWidth, corners);
	for (const auto& corner : corners)
	{
		strip.vertices.push_back({.position = corner.position, .uv = corner.uv, .abgr = abgr});
	}
	for (size_t i = 0; i + 1 < points.size(); ++i)
	{
		const auto a = static_cast<uint16_t>(first + (i * 2));
		for (const auto offset : {0, 1, 2, 2, 1, 3})
		{
			strip.indices.push_back(static_cast<uint16_t>(a + offset));
		}
	}
	return true;
}
} // namespace

std::vector<creature_draw::Body> creature_draw::Bodies(const ecs::Registry& registry, const EntityInstances& instances,
                                                       const MeshBones& meshBones)
{
	using MeshComponent = ecs::components::Mesh; // not graphics::Mesh, which this namespace finds first
	std::vector<Body> bodies;
	registry.Each<const Creature, const MeshComponent>([&](entt::entity entity, const Creature& creature,
	                                                       const MeshComponent& /*mesh*/) {
		const auto instance = instances.find(entity);
		if (instance == instances.end())
		{
			return;
		}
		const auto meshId = instance->second.meshId;
		const auto boneCount = meshBones(meshId);
		if (!boneCount.has_value())
		{
			return;
		}
		Body body {.entity = entity, .instance = instance->second.index, .mesh = meshId};
		// The pose the animations made, if it fits the mesh; else the mesh's rest pose
		if (const auto* animation = registry.TryGet<const CreatureAnimation>(entity);
		    animation != nullptr && *boneCount > 0 && animation->boneMatrices.size() == *boneCount)
		{
			body.bones = animation->boneMatrices;
		}
		// The shape: towards the meshes of the species that are loaded, the base mesh standing in for the others
		if (const auto* morph = registry.TryGet<const CreatureMorph>(entity); morph != nullptr)
		{
			const auto meshes = creature_morph::MeshesOf(creature.species, morph->drawn,
			                                             [&meshBones](entt::id_type id) { return meshBones(id).has_value(); });
			body.morph = MorphTargets {
			    .meshes = {meshes.evilGood, meshes.thinFat, meshes.weakStrong},
			    .weights = glm::abs(glm::vec3(morph->drawn.evilGood, morph->drawn.thinFat, morph->drawn.weakStrong)),
			};
		}
		bodies.push_back(body);
	});
	std::ranges::sort(bodies, {}, &Body::instance);
	return bodies;
}

const creature_draw::Body* creature_draw::Find(std::span<const Body> bodies, uint32_t instance)
{
	const auto found = std::ranges::lower_bound(bodies, instance, {}, &Body::instance);
	return found != bodies.end() && found->instance == instance ? &*found : nullptr;
}

std::vector<creature_draw::EyePart> creature_draw::Eyes(const CreatureEyes& eyes, const HasMesh& hasMesh)
{
	std::vector<EyePart> parts;
	const bool eyeball = hasMesh(CreatureEyes::k_EyeballMeshId);
	const bool eyelid = hasMesh(CreatureEyes::k_EyelidMeshId);
	const auto tint = LidTint(eyes.lidColour);
	for (const auto& eye : eyes.drawn)
	{
		if (eyeball && eye.eyeball.has_value())
		{
			parts.push_back({.mesh = CreatureEyes::k_EyeballMeshId, .model = *eye.eyeball, .tint = std::nullopt});
		}
		if (eyelid && eye.eyelid.has_value())
		{
			parts.push_back({.mesh = CreatureEyes::k_EyelidMeshId, .model = *eye.eyelid, .tint = tint});
		}
	}
	return parts;
}

uint32_t creature_draw::LidTint(const glm::vec3& lidColour)
{
	return 0xFF000000u | (ChannelByte(lidColour.r) << 16u) | (ChannelByte(lidColour.g) << 8u) | ChannelByte(lidColour.b);
}

creature_draw::InstanceRow creature_draw::EyeRow(const glm::mat4& bodyRow, const glm::vec4& bodyColours,
                                                 std::optional<uint32_t> tint)
{
	InstanceRow row {bodyRow[0], bodyRow[1], bodyRow[2], bodyRow[3], bodyColours};
	if (tint.has_value())
	{
		argb_colour::PackInstanceTint(row[4], *tint);
	}
	return row;
}

creature_draw::Hair creature_draw::BuildHair(const CreatureHair& hair, const glm::vec3& eye, const glm::ivec3& light,
                                             const glm::ivec3& added)
{
	Hair out;
	for (const auto& group : hair.groups)
	{
		const auto abgr = world_triangles::ToAbgr(OpaqueArgb(creature_hair::StrandColour(group.colour, light, added)));
		auto& strip = group.textured ? out.textured : out.plain;
		for (const auto& strand : group.strands)
		{
			if (!AddStrand(strip, strand.positions, eye, group.halfWidth, abgr))
			{
				break;
			}
		}
	}
	return out;
}
