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
#include <memory>
#include <optional>
#include <source_location>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "3D/FrameAnim.h"
#include "Common/GameRandom.h"
#include "PSysFile.h"
#include "SpellLink.h"

// The generic particle system: effects, their collections and atoms, and the modifier classes of the spell files.
// See docs/bw1-notes/particles.md.

namespace openblack::audio
{
struct ParticleSound;
}

namespace openblack::psys
{
class Effect;
struct Atom;
struct Collection;
class Modifier;

/// How an effect is drawn: which of the three draw entries its owner calls. It decides whether each atom queues itself
/// for sorting or is drawn at once. Set where the effect is started (manager::SetDrawPath).
enum class DrawPath : uint8_t
{
	/// The effect has no Z object of its own: each sprite, each mesh (opaque ones too), each mist and each chain goes
	/// into the sort queue on its own; a ZR_SurfRevol surface is drawn at once, unsorted. The default, used by spells
	/// and nearly everything else
	Sorted,
	/// One Z object at GetOrigin, and in it every atom drawn at once in draw-walk order (CollectOrdered). Only the seed
	/// graphic on a ball or an icon and the containers whose visual info has SingleZSort set
	Queued,
	/// Every atom drawn at once in draw-walk order, where the call is: the seed's effect in the hand (inside the hand's
	/// Z object)
	Immediate,
};

/// A particle creator and its sprite form; the other kinds (point, mesh, mist, chain, light map...) derive from it in
/// their own files (PSysRegistry.h).
struct Creator
{
	Creator() = default;
	Creator(const Creator&) = default;
	Creator(Creator&&) = default;
	Creator& operator=(const Creator&) = default;
	Creator& operator=(Creator&&) = default;
	virtual ~Creator() = default;

	enum class Kind
	{
		Point,
		Sprite,
		Mesh,  ///< Mesh creators, plain or animated-textured (Creators/Mesh.cpp)
		Chain, ///< The joints of one collection are drawn as a ribbon (Creators/Chain.cpp)
		/// The exploded pieces (Rules/ExplodeObject.h): drawn at once as world triangles, with no Z object, whatever
		/// the draw path
		MeshPiece,
		Other,
	};
	Kind kind {Kind::Point};
	std::string className;
	uint8_t r {255}, g {255}, b {255}, a {255};
	/// SpecColorR/G/B (0 by default)
	int specR {0}, specG {0}, specB {0};
	bool usePlayerColour {false};      ///< UsePlayerColor (off by default)
	float usePlayerColourBlend {1.0f}; ///< UsePlayerColorBlend (1.0 by default)
	float initialScale {1.0f};
	bool randomiseScale {false};
	// sprites
	std::string texture; ///< e.g. S_SpriteSheet3 (the .raw without extension; its alpha is <name>a.raw)
	int fileOffset {0};
	int spritesPerRow {8};
	int numFrames {1};
	int initFrame {0};
	bool randomiseInitFrame {false};
	bool randomiseFrameDirection {false};
	float frameRate {1.0f};
	bool playAnim {false};
	bool loopAnim {true};
	bool additive {true};    ///< UseAdditiveAlpha: mode 13, else mode 6
	bool writeDepth {false}; ///< MaterialUpdateZBuffer: modes 12 / 5
	int scaleAlpha {255};
	float stretch {1.0f};
	bool horizontal {false}; ///< SetHorozontal: a flat XZ quad
	bool centreAtBase {false};
	bool ignoreRotation {false};
	float originX {0.0f}, originY {0.0f};

	/// The creator's part of a new atom (after CommonInitNewAtom), for the classes that have one
	virtual void InitAtom(Effect& /*effect*/, Atom& /*atom*/) const {}
	/// The frame count of its atoms (the N used by frame stepping and the draw): NumFrames
	[[nodiscard]] virtual int FramesPerAtom() const { return numFrames; }
	/// A creator that only picks another one (the good/evil creator): the creator the atom really gets. Itself for the
	/// others.
	[[nodiscard]] virtual const Creator* Resolve(const Effect& /*effect*/) const { return this; }
};

/// UsePlayerColor on an atom colour (r, g, b, a): the player's colour (alpha forced to 255; pure black, the neutral
/// player, becomes white), with a blend b = int(Blend x 255) & 0xFF != 255 moved towards white per channel
/// (255 + ((c - 255) b >> 8), & 0xFF), then each channel of the colour x that >> 8 (alpha too)
[[nodiscard]] std::array<uint8_t, 4> TintWithPlayerColour(std::array<uint8_t, 4> rgba, uint32_t playerArgb, float blend);

/// Position, scale and rotation of the last two steps, lerped at draw time
struct DrawState
{
	glm::vec3 position {0.0f};
	glm::mat3 rotation {1.0f};
	float scale {1.0f};
	float stretch {1.0f};
	float alpha {255.0f};
	float frame {0.0f};
};

/// One particle of a collection
struct Atom
{
	Atom() = default;
	Atom(const Atom&) = delete;
	Atom(Atom&&) = delete;
	Atom& operator=(const Atom&) = delete;
	Atom& operator=(Atom&&) = delete;
	/// Its sounds lose their atom (defined in Audio/Services/SpellSounds.cpp)
	~Atom();

	Collection* collection {nullptr};
	const Creator* creator {nullptr};
	glm::vec3 position {0.0f}; ///< Local to the parent atom in a hierarchy
	glm::vec3 velocity {0.0f};
	glm::mat3 rotation {1.0f};
	float baseScale {1.0f}; ///< InitialScale
	float ruleScale {1.0f};
	float stretch {1.0f};
	std::array<uint8_t, 4> colour {255, 255, 255, 255}; ///< ARGB as r, g, b, a
	/// The specular, D3DCOLOR ARGB: the creator's SpecColorR/G/B, alpha 0 (0 in every dumped spell file); passed to
	/// the draw as is
	uint32_t specular {0};
	float birth {0.0f};
	bool visible {true}; ///< EventConditionAtomInUse
	float frame {0.0f};  ///< Kept in [0, 2N), like the previous step's
	float frameRate {0.0f};
	bool playAnim {false}; ///< PlayAnim: the frame is stepped only when it is set
	float gravity {1.0f};
	uint32_t random {0};
	uint32_t flags {0}; ///< Bit 3: deflected
	DrawState previous;
	DrawState current;
	bool drawn {false};
	std::vector<std::unique_ptr<Collection>> subCollections;
	/// per-modifier data of this atom (latches, phases)
	std::unordered_map<const Modifier*, glm::vec4> data;
	/// A modifier's own data object, keyed by its modifier, destroyed with the atom (the heal chakra's lets go of its
	/// target there)
	std::unordered_map<const Modifier*, std::shared_ptr<void>> modifierData;
	/// The sounds it started, newest first (Audio/Services/SpellSounds.h)
	std::vector<std::shared_ptr<audio::ParticleSound>> sounds;
	/// A mist creator atom's mist clock (Creators/Mist.cpp): seeded when the mist is made and advanced by the draw only
	/// while it is on screen, so it is changed through the const atoms of the draw; unused by other atoms
	mutable graphics::frame_anim::MistClock mist;
	/// That mist's k (Ratio, or a random 2.5..5); unused by other atoms
	float mistK {0.0f};
	/// Its draw offset. Only HandDrawOffset is ported; UR_Lightning gives one to every fork joint. The draw adds
	/// GetOffset to the atom's drawn position every frame, interpolated between the steps or not
	struct HandDrawOffset
	{
		glm::vec3 reference {0.0f};
		float weight {0.0f};
		/// The point, and the weight clamped to 0..1 (a NaN gives 0)
		void SetReference(const glm::vec3& point, float w)
		{
			reference = point;
			weight = w > 0.0f ? (w < 1.0f ? w : 1.0f) : 0.0f;
		}
		/// (the local player's hand position now - the point) x weight
		[[nodiscard]] glm::vec3 GetOffset(const glm::vec3& hand) const { return (hand - reference) * weight; }
	};
	std::optional<HandDrawOffset> drawOffset;
};

/// One live instance of a group
struct Collection
{
	int group {0};
	Atom* parent {nullptr};
	float birth {0.0f};
	float alpha {255.0f};
	/// Flags (3 when made). Bit 0x02: the atoms are drawn interpolated between the last two steps (without it the
	/// current state is drawn as is). UR_Lightning clears it on its forks
	uint8_t flags {3};
	bool hierarchy {false};
	/// The chain of a collection of chain joints: its v-scroll (frame_anim::ChainScroll, advanced by the draw, so
	/// changed through the const collections of the draw) and its rate, set only by UR_SimpleBeam / UR_Plasma (not
	/// ported: 0)
	mutable float chainScroll {0.0f};
	float chainScrollRate {0.0f};
	/// The chain's texture repeats along the ribbon when a rule rewrites them (UR_Lightning's NumTexturesToTile); -1:
	/// the creator's
	int chainTextures {-1};
	std::vector<std::unique_ptr<Atom>> atoms;
	struct Slot
	{
		const Modifier* modifier {nullptr};
		bool attached {true};
		glm::vec4 state {0.0f}; ///< per-collection data: emitter timing and counts
		glm::vec4 extra {0.0f}; ///< more of it (UR_WillowWisp: the amount emitted and the atoms made)
		bool first {true};
		/// an object the rule made and must close later (UR_Explosion: the beam's particle container); 0xFFFFFFFF =
		/// none (entt::null)
		uint32_t object {0xFFFFFFFFu};
	};
	std::vector<Slot> modifiers;
	/// A modifier's collection data, found by its modifier (UR_CloudGather, UR_Tornado and its sub-collections' data),
	/// destroyed with the collection
	std::unordered_map<const Modifier*, std::shared_ptr<void>> modifierData;
};

/// A modifier's data object on a collection, made on the first call
template <class T>
T& CollectionDataOf(Collection& collection, const Modifier* modifier)
{
	auto& slot = collection.modifierData[modifier];
	if (slot == nullptr)
	{
		slot = std::make_shared<T>();
	}
	return *std::static_pointer_cast<T>(slot);
}

/// The same for an atom's data object
template <class T>
T& AtomDataOf(Atom& atom, const Modifier* modifier)
{
	auto& slot = atom.modifierData[modifier];
	if (slot == nullptr)
	{
		slot = std::make_shared<T>();
	}
	return *std::static_pointer_cast<T>(slot);
}

/// A collection modifier (Group, Condition, RemoveOnCloseDown) and the event conditions / float providers it uses
class Modifier
{
public:
	virtual ~Modifier() = default;
	int group {-1};
	bool removeOnCloseDown {false};
	std::string condition;
	/// true while it creates atoms (a create rule or emitter: `finished()` waits for them, mask 4)
	[[nodiscard]] virtual bool Creates() const { return false; }
	/// a class the port doesn't run yet (a spell's effect counts it as a creator until it closes, see AnyCreatorLeft)
	[[nodiscard]] virtual bool Unported() const { return false; }
	/// modifier flag 2 without 4: an effect with no atoms is not finished while it is attached and the effect is not
	/// closing (UR_HealSpellChakra sets 6 and clears 4: it waits for targets)
	[[nodiscard]] virtual bool KeepsAlive() const { return false; }
	/// ModifyAtomCollection; false detaches it from this collection
	virtual bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& slot) const;
	/// ModifyAtomCore; false deletes the atom (remove rules)
	virtual bool ModifyAtom(Effect& /*effect*/, Atom& /*atom*/, Collection::Slot& /*slot*/) const { return true; }
};

/// One effect started from a spell file
class Effect
{
public:
	/// `type` is its NET_GAME_TYPE: picks the random stream of the effect's draws (game_random::psys)
	Effect(std::shared_ptr<const File> file, glm::vec3 origin, float magnitude,
	       game_random::psys::NetGameType type = game_random::psys::NetGameType::Local);
	~Effect();

	/// One step of dt seconds
	void Step(float dt);
	/// Starts closing down
	void CloseDown();
	[[nodiscard]] bool Finished() const;
	[[nodiscard]] bool Closing() const { return _closing; }
	[[nodiscard]] bool DeleteOnCloseDown() const { return _deleteOnCloseDown; }

	void SetOrigin(glm::vec3 origin) { _origin = origin; }
	/// The hand gives the in-hand effect its scale every frame
	void SetMagnitude(float magnitude) { _magnitude = magnitude; }
	[[nodiscard]] glm::vec3 GetOrigin() const { return _origin; }
	[[nodiscard]] float GetAge() const { return _age; }
	[[nodiscard]] float GetCloseAge() const { return _closeAge; }
	[[nodiscard]] float GetMagnitude() const { return _magnitude; }
	[[nodiscard]] float GetDt() const { return _dt; }
	[[nodiscard]] const File& GetFile() const { return *_file; }

	// ---- the spell link (SpellLink.h) ----
	/// Created with a spell: the rules' events go to it; sends event 1
	void SetSink(SpellSink* sink);
	[[nodiscard]] SpellSink* GetSink() const { return _sink; }
	/// The process info of this step (the spell passes its own)
	void SetProcessInfo(const ProcessInfo& info) { _info = info; }
	[[nodiscard]] const ProcessInfo& GetProcessInfo() const { return _info; }
	/// To the spell, 0 without one
	int SendSpellEvent(const SpellEventInfo& event) const;
	/// The spell's level, -1 without one
	[[nodiscard]] int PowerUpLevel() const { return _sink != nullptr ? _sink->PowerUpLevel() : -1; }
	/// The spell's, and true for an effect without a spell
	[[nodiscard]] bool IsMyInterfaceCasting() const { return _sink == nullptr || _sink->IsMyInterfaceCasting(); }
	/// The spell's, false without a spell
	[[nodiscard]] bool IsHumanPlayerCasting() const { return _sink != nullptr && _sink->IsHumanPlayerCasting(); }
	/// The spell's targets, read by the target rules (heal chakra, flocks)
	void AddTarget(entt::entity target) { _targets.push_back(target); }
	/// The last one added, taken out (entt::null when there is none)
	entt::entity TakeTarget()
	{
		if (_targets.empty())
		{
			return entt::null;
		}
		const auto target = _targets.back();
		_targets.pop_back();
		return target;
	}
	[[nodiscard]] const std::vector<entt::entity>& GetTargets() const { return _targets; }
	/// Target points, e.g. a shield's impacts for its sparks. The target count includes points and objects; a point is
	/// taken before an object.
	void AddTargetPoint(const glm::vec3& point) { _targetPoints.push_back(point); }
	bool TakeTargetPoint(glm::vec3& out)
	{
		if (_targetPoints.empty())
		{
			return false;
		}
		out = _targetPoints.back();
		_targetPoints.pop_back();
		return true;
	}
	[[nodiscard]] size_t TargetPointCount() const { return _targetPoints.size(); }
	/// The direction the effect was created with (the spell's)
	void SetDirection(glm::vec3 direction) { _direction = direction; }
	[[nodiscard]] glm::vec3 GetDirection() const { return _direction; }
	/// The casting player (the good/evil creators and colours read it)
	void SetPlayer(int player) { _player = player; }
	[[nodiscard]] int GetPlayer() const { return _player; }
	/// The draw's alpha (0..255; the physical shield sets it): the drawn atoms' alpha x this / 255
	void SetGlobalAlpha(float alpha) { _globalAlpha = alpha; }
	[[nodiscard]] float GetGlobalAlpha() const { return _globalAlpha; }

	// used by the modifiers
	/// A random float on the active stream (0 outside a step)
	[[nodiscard]] float Random(float max, std::source_location where = std::source_location::current());
	/// Random(b - a) + a
	[[nodiscard]] float Random(float a, float b, std::source_location where = std::source_location::current());
	/// 0 .. n - 1 (0 for 0)
	[[nodiscard]] int32_t Rand(int32_t n, std::source_location where = std::source_location::current());
	/// A point in the unit ball
	[[nodiscard]] glm::vec3 RandomInBall(std::source_location where = std::source_location::current());
	[[nodiscard]] game_random::psys::NetGameType NetType() const { return _netType; }
	[[nodiscard]] float FloatProvider(const std::string& name, float fallback) const;
	[[nodiscard]] bool ConditionForCollection(const std::string& name, const Collection& collection) const;
	[[nodiscard]] bool ConditionForAtom(const std::string& name, const Atom& atom) const;
	[[nodiscard]] const Creator* FindCreator(const std::string& name) const;
	/// The common atom init + the creator's: a new atom in the collection, with its NextGroups sub-collections
	Atom& NewAtom(Collection& collection, const Creator* creator, const std::vector<int>& nextGroups);
	/// New sub-collections of these groups under the atom
	void AddSubCollections(Atom& atom, const std::vector<int>& groups);
	/// A new atom in the first root collection of that group (the lightning's light maps go
	/// into their InitiallyCreated group, not under the fork). nullptr when that group has no root collection.
	Atom* NewAtomInGroup(int group, const Creator* creator);
	/// The atom leaves its collection for the first root collection of that group, where it keeps its position,
	/// velocity and data (the tornado's flung objects). With no such collection the original leaves it in none (never
	/// updated nor drawn again): (approximate) here it is deleted. Call it only while `from` is not being iterated.
	void MoveToBaseGroup(Collection& from, const Atom& atom, int group);
	[[nodiscard]] glm::vec3 SpawnPosition(const Collection& collection) const;
	[[nodiscard]] glm::vec3 GlobalPosition(const Atom& atom) const;
	/// A point of the collection's frame. In a hierarchy the frame is the product of the local matrices (rotation,
	/// scale, position) of the ancestor atoms whose group is flagged in Hierarchies; outside one, the world.
	[[nodiscard]] glm::vec3 LocalToGlobal(const Collection& collection, const glm::vec3& local) const;
	[[nodiscard]] glm::vec3 GlobalToLocal(const Collection& collection, const glm::vec3& global) const;
	/// The scale of an atom's frame: baseScale x ruleScale, the Y axis also x the stretch
	[[nodiscard]] static glm::vec3 FrameScale(const Atom& atom);
	[[nodiscard]] float AtomAge(const Atom& atom) const { return _age - atom.birth; }
	[[nodiscard]] float CollectionAge(const Collection& collection) const { return _age - collection.birth; }

	/// Every drawn atom with its interpolated state (t: fraction of the step since the last one, 0..1)
	struct DrawAtom
	{
		const Creator* creator;
		glm::vec3 position;
		glm::mat3 rotation;
		float scale;
		float stretch;
		float alpha;
		float frame;
		std::array<uint8_t, 3> colour;
		uint32_t specular {0}; ///< the atom's specular, not interpolated
		/// The atom it was taken from (none for the fire's and the town belief's)
		const Atom* atom {nullptr};
	};
	/// kind: the sprites (RendererParticles.cpp) or the meshes (Creators/Mesh.cpp, drawn as instances). In draw-walk order:
	/// each collection's atoms, then each atom's children
	/// `hand` (all three collects): the hand's position the atoms' DrawOffset reads, the draw's copy
	/// (DrawSceneDesc::hand); none: Locator::handSystem's, now (the logic side, the tests)
	void Collect(float t, std::vector<DrawAtom>& out, Creator::Kind kind = Creator::Kind::Sprite,
	             const std::optional<glm::vec3>* hand = nullptr) const;
	/// One chain collection: its joints in list order, drawn as a ribbon strip (Creators/Chain.h)
	struct DrawChain
	{
		const Creator* creator;
		std::vector<DrawAtom> joints;
		const Collection* collection {nullptr}; ///< the chain's collection (its scroll)
		/// The effect's origin: the key of a Queued effect's single Z object, which the ribbon is drawn inside, at once.
		/// A Sorted effect's ribbon has its own Z object at the joint n / 2 (manager::SortedChain)
		glm::vec3 origin {0.0f};
		DrawPath path {DrawPath::Sorted}; ///< its effect's (set by manager::CollectChains)
		uint32_t effect {0};              ///< its effect's id (set by manager::CollectChains)
	};
	/// Every collection made of Kind::Chain atoms, interpolated as Collect does
	void CollectChains(float t, std::vector<DrawChain>& out, const std::optional<glm::vec3>* hand = nullptr) const;
	/// One step of the draw walk: an atom (any kind but Chain) or, with chain >= 0, the ribbon chains[chain] of the
	/// collection walked
	struct OrderedItem
	{
		DrawAtom atom;
		int chain {-1};
	};
	/// Every drawn atom of every kind in draw order, per root collection: the collection's atoms in list order, then its
	/// chain (when it has two joints or more), then for each atom in turn its child collections, each walked the same
	/// way
	void CollectOrdered(float t, std::vector<OrderedItem>& items, std::vector<DrawChain>& chains,
	                    const std::optional<glm::vec3>* hand = nullptr) const;
	[[nodiscard]] size_t AtomCount() const { return _atomCount; }

private:
	void CreateCollection(int group, Atom* parent, std::vector<std::unique_ptr<Collection>>& into);
	void UpdateCollection(Collection& collection);
	void PostUpdate(Collection& collection, const glm::vec3& parentPosition, const glm::mat3& parentRotation,
	                const glm::vec3& parentScale);
	void CollectCollection(const Collection& collection, float t, std::vector<DrawAtom>& out, Creator::Kind kind,
	                       const std::optional<glm::vec3>* hand) const;
	void CollectChainsOf(const Collection& collection, float t, std::vector<DrawChain>& out,
	                     const std::optional<glm::vec3>* hand) const;
	void CollectOrderedOf(const Collection& collection, float t, std::vector<OrderedItem>& items,
	                      std::vector<DrawChain>& chains, const std::optional<glm::vec3>* hand) const;
	[[nodiscard]] bool AnyCreatorLeft(const Collection& collection) const;

	std::shared_ptr<const File> _file;
	std::vector<std::unique_ptr<Modifier>> _modifiers; ///< in file order
	std::array<std::vector<const Modifier*>, 25> _groups;
	std::array<bool, 25> _hierarchies {};
	std::unordered_map<std::string, std::unique_ptr<Creator>> _creators;
	std::unordered_map<std::string, float> _floatValues; ///< float providers of this step
	std::vector<std::unique_ptr<Collection>> _roots;
	glm::vec3 _origin;
	float _magnitude;
	float _age {0.0f};
	float _closeAge {0.0f};
	float _dt {0.1f};
	bool _closing {false};
	bool _deleteOnCloseDown {true};
	float _maxSpellAge {-1.0f};
	size_t _atomCount {0};
	game_random::psys::NetGameType _netType;
	SpellSink* _sink {nullptr};
	ProcessInfo _info {};
	std::vector<entt::entity> _targets;
	std::vector<glm::vec3> _targetPoints;
	int _player {-1};
	float _globalAlpha {255.0f};
	glm::vec3 _direction {0.0f};
};

} // namespace openblack::psys
