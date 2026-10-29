/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GltfExport.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <tuple>
#include <vector>

#include <L3DFile.h>
#include <MorphFile.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/matrix.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/vector_relational.hpp>

#include "3D/SkeletalAnimation.h"
#include "3D/VertexBlend.h"
#include "Visual/RenderModes.h"

#define TINYGLTF_USE_CPP14
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"

#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

using namespace openblack;

namespace
{
constexpr uint32_t k_NoBone = std::numeric_limits<uint32_t>::max();
/// The animations are sampled at this rate. The player interpolates linearly between the samples.
constexpr uint32_t k_SampleStepMs = 1000 / 30;

/// All the binary data of the file goes into one buffer, each part aligned to four bytes
class BufferWriter
{
public:
	explicit BufferWriter(tinygltf::Model& model)
	    : _model(model)
	{
		_model.buffers.emplace_back();
	}

	int AddView(std::span<const uint8_t> bytes, int target)
	{
		auto& data = _model.buffers.front().data;
		data.resize((data.size() + 3) & ~size_t {3}, 0);
		tinygltf::BufferView view;
		view.buffer = 0;
		view.byteOffset = data.size();
		view.byteLength = bytes.size();
		view.target = target;
		data.insert(data.end(), bytes.begin(), bytes.end());
		_model.bufferViews.push_back(view);
		return static_cast<int>(_model.bufferViews.size() - 1);
	}

	template <typename T>
	int AddAccessor(std::span<const T> values, int componentType, int type, int target, bool bounds = false)
	{
		const auto bytes = std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(values.data()), values.size_bytes());
		tinygltf::Accessor accessor;
		accessor.bufferView = AddView(bytes, target);
		accessor.componentType = componentType;
		accessor.type = type;
		accessor.count = values.size();
		if constexpr (std::is_same_v<T, glm::vec3>)
		{
			if (bounds)
			{
				glm::vec3 low(std::numeric_limits<float>::max());
				glm::vec3 high(std::numeric_limits<float>::lowest());
				for (const auto& value : values)
				{
					low = glm::min(low, value);
					high = glm::max(high, value);
				}
				accessor.minValues = {low.x, low.y, low.z};
				accessor.maxValues = {high.x, high.y, high.z};
			}
		}
		if constexpr (std::is_same_v<T, float>)
		{
			if (bounds && !values.empty())
			{
				const auto [low, high] = std::ranges::minmax(values);
				accessor.minValues = {low};
				accessor.maxValues = {high};
			}
		}
		_model.accessors.push_back(accessor);
		return static_cast<int>(_model.accessors.size() - 1);
	}

private:
	tinygltf::Model& _model;
};

/// The rest pose of each bone in the space of the mesh, as the game builds it: the parent's matrix times the bone's own
std::vector<glm::mat4> RestMatrices(const l3d::L3DFile& l3d)
{
	const auto& bones = l3d.GetBones();
	std::vector<glm::mat4> matrices;
	matrices.reserve(bones.size());
	for (const auto& bone : bones)
	{
		const auto& o = bone.orientation;
		auto matrix = glm::mat4(o[0], o[1], o[2], 0.0f, o[3], o[4], o[5], 0.0f, o[6], o[7], o[8], 0.0f, bone.position.x,
		                        bone.position.y, bone.position.z, 1.0f);
		if (bone.parent != k_NoBone && bone.parent < matrices.size())
		{
			matrix = matrices[bone.parent] * matrix;
		}
		matrices.push_back(matrix);
	}
	return matrices;
}

/// Changes a point or a matrix from the game's axes to the axes of the file
struct Axes
{
	glm::mat4 flip {1.0f};

	[[nodiscard]] glm::mat4 Matrix(const glm::mat4& m) const { return flip * m * flip; }
	[[nodiscard]] glm::vec3 Point(const glm::vec3& p) const { return glm::vec3(flip * glm::vec4(p, 1.0f)); }
};

/// A matrix split into translation, rotation and scale
struct Trs
{
	glm::vec3 translation;
	glm::quat rotation;
	glm::vec3 scale;
};

Trs Decompose(const glm::mat4& m)
{
	const glm::vec3 scale(glm::length(glm::vec3(m[0])), glm::length(glm::vec3(m[1])), glm::length(glm::vec3(m[2])));
	const glm::mat3 rotation {glm::vec3(m[0]) / scale.x, glm::vec3(m[1]) / scale.y, glm::vec3(m[2]) / scale.z};
	return {.translation = glm::vec3(m[3]), .rotation = glm::normalize(glm::quat_cast(rotation)), .scale = scale};
}

/// Some bones of the game are mirror images (their matrices turn space inside out). A glTF joint cannot be one, so each
/// such joint is given the bone's frame with its x axis turned round. The skin's inverse bind matrices use the same
/// frames, so that the vertices stay where the bones put them.
struct JointFrames
{
	std::vector<bool> mirrored;
	std::vector<uint32_t> parents;

	JointFrames(std::span<const glm::mat4> rest, std::span<const uint32_t> boneParents, const Axes& axes)
	    : parents(boneParents.begin(), boneParents.end())
	{
		std::ranges::transform(rest, std::back_inserter(mirrored),
		                       [&axes](const glm::mat4& m) { return glm::determinant(glm::mat3(axes.Matrix(m))) < 0.0f; });
	}

	[[nodiscard]] glm::mat4 Fix(uint32_t bone) const
	{
		return bone < mirrored.size() && mirrored[bone]
		           ? glm::mat4(-1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f)
		           : glm::mat4(1.0f);
	}

	/// A joint's matrix relative to its parent joint, from the bone's matrix relative to its parent bone in the file's
	/// axes
	[[nodiscard]] glm::mat4 Local(uint32_t bone, const glm::mat4& local) const
	{
		const auto parent = parents[bone];
		return (parent == k_NoBone ? glm::mat4(1.0f) : Fix(parent)) * local * Fix(bone);
	}
};

/// The data of one submesh, with each vertex in the space of the mesh in its rest pose
struct BindVertices
{
	std::vector<glm::vec3> positions;
	std::vector<glm::vec3> normals;
	std::vector<glm::vec2> uvs;
	/// The bone of each vertex, and the bone and weight of the vertex it is blended towards
	std::vector<uint32_t> bones;
	std::vector<uint32_t> partnerBones;
	std::vector<float> partnerWeights;
};

BindVertices PlaceVertices(const l3d::L3DFile& l3d, uint32_t submesh, std::span<const glm::mat4> rest, const Axes& axes,
                           std::span<const uint32_t> baseBones, std::span<const vertex_blend::Partner> partners)
{
	const auto vertices = l3d.GetVertexSpan(submesh);
	BindVertices out;
	out.positions.resize(vertices.size());
	out.normals.resize(vertices.size());
	out.uvs.resize(vertices.size());
	for (size_t i = 0; i < vertices.size(); ++i)
	{
		const auto& vertex = vertices[i];
		const auto bone = i < baseBones.size() ? baseBones[i] : k_NoBone;
		const glm::mat4 place = bone < rest.size() ? rest[bone] : glm::mat4(1.0f);
		const auto position = glm::vec3(place * glm::vec4(vertex.position.x, vertex.position.y, vertex.position.z, 1.0f));
		const auto normal =
		    glm::transpose(glm::inverse(glm::mat3(place))) * glm::vec3(vertex.normal.x, vertex.normal.y, vertex.normal.z);
		out.positions[i] = axes.Point(position);
		out.normals[i] = glm::normalize(glm::mat3(axes.flip) * normal);
		out.uvs[i] = {vertex.texCoord.x, vertex.texCoord.y};
	}
	// At the seams a vertex is drawn part of the way to its partner, each placed by its own bone. Only the position
	// moves.
	std::vector<glm::vec3> placed(out.positions);
	vertex_blend::Apply(placed, partners);
	out.positions = std::move(placed);
	return out;
}

std::string Hex(uint32_t value)
{
	std::array<char, 16> text {};
	std::snprintf(text.data(), text.size(), "%08X", value);
	return text.data();
}

int AddSkinImage(tinygltf::Model& model, BufferWriter& buffer, const l3d::L3DTexture& skin)
{
	constexpr auto k_Size = static_cast<int>(l3d::L3DTexture::k_Width);
	std::vector<uint8_t> rgba(skin.texels.size() * 4);
	for (size_t i = 0; i < skin.texels.size(); ++i)
	{
		const auto& texel = skin.texels[i];
		rgba[(i * 4) + 0] = static_cast<uint8_t>(texel.r * 17);
		rgba[(i * 4) + 1] = static_cast<uint8_t>(texel.g * 17);
		rgba[(i * 4) + 2] = static_cast<uint8_t>(texel.b * 17);
		rgba[(i * 4) + 3] = static_cast<uint8_t>(texel.a * 17);
	}
	std::vector<uint8_t> png;
	stbi_write_png_to_func(
	    [](void* context, void* data, int size) {
		    auto& out = *static_cast<std::vector<uint8_t>*>(context);
		    const auto* bytes = static_cast<const uint8_t*>(data);
		    out.insert(out.end(), bytes, bytes + size);
	    },
	    &png, k_Size, static_cast<int>(l3d::L3DTexture::k_Height), 4, rgba.data(), k_Size * 4);

	tinygltf::Image image;
	image.name = "skin_" + Hex(skin.id);
	image.mimeType = "image/png";
	image.bufferView = buffer.AddView(png, 0);
	model.images.push_back(image);

	tinygltf::Texture texture;
	texture.name = image.name;
	texture.source = static_cast<int>(model.images.size() - 1);
	texture.sampler = 0;
	model.textures.push_back(texture);
	return static_cast<int>(model.textures.size() - 1);
}

int AddMaterial(tinygltf::Model& model, const l3d::L3DMaterial& material, const std::map<uint32_t, int>& textures)
{
	const auto mode = static_cast<graphics::render_modes::Mode>(material.type);
	const auto& desc = graphics::render_modes::Desc(mode);
	tinygltf::Material out;
	out.name = "material_" + std::to_string(model.materials.size()) + "_skin_" + Hex(material.skinID);
	out.pbrMetallicRoughness.metallicFactor = 0.0;
	out.pbrMetallicRoughness.roughnessFactor = 1.0;
	const auto texture = textures.find(material.skinID);
	if (desc.textured && texture != textures.end())
	{
		out.pbrMetallicRoughness.baseColorTexture.index = texture->second;
	}
	else
	{
		const auto& c = material.color.bgra;
		out.pbrMetallicRoughness.baseColorFactor = {c.r / 255.0, c.g / 255.0, c.b / 255.0, 1.0};
	}
	using graphics::render_modes::Blend;
	if (desc.alphaTest || desc.blend == Blend::JustZ)
	{
		out.alphaMode = "MASK";
		out.alphaCutoff = material.alphaCutoutThreshold / 255.0;
	}
	else if (desc.blend == Blend::Standard || desc.blend == Blend::Additive)
	{
		out.alphaMode = "BLEND";
	}
	else
	{
		out.alphaMode = "OPAQUE";
	}
	out.doubleSided = (material.cullMode & 1u) != 0;
	tinygltf::Value::Object extras;
	extras["l3dRenderMode"] = tinygltf::Value(static_cast<int>(material.type));
	extras["l3dAlphaCutoutThreshold"] = tinygltf::Value(static_cast<int>(material.alphaCutoutThreshold));
	extras["l3dCullMode"] = tinygltf::Value(static_cast<int>(material.cullMode));
	extras["additive"] = tinygltf::Value(desc.blend == Blend::Additive);
	out.extras = tinygltf::Value(extras);
	model.materials.push_back(out);
	return static_cast<int>(model.materials.size() - 1);
}

/// The bone of each vertex of a submesh: the groups give a bone to each run of vertices in turn
std::vector<uint32_t> VertexBones(const l3d::L3DFile& l3d, uint32_t submesh)
{
	const auto count = l3d.GetVertexSpan(submesh).size();
	std::vector<uint32_t> bones;
	bones.reserve(count);
	if (l3d.GetSubmeshHeaders()[submesh].flags.hasBones)
	{
		for (const auto& group : l3d.GetVertexGroupSpan(submesh))
		{
			for (uint32_t i = 0; i < group.vertexCount && bones.size() < count; ++i)
			{
				bones.push_back(group.boneIndex);
			}
		}
		// The game places the vertices that no group names with the first bone
		bones.resize(count, 0);
	}
	else
	{
		bones.resize(count, k_NoBone);
	}
	return bones;
}

std::vector<vertex_blend::Partner> VertexPartners(const l3d::L3DFile& l3d, uint32_t submesh)
{
	std::vector<uint32_t> primitiveVertices;
	std::vector<uint32_t> primitiveBlends;
	for (const auto& primitive : l3d.GetPrimitiveSpan(submesh))
	{
		primitiveVertices.push_back(primitive.numVertices);
		const bool read = primitive.vertexBlendsOffset != std::numeric_limits<uint32_t>::max();
		primitiveBlends.push_back(read ? primitive.numVertexBlends : 0);
	}
	std::vector<vertex_blend::Blend> blends;
	for (const auto& blend : l3d.GetBlendSpan(submesh))
	{
		blends.push_back({.vertex = blend.indices[0], .towards = blend.indices[1], .weight = blend.weight});
	}
	return vertex_blend::Partners(primitiveVertices, primitiveBlends, blends);
}

bool SameLayout(const l3d::L3DFile& a, const l3d::L3DFile& b)
{
	if (a.GetSubmeshHeaders().size() != b.GetSubmeshHeaders().size())
	{
		return false;
	}
	for (uint32_t s = 0; s < a.GetSubmeshHeaders().size(); ++s)
	{
		const auto pa = a.GetPrimitiveSpan(s);
		const auto pb = b.GetPrimitiveSpan(s);
		if (pa.size() != pb.size() ||
		    !std::ranges::equal(pa, pb, [](const auto& x, const auto& y) { return x.numVertices == y.numVertices; }))
		{
			return false;
		}
	}
	return true;
}

float LargestBoneDifference(std::span<const glm::mat4> a, std::span<const glm::mat4> b)
{
	if (a.size() != b.size())
	{
		return std::numeric_limits<float>::infinity();
	}
	float largest = 0.0f;
	for (size_t i = 0; i < a.size(); ++i)
	{
		for (glm::length_t c = 0; c < 4; ++c)
		{
			for (glm::length_t r = 0; r < 4; ++r)
			{
				largest = std::max(largest, std::abs(a[i][c][r] - b[i][c][r]));
			}
		}
	}
	return largest;
}

std::vector<std::string> BoneNames(size_t count, const morph::MorphFile* animations)
{
	std::vector<std::string> names(count);
	for (size_t i = 0; i < count; ++i)
	{
		names[i] = "bone_" + std::to_string(i);
	}
	if (animations == nullptr || !animations->GetCreatureActionPoints().has_value())
	{
		return names;
	}
	const auto& points = *animations->GetCreatureActionPoints();
	const std::array<std::pair<int32_t, const char*>, 9> known {{
	    {points.rightHand, "right_hand"},
	    {points.rightFoot, "right_foot"},
	    {points.rightArmpit, "right_armpit"},
	    {points.belly, "belly"},
	    {points.head, "head"},
	    {points.groin, "groin"},
	    {points.leashBone, "leash"},
	    {points.rightEye, "right_eye"},
	    {points.unknownBone, "action_point"},
	}};
	for (const auto& [bone, name] : known)
	{
		if (bone >= 0 && static_cast<size_t>(bone) < count)
		{
			names[static_cast<size_t>(bone)] += std::string("_") + name;
		}
	}
	return names;
}

void AddAnimations(tinygltf::Model& model, BufferWriter& buffer, const morph::MorphFile& file,
                   std::span<const uint32_t> parents, std::span<const glm::mat4> rest, std::span<const int> boneNodes,
                   const Axes& axes, const JointFrames& joints)
{
	const auto skeleton = skeletal_animation::Skeleton::FromRestMatrices(parents, rest);
	const auto* standSource = file.GetBaseAnimation(0);
	if (standSource == nullptr)
	{
		return;
	}
	const auto stand = skeletal_animation::FromMorph(*standSource);
	size_t specIndex = 0;
	for (const auto& set : file.GetAnimationSpecs().animationSets)
	{
		for (const auto& desc : set.animations)
		{
			const auto* source = file.GetBaseAnimation(specIndex++);
			if (source == nullptr || source->keyframes.empty())
			{
				continue;
			}
			const auto animation = skeletal_animation::FromMorph(*source);
			std::vector<uint32_t> times;
			for (uint32_t t = 0; t < animation.duration; t += k_SampleStepMs)
			{
				times.push_back(t);
			}
			times.push_back(animation.duration);

			const auto boneCount = boneNodes.size();
			std::vector<std::vector<glm::vec3>> translations(boneCount);
			std::vector<std::vector<glm::quat>> rotations(boneCount);
			for (const auto time : times)
			{
				const auto poses = skeletal_animation::SampleCycle(animation, stand, time, skeleton);
				for (size_t b = 0; b < boneCount && b < poses.size(); ++b)
				{
					glm::mat4 local(1.0f);
					for (glm::length_t r = 0; r < 3; ++r)
					{
						for (glm::length_t c = 0; c < 3; ++c)
						{
							local[r][c] = poses[b].rotation.at(static_cast<size_t>(r)).at(static_cast<size_t>(c));
						}
					}
					local[3] = glm::vec4(poses[b].translation, 1.0f);
					const auto trs = Decompose(joints.Local(static_cast<uint32_t>(b), axes.Matrix(local)));
					auto rotation = trs.rotation;
					// Keep each rotation on the same side as the one before it, so that the player turns through the smaller
					// angle
					if (!rotations[b].empty() && glm::dot(rotations[b].back(), rotation) < 0.0f)
					{
						rotation = -rotation;
					}
					translations[b].push_back(trs.translation);
					rotations[b].push_back(rotation);
				}
			}

			std::vector<float> seconds;
			seconds.reserve(times.size());
			std::ranges::transform(times, std::back_inserter(seconds), [](uint32_t t) { return t / 1000.0f; });
			tinygltf::Animation out;
			out.name = set.name + "/" + desc.name;
			const auto input = buffer.AddAccessor<float>(seconds, TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_SCALAR, 0, true);
			for (size_t b = 0; b < boneCount; ++b)
			{
				if (translations[b].empty())
				{
					continue;
				}
				std::vector<glm::vec4> quats;
				std::ranges::transform(rotations[b], std::back_inserter(quats),
				                       [](const glm::quat& q) { return glm::vec4(q.x, q.y, q.z, q.w); });
				const std::array<std::pair<const char*, int>, 2> outputs {{
				    {"translation",
				     buffer.AddAccessor<glm::vec3>(translations[b], TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_VEC3, 0)},
				    {"rotation", buffer.AddAccessor<glm::vec4>(quats, TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_VEC4, 0)},
				}};
				for (const auto& [path, output] : outputs)
				{
					tinygltf::AnimationSampler sampler;
					sampler.input = input;
					sampler.output = output;
					sampler.interpolation = "LINEAR";
					out.samplers.push_back(sampler);
					tinygltf::AnimationChannel channel;
					channel.sampler = static_cast<int>(out.samplers.size() - 1);
					channel.target_node = boneNodes[b];
					channel.target_path = path;
					out.channels.push_back(channel);
				}
			}
			tinygltf::Value::Object extras;
			extras["durationMs"] = tinygltf::Value(static_cast<int>(animation.duration));
			extras["looping"] = tinygltf::Value(animation.looping);
			out.extras = tinygltf::Value(extras);
			model.animations.push_back(out);
		}
	}
}
} // namespace

int l3dtool::ExportGltf(const GltfExportOptions& options, const std::filesystem::path& output) noexcept
{
	l3d::L3DFile l3d;
	if (const auto result = l3d.Open(options.mesh); result != l3d::L3DResult::Success)
	{
		std::cerr << options.mesh.string() << ": " << l3d::ResultToStr(result) << '\n';
		return EXIT_FAILURE;
	}
	const Axes axes {.flip = options.rightHanded ? glm::mat4(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f,
	                                                         0.0f, 0.0f, 0.0f, 0.0f, 1.0f)
	                                             : glm::mat4(1.0f)};
	const auto rest = RestMatrices(l3d);

	// The morph targets that have the same layout as the base mesh
	std::vector<std::pair<std::string, l3d::L3DFile>> targets;
	tinygltf::Value::Array targetSources;
	for (const auto& target : options.targets)
	{
		l3d::L3DFile file;
		if (const auto result = file.Open(target.mesh); result != l3d::L3DResult::Success)
		{
			std::cerr << "Warn: morph target " << target.name << " (" << target.mesh.string()
			          << ") not read: " << l3d::ResultToStr(result) << '\n';
			continue;
		}
		if (!SameLayout(l3d, file))
		{
			std::cerr << "Warn: morph target " << target.name << " has other vertices than the base mesh, left out\n";
			continue;
		}
		// The skeleton does not change with the morph targets. A target whose bones are in other places is placed with
		// its own bones, so that at full weight it is the target mesh in its own rest pose.
		const auto difference = LargestBoneDifference(rest, RestMatrices(file));
		if (difference > 1e-3f)
		{
			std::cerr << "Note: the bones of morph target " << target.name << " differ from the base mesh's by up to "
			          << difference << "\n";
		}
		targets.emplace_back(target.name, std::move(file));
		targetSources.emplace_back(target.mesh.filename().string());
	}

	tinygltf::Model model;
	model.asset.version = "2.0";
	model.asset.generator = "openblack l3dtool extract";
	BufferWriter buffer(model);

	tinygltf::Sampler sampler;
	sampler.magFilter = TINYGLTF_TEXTURE_FILTER_LINEAR;
	sampler.minFilter = TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR;
	sampler.wrapS = TINYGLTF_TEXTURE_WRAP_REPEAT;
	sampler.wrapT = TINYGLTF_TEXTURE_WRAP_REPEAT;
	model.samplers.push_back(sampler);
	std::map<uint32_t, int> textures;
	for (const auto& skin : l3d.GetSkins())
	{
		textures.emplace(skin.id, AddSkinImage(model, buffer, skin));
	}
	std::map<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>, int> materials;

	// The scene's root, then one node for each bone in the order of the file
	auto name = options.mesh.stem().string();
	model.nodes.emplace_back().name = name;
	const auto& bones = l3d.GetBones();
	const auto boneNames = BoneNames(bones.size(), options.animations);
	std::vector<int> boneNodes;
	std::vector<uint32_t> parents;
	for (uint32_t i = 0; i < bones.size(); ++i)
	{
		parents.push_back(bones[i].parent < i ? bones[i].parent : k_NoBone);
	}
	const JointFrames joints(rest, parents, axes);
	for (uint32_t i = 0; i < bones.size(); ++i)
	{
		const auto parent = parents[i];
		const glm::mat4 local = parent == k_NoBone ? rest[i] : glm::inverse(rest[parent]) * rest[i];
		const auto trs = Decompose(joints.Local(i, axes.Matrix(local)));
		tinygltf::Node node;
		node.name = boneNames[i];
		node.translation = {trs.translation.x, trs.translation.y, trs.translation.z};
		node.rotation = {trs.rotation.x, trs.rotation.y, trs.rotation.z, trs.rotation.w};
		if (glm::any(glm::greaterThan(glm::abs(trs.scale - glm::vec3(1.0f)), glm::vec3(1e-4f))))
		{
			node.scale = {trs.scale.x, trs.scale.y, trs.scale.z};
		}
		model.nodes.push_back(node);
		boneNodes.push_back(static_cast<int>(model.nodes.size() - 1));
		const auto parentNode = parent == k_NoBone ? 0 : boneNodes[parent];
		model.nodes[static_cast<size_t>(parentNode)].children.push_back(boneNodes.back());
	}

	int skinIndex = -1;
	std::vector<int> meshNodes;
	if (!bones.empty())
	{
		std::vector<glm::mat4> inverseBinds;
		for (uint32_t i = 0; i < rest.size(); ++i)
		{
			auto inverseBind = glm::inverse(axes.Matrix(rest[i]) * joints.Fix(i));
			// glTF wants the last row of an affine matrix to be exactly 0, 0, 0, 1
			inverseBind = glm::row(inverseBind, 3, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
			inverseBinds.push_back(inverseBind);
		}
		tinygltf::Skin skin;
		skin.name = name + "_skin";
		skin.joints = boneNodes;
		skin.inverseBindMatrices =
		    buffer.AddAccessor<glm::mat4>(inverseBinds, TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_MAT4, 0);
		const auto root = std::ranges::find(parents, k_NoBone);
		skin.skeleton = boneNodes[static_cast<size_t>(std::distance(parents.begin(), root))];
		model.skins.push_back(skin);
		skinIndex = 0;
	}

	for (uint32_t s = 0; s < l3d.GetSubmeshHeaders().size(); ++s)
	{
		const auto vertexBones = VertexBones(l3d, s);
		const auto partners = VertexPartners(l3d, s);
		const auto base = PlaceVertices(l3d, s, rest, axes, vertexBones, partners);
		std::vector<BindVertices> placedTargets;
		for (const auto& [targetName, file] : targets)
		{
			const auto targetRest = RestMatrices(file);
			placedTargets.push_back(PlaceVertices(file, s, targetRest.size() == rest.size() ? std::span(targetRest) : rest,
			                                      axes, vertexBones, partners));
		}

		tinygltf::Mesh mesh;
		const auto& header = l3d.GetSubmeshHeaders()[s];
		mesh.name = name + (l3d.GetSubmeshHeaders().size() > 1 ? "_submesh" + std::to_string(s) : std::string());
		uint32_t first = 0;
		const auto indices = l3d.GetIndexSpan(s);
		uint32_t firstIndex = 0;
		for (const auto& primitive : l3d.GetPrimitiveSpan(s))
		{
			const auto count = primitive.numVertices;
			const auto range = [first, count](const auto& values) { return std::span(values).subspan(first, count); };
			tinygltf::Primitive out;
			out.mode = TINYGLTF_MODE_TRIANGLES;
			out.attributes["POSITION"] = buffer.AddAccessor<glm::vec3>(range(base.positions), TINYGLTF_COMPONENT_TYPE_FLOAT,
			                                                           TINYGLTF_TYPE_VEC3, TINYGLTF_TARGET_ARRAY_BUFFER, true);
			out.attributes["NORMAL"] = buffer.AddAccessor<glm::vec3>(range(base.normals), TINYGLTF_COMPONENT_TYPE_FLOAT,
			                                                         TINYGLTF_TYPE_VEC3, TINYGLTF_TARGET_ARRAY_BUFFER);
			out.attributes["TEXCOORD_0"] = buffer.AddAccessor<glm::vec2>(range(base.uvs), TINYGLTF_COMPONENT_TYPE_FLOAT,
			                                                             TINYGLTF_TYPE_VEC2, TINYGLTF_TARGET_ARRAY_BUFFER);
			if (skinIndex >= 0)
			{
				std::vector<std::array<uint16_t, 4>> vertexJoints(count);
				std::vector<glm::vec4> weights(count);
				for (uint32_t v = 0; v < count; ++v)
				{
					const auto own = static_cast<uint16_t>(std::min<uint32_t>(vertexBones[first + v], 0xFFFF));
					vertexJoints[v] = {own, 0, 0, 0};
					weights[v] = {1.0f, 0.0f, 0.0f, 0.0f};
					const auto& partner = partners[first + v];
					if (partner.Blended())
					{
						const auto other = static_cast<uint16_t>(vertexBones[static_cast<size_t>(partner.vertex)]);
						if (other != own)
						{
							vertexJoints[v][1] = other;
							weights[v] = {1.0f - partner.weight, partner.weight, 0.0f, 0.0f};
						}
					}
				}
				out.attributes["JOINTS_0"] = buffer.AddAccessor<std::array<uint16_t, 4>>(
				    vertexJoints, TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT, TINYGLTF_TYPE_VEC4, TINYGLTF_TARGET_ARRAY_BUFFER);
				out.attributes["WEIGHTS_0"] = buffer.AddAccessor<glm::vec4>(weights, TINYGLTF_COMPONENT_TYPE_FLOAT,
				                                                            TINYGLTF_TYPE_VEC4, TINYGLTF_TARGET_ARRAY_BUFFER);
			}
			for (const auto& target : placedTargets)
			{
				std::vector<glm::vec3> positions(count);
				std::vector<glm::vec3> normals(count);
				for (uint32_t v = 0; v < count; ++v)
				{
					positions[v] = target.positions[first + v] - base.positions[first + v];
					normals[v] = target.normals[first + v] - base.normals[first + v];
				}
				std::map<std::string, int> attributes;
				attributes["POSITION"] = buffer.AddAccessor<glm::vec3>(positions, TINYGLTF_COMPONENT_TYPE_FLOAT,
				                                                       TINYGLTF_TYPE_VEC3, TINYGLTF_TARGET_ARRAY_BUFFER, true);
				attributes["NORMAL"] = buffer.AddAccessor<glm::vec3>(normals, TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_VEC3,
				                                                     TINYGLTF_TARGET_ARRAY_BUFFER);
				out.targets.push_back(attributes);
			}
			const auto source = indices.subspan(firstIndex, primitive.numTriangles * 3);
			std::vector<uint16_t> triangles(source.begin(), source.end());
			// A flipped axis turns each triangle over: change the order of its corners so that its front stays in front
			if (options.rightHanded)
			{
				for (size_t t = 0; t + 2 < triangles.size(); t += 3)
				{
					std::swap(triangles[t + 1], triangles[t + 2]);
				}
			}
			out.indices = buffer.AddAccessor<uint16_t>(triangles, TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT, TINYGLTF_TYPE_SCALAR,
			                                           TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER);
			const auto& material = primitive.material;
			const auto key = std::tuple(material.skinID, static_cast<uint32_t>(material.type),
			                            static_cast<uint32_t>(material.cullMode), material.color.raw);
			auto found = materials.find(key);
			if (found == materials.end())
			{
				found = materials.emplace(key, AddMaterial(model, material, textures)).first;
			}
			out.material = found->second;
			mesh.primitives.push_back(out);
			first += count;
			firstIndex += primitive.numTriangles * 3;
		}
		if (!targets.empty())
		{
			mesh.weights.assign(targets.size(), 0.0);
			tinygltf::Value::Array targetNames;
			for (const auto& [targetName, file] : targets)
			{
				targetNames.emplace_back(targetName);
			}
			tinygltf::Value::Object extras;
			extras["targetNames"] = tinygltf::Value(targetNames);
			extras["targetSources"] = tinygltf::Value(targetSources);
			mesh.extras = tinygltf::Value(extras);
		}
		tinygltf::Value::Object meshExtras =
		    mesh.extras.IsObject() ? mesh.extras.Get<tinygltf::Value::Object>() : tinygltf::Value::Object {};
		meshExtras["l3dLodMask"] = tinygltf::Value(static_cast<int>(header.flags.lodMask));
		mesh.extras = tinygltf::Value(meshExtras);
		model.meshes.push_back(mesh);

		tinygltf::Node node;
		node.name = mesh.name;
		node.mesh = static_cast<int>(model.meshes.size() - 1);
		node.skin = skinIndex;
		model.nodes.push_back(node);
		// A skinned mesh does not move with its parents, so its node goes at the top of the scene
		meshNodes.push_back(static_cast<int>(model.nodes.size() - 1));
	}

	if (options.animations != nullptr && !bones.empty())
	{
		AddAnimations(model, buffer, *options.animations, parents, rest, boneNodes, axes, joints);
	}

	tinygltf::Scene scene;
	scene.name = name;
	scene.nodes.push_back(0);
	scene.nodes.insert(scene.nodes.end(), meshNodes.begin(), meshNodes.end());
	tinygltf::Value::Object notes;
	for (const auto& [key, value] : options.notes)
	{
		notes[key] = tinygltf::Value(value);
	}
	notes["source"] = tinygltf::Value(options.mesh.filename().string());
	scene.extras = tinygltf::Value(notes);
	model.scenes.push_back(scene);
	model.defaultScene = 0;

	const bool binary = output.extension() == ".glb";
	tinygltf::TinyGLTF writer;
	if (!writer.WriteGltfSceneToFile(&model, output.string(), true, true, !binary, binary))
	{
		std::cerr << "Err: Failed to write glTF.\n";
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
