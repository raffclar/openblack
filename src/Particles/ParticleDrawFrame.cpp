/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ParticleDrawFrame.h"

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <bit>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <glm/geometric.hpp>
#include <glm/matrix.hpp>

#include "Common/ModelInverseSquareRoot.h"
#include "Common/X87Matrix.h"
#include "Graphics/ZSort.h"
#include "ParticleCreators.h"
#include "ParticleSurfaces.h"
#include "VolumeGlow.h"

using namespace openblack;
using namespace openblack::particles;
using namespace openblack::particles::draw;
using openblack::graphics::render_modes::Mode;

namespace
{
/// A player's glows run through 32 frames of the sprite sheet backwards, one a little faster than the other, and the
/// second turns, each by so much a millisecond
constexpr float k_GlowFrames = 32.0f;
constexpr std::array<float, 2> k_GlowFrameRates {0.02f, 0.023f};
constexpr float k_GlowSpinRate = 0.002f;
constexpr float k_TwoPi = 2.0f * std::numbers::pi_v<float>;
/// The glows are this much bigger than the symbol, and this faint of its alpha, out of 256
constexpr float k_GlowScale = 1.5f;
constexpr uint32_t k_GlowAlpha = 100;
/// The glows' sheet and the players' symbols, four to a row
constexpr std::string_view k_GlowTexture = "S_SpriteSheet3";
constexpr std::string_view k_SymbolTexture = "PlayersSymbols";
constexpr int k_GlowCellsPerRow = 8;
/// A mist's animation counts so many a millisecond up to 900, then starts again
constexpr float k_MistCountRate = 0.255f;
constexpr int k_MistCountWrap = 900;
/// A light map lies this far across the land from its atom
constexpr glm::vec3 k_LightMapOffset {10.0f, 0.0f, 10.0f};
constexpr float k_MillisecondsPerSecond = 1000.0f;
constexpr float k_ByteMax = 255.0f;
/// A model's shape comes from its axes only when they are this long
constexpr float k_MinimumAxis = 1e-4f;

/// The game's float for 1 / 255, which a stamp's alpha is multiplied by
constexpr float k_InverseByte = std::bit_cast<float>(0x3B808081u);

uint32_t Abgr(const std::array<uint8_t, 3>& rgb, float alpha)
{
	const auto a = static_cast<uint32_t>(std::clamp(alpha, 0.0f, k_ByteMax));
	return (a << 24u) | (static_cast<uint32_t>(rgb[2]) << 16u) | (static_cast<uint32_t>(rgb[1]) << 8u) | rgb[0];
}

/// A glow's frame at an age: it starts at the first and runs backwards
int GlowCell(float milliseconds, float rate)
{
	const float run = std::fmod(milliseconds * rate, k_GlowFrames);
	const float phase = run > 0.0f ? k_GlowFrames - run : 0.0f;
	return static_cast<int>(phase);
}

/// Sprites added to what is behind without writing depth give the same picture in any order
bool Commutes(const Frame& frame, const Item& item)
{
	if (item.kind != ItemKind::Sprite)
	{
		return false;
	}
	const auto& mode = graphics::render_modes::Desc(frame.materials[frame.spriteMaterials[item.index]].mode);
	return mode.blend == graphics::render_modes::Blend::Additive && !mode.zWrite;
}

/// Each run of items that commute is gathered by material, keeping their order otherwise, so that it takes a draw for
/// each material rather than one each time the material changes
void GatherCommutingRuns(const Frame& frame, std::vector<uint32_t>& items)
{
	const auto materialOf = [&frame](uint32_t index) { return frame.spriteMaterials[frame.items[index].index]; };
	auto first = items.begin();
	while (first != items.end())
	{
		if (!Commutes(frame, frame.items[*first]))
		{
			++first;
			continue;
		}
		const auto last =
		    std::find_if(first, items.end(), [&frame](uint32_t index) { return !Commutes(frame, frame.items[index]); });
		// By the material each first comes in, so the run starts with the material it started with. Each item gets the
		// place of its material in that order one time. Then each item goes after the items of the places before its own
		// place, and after the items of its own place that came before it.
		std::vector<uint32_t> order;
		std::vector<uint32_t> counts;
		std::vector<std::pair<uint32_t, uint32_t>> ranked;
		ranked.reserve(static_cast<size_t>(last - first));
		for (auto it = first; it != last; ++it)
		{
			const auto material = materialOf(*it);
			auto place = std::ranges::find(order, material);
			if (place == order.end())
			{
				place = order.insert(order.end(), material);
				counts.push_back(0);
			}
			const auto rank = static_cast<uint32_t>(place - order.begin());
			++counts[rank];
			ranked.emplace_back(rank, *it);
		}
		uint32_t start = 0;
		for (auto& count : counts)
		{
			start += std::exchange(count, start);
		}
		for (const auto& [rank, item] : ranked)
		{
			first[counts[rank]++] = item;
		}
		first = last;
	}
}

sprites::SpriteInstance FacingSprite(const glm::vec3& position, float halfWidth, float angle, int cell, int cellsPerRow,
                                     const glm::vec4& colour)
{
	const auto uv = maths::SpriteCellUv(cell, cellsPerRow);
	return {
	    .positionHalfWidth = {position, halfWidth},
	    .shape = {halfWidth, angle, 0.0f, 0.0f},
	    .uv = {uv.corner, uv.size},
	    .colour = colour,
	    .flags = glm::vec4(0.0f),
	};
}

/// Adds one item of a group, as the frame builds it
class GroupBuilder
{
public:
	GroupBuilder(Frame& frame, const Sources& sources)
	    : _frame(frame)
	    , _sources(sources)
	{
	}

	void Sprite(const sprites::SpriteInstance& instance, std::string_view texture, Mode mode)
	{
		const auto ids = _sources.textures ? _sources.textures(texture) : std::nullopt;
		if (!ids.has_value())
		{
			return;
		}
		const auto material = _frame.MaterialIndex({.texture = ids->first, .alphaTexture = ids->second, .mode = mode});
		_frame.items.push_back({.kind = ItemKind::Sprite,
		                        .index = static_cast<uint32_t>(_frame.sprites.size()),
		                        .sortPoint = glm::vec3(instance.positionHalfWidth)});
		_frame.sprites.push_back(instance);
		_frame.spriteMaterials.push_back(material);
	}

private:
	Frame& _frame;
	const Sources& _sources;
};

/// A colour as the shader takes it, from the lowest byte: red, green, blue, alpha
uint32_t AbgrOf(surface::Argb argb)
{
	return (argb & 0xFF00FF00u) | ((argb >> 16u) & 0xFFu) | ((argb & 0xFFu) << 16u);
}

/// A surface of revolution: the atom's own surface placed in the world by the atom's frame, laid over the land again
/// when asked, its colours tinted by the atom's, its texture slid on
void AddSurface(Frame& frame, const SurfaceCreator& creator, const Effect::DrawAtom& atom, const Sources& sources)
{
	const auto ids = sources.textures ? sources.textures(creator.texture) : std::nullopt;
	if (!ids.has_value() || atom.surface == nullptr || atom.surface->mesh.vertices.empty())
	{
		return;
	}
	const auto& mesh = atom.surface->mesh;
	glm::mat4 toWorld(atom.rotation);
	toWorld[0] *= atom.scale;
	toWorld[1] *= atom.scale * atom.stretch;
	toWorld[2] *= atom.scale;
	toWorld[3] = glm::vec4(atom.position, 1.0f);
	const auto land = [&sources](glm::vec3 point) {
		return sources.landHeight ? sources.landHeight({point.x, point.z}) : 0.0f;
	};
	const float landAtMiddle = creator.clampToLand ? land(atom.position) : 0.0f;
	// The atom's colour and its drawn alpha tint every point, unless it is plain white
	const auto alpha = static_cast<uint32_t>(std::clamp(atom.alpha, 0.0f, k_ByteMax));
	const surface::Argb tint =
	    (alpha << 24u) | (static_cast<uint32_t>(atom.rgb[0]) << 16u) | (static_cast<uint32_t>(atom.rgb[1]) << 8u) | atom.rgb[2];
	const auto uvOffset = SurfaceUvOffset(*atom.surface, creator, atom.fraction);
	const auto mode = creator.additive
	                      ? (creator.writeDepth ? Mode::AlphaTexturedAlphaAdditive : Mode::AlphaTexturedAlphaAdditiveNz)
	                      : (creator.writeDepth ? Mode::AlphaTexturedAlpha : Mode::AlphaTexturedAlphaNz);
	const auto material = frame.MaterialIndex({.texture = ids->first, .alphaTexture = ids->second, .mode = mode});
	const auto firstVertex = static_cast<uint32_t>(frame.surfaceVertices.size());
	const bool lit = creator.lit && mesh.normals.size() == mesh.vertices.size();
	for (size_t i = 0; i < mesh.vertices.size(); ++i)
	{
		const auto& vertex = mesh.vertices[i];
		auto position = glm::vec3(toWorld * glm::vec4(vertex.position, 1.0f));
		if (creator.clampToLand)
		{
			position.y += land(position) - landAtMiddle;
		}
		const auto colour = tint != 0xFFFFFFFFu ? surface::Modulate(vertex.colour, tint) : vertex.colour;
		frame.surfaceVertices.push_back({
		    .position = position,
		    .uv = vertex.uv + uvOffset,
		    .abgr = AbgrOf(colour),
		    .specularAbgr = AbgrOf(vertex.specular),
		});
		frame.surfaceNormals.push_back(lit ? mesh.normals[i] : glm::vec3(0.0f));
	}
	const auto firstIndex = static_cast<uint32_t>(frame.surfaceIndices.size());
	frame.surfaceIndices.insert(frame.surfaceIndices.end(), mesh.indices.begin(), mesh.indices.end());
	frame.items.push_back(
	    {.kind = ItemKind::Surface, .index = static_cast<uint32_t>(frame.surfaces.size()), .sortPoint = atom.position});
	frame.surfaces.push_back({
	    .material = material,
	    .firstVertex = firstVertex,
	    .vertexCount = static_cast<uint32_t>(mesh.vertices.size()),
	    .firstIndex = firstIndex,
	    .indexCount = static_cast<uint32_t>(mesh.indices.size()),
	    .doubleSided = creator.doubleSided,
	    .lit = lit,
	    .worldToAtom = x87::Inverse(toWorld),
	});
}

/// The shell an atom lays over an object's model, its texture slid on by the game's time since it was made
void AddShell(Frame& frame, const Effect::DrawAtom& atom, const ObjectGlow& glow, const Sources& sources)
{
	const auto name = std::string("raw/") + std::string(volume_glow::k_Textures.at(static_cast<size_t>(glow.texture)));
	const auto position = sources.objectPosition ? sources.objectPosition(glow.object) : std::nullopt;
	frame.items.push_back({.kind = ItemKind::Shell,
	                       .index = static_cast<uint32_t>(frame.shells.size()),
	                       .sortPoint = position.value_or(atom.position)});
	frame.shells.push_back({
	    .object = glow.object,
	    .zoom = glow.zoom,
	    .argb = glow.argb,
	    .texture = entt::hashed_string(name.c_str()).value(),
	    .alphaTexture = entt::hashed_string((name + "a").c_str()).value(),
	    .uvOffset = volume_glow::UvOffset(glow.animSpeed, volume_glow::SinceStarted(sources.gameMilliseconds, glow.startedAt)),
	    .textureFraction = glow.textureFraction,
	});
}

/// The shower of sparks an atom sends up from its object, each spark as strong as the atom is seen; nothing once the object
/// has gone
void AddShower(GroupBuilder& builder, const Effect::DrawAtom& atom, const ObjectShower& shower, const Sources& sources)
{
	const auto from = sources.objectPosition ? sources.objectPosition(shower.object) : std::nullopt;
	if (!shower.sparks.has_value() || !from.has_value())
	{
		return;
	}
	const auto strength = static_cast<uint8_t>(std::clamp(atom.alpha, 0.0f, k_ByteMax));
	for (size_t i = 0; i < rising_sparks::k_Sparks; ++i)
	{
		const auto look = rising_sparks::LookOf(*shower.sparks, i, *from, strength);
		if (!look.has_value())
		{
			continue;
		}
		builder.Sprite(FacingSprite(look->position, look->halfSize, look->angle, static_cast<int>(look->picture),
		                            static_cast<int>(rising_sparks::k_SparkSheetPictures),
		                            glm::vec4(1.0f, 1.0f, 1.0f, static_cast<float>(look->alpha) / k_ByteMax)),
		               rising_sparks::k_SparkSheet, Mode::AlphaTexturedAlphaAdditiveNz);
	}
}

void AddAtom(Frame& frame, GroupBuilder& builder, const Effect::DrawAtom& atom, int player, const Sources& sources,
             const glm::vec3& origin)
{
	if (atom.objectGlow.has_value())
	{
		AddShell(frame, atom, *atom.objectGlow, sources);
		return;
	}
	if (atom.shower != nullptr)
	{
		AddShower(builder, atom, *atom.shower, sources);
		return;
	}
	const auto& creator = *atom.creator;
	switch (creator.kind)
	{
	case Creator::Kind::Sprite:
		builder.Sprite(sprites::InstanceOf(atom), creator.texture, sprites::RenderMode(creator));
		break;
	case Creator::Kind::Symbol:
	{
		// An atom may show a player of its own, as each of a village's symbols of belief does
		const int shown = atom.player >= 0 ? atom.player : player;
		const uint32_t rgb = sources.playerColour ? sources.playerColour(shown) : 0xFFFFFFu;
		const auto symbol = SymbolOf(atom, rgb, shown, sources.symbolsInARow);
		// Added to what is behind, writing no depth
		const auto additive = Mode::AlphaTexturedAlphaAdditiveNz;
		builder.Sprite(symbol.instances[0], k_GlowTexture, additive);
		builder.Sprite(symbol.instances[1], k_GlowTexture, additive);
		builder.Sprite(symbol.instances[2], k_SymbolTexture, additive);
		break;
	}
	case Creator::Kind::Mesh:
	{
		const auto& mesh = static_cast<const MeshCreator&>(creator);
		if (!mesh.mesh.has_value())
		{
			break;
		}
		glm::mat3 axes = atom.rotation * atom.scale;
		axes[1] *= atom.stretch;
		glm::vec2 uv(0.0f);
		if (mesh.type == MeshCreator::Type::AnimTextured)
		{
			uv = mesh.UvOffset(maths::FrameIndex(atom.frame, mesh.FramesPerAtom(), mesh.loopAnim));
		}
		frame.items.push_back(
		    {.kind = ItemKind::Mesh, .index = static_cast<uint32_t>(frame.meshes.size()), .sortPoint = atom.position});
		frame.meshes.push_back({
		    .creator = &creator,
		    .mesh = *mesh.mesh,
		    .axes = axes,
		    .position = atom.position,
		    .colour = glm::vec4(glm::vec3(atom.rgb[0], atom.rgb[1], atom.rgb[2]) / k_ByteMax,
		                        std::clamp(atom.alpha / k_ByteMax, 0.0f, 1.0f)),
		    .uvOffset = uv,
		    .cutBelow = mesh.drawCutByPlane ? std::optional<float>(origin.y) : std::nullopt,
		});
		break;
	}
	case Creator::Kind::Mist:
	{
		const auto& mist = static_cast<const MistCreator&>(creator);
		float shape = atom.creatorValue.x;
		// The shape may come from the atom's stretch, as a storm's clouds gather
		const glm::mat3 matrix =
		    atom.rotation * glm::mat3(atom.scale, 0.0f, 0.0f, 0.0f, atom.scale * atom.stretch, 0.0f, 0.0f, 0.0f, atom.scale);
		if (mist.ratioFromMatrix && std::abs(matrix[0][0]) > k_MinimumAxis)
		{
			shape = matrix[1][1] / matrix[0][0];
		}
		const auto counted = static_cast<int>(atom.age * k_MillisecondsPerSecond * k_MistCountRate);
		const auto alpha = static_cast<uint32_t>(std::clamp(atom.alpha, 0.0f, k_ByteMax));
		frame.items.push_back(
		    {.kind = ItemKind::Mist, .index = static_cast<uint32_t>(frame.mists.size()), .sortPoint = atom.position});
		frame.mists.push_back({
		    .position = atom.position,
		    .size = atom.scale,
		    .argb = (alpha << 24u) | (static_cast<uint32_t>(atom.rgb[0]) << 16u) | (static_cast<uint32_t>(atom.rgb[1]) << 8u) |
		            atom.rgb[2],
		    .shape = shape,
		    .counter = (static_cast<int>(atom.creatorValue.y) + counted) % k_MistCountWrap,
		});
		// A mist with a light map stamps its first frame on the land under it, as strongly as it is opaque. A grey map
		// is a shadow, as a storm's clouds cast.
		if (mist.lightMap.has_value())
		{
			frame.lightStamps.push_back({
			    .bitmap = *mist.lightMap,
			    .frame = 0,
			    .pitch = mist.lightMapPitch,
			    .centre = atom.position + k_LightMapOffset,
			    .strength = StampStrength(static_cast<uint8_t>(alpha)),
			    .shadow = mist.shadowMap,
			});
		}
		break;
	}
	case Creator::Kind::LightMap:
	{
		const auto& light = static_cast<const LightMapCreator&>(creator);
		if (!light.bitmap.has_value())
		{
			break;
		}
		glm::vec3 position = atom.position;
		// It moves about every frame when asked to: the first number across z, the next up, the last across x
		if (light.useJitter && sources.random)
		{
			const float first = sources.random(light.jitter);
			const float second = sources.random(light.jitter);
			const float third = sources.random(light.jitter);
			position += glm::vec3(third, second, first);
		}
		frame.lightStamps.push_back({
		    .bitmap = *light.bitmap,
		    .frame = maths::FrameIndex(atom.frame, light.numFrames, light.loopAnim) & 0xFF,
		    .pitch = light.pitch,
		    .centre = position + k_LightMapOffset,
		    .strength = StampStrength(static_cast<uint8_t>(std::clamp(atom.alpha, 0.0f, k_ByteMax))),
		});
		break;
	}
	case Creator::Kind::Surface:
		AddSurface(frame, static_cast<const SurfaceCreator&>(creator), atom, sources);
		break;
	case Creator::Kind::Fragment:
		if (atom.fragment != nullptr)
		{
			frame.items.push_back({.kind = ItemKind::Fragment,
			                       .index = static_cast<uint32_t>(frame.fragments.size()),
			                       .sortPoint = atom.position});
			frame.fragments.push_back({
			    .shape = atom.fragment,
			    .axes = atom.rotation * atom.scale,
			    .position = atom.position,
			    .rgb = atom.rgb,
			    .alpha = std::clamp(atom.alpha / k_ByteMax, 0.0f, 1.0f),
			});
		}
		break;
	case Creator::Kind::Point:
	case Creator::Kind::Chain:
	case Creator::Kind::Other:
		break;
	}
}
} // namespace

float openblack::particles::draw::StampStrength(uint8_t alpha)
{
	return static_cast<float>(alpha) * k_InverseByte;
}

void Frame::Clear()
{
	materials.clear();
	sprites.clear();
	spriteMaterials.clear();
	chainVertices.clear();
	chainJoints.clear();
	chains.clear();
	meshes.clear();
	mists.clear();
	surfaceVertices.clear();
	surfaceIndices.clear();
	surfaceNormals.clear();
	surfaces.clear();
	fragments.clear();
	shells.clear();
	lightStamps.clear();
	items.clear();
	groups.clear();
}

uint32_t Frame::MaterialIndex(const Material& material)
{
	const auto found = std::ranges::find(materials, material);
	if (found != materials.end())
	{
		return static_cast<uint32_t>(std::distance(materials.begin(), found));
	}
	materials.push_back(material);
	return static_cast<uint32_t>(materials.size() - 1);
}

SymbolSprites draw::SymbolOf(const Effect::DrawAtom& atom, uint32_t playerRgb, int player, int symbolsInARow)
{
	// The symbol takes the player's colour times the atom's; the glows are a faint copy, the second white
	const auto channel = [&](int shift, uint8_t value) {
		return static_cast<float>((((playerRgb >> static_cast<uint32_t>(shift)) & 0xFFu) * value) >> 8u);
	};
	const auto atomAlpha = static_cast<uint32_t>(std::clamp(atom.alpha, 0.0f, k_ByteMax));
	const uint32_t symbolAlpha = (atomAlpha * 0xFFu) >> 8u;
	const uint32_t glowAlpha = (symbolAlpha * k_GlowAlpha) >> 8u;
	const glm::vec3 rgb = glm::vec3(channel(16, atom.rgb[0]), channel(8, atom.rgb[1]), channel(0, atom.rgb[2])) / k_ByteMax;
	const float milliseconds = atom.age * k_MillisecondsPerSecond;
	const float spin = std::fmod(milliseconds * k_GlowSpinRate, k_TwoPi);
	const float glow = static_cast<float>(glowAlpha) / k_ByteMax;
	const float scale = std::max(atom.scale, sprites::k_MinimumHalfWidth);
	return {{
	    FacingSprite(atom.position, scale * k_GlowScale, 0.0f, GlowCell(milliseconds, k_GlowFrameRates[0]), k_GlowCellsPerRow,
	                 glm::vec4(rgb, glow)),
	    FacingSprite(atom.position, scale * k_GlowScale, spin, GlowCell(milliseconds, k_GlowFrameRates[1]), k_GlowCellsPerRow,
	                 glm::vec4(1.0f, 1.0f, 1.0f, glow)),
	    FacingSprite(atom.position, scale, 0.0f, std::max(player, 0), symbolsInARow,
	                 glm::vec4(rgb, static_cast<float>(symbolAlpha) / k_ByteMax)),
	}};
}

void draw::AppendChain(std::vector<ChainVertex>& out, std::vector<ChainJoint>& outJoints, const Creator& creator,
                       std::span<const Effect::DrawAtom> joints, int textureRepeats, float textureScroll)
{
	if (joints.size() < 2)
	{
		return;
	}
	const auto& chain = static_cast<const ChainCreator&>(creator);
	const auto segments = static_cast<int>(joints.size()) - 1;
	for (const auto& joint : joints)
	{
		outJoints.push_back({.position = joint.position, .halfWidth = joint.scale});
	}
	for (int i = 0; i < segments; ++i)
	{
		const auto& head = joints[static_cast<size_t>(i)];
		const auto& tail = joints[static_cast<size_t>(i) + 1];
		const auto uv = chain.SegmentUv(i, segments, textureRepeats, textureScroll);
		const uint32_t headColour = Abgr(head.rgb, head.alpha);
		const uint32_t tailColour = Abgr(tail.rgb, tail.alpha);
		// The first joint's two sides, then the next joint's; each pass places them
		out.push_back({.position = head.position, .uv = uv[0], .abgr = headColour});
		out.push_back({.position = head.position, .uv = uv[1], .abgr = headColour});
		out.push_back({.position = tail.position, .uv = uv[2], .abgr = tailColour});
		out.push_back({.position = tail.position, .uv = uv[3], .abgr = tailColour});
	}
}

void draw::PlaceChainCorners(std::span<ChainVertex> corners, std::span<const ChainJoint> joints, const glm::vec3& eye)
{
	if (joints.size() < 2)
	{
		return;
	}
	const size_t segments = std::min(joints.size() - 1, corners.size() / 4);
	for (size_t i = 0; i < segments; ++i)
	{
		const auto along = joints[i + 1].position - joints[i].position;
		for (size_t end = 0; end < 2; ++end)
		{
			const auto& joint = joints[i + end];
			// The side is square to the segment and to the line to the eye. The 3D engine's inverse square root makes it
			// about one long.
			const auto toEye = eye - joint.position;
			glm::vec3 side {(toEye.y * along.z) - (toEye.z * along.y), (toEye.z * along.x) - (along.z * toEye.x),
			                (along.y * toEye.x) - (toEye.y * along.x)};
			side *= gutils::ModelInverseSquareRoot(((side.z * side.z) + (side.x * side.x)) + (side.y * side.y));
			// Then the side gets the length of the joint's half width
			if (side != glm::vec3(0.0f))
			{
				side *= joint.halfWidth / std::sqrt(((side.z * side.z) + (side.y * side.y)) + (side.x * side.x));
			}
			corners[(i * 4) + (end * 2)].position = joint.position + side;
			corners[(i * 4) + (end * 2) + 1].position = joint.position - side;
		}
	}
	// Where two segments meet, their corners on each side meet half way
	for (size_t i = 1; i < segments; ++i)
	{
		for (size_t side = 0; side < 2; ++side)
		{
			auto& before = corners[((i - 1) * 4) + 2 + side].position;
			auto& after = corners[(i * 4) + side].position;
			const auto middle = (before + after) * 0.5f;
			before = middle;
			after = middle;
		}
	}
}

void draw::AddEffect(Frame& frame, const Effect::DrawWalk& walk, DrawPath path, const glm::vec3& origin, int player,
                     const Sources& sources)
{
	const auto firstItem = static_cast<uint32_t>(frame.items.size());
	const int shown = player >= 0 ? player : k_NeutralPlayer;
	GroupBuilder builder(frame, sources);
	for (const auto& step : walk.steps)
	{
		if (!step.chain)
		{
			AddAtom(frame, builder, walk.atoms.at(step.index), shown, sources, origin);
			continue;
		}
		const auto& chain = walk.chains.at(step.index);
		const auto ids = sources.textures ? sources.textures(chain.creator->texture) : std::nullopt;
		if (!ids.has_value())
		{
			continue;
		}
		const auto joints = std::span(walk.joints).subspan(chain.firstJoint, chain.jointCount);
		const auto material = frame.MaterialIndex(
		    {.texture = ids->first, .alphaTexture = ids->second, .mode = sprites::RenderMode(*chain.creator)});
		// It takes its place by its middle joint
		frame.items.push_back({.kind = ItemKind::Chain,
		                       .index = static_cast<uint32_t>(frame.chains.size()),
		                       .sortPoint = joints[joints.size() / 2].position});
		frame.chains.push_back({.material = material,
		                        .firstVertex = static_cast<uint32_t>(frame.chainVertices.size()),
		                        .segments = chain.jointCount - 1,
		                        .firstJoint = static_cast<uint32_t>(frame.chainJoints.size())});
		AppendChain(frame.chainVertices, frame.chainJoints, *chain.creator, joints, chain.textureRepeats, chain.textureScroll);
	}
	const auto count = static_cast<uint32_t>(frame.items.size()) - firstItem;
	if (count > 0)
	{
		frame.groups.push_back({.path = path, .origin = origin, .firstItem = firstItem, .itemCount = count});
	}
}

void draw::AddPlayerSymbol(Frame& frame, const PlayerSymbol& symbol, const Sources& sources)
{
	const auto firstItem = static_cast<uint32_t>(frame.items.size());
	Effect::DrawAtom atom {};
	atom.position = symbol.position;
	atom.scale = symbol.scale;
	atom.alpha = k_ByteMax;
	atom.rgb = {0xFF, 0xFF, 0xFF};
	atom.age = symbol.milliseconds / k_MillisecondsPerSecond;
	const uint32_t rgb = sources.playerColour ? sources.playerColour(symbol.player) : 0xFFFFFFu;
	const auto sprites = SymbolOf(atom, rgb, symbol.player, sources.symbolsInARow);
	GroupBuilder builder(frame, sources);
	// Added to what is behind, writing no depth, all three at once where the symbol stands
	const auto additive = Mode::AlphaTexturedAlphaAdditiveNz;
	builder.Sprite(sprites.instances[0], k_GlowTexture, additive);
	builder.Sprite(sprites.instances[1], k_GlowTexture, additive);
	builder.Sprite(sprites.instances[2], k_SymbolTexture, additive);
	const auto count = static_cast<uint32_t>(frame.items.size()) - firstItem;
	if (count > 0)
	{
		frame.groups.push_back(
		    {.path = DrawPath::Queued, .origin = symbol.position, .firstItem = firstItem, .itemCount = count});
	}
}

void draw::AddFightMarks(Frame& frame, std::span<const FightMark> marks)
{
	if (marks.empty())
	{
		return;
	}
	static constexpr auto k_Sheet = entt::hashed_string("raw/blobs");
	static constexpr auto k_SheetAlpha = entt::hashed_string("raw/blobsa");
	// Added to what is behind, writing no depth
	const auto material = frame.MaterialIndex(
	    {.texture = k_Sheet.value(), .alphaTexture = k_SheetAlpha.value(), .mode = Mode::AlphaTexturedAlphaAdditiveNz});
	for (const auto& mark : marks)
	{
		const auto firstItem = static_cast<uint32_t>(frame.items.size());
		// Square, facing the screen, unturned and centred on where it sits
		frame.items.push_back(
		    {.kind = ItemKind::Sprite, .index = static_cast<uint32_t>(frame.sprites.size()), .sortPoint = mark.position});
		frame.sprites.push_back({.positionHalfWidth = glm::vec4(mark.position, mark.halfSize),
		                         .shape = glm::vec4(mark.halfSize, 0.0f, 0.0f, 0.0f),
		                         .uv = mark.uv,
		                         .colour = mark.colour,
		                         .flags = glm::vec4(0.0f)});
		frame.spriteMaterials.push_back(material);
		frame.groups.push_back({.path = DrawPath::Sorted, .origin = mark.position, .firstItem = firstItem, .itemCount = 1});
	}
}

void draw::AddLearningMarks(Frame& frame, std::span<const LearningMark> marks)
{
	if (marks.empty())
	{
		return;
	}
	// The bulb keeps its model's own materials and is lit by the scene, its light coloured by the mark
	static const MeshCreator k_Bulb = [] {
		MeshCreator bulb;
		bulb.kind = Creator::Kind::Mesh;
		bulb.className = "LearningBulb";
		bulb.changeMaterial = false;
		bulb.dynamicLighting = true;
		return bulb;
	}();
	static constexpr auto k_Sheet = entt::hashed_string("raw/smoke");
	static constexpr auto k_SheetAlpha = entt::hashed_string("raw/smokea");
	// The glow is the smoke sheet's first cell, of eight a row, blended over what is behind and writing no depth
	constexpr float k_Cell = 1.0f / 8.0f;
	const auto material = frame.MaterialIndex(
	    {.texture = k_Sheet.value(), .alphaTexture = k_SheetAlpha.value(), .mode = Mode::AlphaTexturedAlphaNz});
	for (const auto& mark : marks)
	{
		const auto firstItem = static_cast<uint32_t>(frame.items.size());
		frame.items.push_back(
		    {.kind = ItemKind::Mesh, .index = static_cast<uint32_t>(frame.meshes.size()), .sortPoint = mark.bulb});
		frame.meshes.push_back({.creator = &k_Bulb,
		                        .mesh = mark.bulbMesh,
		                        .axes = glm::mat3(mark.bulbScale),
		                        .position = mark.bulb,
		                        .colour = mark.bulbColour,
		                        .uvOffset = glm::vec2(0.0f),
		                        .cutBelow = std::nullopt});
		frame.items.push_back(
		    {.kind = ItemKind::Sprite, .index = static_cast<uint32_t>(frame.sprites.size()), .sortPoint = mark.glow});
		frame.sprites.push_back({.positionHalfWidth = glm::vec4(mark.glow, mark.glowHalfSize),
		                         .shape = glm::vec4(mark.glowHalfSize, 0.0f, 0.0f, 0.0f),
		                         .uv = glm::vec4(0.0f, 0.0f, k_Cell, k_Cell),
		                         .colour = mark.glowColour,
		                         .flags = glm::vec4(0.0f)});
		frame.spriteMaterials.push_back(material);
		frame.groups.push_back({.path = DrawPath::Sorted, .origin = mark.glow, .firstItem = firstItem, .itemCount = 2});
	}
}

void draw::AddLightSheet(Frame& frame, std::span<const LightSheet::Vertex> vertices, std::span<const uint32_t> triangles,
                         const glm::vec3& sortPoint, LightSheet::Look look)
{
	if (vertices.empty() || triangles.empty())
	{
		return;
	}
	static constexpr auto k_Stars = entt::hashed_string("raw/S_LightSheetStars");
	static constexpr auto k_StarsAlpha = entt::hashed_string("raw/S_LightSheetStarsa");
	static constexpr auto k_ForceField = entt::hashed_string("raw/forcefield");
	static constexpr auto k_ForceFieldAlpha = entt::hashed_string("raw/forcefielda");
	const bool forceField = look == LightSheet::Look::ForceField;
	// Both are drawn the same way
	const auto material = frame.MaterialIndex({.texture = forceField ? k_ForceField.value() : k_Stars.value(),
	                                           .alphaTexture = forceField ? k_ForceFieldAlpha.value() : k_StarsAlpha.value(),
	                                           .mode = Mode::AlphaTexturedAlphaAdditiveNz});
	const auto firstItem = static_cast<uint32_t>(frame.items.size());
	const auto firstVertex = static_cast<uint32_t>(frame.surfaceVertices.size());
	for (const auto& vertex : vertices)
	{
		frame.surfaceVertices.push_back({.position = vertex.position,
		                                 .uv = vertex.uv,
		                                 .abgr = AbgrOf(vertex.argb),
		                                 .specularAbgr = AbgrOf(vertex.specularArgb)});
		frame.surfaceNormals.emplace_back(0.0f);
	}
	const auto firstIndex = static_cast<uint32_t>(frame.surfaceIndices.size());
	frame.surfaceIndices.insert(frame.surfaceIndices.end(), triangles.begin(), triangles.end());
	frame.items.push_back(
	    {.kind = ItemKind::Surface, .index = static_cast<uint32_t>(frame.surfaces.size()), .sortPoint = sortPoint});
	frame.surfaces.push_back({
	    .material = material,
	    .firstVertex = firstVertex,
	    .vertexCount = static_cast<uint32_t>(vertices.size()),
	    .firstIndex = firstIndex,
	    .indexCount = static_cast<uint32_t>(triangles.size()),
	    .doubleSided = true,
	    .lit = false,
	    .worldToAtom = glm::mat4(1.0f),
	});
	frame.groups.push_back({.path = DrawPath::Sorted, .origin = sortPoint, .firstItem = firstItem, .itemCount = 1});
}

void draw::Order(const Frame& frame, const glm::vec3& camera, std::optional<glm::vec3> hand, std::vector<Command>& commands,
                 std::vector<uint32_t>& spriteOrder)
{
	commands.clear();
	spriteOrder.clear();
	// Items in turn: runs of sprites that share a material join one command, and each command takes a place a little
	// nearer than the one before, so the things of one place keep their order
	const auto emit = [&](std::span<const uint32_t> items, auto depthOf) {
		const auto firstCommand = commands.size();
		for (const auto index : items)
		{
			const auto& item = frame.items[index];
			const uint32_t material = item.kind == ItemKind::Sprite    ? frame.spriteMaterials[item.index]
			                          : item.kind == ItemKind::Chain   ? frame.chains[item.index].material
			                          : item.kind == ItemKind::Surface ? frame.surfaces[item.index].material
			                                                           : 0;
			if (item.kind == ItemKind::Sprite && commands.size() > firstCommand && commands.back().kind == ItemKind::Sprite &&
			    commands.back().material == material)
			{
				++commands.back().count;
				spriteOrder.push_back(item.index);
				continue;
			}
			uint32_t depth = depthOf(item);
			if (commands.size() > firstCommand)
			{
				depth = std::min(depth, commands.back().depth > 0 ? commands.back().depth - 1 : 0);
			}
			if (item.kind == ItemKind::Sprite)
			{
				commands.push_back({.kind = ItemKind::Sprite,
				                    .first = static_cast<uint32_t>(spriteOrder.size()),
				                    .count = 1,
				                    .material = material,
				                    .depth = depth});
				spriteOrder.push_back(item.index);
			}
			else
			{
				commands.push_back({.kind = item.kind, .first = item.index, .count = 1, .material = material, .depth = depth});
			}
		}
	};

	// The sorted effects' things all together, the farthest first, and things at the same distance in the order they
	// come. A distance is a sum of squares, thus it is never negative and never negative zero, so its bits give the same
	// order as its value. Each key holds the inverted bits of the distance above the place where the thing comes, so
	// that a plain sort of the keys gives that order.
	std::vector<uint32_t> come;
	std::vector<uint64_t> keys;
	for (const auto& group : frame.groups)
	{
		if (group.path != DrawPath::Sorted)
		{
			continue;
		}
		for (uint32_t i = group.firstItem; i < group.firstItem + group.itemCount; ++i)
		{
			const auto inverted = ~graphics::zsort::Depth(frame.items[i].sortPoint, camera);
			keys.push_back((static_cast<uint64_t>(inverted) << 32u) | come.size());
			come.push_back(i);
		}
	}
	std::ranges::sort(keys);
	std::vector<uint32_t> items;
	items.reserve(keys.size());
	std::ranges::transform(keys, std::back_inserter(items), [&come](uint64_t key) { return come[key & 0xFFFFFFFFu]; });
	GatherCommutingRuns(frame, items);
	emit(items, [&camera](const Item& item) { return graphics::zsort::Depth(item.sortPoint, camera); });

	// Each of the others at one place, in its own order
	for (const auto& group : frame.groups)
	{
		if (group.path == DrawPath::Sorted || group.path == DrawPath::BeforeLand)
		{
			continue;
		}
		items.clear();
		for (uint32_t i = group.firstItem; i < group.firstItem + group.itemCount; ++i)
		{
			items.push_back(i);
		}
		GatherCommutingRuns(frame, items);
		// Just after the hand, which draws itself at its own place
		const bool afterHand = group.path == DrawPath::Immediate && hand.has_value();
		const auto base = graphics::zsort::Depth(afterHand ? *hand : group.origin, camera);
		const uint32_t depth = afterHand && base > 0 ? base - 1 : base;
		emit(items, [depth](const Item&) { return depth; });
	}
}

void draw::OrderBeforeLand(const Frame& frame, std::vector<Command>& commands, std::vector<uint32_t>& spriteOrder)
{
	commands.clear();
	spriteOrder.clear();
	for (const auto& group : frame.groups)
	{
		if (group.path != DrawPath::BeforeLand)
		{
			continue;
		}
		std::vector<uint32_t> items;
		items.reserve(group.itemCount);
		for (uint32_t i = group.firstItem; i < group.firstItem + group.itemCount; ++i)
		{
			items.push_back(i);
		}
		GatherCommutingRuns(frame, items);
		for (const auto index : items)
		{
			const auto& item = frame.items[index];
			const uint32_t material = item.kind == ItemKind::Sprite    ? frame.spriteMaterials[item.index]
			                          : item.kind == ItemKind::Chain   ? frame.chains[item.index].material
			                          : item.kind == ItemKind::Surface ? frame.surfaces[item.index].material
			                                                           : 0;
			if (item.kind == ItemKind::Sprite)
			{
				if (!commands.empty() && commands.back().kind == ItemKind::Sprite && commands.back().material == material)
				{
					++commands.back().count;
				}
				else
				{
					commands.push_back({.kind = ItemKind::Sprite,
					                    .first = static_cast<uint32_t>(spriteOrder.size()),
					                    .count = 1,
					                    .material = material,
					                    .depth = 0});
				}
				spriteOrder.push_back(item.index);
				continue;
			}
			commands.push_back({.kind = item.kind, .first = item.index, .count = 1, .material = material, .depth = 0});
		}
	}
}
