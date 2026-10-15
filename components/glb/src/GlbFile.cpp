/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE

#include "GlbFile.h"

#include <cstring>

#include <algorithm>
#include <fstream>
#include <limits>

#include "tiny_gltf.h"

namespace openblack::glb
{
namespace
{

/// The images are not used: the loader keeps their bytes as they are
bool KeepImage(tinygltf::Image* /*unused*/, const int /*unused*/, std::string* /*unused*/, std::string* /*unused*/,
               int /*unused*/, int /*unused*/, const unsigned char* /*unused*/, int /*unused*/, void* /*unused*/)
{
	return true;
}

/// One component of an accessor as a float, normalised integers mapped to 0 to 1
std::optional<float> Component(const uint8_t* at, int componentType, bool normalized)
{
	switch (componentType)
	{
	case TINYGLTF_COMPONENT_TYPE_FLOAT:
	{
		float value;
		std::memcpy(&value, at, sizeof(value));
		return value;
	}
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
		return normalized ? static_cast<float>(*at) / 255.0f : static_cast<float>(*at);
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
	{
		uint16_t value;
		std::memcpy(&value, at, sizeof(value));
		return normalized ? static_cast<float>(value) / 65535.0f : static_cast<float>(value);
	}
	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
	{
		uint32_t value;
		std::memcpy(&value, at, sizeof(value));
		return static_cast<float>(value);
	}
	default:
		return std::nullopt;
	}
}

/// The elements of an accessor, each N floats wide (extra components are dropped, missing ones are zero)
template <size_t N>
std::optional<std::vector<std::array<float, N>>> ReadAccessor(const tinygltf::Model& gltf, int index)
{
	if (index < 0 || static_cast<size_t>(index) >= gltf.accessors.size())
	{
		return std::nullopt;
	}
	const auto& accessor = gltf.accessors[static_cast<size_t>(index)];
	if (accessor.bufferView < 0 || static_cast<size_t>(accessor.bufferView) >= gltf.bufferViews.size())
	{
		return std::nullopt;
	}
	const auto& view = gltf.bufferViews[static_cast<size_t>(accessor.bufferView)];
	const auto& buffer = gltf.buffers.at(static_cast<size_t>(view.buffer)).data;
	const auto width = static_cast<size_t>(tinygltf::GetNumComponentsInType(static_cast<uint32_t>(accessor.type)));
	const auto componentSize =
	    static_cast<size_t>(tinygltf::GetComponentSizeInBytes(static_cast<uint32_t>(accessor.componentType)));
	const auto stride = view.byteStride != 0 ? view.byteStride : width * componentSize;
	const auto start = view.byteOffset + accessor.byteOffset;
	if (accessor.count > 0 && start + (stride * (accessor.count - 1)) + (width * componentSize) > buffer.size())
	{
		return std::nullopt;
	}
	std::vector<std::array<float, N>> values(accessor.count);
	for (size_t i = 0; i < accessor.count; ++i)
	{
		auto& value = values[i];
		value.fill(0.0f);
		for (size_t c = 0; c < std::min(width, N); ++c)
		{
			const auto component = Component(buffer.data() + start + (stride * i) + (c * componentSize), accessor.componentType,
			                                 accessor.normalized);
			if (!component)
			{
				return std::nullopt;
			}
			value.at(c) = *component;
		}
	}
	return values;
}

GlbResult ReadPrimitive(const tinygltf::Model& gltf, const tinygltf::Primitive& source, Primitive& out)
{
	const auto attribute = [&source](const char* name) {
		const auto found = source.attributes.find(name);
		return found != source.attributes.end() ? found->second : -1;
	};
	auto positions = ReadAccessor<3>(gltf, attribute("POSITION"));
	auto normals = ReadAccessor<3>(gltf, attribute("NORMAL"));
	auto joints = ReadAccessor<4>(gltf, attribute("JOINTS_0"));
	auto weights = ReadAccessor<4>(gltf, attribute("WEIGHTS_0"));
	auto indices = ReadAccessor<1>(gltf, source.indices);
	if (!positions || !normals || !joints || !weights || !indices)
	{
		return GlbResult::ErrBadAccessor;
	}
	out.positions = std::move(*positions);
	out.normals = std::move(*normals);
	out.weights = std::move(*weights);
	out.joints.reserve(joints->size());
	std::ranges::transform(*joints, std::back_inserter(out.joints), [](const std::array<float, 4>& j) {
		return std::array<uint16_t, 4> {static_cast<uint16_t>(j[0]), static_cast<uint16_t>(j[1]), static_cast<uint16_t>(j[2]),
		                                static_cast<uint16_t>(j[3])};
	});
	out.indices.reserve(indices->size());
	std::ranges::transform(*indices, std::back_inserter(out.indices),
	                       [](const std::array<float, 1>& i) { return static_cast<uint32_t>(i[0]); });
	if (const auto uv = attribute("TEXCOORD_0"); uv >= 0)
	{
		auto texCoords = ReadAccessor<2>(gltf, uv);
		if (!texCoords)
		{
			return GlbResult::ErrBadAccessor;
		}
		out.texCoords = std::move(*texCoords);
	}
	return GlbResult::Success;
}

std::optional<Channel> ReadChannel(const tinygltf::Model& gltf, const tinygltf::Animation& animation,
                                   const tinygltf::AnimationChannel& channel)
{
	if (channel.target_node < 0 || channel.sampler < 0 || static_cast<size_t>(channel.sampler) >= animation.samplers.size())
	{
		return std::nullopt;
	}
	Channel out {};
	out.node = static_cast<uint32_t>(channel.target_node);
	if (channel.target_path == "translation")
	{
		out.target = Target::Translation;
	}
	else if (channel.target_path == "rotation")
	{
		out.target = Target::Rotation;
	}
	else if (channel.target_path == "scale")
	{
		out.target = Target::Scale;
	}
	else
	{
		return std::nullopt;
	}
	const auto& sampler = animation.samplers[static_cast<size_t>(channel.sampler)];
	out.interpolation = sampler.interpolation == "STEP"          ? Interpolation::Step
	                    : sampler.interpolation == "CUBICSPLINE" ? Interpolation::CubicSpline
	                                                             : Interpolation::Linear;
	auto times = ReadAccessor<1>(gltf, sampler.input);
	auto values = ReadAccessor<4>(gltf, sampler.output);
	if (!times || !values)
	{
		return std::nullopt;
	}
	std::ranges::transform(*times, std::back_inserter(out.times), [](const std::array<float, 1>& t) { return t[0]; });
	if (out.interpolation == Interpolation::CubicSpline)
	{
		// Each key is an in-tangent, a value and an out-tangent: keep the values
		for (size_t i = 1; i < values->size(); i += 3)
		{
			out.values.push_back(values->at(i));
		}
	}
	else
	{
		out.values = std::move(*values);
	}
	if (out.values.size() != out.times.size() || out.times.empty())
	{
		return std::nullopt;
	}
	return out;
}

} // namespace

float Animation::Duration() const
{
	float duration = 0.0f;
	for (const auto& channel : channels)
	{
		duration = std::max(duration, channel.times.back());
	}
	return duration;
}

std::string_view ResultToStr(GlbResult result)
{
	switch (result)
	{
	case GlbResult::Success:
		return "success";
	case GlbResult::ErrParse:
		return "the file is not a valid glb";
	case GlbResult::ErrNoSkinnedMesh:
		return "the file has no skinned mesh";
	case GlbResult::ErrBadAccessor:
		return "an accessor of the file is not valid";
	}
	return "unknown";
}

GlbResult Read(std::span<const uint8_t> bytes, Model& model)
{
	tinygltf::TinyGLTF loader;
	loader.SetImageLoader(KeepImage, nullptr);
	tinygltf::Model gltf;
	std::string err;
	std::string warn;
	if (bytes.size() > std::numeric_limits<unsigned int>::max() ||
	    !loader.LoadBinaryFromMemory(&gltf, &err, &warn, bytes.data(), static_cast<unsigned int>(bytes.size())))
	{
		return GlbResult::ErrParse;
	}

	model = {};
	model.nodes.resize(gltf.nodes.size());
	for (size_t i = 0; i < gltf.nodes.size(); ++i)
	{
		const auto& source = gltf.nodes[i];
		auto& node = model.nodes[i];
		node.name = source.name;
		if (source.translation.size() == 3)
		{
			std::ranges::transform(source.translation, node.translation.begin(),
			                       [](double v) { return static_cast<float>(v); });
		}
		if (source.rotation.size() == 4)
		{
			std::ranges::transform(source.rotation, node.rotation.begin(), [](double v) { return static_cast<float>(v); });
		}
		if (source.scale.size() == 3)
		{
			std::ranges::transform(source.scale, node.scale.begin(), [](double v) { return static_cast<float>(v); });
		}
	}
	for (size_t i = 0; i < gltf.nodes.size(); ++i)
	{
		for (const auto child : gltf.nodes[i].children)
		{
			if (child >= 0 && static_cast<size_t>(child) < model.nodes.size())
			{
				model.nodes[static_cast<size_t>(child)].parent = static_cast<uint32_t>(i);
			}
		}
	}

	const auto meshNode =
	    std::ranges::find_if(gltf.nodes, [](const tinygltf::Node& node) { return node.mesh >= 0 && node.skin >= 0; });
	if (meshNode == gltf.nodes.end() || static_cast<size_t>(meshNode->skin) >= gltf.skins.size() ||
	    static_cast<size_t>(meshNode->mesh) >= gltf.meshes.size())
	{
		return GlbResult::ErrNoSkinnedMesh;
	}
	const auto& skin = gltf.skins[static_cast<size_t>(meshNode->skin)];
	auto inverseBinds = ReadAccessor<16>(gltf, skin.inverseBindMatrices);
	if (!inverseBinds || inverseBinds->size() != skin.joints.size())
	{
		return GlbResult::ErrBadAccessor;
	}
	auto& outSkin = model.skin.emplace();
	std::ranges::transform(skin.joints, std::back_inserter(outSkin.joints), [](int j) { return static_cast<uint32_t>(j); });
	outSkin.inverseBindMatrices = std::move(*inverseBinds);

	for (const auto& primitive : gltf.meshes[static_cast<size_t>(meshNode->mesh)].primitives)
	{
		if (primitive.mode != -1 && primitive.mode != TINYGLTF_MODE_TRIANGLES)
		{
			continue;
		}
		if (const auto result = ReadPrimitive(gltf, primitive, model.primitives.emplace_back()); result != GlbResult::Success)
		{
			return result;
		}
	}

	for (const auto& animation : gltf.animations)
	{
		auto& out = model.animations.emplace_back();
		out.name = animation.name;
		for (const auto& channel : animation.channels)
		{
			if (auto read = ReadChannel(gltf, animation, channel))
			{
				out.channels.push_back(std::move(*read));
			}
		}
		if (out.channels.empty())
		{
			model.animations.pop_back();
		}
	}
	return GlbResult::Success;
}

} // namespace openblack::glb
