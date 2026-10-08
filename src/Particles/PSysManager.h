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

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "PSys.h"

namespace openblack::psys
{

/// The running effects and the script's spot visuals (CHL SPECIAL_EFFECT_*)
namespace manager
{
/// The draw path of the effects (psys::DrawPath, PSys.h)
using DrawPath = psys::DrawPath;

/// While false, the renderer still draws every effect as one Z object at its origin (Collect, CollectChains,
/// surf_revol::Collect with their origin key), Mist.cpp hands every mist to mists::Submit and RenderingSystem routes the
/// PSys mesh atoms as before (opaque / translucent / cut ranges). Set to true together with the Renderer side that reads
/// CollectSorted / CollectQueued / HandEffects and RenderContext::psysAtoms: then only the Sorted mists go to
/// mists::Submit and RenderingSystem puts every PSys mesh atom in RenderContext::psysAtomDrawDescs, out of the old
/// ranges.
inline constexpr bool k_DrawByPath = true;

/// An effect from a spell file (e.g. "SF_Smoke"); 0 if the file is missing. Its draw path is DrawPath::Sorted, as a
/// spell draws it, until SetDrawPath changes it. `type` is the effect's NET_GAME_TYPE: Synced for a spell's own effect
/// and the spot visuals, Local for the others (Effect)
uint32_t Start(const std::string& file, glm::vec3 origin, float magnitude,
               game_random::psys::NetGameType type = game_random::psys::NetGameType::Local);
/// The same with a file already parsed (File::Parse, the tests); 0 for nullptr
uint32_t Start(std::shared_ptr<const File> file, glm::vec3 origin, float magnitude,
               game_random::psys::NetGameType type = game_random::psys::NetGameType::Local);
void CloseDown(uint32_t id);
void SetOrigin(uint32_t id, glm::vec3 origin);

/// An effect for a spell: the spell owns the effect and steps it itself once per turn with its process info, so
/// ProcessTurn leaves it alone. 0 if the file is missing.
uint32_t StartForSpell(const std::string& file, glm::vec3 origin, glm::vec3 direction, float magnitude, SpellSink* sink,
                       game_random::psys::NetGameType type = game_random::psys::NetGameType::Local);
/// The spell's step: one step of dt with that info; false once it is finished, and then it is gone
bool ProcessForSpell(uint32_t id, const ProcessInfo& info, float dt);
/// Deletes the effect (when its spell is deleted or finished)
void Delete(uint32_t id);
/// An effect stepped every frame (with the frame's game time: the in-hand and the utility effects): drawn where its
/// last step left it, not interpolated over the game turn
void SetPerFrame(uint32_t id);
/// How the effect's owner draws it (sorted, queued or immediate): set once where the effect is started. Sorted for a
/// missing id
void SetDrawPath(uint32_t id, DrawPath path);
[[nodiscard]] DrawPath GetDrawPath(uint32_t id);
/// A spot visual's draw path from its info's SingleZSort: 1 draws its effect as one Z object (Queued), else Sorted
[[nodiscard]] DrawPath SpotVisualDrawPath(uint32_t singleZSort);
/// nullptr when gone
[[nodiscard]] Effect* Find(uint32_t id);
/// The id of a running effect (0 if it is not one)
[[nodiscard]] uint32_t IdOf(const Effect* effect);

/// A spot visual with a specified duration: SPOT_VISUAL index (into the spot visual info), seconds (< 0: forever; 0:
/// the entry's own life), an owner object it follows and whose loss ends it. Returns the container object for the
/// script (deleting it closes the effect), or entt::null. `magnitude` is the effect's magnitude (and scale): 1 for the
/// scripts, 8 for the smoke of UR_Explosion.
entt::entity CreateSpotVisual(int spotVisual, glm::vec3 position, float seconds, entt::entity owner, float magnitude = 1.0f);
/// The same with the duration in game turns (< 0: forever, else that many Process turns, 0 closing it at the first
/// one): UR_Explosion's 60 and TicksForSeconds(4) & 0xFFFF
entt::entity CreateSpotVisualTurns(int spotVisual, glm::vec3 position, int turns, entt::entity owner, float magnitude = 1.0f);
/// Closes down a container made by CreateSpotVisual (nothing for entt::null)
void CloseSpotVisual(entt::entity object);

/// One game turn: the containers, and every effect stepped with the turn length
void ProcessTurn(float turnSeconds);
/// OPENBLACK_TEST_PSYS: a test effect once the map is loaded
void RunDebugHooks();
void Clear();

/// What the collects read of the frame: the fraction of the game turn the effects are drawn at and the camera for the
/// town belief (none: no town belief). The draw passes its copies (DrawSceneDesc::clock and camera); the logic side and
/// the tests the live values (LiveInputs)
struct FrameInputs
{
	float turnFraction {0.0f};
	std::optional<glm::vec3> camera;
	/// the hand's position the atoms' draw offset reads (none: no hand system)
	std::optional<glm::vec3> hand;
};
/// game_clock::TurnFraction and Locator::camera's position, now
[[nodiscard]] FrameInputs LiveInputs();

/// For the renderer: every effect with atoms to draw, interpolated since the last turn
struct Drawable
{
	glm::vec3 origin;
	std::vector<Effect::DrawAtom> atoms;
	/// The draw fraction of the step this effect is drawn with, handed down to every atom (1 for the effects that step
	/// every frame)
	float t {1.0f};
	/// The effect's draw path (SetDrawPath). A DrawableSource's drawable is Queued unless it says otherwise: one Z object
	/// at its origin (the fire's FireGraphic); the town belief is Sorted
	DrawPath path {DrawPath::Queued};
	/// The effect's id (0 for a DrawableSource's)
	uint32_t effect {0};
};
/// kind: the sprites (with the town belief sprites), or the mesh atoms (Creators/Mesh.h)
std::vector<Drawable> Collect(Creator::Kind kind = Creator::Kind::Sprite, const FrameInputs& inputs = LiveInputs());
/// Every chain collection of every running effect, for the ribbon pass (Creators/Chain.h, Graphics/RendererChain.cpp)
std::vector<Effect::DrawChain> CollectChains(const FrameInputs& inputs = LiveInputs());
/// Other drawers of PSys-style sprites (the fire's FireGraphic, ECS/Fire): Collect appends what they give
using DrawableSource = void (*)(std::vector<Drawable>& out);
void AddDrawableSource(DrawableSource source);

/// One atom of a Sorted effect with the point of its own Z object: the renderer sorts it by zsort::Key(key, camera),
/// (x^2 + y^2) + z^2 of key - camera
struct SortedAtom
{
	glm::vec3 key;
	Effect::DrawAtom atom;
	uint32_t effect; ///< the effect's id (0: the town belief)
	float t;         ///< the effect's draw fraction (Drawable::t)
};
/// One chain of a Sorted effect: its own Z object at the joint n / 2 (index (n - (n >> 31)) >> 1; only once the frame
/// has started and n != 0), drawn when the Z objects are drained
struct SortedChain
{
	glm::vec3 key;
	Effect::DrawChain chain;
	uint32_t effect;
	float t;
};
/// This frame's atoms of every Sorted effect, each list in effect order then the effect's own draw order
struct SortedFrame
{
	/// Kind::Sprite atoms (ParticleSpriteCreator, the town belief's symbols): key = the sprite's position, the atom's
	/// position raised by height x size x 0.5 with CentreAtBase. The town belief's ParticlePlayerSymbol: at its position
	std::vector<SortedAtom> sprites;
	/// Kind::Mesh atoms (3D objects, animated or textured, opaque or not, cut or not): key = the object's translation =
	/// the atom's position. The original's on-screen check is the renderer's test. Drawn as instances:
	/// RenderContext::psysAtoms
	std::vector<SortedAtom> meshes;
	/// MistCreator atoms: key = the mist's position = the atom's position (mist_atoms::Describe gives the MistDesc)
	std::vector<SortedAtom> mists;
	/// ZR_SurfRevol atoms: no Z object, drawn at once when the effect is drawn (whatever the draw path); the key is the
	/// atom's position, unused. Their vertices: surf_revol::Collect, matched by Surface::atom
	std::vector<SortedAtom> surfaces;
	/// The chain of each collection with two joints or more (Effect::DrawChain)
	std::vector<SortedChain> chains;
	/// The other drawn atoms (Kind::Point, light maps, the storm's carried objects...), in case a renderer wants them
	std::vector<SortedAtom> others;
	/// The atoms drawn at once during the walk, the mesh pieces and the ZR_SurfRevol surfaces, whatever the draw path:
	/// in effect order ((inferred) g_Effects' order for the owner lists' own), then each effect's draw order; the
	/// sources' atoms (none of these kinds today) after the effects. The renderer draws them in this order, a piece by
	/// its tag in mesh_pieces' frame, a surface by Surface::atom
	std::vector<const Atom*> atOnce;
};
[[nodiscard]] SortedFrame CollectSorted(const FrameInputs& inputs = LiveInputs());

/// A Queued or Immediate effect: all of it drawn at once, its items in the effect's draw order (Effect::CollectOrdered:
/// sprites, meshes, mists, surfaces... of a collection, then its chain, then the child collections)
struct OrderedEffect
{
	uint32_t effect; ///< the effect's id (0 for a DrawableSource's, the fire)
	DrawPath path;
	/// The effect's origin: the key of its single Z object ((x^2 + y^2) + z^2), Queued only
	glm::vec3 origin;
	float t;
	/// item.chain >= 0: chains[item.chain], whose item.atom is its joint n / 2; else item.atom is the atom to draw (by
	/// its creator: Kind::Sprite, Kind::Mesh -> RenderContext::psysAtomIndex[item.atom.atom], MistCreator ->
	/// mist_atoms::Describe, SurfRevolCreator -> surf_revol::Surface::atom, the others not drawn)
	std::vector<Effect::OrderedItem> items;
	std::vector<Effect::DrawChain> chains;
};
/// Every Queued effect (the seed graphic on a ball / icon, the containers) and the Queued DrawableSources (the fire),
/// one Z object each at its origin
[[nodiscard]] std::vector<OrderedEffect> CollectQueued(const FrameInputs& inputs = LiveInputs());
/// Every Immediate effect: the spell in the hand (at fraction 1), drawn inside the hand's Z object after the hand and
/// what it holds
[[nodiscard]] std::vector<OrderedEffect> HandEffects(const FrameInputs& inputs = LiveInputs());
} // namespace manager

} // namespace openblack::psys
