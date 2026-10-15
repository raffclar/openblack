/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GltfCreature.h"

#include <charconv>

#include <algorithm>
#include <limits>
#include <numeric>
#include <ranges>
#include <string_view>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/mat3x3.hpp>

namespace openblack::gltf_creature
{
namespace
{
constexpr uint32_t k_NoBone = std::numeric_limits<uint32_t>::max();
/// A second joint lighter than this, as a share of the two heaviest, moves nothing
constexpr float k_SmallestBlend = 1e-3f;

/// Turns z round: from the file's axes to the game's and back
const glm::mat4 k_Flip = glm::scale(glm::vec3(1.0f, 1.0f, -1.0f));

glm::mat4 Flipped(const glm::mat4& m)
{
	return k_Flip * m * k_Flip;
}

glm::quat Quat(const std::array<float, 4>& q)
{
	return glm::normalize(glm::quat(q[3], q[0], q[1], q[2]));
}

glm::vec3 Vec3(const std::array<float, 4>& v)
{
	return {v[0], v[1], v[2]};
}

/// The bone n of a name "bone_<n>..."
std::optional<uint32_t> BoneNumber(std::string_view name)
{
	constexpr std::string_view k_Prefix = "bone_";
	if (!name.starts_with(k_Prefix))
	{
		return std::nullopt;
	}
	name.remove_prefix(k_Prefix.size());
	uint32_t number = 0;
	const auto [end, error] = std::from_chars(name.data(), name.data() + name.size(), number);
	if (error != std::errc {} || end == name.data())
	{
		return std::nullopt;
	}
	return number;
}

/// A property of a channel at a time, between its keys as the channel interpolates
std::array<float, 4> SampleChannel(const glb::Channel& channel, float seconds)
{
	const auto& times = channel.times;
	if (seconds <= times.front() || times.size() == 1)
	{
		return channel.values.front();
	}
	if (seconds >= times.back())
	{
		return channel.values.back();
	}
	const auto next = static_cast<size_t>(std::distance(times.begin(), std::ranges::upper_bound(times, seconds)));
	const auto previous = next - 1;
	const auto& from = channel.values[previous];
	const auto& to = channel.values[next];
	if (channel.interpolation == glb::Interpolation::Step)
	{
		return from;
	}
	const auto span = times[next] - times[previous];
	const auto t = span > 0.0f ? (seconds - times[previous]) / span : 0.0f;
	if (channel.target == glb::Target::Rotation)
	{
		const auto q = glm::slerp(Quat(from), Quat(to), t);
		return {q.x, q.y, q.z, q.w};
	}
	std::array<float, 4> value {};
	for (size_t i = 0; i < value.size(); ++i)
	{
		value.at(i) = from.at(i) + ((to.at(i) - from.at(i)) * t);
	}
	return value;
}

/// The node's matrix from a translation and a rotation. The bones of the game don't scale, so scale is left out.
glm::mat4 NodeMatrix(const glm::vec3& translation, const glm::quat& rotation)
{
	return glm::translate(translation) * glm::mat4_cast(rotation);
}

/// For each bone, its joint's node and the nodes between it and its parent bone's node, top first
struct Chains
{
	std::vector<std::vector<uint32_t>> nodes;
	std::vector<uint32_t> parents;
};

std::optional<Chains> ChainsOf(const glb::Model& model, std::span<const uint32_t> bones)
{
	if (!model.skin.has_value() || bones.size() != model.skin->joints.size())
	{
		return std::nullopt;
	}
	const auto& joints = model.skin->joints;
	std::vector<std::optional<uint32_t>> boneOfNode(model.nodes.size());
	for (size_t j = 0; j < joints.size(); ++j)
	{
		if (joints[j] >= model.nodes.size() || bones[j] >= bones.size())
		{
			return std::nullopt;
		}
		boneOfNode[joints[j]] = bones[j];
	}
	Chains chains {.nodes = std::vector<std::vector<uint32_t>>(bones.size()),
	               .parents = std::vector<uint32_t>(bones.size(), k_NoBone)};
	for (size_t j = 0; j < joints.size(); ++j)
	{
		const auto bone = bones[j];
		auto& chain = chains.nodes[bone];
		chain.push_back(joints[j]);
		for (auto parent = model.nodes[joints[j]].parent; parent.has_value(); parent = model.nodes[*parent].parent)
		{
			if (boneOfNode[*parent].has_value())
			{
				chains.parents[bone] = *boneOfNode[*parent];
				break;
			}
			chain.push_back(*parent);
		}
		std::ranges::reverse(chain);
		if (chains.parents[bone] != k_NoBone && chains.parents[bone] >= bone)
		{
			return std::nullopt;
		}
	}
	return chains;
}

/// A bone's matrix relative to its parent bone in the game's axes, at a time of an animation or at rest
glm::mat4 ChainMatrix(const glb::Model& model, const glb::Animation* animation, std::span<const uint32_t> chain, float seconds)
{
	glm::mat4 matrix(1.0f);
	for (const auto node : chain)
	{
		if (animation != nullptr)
		{
			matrix = matrix * SampleLocal(model, *animation, node, seconds);
		}
		else
		{
			const auto& n = model.nodes[node];
			matrix = matrix * NodeMatrix(glm::make_vec3(n.translation.data()), Quat(n.rotation));
		}
	}
	return Flipped(matrix);
}

/// The game's row-major rotation from the matrix's columns, in the form the poses keep
skeletal_animation::Matrix PoseRotation(const glm::mat4& m)
{
	skeletal_animation::Matrix rotation {};
	for (size_t r = 0; r < 3; ++r)
	{
		for (size_t c = 0; c < 3; ++c)
		{
			rotation.at(r).at(c) = m[static_cast<glm::length_t>(r)][static_cast<glm::length_t>(c)];
		}
	}
	return rotation;
}

} // namespace

std::vector<uint32_t> BonesOfJoints(const glb::Model& model)
{
	if (!model.skin.has_value())
	{
		return {};
	}
	const auto& joints = model.skin->joints;
	std::vector<uint32_t> bones(joints.size());
	std::iota(bones.begin(), bones.end(), 0u);
	std::vector<uint32_t> named;
	for (const auto joint : joints)
	{
		const auto number = joint < model.nodes.size() ? BoneNumber(model.nodes[joint].name) : std::nullopt;
		if (!number.has_value())
		{
			return bones;
		}
		named.push_back(*number);
	}
	auto sorted = named;
	std::ranges::sort(sorted);
	if (!std::ranges::equal(sorted, bones))
	{
		return bones;
	}
	return named;
}

std::vector<glm::mat4> Rest::World() const
{
	std::vector<glm::mat4> world(locals.size());
	for (size_t i = 0; i < locals.size(); ++i)
	{
		world[i] = parents[i] != k_NoBone && parents[i] < i ? world[parents[i]] * locals[i] : locals[i];
	}
	return world;
}

std::optional<Rest> RestOf(const glb::Model& model, std::span<const uint32_t> bones)
{
	const auto chains = ChainsOf(model, bones);
	if (!chains.has_value())
	{
		return std::nullopt;
	}
	Rest rest {.parents = chains->parents, .locals = {}};
	rest.locals.reserve(bones.size());
	for (const auto& chain : chains->nodes)
	{
		rest.locals.push_back(ChainMatrix(model, nullptr, chain, 0.0f));
	}
	return rest;
}

l3d::L3DFile BuildMesh(const glb::Model& model, std::span<const uint32_t> bones, const Rest& rest, const l3d::L3DTexture& skin)
{
	const auto world = rest.World();
	std::vector<glm::mat4> inverseWorld;
	std::ranges::transform(world, std::back_inserter(inverseWorld), [](const glm::mat4& m) { return glm::inverse(m); });

	struct Vertex
	{
		uint32_t bone;
		l3d::L3DVertex vertex;
	};
	struct Pull
	{
		uint32_t vertex;
		uint32_t towards;
		float weight;
	};
	std::vector<Vertex> vertices;
	std::vector<uint32_t> triangles;
	std::vector<Pull> pulls;
	const auto place = [&](uint32_t bone, const glm::vec3& position, const glm::vec3& normal, const glm::vec2& uv) {
		const auto local = glm::vec3(inverseWorld[bone] * glm::vec4(position, 1.0f));
		const auto turned = glm::normalize(glm::mat3(inverseWorld[bone]) * normal);
		return Vertex {.bone = bone,
		               .vertex = {.position = {local.x, local.y, local.z},
		                          .texCoord = {uv.x, uv.y},
		                          .normal = {turned.x, turned.y, turned.z}}};
	};

	// The vertices of the triangles first, then the partners in no triangle
	std::vector<Vertex> partners;
	std::vector<Pull> partnerPulls;
	for (const auto& primitive : model.primitives)
	{
		const auto first = static_cast<uint32_t>(vertices.size());
		for (size_t i = 0; i < primitive.positions.size(); ++i)
		{
			const auto& weights = primitive.weights.at(i);
			const auto& joints = primitive.joints.at(i);
			std::array<size_t, 4> order {0, 1, 2, 3};
			std::ranges::sort(order, [&weights](size_t a, size_t b) { return weights.at(a) > weights.at(b); });
			const auto boneOf = [&](size_t slot) {
				const auto joint = joints.at(slot);
				return joint < bones.size() ? bones[joint] : 0u;
			};
			const auto position = glm::vec3(k_Flip * glm::vec4(glm::make_vec3(primitive.positions[i].data()), 1.0f));
			const auto normal = glm::vec3(k_Flip * glm::vec4(glm::make_vec3(primitive.normals.at(i).data()), 0.0f));
			const auto uv = i < primitive.texCoords.size() ? glm::make_vec2(primitive.texCoords[i].data()) : glm::vec2(0.0f);
			const auto bone = boneOf(order[0]);
			vertices.push_back(place(bone, position, normal, uv));

			const auto heaviest = weights.at(order[0]);
			const auto second = weights.at(order[1]);
			const auto share = heaviest + second > 0.0f ? second / (heaviest + second) : 0.0f;
			if (share > k_SmallestBlend && boneOf(order[1]) != bone)
			{
				partnerPulls.push_back({.vertex = static_cast<uint32_t>(vertices.size() - 1),
				                        .towards = static_cast<uint32_t>(partners.size()),
				                        .weight = std::min(share, 0.5f)});
				partners.push_back(place(boneOf(order[1]), position, normal, uv));
			}
		}
		std::ranges::transform(primitive.indices, std::back_inserter(triangles),
		                       [first](uint32_t index) { return index + first; });
	}
	const auto triangleVertices = static_cast<uint32_t>(vertices.size());
	vertices.insert(vertices.end(), partners.begin(), partners.end());
	std::ranges::transform(partnerPulls, std::back_inserter(pulls), [triangleVertices](Pull pull) {
		pull.towards += triangleVertices;
		return pull;
	});

	// Each bone's vertices run together, as the groups give them a bone a run at a time
	std::vector<uint32_t> order(vertices.size());
	std::iota(order.begin(), order.end(), 0u);
	std::ranges::stable_sort(order, [&vertices](uint32_t a, uint32_t b) { return vertices[a].bone < vertices[b].bone; });
	std::vector<uint32_t> placeOf(vertices.size());
	for (uint32_t i = 0; i < order.size(); ++i)
	{
		placeOf[order[i]] = i;
	}

	l3d::L3DFile::Parts parts {};
	parts.flags = l3d::L3DMeshFlags::HasBones;
	for (const auto index : order)
	{
		const auto& vertex = vertices[index];
		parts.vertices.push_back(vertex.vertex);
		if (parts.vertexGroups.empty() || parts.vertexGroups.back().boneIndex != vertex.bone)
		{
			parts.vertexGroups.push_back({.vertexCount = 0, .boneIndex = static_cast<uint16_t>(vertex.bone)});
		}
		++parts.vertexGroups.back().vertexCount;
	}
	// Turning z round turns each triangle over: change the order of its corners so that its front stays in front
	for (size_t t = 0; t + 2 < triangles.size(); t += 3)
	{
		parts.indices.push_back(static_cast<uint16_t>(placeOf[triangles[t]]));
		parts.indices.push_back(static_cast<uint16_t>(placeOf[triangles[t + 2]]));
		parts.indices.push_back(static_cast<uint16_t>(placeOf[triangles[t + 1]]));
	}
	for (const auto& pull : pulls)
	{
		parts.blends.push_back(
		    {.indices = {static_cast<uint16_t>(placeOf[pull.vertex]), static_cast<uint16_t>(placeOf[pull.towards])},
		     .weight = pull.weight});
	}

	for (size_t i = 0; i < rest.locals.size(); ++i)
	{
		const auto& local = rest.locals[i];
		l3d::L3DBone bone {};
		bone.parent = rest.parents[i];
		bone.firstChild = k_NoBone;
		bone.rightSibling = k_NoBone;
		for (glm::length_t c = 0; c < 3; ++c)
		{
			for (glm::length_t r = 0; r < 3; ++r)
			{
				bone.orientation.at(static_cast<size_t>((c * 3) + r)) = local[c][r];
			}
		}
		bone.position = {local[3].x, local[3].y, local[3].z};
		parts.bones.push_back(bone);
	}
	// Each bone's first child and its next sibling
	for (uint32_t i = 0; i < parts.bones.size(); ++i)
	{
		const auto parent = parts.bones[i].parent;
		if (parent == k_NoBone)
		{
			continue;
		}
		auto* link = &parts.bones[parent].firstChild;
		while (*link != k_NoBone)
		{
			link = &parts.bones[*link].rightSibling;
		}
		*link = i;
	}

	l3d::L3DPrimitiveHeader primitive {};
	primitive.material.type = l3d::L3DMaterial::Type::Textured;
	primitive.material.alphaCutoutThreshold = 0;
	// Seen from both sides: the bones of the skeleton are thin
	primitive.material.cullMode = 1;
	primitive.material.skinID = skin.id;
	primitive.material.color.raw = 0xFFFFFFFF;
	primitive.numVertices = static_cast<uint32_t>(parts.vertices.size());
	primitive.numTriangles = static_cast<uint32_t>(parts.indices.size() / 3);
	primitive.numGroups = static_cast<uint32_t>(parts.vertexGroups.size());
	primitive.numVertexBlends = static_cast<uint32_t>(parts.blends.size());
	parts.primitives.push_back(primitive);

	l3d::L3DSubmeshHeader submesh {};
	submesh.flags.hasBones = 1;
	submesh.flags.unknown2 = 0b10;
	submesh.flags.lodMask = 0b111;
	submesh.numPrimitives = 1;
	submesh.numBones = static_cast<uint32_t>(parts.bones.size());
	parts.submeshes.push_back(submesh);
	parts.skins.push_back(skin);

	l3d::L3DFile file;
	file.Assemble(std::move(parts));
	return file;
}

glm::mat4 SampleLocal(const glb::Model& model, const glb::Animation& animation, uint32_t node, float seconds)
{
	const auto& rest = model.nodes.at(node);
	auto translation = glm::make_vec3(rest.translation.data());
	auto rotation = Quat(rest.rotation);
	for (const auto& channel : animation.channels)
	{
		if (channel.node != node)
		{
			continue;
		}
		const auto value = SampleChannel(channel, seconds);
		if (channel.target == glb::Target::Translation)
		{
			translation = Vec3(value);
		}
		else if (channel.target == glb::Target::Rotation)
		{
			rotation = Quat(value);
		}
	}
	return NodeMatrix(translation, rotation);
}

skeletal_animation::Animation ToAnimation(const glb::Model& model, const glb::Animation& animation,
                                          std::span<const uint32_t> bones, const skeletal_animation::Skeleton& skeleton,
                                          const ClipTiming& timing)
{
	using namespace skeletal_animation;
	const auto chains = ChainsOf(model, bones);
	const auto count = skeleton.parents.size();
	Animation out {
	    .duration = timing.durationMs,
	    .looping = timing.looping,
	    .rotatedJoints = {},
	    .translatedJoints = {},
	    .frames = {},
	    .displacement = timing.displacement,
	};
	if (!chains.has_value() || chains->nodes.size() != count || timing.frames == 0)
	{
		return out;
	}
	out.rotatedJoints.resize(count);
	std::iota(out.rotatedJoints.begin(), out.rotatedJoints.end(), 0u);
	out.translatedJoints = out.rotatedJoints;

	// A cycle's last keyframe leads back to its first; a pose range's last keyframe is its end
	const auto steps = timing.looping || timing.frames == 1 ? timing.frames : timing.frames - 1;
	for (size_t frame = 0; frame < timing.frames; ++frame)
	{
		// The file's animation is spread over the length the game plays it for
		const auto seconds = animation.Duration() * static_cast<float>(frame) / static_cast<float>(steps);
		auto& keyframe = out.frames.emplace_back();
		keyframe.eulerAngles.reserve(count);
		keyframe.translations.reserve(count);
		for (size_t bone = 0; bone < count; ++bone)
		{
			const auto local = ChainMatrix(model, &animation, chains->nodes[bone], seconds);
			// The keyframe rotation is the bone's turn from its rest, with the parents' rest turns taken off as the
			// game puts them on when it poses the bone
			auto turn = Multiply(skeleton.inverseRestRotations[bone], PoseRotation(local));
			const auto parent = skeleton.parents[bone];
			if (bone != 0 && parent != k_NoParent)
			{
				turn = Multiply(turn, skeleton.restRotations[parent]);
			}
			keyframe.eulerAngles.push_back(EulerYXZ(turn));
			keyframe.translations.emplace_back(local[3]);
		}
	}
	return out;
}

std::vector<std::optional<size_t>> MatchClips(std::span<const std::string> names, std::span<const glb::Animation> animations)
{
	const auto lower = [](std::string_view text) {
		std::string out(text);
		std::ranges::transform(out, out.begin(),
		                       [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; });
		return out;
	};
	std::vector<std::optional<size_t>> matches;
	matches.reserve(names.size());
	for (const auto& name : names)
	{
		const auto wanted = lower(name);
		const auto found =
		    std::ranges::find_if(animations, [&](const glb::Animation& animation) { return lower(animation.name) == wanted; });
		matches.push_back(found != animations.end()
		                      ? std::optional(static_cast<size_t>(std::distance(animations.begin(), found)))
		                      : std::nullopt);
	}
	return matches;
}

std::unique_ptr<l3d::L3DTexture> SkinFromRgba(std::span<const uint8_t> rgba, uint32_t width, uint32_t height, uint32_t id)
{
	auto made = std::make_unique<l3d::L3DTexture>();
	auto& skin = *made;
	skin.id = id;
	constexpr uint32_t k_Width = l3d::L3DTexture::k_Width;
	constexpr uint32_t k_Height = l3d::L3DTexture::k_Height;
	if (width == 0 || height == 0 || rgba.size() < static_cast<size_t>(width) * height * 4)
	{
		return made;
	}
	for (uint32_t y = 0; y < k_Height; ++y)
	{
		const auto top = (y * height) / k_Height;
		const auto bottom = std::max(top + 1, ((y + 1) * height) / k_Height);
		for (uint32_t x = 0; x < k_Width; ++x)
		{
			const auto left = (x * width) / k_Width;
			const auto right = std::max(left + 1, ((x + 1) * width) / k_Width);
			std::array<uint32_t, 4> sum {};
			for (auto sy = top; sy < bottom; ++sy)
			{
				for (auto sx = left; sx < right; ++sx)
				{
					const auto at = (static_cast<size_t>(sy) * width + sx) * 4;
					for (size_t c = 0; c < sum.size(); ++c)
					{
						sum.at(c) += rgba[at + c];
					}
				}
			}
			const auto area = (bottom - top) * (right - left);
			// Each channel keeps 4 bits, the nearest to its average
			const auto four = [area](uint32_t total) {
				return static_cast<uint16_t>(((total * 15) + (area * 255 / 2)) / (area * 255));
			};
			auto& texel = skin.texels.at(static_cast<size_t>(y) * k_Width + x);
			texel.r = four(sum[0]) & 0xFu;
			texel.g = four(sum[1]) & 0xFu;
			texel.b = four(sum[2]) & 0xFu;
			texel.a = four(sum[3]) & 0xFu;
		}
	}
	return made;
}

} // namespace openblack::gltf_creature
