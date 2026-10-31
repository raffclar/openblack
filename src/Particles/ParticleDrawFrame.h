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

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <entt/core/fwd.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "LightSheet.h"
#include "ParticleDrawPath.h"
#include "ParticleEffect.h"
#include "ParticleSprites.h"
#include "Visual/RenderModes.h"

/// What the particle effects draw in a frame, gathered once whatever the camera, and the order a camera draws it in,
/// each effect in one of the three ways of DrawPath
namespace openblack::particles::draw
{

/// The sheets a sprite or a ribbon is drawn from and how it blends: the things that share one can share a draw
struct Material
{
	entt::id_type texture;
	entt::id_type alphaTexture;
	graphics::render_modes::Mode mode;

	[[nodiscard]] bool operator==(const Material& other) const = default;
};

/// One corner of a ribbon as it is drawn. Where it lies depends on where the camera is, so each pass places it again
/// from the ribbon's joints before the pass draws it.
struct ChainVertex
{
	glm::vec3 position;
	glm::vec2 uv;
	/// Its colour and alpha, as bytes from the lowest: red, green, blue, alpha
	uint32_t abgr;
};
static_assert(sizeof(ChainVertex) == 24);

/// One joint of a ribbon: where it is and half the ribbon's width there
struct ChainJoint
{
	glm::vec3 position;
	float halfWidth;
};

/// The corners of a ribbon, in its chain's vertices: four to each segment; and its joints, one more than its segments
struct ChainDraw
{
	uint32_t material;
	uint32_t firstVertex;
	uint32_t segments;
	uint32_t firstJoint;
};

/// A model drawn for an atom
struct MeshDraw
{
	/// What the model is and how it is drawn (a MeshCreator)
	const Creator* creator;
	entt::id_type mesh;
	/// Its axes, each scaled, and where it is. A model that faces the camera is turned to it where it is drawn.
	glm::mat3 axes;
	glm::vec3 position;
	/// Its colour and alpha, 0..1
	glm::vec4 colour;
	/// How far its texture has moved on
	glm::vec2 uvOffset;
	/// Nothing of it is drawn below this height, when given
	std::optional<float> cutBelow;
};

/// A point of a surface of revolution, in the world (see SurfaceCreator)
struct SurfaceVertex
{
	glm::vec3 position;
	glm::vec2 uv;
	/// Its colour and alpha, as bytes from the lowest: red, green, blue, alpha
	uint32_t abgr;
	/// The specular colour added after the texture, the same way round
	uint32_t specularAbgr;
};
static_assert(sizeof(SurfaceVertex) == 28);

/// A surface of revolution drawn for an atom: its points and triangles in the frame's lists
struct SurfaceDraw
{
	uint32_t material;
	uint32_t firstVertex;
	uint32_t vertexCount;
	uint32_t firstIndex;
	uint32_t indexCount;
	/// Its faces are seen from both sides
	bool doubleSided;
	/// It is lit by the light: each point's normal, in the atom's frame, is in the frame's surface normals at the same
	/// place as the point, and the world goes to the atom's frame by this matrix
	bool lit;
	glm::mat4 worldToAtom;
};

/// A puff of mist drawn for an atom
struct MistDraw
{
	glm::vec3 position;
	float size;
	/// The atom's colour, 0xAARRGGBB, which the land's light tints where it is drawn
	uint32_t argb;
	/// How much wider than tall it is seen level
	float shape;
	/// Its animation's counter
	int counter;
};

/// A piece of a broken model drawn for an atom
struct FragmentDraw
{
	std::shared_ptr<const blast::MeshFragment> shape;
	/// Its axes, each scaled, and where it is
	glm::mat3 axes;
	glm::vec3 position;
	/// Its particle's colour, 0 to 255
	std::array<uint8_t, 3> rgb;
	/// 0..1
	float alpha;
};

/// The glowing shell laid over an object's model (see volume_glow), drawn by the object as it is drawn this frame
struct ShellDraw
{
	entt::entity object;
	/// How far out it reaches, in parts of the model's half diagonal
	float zoom;
	/// 0xAARRGGBB
	uint32_t argb;
	/// Its texture and that texture's alpha
	entt::id_type texture;
	entt::id_type alphaTexture;
	/// How far its texture has slid
	glm::vec2 uvOffset;
	glm::vec2 textureFraction;
	/// 0xRRGGBB: the colour added after the texture on each corner on the model, also where the shell does not glow. The
	/// particles' shells add their atom's, which no rule of theirs sets.
	uint32_t specularRgb {0};
};

/// A frame of a light map stamped on the land, centred on a point 10 units past its particle along x and z
struct LightStamp
{
	entt::id_type bitmap;
	int frame;
	int pitch;
	glm::vec3 centre;
	/// 0..1
	float strength;
	/// It darkens the land's luminosity as a cloud's shadow does, and does not light the land's colours
	bool shadow {false};
};

/// The strength of a particle's stamp from the alpha of its colour: alpha times the game's float for 1 / 255
[[nodiscard]] float StampStrength(uint8_t alpha);

enum class ItemKind : uint8_t
{
	Sprite,
	Chain,
	Mesh,
	Mist,
	Surface,
	Fragment,
	Shell,
};

/// One thing drawn, by its index in the frame's list of its kind
struct Item
{
	ItemKind kind;
	uint32_t index;
	/// Where it takes its place among what blends when its effect is sorted
	glm::vec3 sortPoint;
};

/// An effect's items, in the order the game draws it all at once
struct Group
{
	DrawPath path;
	/// Where a queued effect takes its place
	glm::vec3 origin;
	uint32_t firstItem;
	uint32_t itemCount;
};

struct Frame
{
	std::vector<Material> materials;
	std::vector<sprites::SpriteInstance> sprites;
	/// Each sprite's material
	std::vector<uint32_t> spriteMaterials;
	std::vector<ChainVertex> chainVertices;
	std::vector<ChainJoint> chainJoints;
	std::vector<ChainDraw> chains;
	std::vector<MeshDraw> meshes;
	std::vector<MistDraw> mists;
	std::vector<SurfaceVertex> surfaceVertices;
	/// Each surface's triangles, counted from its first point
	std::vector<uint32_t> surfaceIndices;
	/// The lit surfaces' points' normals, beside their points
	std::vector<glm::vec3> surfaceNormals;
	std::vector<SurfaceDraw> surfaces;
	std::vector<FragmentDraw> fragments;
	std::vector<ShellDraw> shells;
	std::vector<LightStamp> lightStamps;
	std::vector<Item> items;
	std::vector<Group> groups;

	void Clear();
	[[nodiscard]] bool Empty() const { return items.empty() && lightStamps.empty(); }
	/// The index of a material, added when it is new
	uint32_t MaterialIndex(const Material& material);
};

/// The squares in a row of the players' symbols sheet of a land
constexpr int k_LandSymbolsInARow = 4;

/// What building a frame needs from the game
struct Sources
{
	/// A sheet and its alpha by the name a creator spells it with; none when there is no such sheet
	std::function<std::optional<std::pair<entt::id_type, entt::id_type>>(std::string_view)> textures;
	/// A player's colour, 0xRRGGBB
	std::function<uint32_t(int)> playerColour;
	/// A random number up to a value, drawn again each frame for the light maps that move about
	std::function<float(float)> random;
	/// The height of the land at a point of the ground, for the surfaces laid over it; level land without
	std::function<float(glm::vec2)> landHeight;
	/// Where an object stands, which a shell over its model takes its place among what blends by; none once it has gone
	std::function<std::optional<glm::vec3>(entt::entity)> objectPosition;
	/// The game's milliseconds now, by which the glowing shells' textures slide
	uint64_t gameMilliseconds {0};
	/// The squares in a row of the players' symbols sheet: 4 on a land, more in a network game with more gods
	int symbolsInARow {k_LandSymbolsInARow};
};

/// The player whose symbol and colour an effect without one shows
constexpr int k_NeutralPlayer = 7;

/// Adds an effect's walk to the frame, drawn in one of the three ways, its symbols those of a player
void AddEffect(Frame& frame, const Effect::DrawWalk& walk, DrawPath path, const glm::vec3& origin, int player,
               const Sources& sources);
/// Adds a sheet of light to the frame, sorted with everything else that blends by a point: added to what is behind it,
/// seen from both sides, its texture (a sheet of stars, or the force field's) repeating along and up it
void AddLightSheet(Frame& frame, std::span<const LightSheet::Vertex> vertices, std::span<const uint32_t> triangles,
                   const glm::vec3& sortPoint, LightSheet::Look look = LightSheet::Look::Stars);

/// A sparkle marking a queued fight move: a frame of the blob sheet facing the screen, added to what is behind it
struct FightMark
{
	glm::vec3 position;
	/// Half its width and height
	float halfSize;
	/// Its frame's top left corner in the sheet and the frame's size
	glm::vec4 uv;
	/// Red, green, blue and alpha, 0..1
	glm::vec4 colour;
};
/// Adds the fight sparkles to the frame, each taking its own place among what blends
void AddFightMarks(Frame& frame, std::span<const FightMark> marks);

/// The mark over a creature that learns: a light bulb model and a soft glow over it, facing the screen
struct LearningMark
{
	/// The glow: its middle, half its width and height, and its colour and alpha, 0..1
	glm::vec3 glow;
	float glowHalfSize;
	glm::vec4 glowColour;
	/// The light bulb: where it stands, how much it is scaled, and the colour its light is multiplied by, 0..1
	glm::vec3 bulb;
	float bulbScale;
	glm::vec4 bulbColour;
	/// The light bulb's model
	entt::id_type bulbMesh;
};
/// Adds the learning marks to the frame, each with its bulb and then its glow at the glow's place among what blends
void AddLearningMarks(Frame& frame, std::span<const LearningMark> marks);

/// The three sprites of a player's symbol, in the order drawn: a still glow, a turning glow, then the symbol. Their
/// materials are the glow's for the first two and the symbol's for the third.
struct SymbolSprites
{
	std::array<sprites::SpriteInstance, 3> instances;
};
[[nodiscard]] SymbolSprites SymbolOf(const Effect::DrawAtom& atom, uint32_t playerRgb, int player,
                                     int symbolsInARow = k_LandSymbolsInARow);

/// A player's symbol drawn by itself, between its two glows, as over a worthy artefact
struct PlayerSymbol
{
	glm::vec3 position {0.0f};
	float scale {1.0f};
	/// The game milliseconds it has been drawn for, which turn its glows
	float milliseconds {0.0f};
	int player {k_NeutralPlayer};
};
/// Adds a player's symbol to the frame, its three sprites drawn together at its place among what blends
void AddPlayerSymbol(Frame& frame, const PlayerSymbol& symbol, const Sources& sources);

/// The corners of a ribbon through its joints, four to a segment, with their texture coordinates from its creator, slid
/// along it by a scroll in sheet heights, and their colours; and its joints, which place the corners for each pass
void AppendChain(std::vector<ChainVertex>& out, std::vector<ChainJoint>& outJoints, const Creator& creator,
                 std::span<const Effect::DrawAtom> joints, int textureRepeats = -1, float textureScroll = 0.0f);
/// Places a ribbon's corners as seen from a point, four to a segment: each joint of a segment moved to either side of it,
/// square to the segment and to the line to the eye, by its half width; then where two segments meet, each pair of
/// their corners there moved to half way between them. Each step is in floats, in the game's order.
void PlaceChainCorners(std::span<ChainVertex> corners, std::span<const ChainJoint> joints, const glm::vec3& eye);

/// One draw: a run of sprites that share a material, or one ribbon, model or mist
struct Command
{
	ItemKind kind;
	/// Sprites: a run of the pass's sprite order. The others: the index of their kind.
	uint32_t first;
	uint32_t count;
	/// Sprites and ribbons: their material
	uint32_t material;
	/// Its place among what blends: the greatest first
	uint32_t depth;
};

/// The order a camera draws the frame in, with the sprites, in the order the commands draw them, as indices of the
/// frame's sprites. A sorted effect's things each go by their distance from the camera; runs of sprites next to each
/// other in that order that share a material are drawn together. A queued effect goes by its origin, and one drawn in
/// the hand just after the hand, when there is one, else by its origin. The things of one place are drawn in turn.
/// Sprites added to what is behind that write no depth give the same picture in any order, so each run of them is drawn
/// a material at a time.
void Order(const Frame& frame, const glm::vec3& camera, std::optional<glm::vec3> hand, std::vector<Command>& commands,
           std::vector<uint32_t>& spriteOrder);
/// The order the effects drawn before the land are drawn in, each in its own order, the sprites as for Order. The view
/// they are drawn in keeps the order they are given.
void OrderBeforeLand(const Frame& frame, std::vector<Command>& commands, std::vector<uint32_t>& spriteOrder);

} // namespace openblack::particles::draw
