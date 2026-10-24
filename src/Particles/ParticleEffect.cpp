/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ParticleEffect.h"

#include <charconv>
#include <cstring>

#include <algorithm>
#include <iterator>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <typeinfo>

#include <glm/matrix.hpp>

#include "Common/GameRandom.h"
#include "GestureTrail.h"
#include "LightSheet.h"
#include "ParticleArchiveValues.h"
#include "ParticleBlast.h"
#include "ParticleClassRegistry.h"
#include "ParticleCreators.h"
#include "ParticleMaths.h"
#include "ParticleSurfaces.h"

using namespace openblack;
using namespace openblack::particles;

namespace
{
/// A sprite drawn with a random size takes between this and 1 of its creator's scale
constexpr float k_RandomScaleMinimum = 0.3f;
constexpr float k_RandomScaleRange = 1.0f - k_RandomScaleMinimum;
/// The random number every atom carries is below this
constexpr int32_t k_AtomRandomRange = 0x100;
/// Above this of k_AtomRandomRange an atom plays its animation backwards, when its creator asks for half to
constexpr int32_t k_BackwardsAbove = 0x80;
/// An atom drawn shifted from where it is loses this share of the shift a second
constexpr float k_DrawOffsetFade = 0.5f;
/// An atom fainter than this is not drawn
constexpr float k_MinimumDrawnAlpha = 1.0f;

/// A class the game does not run yet: it is attached so its group behaves the same, but it does nothing
class UnportedModifier final: public Modifier
{
public:
	[[nodiscard]] bool Unported() const override { return true; }
};

/// A channel of an atom's colour with the light added to it
uint8_t Lit(const Atom& atom, size_t channel)
{
	return static_cast<uint8_t>(std::min(255, atom.rgba.at(channel) + atom.specular.at(channel)));
}

/// Whether an atom is drawn: one that has been through its steps (or its first, when its rule asks), seen, and with
/// something to draw it as
bool IsDrawn(const Atom& atom)
{
	return atom.visible && atom.drawn && (atom.creator != nullptr || atom.shower != nullptr) &&
	       (atom.steps >= 2 || atom.drawOnFirstUpdate);
}

bool IsCreatorClass(std::string_view className)
{
	return className.starts_with("Particle") && className.find("Creator") != std::string_view::npos;
}

/// The creator of the one rule of a collection that keeps a creator of this kind of its own, none when no rule or more
/// than one rule keeps one
const Creator* OwnCreatorOf(const Collection& collection, Creator::Kind kind)
{
	const Creator* found = nullptr;
	for (const auto& slot : collection.modifiers)
	{
		const auto* own = slot.modifier != nullptr ? slot.modifier->OwnCreator() : nullptr;
		if (own == nullptr || own->kind != kind || own == found)
		{
			continue;
		}
		if (found != nullptr)
		{
			return nullptr;
		}
		found = own;
	}
	return found;
}

/// The creator of the one rule that keeps a state for a particle and has a creator of its own, none when no rule or more
/// than one rule does
const Creator* RuleCreatorOf(const Atom& atom)
{
	const Creator* found = nullptr;
	for (const auto& entry : atom.data)
	{
		const auto* own = entry.first != nullptr ? entry.first->OwnCreator() : nullptr;
		if (own == nullptr || own == found)
		{
			continue;
		}
		if (found != nullptr)
		{
			return nullptr;
		}
		found = own;
	}
	return found;
}

/// A save names the creator that a rule keeps by the number of the rule. No object of a file has a name with this
/// start.
constexpr std::string_view k_RuleCreatorPrefix = "#rule ";

std::string RuleCreatorName(int rule)
{
	return std::string(k_RuleCreatorPrefix) + std::to_string(rule);
}

/// The number of the rule that a saved name gives, if the name is the name of a creator that a rule keeps
std::optional<int> RuleCreatorNumber(std::string_view name)
{
	if (!name.starts_with(k_RuleCreatorPrefix))
	{
		return std::nullopt;
	}
	name.remove_prefix(k_RuleCreatorPrefix.size());
	int rule = -1;
	const auto [end, error] = std::from_chars(name.data(), name.data() + name.size(), rule);
	if (error != std::errc {} || end != name.data() + name.size())
	{
		return std::nullopt;
	}
	return rule;
}
} // namespace

bool Modifier::ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& slot) const
{
	std::erase_if(collection.atoms, [&](const std::unique_ptr<Atom>& atom) {
		return effect.ConditionForAtom(condition, *atom) && !ModifyAtom(effect, *atom, slot);
	});
	return true;
}

Effect::Effect(std::shared_ptr<const psys::ParticleFile> file, EffectServices services, glm::vec3 origin, float magnitude,
               bool synced)
    : _file(std::move(file))
    , _services(services)
    , _origin(origin)
    , _magnitude(magnitude)
    , _synced(synced)
{
	const auto hierarchies = _file->header.IntArray("Hierarchies");
	for (size_t g = 0; g < _hierarchies.size() && g < hierarchies.size(); ++g)
	{
		_hierarchies.at(g) = hierarchies[g] != 0;
	}
	_deleteOnCloseDown = _file->header.Bool("DeleteOnCloseDown", true);
	_maxSpellAge = _file->header.Float("MaxSpellAge", -1.0f);

	for (const auto& object : _file->objects)
	{
		if (const auto* factory = _services.classes.FindCreator(object.className))
		{
			_creators.insert_or_assign(object.name, (*factory)(object));
			continue;
		}
		if (IsCreatorClass(object.className))
		{
			// Meshes, ribbons and the like make atoms that are not drawn yet
			auto creator = std::make_unique<Creator>();
			ReadCreatorProperties(object, *creator);
			_creators.insert_or_assign(object.name, std::move(creator));
			if (std::ranges::find(_unported, object.className) == _unported.end())
			{
				_unported.push_back(object.className);
			}
			continue;
		}
		std::unique_ptr<Modifier> modifier;
		if (const auto* factory = _services.classes.FindModifier(object.className))
		{
			modifier = (*factory)(object);
		}
		else if (object.Has("Group"))
		{
			modifier = std::make_unique<UnportedModifier>();
			if (std::ranges::find(_unported, object.className) == _unported.end())
			{
				_unported.push_back(object.className);
			}
		}
		if (!modifier)
		{
			// Conditions and float providers are looked up by name when they are used
			continue;
		}
		modifier->group = object.Int("Group", -1);
		modifier->removeOnCloseDown = object.Bool("RemoveOnCloseDown", false);
		modifier->condition = object.String("Condition");
		if (modifier->group >= 0 && modifier->group < static_cast<int>(k_GroupCount))
		{
			_groups.at(static_cast<size_t>(modifier->group)).push_back(modifier.get());
			_modifiers.push_back(std::move(modifier));
		}
	}

	// The groups made at the start, at the origin
	const auto initially = _file->header.IntArray("InitiallyCreated");
	for (size_t g = 0; g < initially.size() && g < k_GroupCount; ++g)
	{
		if (initially[g] != 0)
		{
			CreateCollection(static_cast<int>(g), nullptr, _roots);
		}
	}
}

Atom::~Atom()
{
	for (const auto& sound : sounds)
	{
		sound->atom = nullptr;
	}
	if (carried)
	{
		carried->atom = nullptr;
		if (drawn)
		{
			carried->position = current.position;
			carried->rotation = current.rotation;
		}
	}
}

Effect::~Effect() = default;

void Effect::SetSink(SpellSink* sink)
{
	_sink = sink;
	if (_sink != nullptr)
	{
		SendSpellEvent({.type = SpellEventInfo::Type::Started, .position = _origin});
	}
}

bool Effect::SendSpellEvent(const SpellEventInfo& event) const
{
	return _sink != nullptr && _sink->SpellEvent(event);
}

bool Effect::IsHierarchy(int group) const
{
	return group >= 0 && group < static_cast<int>(k_GroupCount) && _hierarchies.at(static_cast<size_t>(group));
}

float Effect::Random(float max)
{
	return _services.random.ParticleFloatRand(max);
}

float Effect::Random(float min, float max)
{
	return _services.random.ParticleFloatRand(min, max);
}

int32_t Effect::Rand(int32_t n)
{
	return _services.random.ParticleRand(n);
}

glm::vec3 Effect::RandomInBall()
{
	return _services.random.ParticleRandR3();
}

float Effect::FloatProvider(const std::string& name, float fallback) const
{
	const auto it = _floatValues.find(name);
	return it == _floatValues.end() ? fallback : it->second;
}

bool Effect::ConditionForCollection(const std::string& name, const Collection& collection) const
{
	const auto* object = _file->Find(name);
	if (object == nullptr)
	{
		return true;
	}
	const auto* test = _services.classes.FindCollectionCondition(object->className);
	if (test == nullptr)
	{
		// An atom's condition is asked of each atom; one the game does not run yet holds
		return true;
	}
	return (*test)(*this, *object, collection) != object->Bool("InvertResponse", false);
}

bool Effect::ConditionForAtom(const std::string& name, const Atom& atom) const
{
	const auto* object = _file->Find(name);
	if (object == nullptr)
	{
		return true;
	}
	const auto* test = _services.classes.FindAtomCondition(object->className);
	if (test == nullptr)
	{
		return ConditionForCollection(name, *atom.collection);
	}
	return (*test)(*this, *object, atom) != object->Bool("InvertResponse", false);
}

const Creator* Effect::FindCreator(const std::string& name) const
{
	const auto it = _creators.find(name);
	return it == _creators.end() ? nullptr : it->second.get();
}

glm::vec3 Effect::GlobalPosition(const Atom& atom) const
{
	return atom.collection != nullptr ? LocalToGlobal(*atom.collection, atom.position) : atom.position;
}

glm::vec3 Effect::FrameScale(const Atom& atom)
{
	const float s = atom.baseScale * atom.ruleScale;
	return {s, s * atom.stretch, s};
}

glm::mat4 Effect::FrameMatrix(const Atom& atom)
{
	const auto scale = FrameScale(atom);
	glm::mat4 frame {1.0f};
	for (glm::length_t row = 0; row < 3; ++row)
	{
		frame[row] = glm::vec4(atom.rotation[row] * scale[row], 0.0f);
	}
	frame[3] = glm::vec4(atom.position, 1.0f);
	return frame;
}

glm::vec3 Effect::LocalToGlobal(const Collection& collection, const glm::vec3& local) const
{
	return collection.hierarchy ? maths::FrameToWorld(HierarchyFrames(collection), local) : local;
}

glm::vec3 Effect::GlobalToLocal(const Collection& collection, const glm::vec3& global) const
{
	return collection.hierarchy ? maths::WorldToFrame(HierarchyFrames(collection), global) : global;
}

std::vector<glm::mat4> Effect::HierarchyFrames(const Collection& collection) const
{
	std::vector<glm::mat4> frames;
	for (const Atom* a = collection.parent; a != nullptr && a->collection != nullptr; a = a->collection->parent)
	{
		if (IsHierarchy(a->collection->group))
		{
			frames.push_back(FrameMatrix(*a));
		}
	}
	return frames;
}

glm::vec3 Effect::SpawnPosition(const Collection& collection) const
{
	if (collection.parent == nullptr)
	{
		return _origin;
	}
	const auto* parentCollection = collection.parent->collection;
	if (parentCollection != nullptr && IsHierarchy(parentCollection->group))
	{
		return glm::vec3(0.0f);
	}
	return collection.parent->position;
}

Atom& Effect::NewAtom(Collection& collection, const Creator* creator, std::span<const int> nextGroups)
{
	auto atom = std::make_unique<Atom>();
	atom->collection = &collection;
	atom->creator = creator;
	atom->birth = _age;
	atom->position = SpawnPosition(collection);
	atom->random = static_cast<uint32_t>(Rand(k_AtomRandomRange));
	if (creator != nullptr)
	{
		atom->rgba = creator->rgba;
		if (creator->usePlayerColour && _player >= 0)
		{
			atom->rgba =
			    maths::TintWithPlayerColour(atom->rgba, _services.world.PlayerColour(_player), creator->playerColourBlend);
		}
		atom->baseScale = creator->initialScale;
		// Only sprites draw a random size; the other creators that want one draw it themselves
		if (creator->kind == Creator::Kind::Sprite && creator->randomiseScale)
		{
			const float r = Random(k_RandomScaleRange);
			const float scale = r + k_RandomScaleMinimum;
			atom->baseScale = scale * creator->initialScale;
		}
		atom->stretch = creator->stretch;
		if (creator->kind == Creator::Kind::Sprite)
		{
			atom->frame = creator->randomiseInitFrame ? static_cast<float>(Rand(creator->numFrames))
			                                          : static_cast<float>(creator->initFrame);
			atom->frameRate = creator->frameRate;
			atom->playAnim = creator->playAnim;
			if (creator->randomiseFrameDirection && Rand(k_AtomRandomRange) > k_BackwardsAbove)
			{
				atom->frameRate = -atom->frameRate;
			}
		}
		creator->InitAtom(*this, *atom);
	}
	auto& result = *atom;
	collection.atoms.push_back(std::move(atom));
	for (const int group : nextGroups)
	{
		if (group >= 0 && group < static_cast<int>(k_GroupCount))
		{
			CreateCollection(group, &result, result.subCollections);
		}
	}
	return result;
}

void Effect::AddSubCollections(Atom& atom, std::span<const int> groups)
{
	for (const int group : groups)
	{
		if (group >= 0 && group < static_cast<int>(k_GroupCount))
		{
			CreateCollection(group, &atom, atom.subCollections);
		}
	}
}

Atom* Effect::NewAtomInGroup(int group, const Creator* creator)
{
	if (group < 0 || group >= static_cast<int>(k_GroupCount))
	{
		return nullptr;
	}
	auto found = std::ranges::find(_roots, group, [](const auto& root) { return root->group; });
	if (found == _roots.end())
	{
		CreateCollection(group, nullptr, _roots);
		found = std::prev(_roots.end());
	}
	return &NewAtom(**found, creator, {});
}

void Effect::MoveToGroup(Atom& atom, int group)
{
	auto* from = atom.collection;
	if (from == nullptr || group < 0 || group >= static_cast<int>(k_GroupCount))
	{
		return;
	}
	const auto found = std::ranges::find(from->atoms, &atom, [](const auto& a) { return a.get(); });
	if (found == from->atoms.end())
	{
		return;
	}
	auto into = std::ranges::find(_roots, group, [](const auto& root) { return root->group; });
	if (into == _roots.end())
	{
		CreateCollection(group, nullptr, _roots);
		into = std::prev(_roots.end());
	}
	auto moved = std::move(*found);
	from->atoms.erase(found);
	moved->position = LocalToGlobal(*from, moved->position);
	moved->collection = into->get();
	moved->position = GlobalToLocal(*moved->collection, moved->position);
	(*into)->atoms.push_back(std::move(moved));
}

void Effect::CreateCollection(int group, Atom* parent, std::vector<std::unique_ptr<Collection>>& into)
{
	auto collection = std::make_unique<Collection>();
	collection->group = group;
	collection->parent = parent;
	collection->birth = _age;
	// In a hierarchy when any ancestor's group is one, not only the parent's
	collection->hierarchy = parent != nullptr && (IsHierarchy(parent->collection->group) || parent->collection->hierarchy);
	for (const auto* modifier : _groups.at(static_cast<size_t>(group)))
	{
		collection->modifiers.push_back({.modifier = modifier});
	}
	into.push_back(std::move(collection));
}

void Effect::UpdateCollection(Collection& collection)
{
	for (auto& slot : collection.modifiers)
	{
		if (!slot.attached)
		{
			continue;
		}
		if (_closing && slot.modifier->removeOnCloseDown)
		{
			slot.attached = false;
			continue;
		}
		if (!ConditionForCollection(slot.modifier->condition, collection))
		{
			continue;
		}
		if (!slot.modifier->ModifyCollection(*this, collection, slot))
		{
			slot.attached = false;
		}
	}
	// By index: a rule may add a collection to an atom while they are walked, and it runs this step too
	for (size_t a = 0; a < collection.atoms.size(); ++a)
	{
		auto& atom = collection.atoms[a];
		for (size_t s = 0; s < atom->subCollections.size(); ++s)
		{
			UpdateCollection(*atom->subCollections[s]);
		}
	}
}

void Effect::PostUpdate(Collection& collection, const glm::vec3& parentPosition, const glm::mat3& parentRotation,
                        const glm::vec3& parentScale)
{
	// The parent's frame is that of the nearest ancestor atom whose group is a hierarchy; its scale also scales the atoms
	// drawn in it
	const bool flagged = IsHierarchy(collection.group);
	for (auto& atom : collection.atoms)
	{
		atom->previous = atom->current;
		auto& draw = atom->current;
		draw.rotation = collection.hierarchy ? parentRotation * atom->rotation : atom->rotation;
		draw.position =
		    collection.hierarchy ? parentPosition + parentRotation * (parentScale * atom->position) : atom->position;
		// Approximated: the drawn size takes its parents' scale across, not their stretch
		draw.scale = atom->baseScale * atom->ruleScale * (collection.hierarchy ? parentScale.x : 1.0f);
		draw.stretch = atom->stretch;
		draw.alpha = static_cast<float>(atom->rgba[3]) * collection.alpha / 255.0f;
		float previousFrame = atom->frame;
		maths::AdvanceFrame(previousFrame, atom->frame, _dt, atom->frameRate,
		                    atom->creator != nullptr ? atom->creator->numFrames : 0, atom->playAnim);
		atom->previous.frame = previousFrame;
		draw.frame = atom->frame;
		if (!atom->drawn)
		{
			// A new atom is drawn where it first is, but its animation runs on from its first frame through the step
			atom->previous = draw;
			atom->previous.frame = previousFrame;
			atom->drawn = true;
		}
		atom->steps = static_cast<uint8_t>(std::min(atom->steps + 1, 2));
		const glm::vec3 subScale =
		    flagged ? FrameScale(*atom) * (collection.hierarchy ? parentScale : glm::vec3(1.0f)) : parentScale;
		for (auto& sub : atom->subCollections)
		{
			PostUpdate(*sub, flagged ? draw.position : parentPosition, flagged ? draw.rotation : parentRotation, subScale);
		}
	}
}

void Effect::UpdateFloatProviders()
{
	_floatValues.clear();
	for (const auto& object : _file->objects)
	{
		const auto& c = object.className;
		if (!c.ends_with("FloatProvider"))
		{
			continue;
		}
		float value = 1.0f;
		const float scaleBy = object.Float("ScaleBy", 1.0f);
		if (c == "ConstFloatProvider")
		{
			value = object.Float("ConstValue", 1.0f);
		}
		else if (c == "MagnitudeFloatProvider" || c == "MagnitudeTimesStrengthFloatProvider" || c == "StrengthFloatProvider")
		{
			float base = _magnitude;
			if (c == "StrengthFloatProvider")
			{
				base = _info.power;
			}
			else if (c == "MagnitudeTimesStrengthFloatProvider")
			{
				base = _magnitude * _info.power;
			}
			value = std::clamp(base * scaleBy, object.Float("Minimum", -1e6f), object.Float("Maximum", 1e6f));
		}
		else if (c == "RenderHandScaleFloatProvider")
		{
			// The hand's drawn scale reaches the effect as its magnitude
			value = _magnitude * scaleBy;
		}
		else if (c == "RenderHandScaleTimesStrengthFloatProvider")
		{
			value = _magnitude * _info.power * scaleBy;
		}
		_floatValues.insert_or_assign(object.name, value);
	}
}

void Effect::Step(float dt)
{
	if (_firstStepTwice)
	{
		_firstStepTwice = false;
		StepOnce(dt);
	}
	StepOnce(dt);
}

void Effect::StepOnce(float dt)
{
	// Its random numbers come from its own stream during the step, and give 0 outside it
	const ParticleRandomStep step(_services.random, _synced);
	_dt = dt;
	UpdateFloatProviders();
	// By index: a rule may make a group's collection at the start while they are walked
	for (size_t r = 0; r < _roots.size(); ++r)
	{
		UpdateCollection(*_roots[r]);
	}
	_atomCount = 0;
	const auto count = [this](const auto& self, const Collection& c) -> void {
		_atomCount += c.atoms.size();
		for (const auto& atom : c.atoms)
		{
			for (const auto& sub : atom->subCollections)
			{
				self(self, *sub);
			}
		}
	};
	for (auto& root : _roots)
	{
		PostUpdate(*root, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		count(count, *root);
	}
	_age += dt;
}

void Effect::CloseDown()
{
	if (!_closing)
	{
		_closing = true;
		_closeAge = _age;
	}
}

bool Effect::AnyCreatorLeft(const Collection& collection) const
{
	for (const auto& slot : collection.modifiers)
	{
		const bool creates = slot.modifier->Creates() || (!_closing && slot.modifier->KeepsAlive()) ||
		                     (_sink != nullptr && !_closing && slot.modifier->Unported());
		if (slot.attached && creates && !(_closing && slot.modifier->removeOnCloseDown))
		{
			return true;
		}
	}
	return std::ranges::any_of(collection.atoms, [this](const auto& atom) {
		return std::ranges::any_of(atom->subCollections, [this](const auto& sub) { return AnyCreatorLeft(*sub); });
	});
}

bool Effect::Finished() const
{
	if (_maxSpellAge > 0.0f && _age > _maxSpellAge)
	{
		return true;
	}
	if (_atomCount != 0)
	{
		return false;
	}
	return std::ranges::none_of(_roots, [this](const auto& root) { return AnyCreatorLeft(*root); });
}

size_t Effect::CollectionCount() const
{
	const auto count = [](const auto& self, const Collection& c) -> size_t {
		size_t total = 1;
		for (const auto& atom : c.atoms)
		{
			for (const auto& sub : atom->subCollections)
			{
				total += self(self, *sub);
			}
		}
		return total;
	};
	size_t total = 0;
	for (const auto& root : _roots)
	{
		total += count(count, *root);
	}
	return total;
}

const Atom& openblack::particles::DrawOffsetHolder(const Atom& atom)
{
	const Atom* holder = &atom;
	while (holder->sharesParentOffset && holder->collection != nullptr && holder->collection->parent != nullptr)
	{
		holder = holder->collection->parent;
	}
	return *holder;
}

bool openblack::particles::HasDrawOffset(const Atom& atom)
{
	const auto& holder = DrawOffsetHolder(atom);
	return holder.drawOffset.has_value() || holder.handOffset.has_value();
}

void openblack::particles::FollowLiveHand(Atom& atom, glm::vec3 reference, float weight)
{
	if (atom.drawOffset.has_value() || atom.sharesParentOffset)
	{
		return;
	}
	atom.handOffset = HandOffset {.reference = reference, .weight = std::clamp(weight, 0.0f, 1.0f)};
}

bool openblack::particles::ShareParentDrawOffset(Atom& atom)
{
	if (HasDrawOffset(atom) || atom.collection == nullptr || atom.collection->parent == nullptr ||
	    !HasDrawOffset(*atom.collection->parent))
	{
		return false;
	}
	atom.sharesParentOffset = true;
	return true;
}

std::optional<Effect::DrawAtom> Effect::Interpolate(const Atom& atom, float t, bool interpolated) const
{
	const float k = interpolated ? std::clamp(t, 0.0f, 1.0f) : 1.0f;
	const auto& a = atom.previous;
	const auto& b = atom.current;
	const float alpha = (a.alpha + (b.alpha - a.alpha) * k) * _globalAlpha / 255.0f;
	if (!IsDrawn(atom))
	{
		return std::nullopt;
	}
	// The drawn time lies between the last two steps; the effect's age is the current step's end
	const float drawnAt = _age - (_dt * (1.0f - k));
	const float age = drawnAt - atom.birth;
	// The draw offset is the atom's own or its parent atom's
	const auto& holder = DrawOffsetHolder(atom);
	glm::vec3 shift(0.0f);
	if (holder.drawOffset.has_value())
	{
		shift = *holder.drawOffset * std::clamp(1.0f - (drawnAt - holder.drawOffsetFrom) * k_DrawOffsetFade, 0.0f, 1.0f);
	}
	return DrawAtom {
	    .creator = atom.creator,
	    .position = a.position + (b.position - a.position) * k + shift,
	    .rotation = a.rotation + (b.rotation - a.rotation) * k,
	    .scale = a.scale + (b.scale - a.scale) * k,
	    .stretch = a.stretch + (b.stretch - a.stretch) * k,
	    .alpha = alpha,
	    .frame = atom.creator != nullptr ? maths::LerpFrame(a.frame, b.frame, t, atom.creator->loopAnim) : 0.0f,
	    .rgb = {Lit(atom, 0), Lit(atom, 1), Lit(atom, 2)},
	    .age = std::max(age, 0.0f),
	    .creatorValue = atom.creatorValue,
	    .fraction = k,
	    .surface = atom.surface.get(),
	    .offsetWeight = atom.drawWeight,
	    .handOffset = holder.handOffset,
	    .fragment = atom.fragment,
	    .player = atom.player,
	    .objectGlow = atom.objectGlow,
	    .shower = atom.shower.get(),
	};
}

void Effect::WalkCollection(const Collection& collection, float t, DrawWalk& out) const
{
	// The newest atom first, as the game keeps its lists
	const Creator* chainCreator = nullptr;
	for (const auto& atom : std::ranges::reverse_view(collection.atoms))
	{
		const auto drawn = Interpolate(*atom, t, collection.interpolated);
		if (!drawn.has_value())
		{
			continue;
		}
		// An atom that lays a shell over an object's model, or sends up a shower of sparks from it, is drawn as that,
		// whatever its creator
		if (drawn->objectGlow.has_value() || drawn->shower != nullptr)
		{
			out.steps.push_back({.chain = false, .index = static_cast<uint32_t>(out.atoms.size())});
			out.atoms.push_back(*drawn);
			continue;
		}
		const auto kind = drawn->creator->kind;
		if (kind == Creator::Kind::Chain)
		{
			chainCreator = drawn->creator;
		}
		else if (kind != Creator::Kind::Point && kind != Creator::Kind::Other && drawn->alpha >= k_MinimumDrawnAlpha)
		{
			out.steps.push_back({.chain = false, .index = static_cast<uint32_t>(out.atoms.size())});
			out.atoms.push_back(*drawn);
		}
	}
	// Then the ribbon through its joints, from the first made
	if (chainCreator != nullptr)
	{
		const auto first = static_cast<uint32_t>(out.joints.size());
		for (const auto& atom : collection.atoms)
		{
			if (atom->creator != nullptr && atom->creator->kind == Creator::Kind::Chain)
			{
				if (const auto joint = Interpolate(*atom, t, collection.interpolated))
				{
					out.joints.push_back(*joint);
				}
			}
		}
		const auto count = static_cast<uint32_t>(out.joints.size()) - first;
		if (count >= 2)
		{
			out.steps.push_back({.chain = true, .index = static_cast<uint32_t>(out.chains.size())});
			out.chains.push_back({.creator = chainCreator,
			                      .firstJoint = first,
			                      .jointCount = count,
			                      .textureRepeats = collection.textureRepeats,
			                      .textureScroll = collection.textureScroll});
		}
		else
		{
			out.joints.resize(first);
		}
	}
	// Then what is under each atom in turn
	for (const auto& atom : std::ranges::reverse_view(collection.atoms))
	{
		for (const auto& sub : std::ranges::reverse_view(atom->subCollections))
		{
			WalkCollection(*sub, t, out);
		}
	}
}

void Effect::Walk(float t, DrawWalk& out) const
{
	if (_hidden)
	{
		return;
	}
	for (const auto& root : std::ranges::reverse_view(_roots))
	{
		WalkCollection(*root, t, out);
	}
}

void Effect::Collect(float t, std::vector<DrawAtom>& out, Creator::Kind kind) const
{
	DrawWalk walk;
	Walk(t, walk);
	if (kind == Creator::Kind::Chain)
	{
		out.insert(out.end(), walk.joints.begin(), walk.joints.end());
		return;
	}
	std::ranges::copy_if(walk.atoms, std::back_inserter(out),
	                     [kind](const DrawAtom& atom) { return atom.creator != nullptr && atom.creator->kind == kind; });
}

void Effect::ForEachDrawnShower(const std::function<void(ObjectShower&)>& visit)
{
	if (_hidden)
	{
		return;
	}
	const auto walk = [&visit](const auto& self, Collection& collection) -> void {
		for (const auto& atom : collection.atoms)
		{
			if (atom->shower != nullptr && IsDrawn(*atom))
			{
				visit(*atom->shower);
			}
			for (const auto& sub : atom->subCollections)
			{
				self(self, *sub);
			}
		}
	};
	for (const auto& root : _roots)
	{
		walk(walk, *root);
	}
}

void Effect::ScrollChains(float milliseconds)
{
	if (_hidden)
	{
		return;
	}
	const auto walk = [milliseconds](const auto& self, Collection& collection) -> void {
		// A ribbon shows only through two joints or more
		const ChainCreator* chain = nullptr;
		size_t joints = 0;
		for (const auto& atom : collection.atoms)
		{
			if (atom->creator == nullptr || atom->creator->kind != Creator::Kind::Chain || !IsDrawn(*atom))
			{
				continue;
			}
			++joints;
			if (!atom->objectGlow.has_value() && atom->shower == nullptr)
			{
				chain = static_cast<const ChainCreator*>(atom->creator);
			}
		}
		if (chain != nullptr && joints >= 2)
		{
			collection.textureScroll = chain->ScrolledOn(collection.textureScroll, milliseconds, collection.textureSpeed);
		}
		for (const auto& atom : collection.atoms)
		{
			for (const auto& sub : atom->subCollections)
			{
				self(self, *sub);
			}
		}
	};
	for (const auto& root : _roots)
	{
		walk(walk, *root);
	}
}

std::optional<entt::entity> Effect::TakeTarget()
{
	if (_targets.empty())
	{
		return std::nullopt;
	}
	const auto target = _targets.back();
	_targets.pop_back();
	return target;
}

std::optional<PlasmaCommand> Effect::TakePlasma()
{
	if (_plasma.empty())
	{
		return std::nullopt;
	}
	const auto command = _plasma.back();
	_plasma.pop_back();
	return command;
}

std::optional<mana_path::Spark> Effect::TakeManaPathSpark()
{
	if (_manaPathSparks.empty())
	{
		return std::nullopt;
	}
	const auto spark = _manaPathSparks.back();
	_manaPathSparks.pop_back();
	return spark;
}

std::optional<glm::vec3> Effect::TakeTargetPosition()
{
	if (_targetPositions.empty())
	{
		return std::nullopt;
	}
	const auto position = _targetPositions.back();
	_targetPositions.pop_back();
	return position;
}

namespace
{

/// The kinds of the things in the tables, in the order that the tables go into a save
template <typename Type>
void SaveItems(EffectWriter& out, const SharedTable<Type>& table, auto&& write)
{
	out.Out().WriteSize(table.items.size());
	for (const auto& item : table.items)
	{
		write(*item);
	}
}

template <typename Type>
void LoadItems(EffectReader& in, SharedTable<Type>& table, auto&& read)
{
	table.items.clear();
	table.numbers.clear();
	const auto size = in.In().ReadSize();
	if (!size.has_value())
	{
		return;
	}
	for (size_t i = 0; i < *size && !in.Failed(); ++i)
	{
		auto item = std::make_shared<Type>();
		read(*item);
		table.Add(item);
	}
}

void SaveSound(EffectWriter& out, const ParticleSound& sound)
{
	out.WriteValue(sound.action.sound);
	out.WriteValue(sound.action.looping);
	out.WriteValue(sound.action.onlyOne);
	out.WriteValue(sound.action.softRelease);
	out.WriteValue(sound.action.useSurface);
	out.WriteValue(sound.size);
	out.WriteValue(sound.travelsAtSoundSpeed);
	out.WriteValue(sound.onLand);
	out.WriteValue(sound.surface);
	out.WriteValue(sound.alignment);
}

void LoadSound(EffectReader& in, ParticleSound& sound)
{
	in.ReadValue(sound.action.sound);
	in.ReadValue(sound.action.looping);
	in.ReadValue(sound.action.onlyOne);
	in.ReadValue(sound.action.softRelease);
	in.ReadValue(sound.action.useSurface);
	in.ReadValue(sound.size);
	in.ReadValue(sound.travelsAtSoundSpeed);
	in.ReadValue(sound.onLand);
	in.ReadValue(sound.surface);
	in.ReadValue(sound.alignment);
}

void SaveMesh(EffectWriter& out, const surface::Mesh& mesh)
{
	out.WritePlainList(mesh.vertices);
	out.WritePlainList(mesh.indices);
	out.WritePlainList(mesh.normals);
}

void LoadMesh(EffectReader& in, surface::Mesh& mesh)
{
	in.ReadPlainList(mesh.vertices);
	in.ReadPlainList(mesh.indices);
	in.ReadPlainList(mesh.normals);
}

} // namespace

void EffectWriter::Write(bool value)
{
	ecs::reflection::Save(*_out, value);
}
void EffectWriter::Write(int32_t value)
{
	_out->WriteRaw(value);
}
void EffectWriter::Write(uint32_t value)
{
	_out->WriteRaw(value);
}
void EffectWriter::Write(uint64_t value)
{
	_out->WriteRaw(value);
}
void EffectWriter::Write(float value)
{
	_out->WriteRaw(value);
}
void EffectWriter::Write(glm::vec3 value)
{
	ecs::reflection::Save(*_out, value);
}
void EffectWriter::Write(entt::entity value)
{
	_out->WriteRaw(value);
}
void EffectWriter::WriteSize(size_t size)
{
	_out->WriteSize(size);
}
void EffectWriter::WriteBytes(const void* bytes, size_t size)
{
	_out->Write({static_cast<const uint8_t*>(bytes), size});
}

bool EffectReader::Failed() const
{
	return _in->Failed();
}
void EffectReader::Read(bool& value)
{
	ecs::reflection::Load(*_in, value);
}
void EffectReader::Read(int32_t& value)
{
	_in->ReadRaw(value);
}
void EffectReader::Read(uint32_t& value)
{
	_in->ReadRaw(value);
}
void EffectReader::Read(uint64_t& value)
{
	_in->ReadRaw(value);
}
void EffectReader::Read(float& value)
{
	_in->ReadRaw(value);
}
void EffectReader::Read(glm::vec3& value)
{
	ecs::reflection::Load(*_in, value);
}
void EffectReader::Read(entt::entity& value)
{
	ecs::reflection::Load(*_in, value);
}
std::optional<size_t> EffectReader::ReadSize()
{
	return _in->ReadSize();
}
bool EffectReader::ReadBytes(void* bytes, size_t size)
{
	const auto taken = _in->Take(size);
	if (!taken.has_value())
	{
		return false;
	}
	std::memcpy(bytes, taken->data(), size);
	return true;
}

void openblack::particles::SaveSharedTables(EffectWriter& out)
{
	auto& tables = out.Tables();
	// A bolt links to the bolts it meets: those go in the table too, before the table is written
	for (size_t i = 0; i < tables.bolts.items.size(); ++i)
	{
		const auto bolt = tables.bolts.items[i];
		tables.bolts.Add(bolt->linkedTo.lock());
		tables.bolts.Add(bolt->linkedFrom.lock());
	}
	SaveItems(out, tables.sounds, [&out](const ParticleSoundLink& link) {
		SaveSound(out, link.sound);
		out.WriteValue(link.position);
		out.WriteValue(link.fadeStep);
	});
	SaveItems(out, tables.bolts, [&out, &tables](const BoltShare& bolt) {
		out.WriteValue(bolt.order);
		out.WriteValue(bolt.origin);
		out.WriteValue(bolt.centroid);
		out.WriteValue(bolt.heading);
		out.WriteValue(bolt.hasTargets);
		out.Out().WriteRaw(tables.bolts.Add(bolt.linkedTo.lock()));
		out.Out().WriteRaw(tables.bolts.Add(bolt.linkedFrom.lock()));
		out.WriteValue(bolt.meeting);
	});
	SaveItems(out, tables.shields, [&out](const ShieldSphere& shield) {
		out.WriteValue(shield.centre);
		out.WriteValue(shield.radius);
		out.WriteValue(shield.spell);
		out.WriteValue(shield.impacts);
	});
	SaveItems(out, tables.sheets, [&out](const LightSheet& sheet) { out.WriteValue(sheet); });
	SaveItems(out, tables.carried, [&out](const CarriedObject& carried) {
		out.WriteValue(carried.object);
		out.WriteValue(carried.carrier);
		out.WriteValue(carried.position);
		out.WriteValue(carried.rotation);
	});
	SaveItems(out, tables.trails, [&out](const GestureTrail& trail) {
		out.WriteValue(trail.drawn);
		out.WriteValue(trail.ideal);
	});
}

void openblack::particles::LoadSharedTables(EffectReader& in)
{
	auto& tables = in.Tables();
	LoadItems(in, tables.sounds, [&in](ParticleSoundLink& link) {
		LoadSound(in, link.sound);
		in.ReadValue(link.position);
		in.ReadValue(link.fadeStep);
	});
	// The links between the bolts are numbers until every bolt is there
	std::vector<std::pair<uint32_t, uint32_t>> links;
	LoadItems(in, tables.bolts, [&in, &links](BoltShare& bolt) {
		in.ReadValue(bolt.order);
		in.ReadValue(bolt.origin);
		in.ReadValue(bolt.centroid);
		in.ReadValue(bolt.heading);
		in.ReadValue(bolt.hasTargets);
		auto& link = links.emplace_back();
		in.ReadPlain(link.first);
		in.ReadPlain(link.second);
		in.ReadValue(bolt.meeting);
	});
	for (size_t i = 0; i < links.size() && i < tables.bolts.items.size(); ++i)
	{
		tables.bolts.items[i]->linkedTo = tables.bolts.At(links[i].first);
		tables.bolts.items[i]->linkedFrom = tables.bolts.At(links[i].second);
	}
	LoadItems(in, tables.shields, [&in](ShieldSphere& shield) {
		in.ReadValue(shield.centre);
		in.ReadValue(shield.radius);
		in.ReadValue(shield.spell);
		in.ReadValue(shield.impacts);
	});
	LoadItems(in, tables.sheets, [&in](LightSheet& sheet) { in.ReadValue(sheet); });
	LoadItems(in, tables.carried, [&in](CarriedObject& carried) {
		in.ReadValue(carried.object);
		in.ReadValue(carried.carrier);
		in.ReadValue(carried.position);
		in.ReadValue(carried.rotation);
	});
	LoadItems(in, tables.trails, [&in](GestureTrail& trail) {
		in.ReadValue(trail.drawn);
		in.ReadValue(trail.ideal);
	});
}

void Modifier::SaveSlotData(const Collection::Slot& slot, EffectWriter& out) const
{
	out.Out().SetPlace("particle rule", typeid(*this).name());
	out.Out().Unsupported(slot.data.type().name());
}

void Modifier::SaveHeld(const AtomRuleData& /*data*/, EffectWriter& out) const
{
	out.Out().SetPlace("particle rule held", typeid(*this).name());
	out.Out().Unsupported("held");
}

int Effect::ModifierNumber(const Modifier* modifier) const
{
	const auto found = std::ranges::find_if(_modifiers, [modifier](const auto& owned) { return owned.get() == modifier; });
	return found != _modifiers.end() ? static_cast<int>(found - _modifiers.begin()) : -1;
}

const Modifier* Effect::ModifierAt(int number) const
{
	return number >= 0 && number < static_cast<int>(_modifiers.size()) ? _modifiers[static_cast<size_t>(number)].get()
	                                                                   : nullptr;
}

void Effect::Save(EffectWriter& out) const
{
	out.StartEffect(*this);
	out.WriteValue(_origin);
	out.WriteValue(_magnitude);
	out.WriteValue(_age);
	out.WriteValue(_closeAge);
	out.WriteValue(_dt);
	out.WriteValue(_firstStepTwice);
	out.WriteValue(_closing);
	out.WriteValue(_atomCount);
	out.WriteValue(_player);
	out.WriteValue(_globalAlpha);
	out.WriteValue(_hidden);
	out.WriteValue(_direction);
	out.WritePlain(_info);
	out.WriteValue(_targets);
	out.WriteValue(_targetPositions);
	out.WritePlainList(_plasma);
	out.WritePlainList(_manaPathSparks);
	out.WriteValue(_floatValues);
	out.Out().WriteSize(_roots.size());
	for (const auto& root : _roots)
	{
		SaveStructure(*root, out);
	}
	for (const auto& root : _roots)
	{
		SaveRuleState(*root, out);
	}
}

bool Effect::Load(EffectReader& in)
{
	in.StartEffect(*this);
	in.ReadValue(_origin);
	in.ReadValue(_magnitude);
	in.ReadValue(_age);
	in.ReadValue(_closeAge);
	in.ReadValue(_dt);
	in.ReadValue(_firstStepTwice);
	in.ReadValue(_closing);
	in.ReadValue(_atomCount);
	in.ReadValue(_player);
	in.ReadValue(_globalAlpha);
	in.ReadValue(_hidden);
	in.ReadValue(_direction);
	in.ReadPlain(_info);
	in.ReadValue(_targets);
	in.ReadValue(_targetPositions);
	in.ReadPlainList(_plasma);
	in.ReadPlainList(_manaPathSparks);
	in.ReadValue(_floatValues);
	_roots.clear();
	const auto roots = in.In().ReadSize();
	if (!roots.has_value())
	{
		return false;
	}
	for (size_t i = 0; i < *roots && !in.Failed(); ++i)
	{
		auto root = LoadStructure(in, nullptr);
		if (!root)
		{
			return false;
		}
		_roots.push_back(std::move(root));
	}
	for (const auto& root : _roots)
	{
		LoadRuleState(*root, in);
	}
	return !in.Failed();
}

void Effect::SaveSubtree(const Collection& collection, EffectWriter& out) const
{
	SaveStructure(collection, out);
	SaveRuleState(collection, out);
}

std::unique_ptr<Collection> Effect::LoadSubtree(EffectReader& in, Atom* parent)
{
	auto collection = LoadStructure(in, parent);
	if (collection)
	{
		LoadRuleState(*collection, in);
	}
	return collection;
}

void Effect::SaveStructure(const Collection& collection, EffectWriter& out) const
{
	out.NumberCollection(&collection);
	out.WriteValue(collection.group);
	out.WriteValue(collection.birth);
	out.WriteValue(collection.alpha);
	out.WriteValue(collection.emitting);
	out.WriteValue(collection.interpolated);
	out.WriteValue(collection.hierarchy);
	out.WriteValue(collection.textureRepeats);
	out.WriteValue(collection.textureSpeed);
	out.WriteValue(collection.textureScroll);
	out.Out().WriteSize(collection.modifiers.size());
	for (const auto& slot : collection.modifiers)
	{
		out.WriteValue(ModifierNumber(slot.modifier));
		out.WriteValue(slot.attached);
		out.WritePlain(slot.emitter);
		out.WriteValue(slot.state);
		out.WriteValue(slot.extra);
		out.WriteValue(slot.first);
	}
	out.Out().WriteSize(collection.atoms.size());
	for (const auto& owned : collection.atoms)
	{
		const auto& atom = *owned;
		out.NumberAtom(&atom);
		// A creator goes by its name in the file
		std::string creator;
		if (atom.creator != nullptr)
		{
			const auto found =
			    std::ranges::find_if(_creators, [&atom](const auto& entry) { return entry.second.get() == atom.creator; });
			if (found != _creators.end())
			{
				creator = found->first;
			}
			else if (const auto rule = std::ranges::find_if(
			             _modifiers, [&atom](const auto& modifier) { return modifier->OwnCreator() == atom.creator; });
			         rule != _modifiers.end())
			{
				creator = RuleCreatorName(static_cast<int>(rule - _modifiers.begin()));
			}
		}
		out.WriteValue(creator);
		out.WriteValue(atom.position);
		out.WriteValue(atom.velocity);
		out.WriteValue(atom.rotation);
		out.WriteValue(atom.baseScale);
		out.WriteValue(atom.ruleScale);
		out.WriteValue(atom.stretch);
		out.WriteValue(atom.rgba);
		out.WriteValue(atom.specular);
		out.WriteValue(atom.birth);
		out.WriteValue(atom.visible);
		out.WriteValue(atom.frame);
		out.WriteValue(atom.frameRate);
		out.WriteValue(atom.playAnim);
		out.WriteValue(atom.gravity);
		out.WriteValue(atom.creatorValue);
		out.WriteValue(atom.random);
		out.WriteValue(atom.player);
		out.WriteValue(atom.deflected);
		out.WriteValue(atom.drawWeight);
		out.WritePlain(atom.previous);
		out.WritePlain(atom.current);
		out.WriteValue(atom.drawn);
		out.WriteValue(atom.steps);
		out.WriteValue(atom.drawOnFirstUpdate);
		out.WriteValue(atom.drawOffset);
		out.WriteValue(atom.drawOffsetFrom);
		out.WriteValue(atom.handOffset.has_value());
		out.WriteValue(atom.handOffset.value_or(HandOffset {}).reference);
		out.WriteValue(atom.handOffset.value_or(HandOffset {}).weight);
		out.WriteValue(atom.sharesParentOffset);
		out.Out().WriteSize(atom.sounds.size());
		for (const auto& sound : atom.sounds)
		{
			out.WriteShared(sound);
		}
		out.WriteShared(atom.carried);
		out.WriteValue(atom.fragment != nullptr);
		if (atom.fragment != nullptr)
		{
			out.WriteValue(atom.fragment->mesh);
			out.WriteValue(atom.fragment->positions);
			out.WriteValue(atom.fragment->uvs);
			out.WriteValue(atom.fragment->normals);
			out.WriteValue(atom.fragment->skins);
		}
		out.WriteValue(atom.surface != nullptr);
		if (atom.surface != nullptr)
		{
			SaveMesh(out, atom.surface->still);
			SaveMesh(out, atom.surface->mesh);
			out.WriteValue(atom.surface->uvPrevious);
			out.WriteValue(atom.surface->uvLast);
			out.WriteValue(atom.surface->uvCurrent);
		}
		out.WriteValue(atom.objectGlow.has_value());
		if (atom.objectGlow.has_value())
		{
			out.WritePlain(*atom.objectGlow);
		}
		out.WriteValue(atom.shower != nullptr);
		if (atom.shower != nullptr)
		{
			out.WriteValue(atom.shower->object);
			out.WriteValue(atom.shower->sparks.has_value());
			if (atom.shower->sparks.has_value())
			{
				out.WritePlain(*atom.shower->sparks);
			}
			out.WritePlain(atom.shower->clock);
		}
		out.Out().WriteSize(atom.subCollections.size());
		for (const auto& sub : atom.subCollections)
		{
			SaveStructure(*sub, out);
		}
	}
}

std::unique_ptr<Collection> Effect::LoadStructure(EffectReader& in, Atom* parent)
{
	auto collection = std::make_unique<Collection>();
	in.NumberCollection(collection.get());
	collection->parent = parent;
	in.ReadValue(collection->group);
	in.ReadValue(collection->birth);
	in.ReadValue(collection->alpha);
	in.ReadValue(collection->emitting);
	in.ReadValue(collection->interpolated);
	in.ReadValue(collection->hierarchy);
	in.ReadValue(collection->textureRepeats);
	in.ReadValue(collection->textureSpeed);
	in.ReadValue(collection->textureScroll);
	if (collection->group < 0 || collection->group >= static_cast<int>(k_GroupCount))
	{
		in.In().Fail();
		return nullptr;
	}
	const auto slots = in.In().ReadSize();
	if (!slots.has_value())
	{
		return nullptr;
	}
	for (size_t i = 0; i < *slots && !in.Failed(); ++i)
	{
		auto& slot = collection->modifiers.emplace_back();
		int number = -1;
		in.ReadValue(number);
		slot.modifier = ModifierAt(number);
		in.ReadValue(slot.attached);
		in.ReadPlain(slot.emitter);
		in.ReadValue(slot.state);
		in.ReadValue(slot.extra);
		in.ReadValue(slot.first);
		if (slot.modifier == nullptr)
		{
			// The file of the effect changed since the save
			in.In().Fail();
			return nullptr;
		}
	}
	const auto atoms = in.In().ReadSize();
	if (!atoms.has_value())
	{
		return nullptr;
	}
	for (size_t i = 0; i < *atoms && !in.Failed(); ++i)
	{
		auto& atom = *collection->atoms.emplace_back(std::make_unique<Atom>());
		in.NumberAtom(&atom);
		atom.collection = collection.get();
		std::string creator;
		in.ReadValue(creator);
		if (const auto rule = RuleCreatorNumber(creator); rule.has_value())
		{
			const auto* modifier = ModifierAt(*rule);
			atom.creator = modifier != nullptr ? modifier->OwnCreator() : nullptr;
		}
		else
		{
			atom.creator = creator.empty() ? nullptr : FindCreator(creator);
		}
		in.ReadValue(atom.position);
		in.ReadValue(atom.velocity);
		in.ReadValue(atom.rotation);
		in.ReadValue(atom.baseScale);
		in.ReadValue(atom.ruleScale);
		in.ReadValue(atom.stretch);
		in.ReadValue(atom.rgba);
		in.ReadValue(atom.specular);
		in.ReadValue(atom.birth);
		in.ReadValue(atom.visible);
		in.ReadValue(atom.frame);
		in.ReadValue(atom.frameRate);
		in.ReadValue(atom.playAnim);
		in.ReadValue(atom.gravity);
		in.ReadValue(atom.creatorValue);
		in.ReadValue(atom.random);
		in.ReadValue(atom.player);
		in.ReadValue(atom.deflected);
		in.ReadValue(atom.drawWeight);
		in.ReadPlain(atom.previous);
		in.ReadPlain(atom.current);
		in.ReadValue(atom.drawn);
		in.ReadValue(atom.steps);
		in.ReadValue(atom.drawOnFirstUpdate);
		in.ReadValue(atom.drawOffset);
		in.ReadValue(atom.drawOffsetFrom);
		bool followsHand = false;
		HandOffset handOffset;
		in.ReadValue(followsHand);
		in.ReadValue(handOffset.reference);
		in.ReadValue(handOffset.weight);
		if (followsHand)
		{
			atom.handOffset = handOffset;
		}
		in.ReadValue(atom.sharesParentOffset);
		const auto sounds = in.In().ReadSize();
		for (size_t s = 0; sounds.has_value() && s < *sounds && !in.Failed(); ++s)
		{
			if (auto sound = in.ReadShared<ParticleSoundLink>())
			{
				sound->atom = &atom;
				atom.sounds.push_back(std::move(sound));
			}
		}
		atom.carried = in.ReadShared<CarriedObject>();
		if (atom.carried)
		{
			atom.carried->atom = &atom;
		}
		bool has = false;
		in.ReadValue(has);
		if (has)
		{
			auto fragment = std::make_shared<blast::MeshFragment>();
			in.ReadValue(fragment->mesh);
			in.ReadValue(fragment->positions);
			in.ReadValue(fragment->uvs);
			in.ReadValue(fragment->normals);
			in.ReadValue(fragment->skins);
			atom.fragment = std::move(fragment);
			// An older save kept no creator for a fragment. The one rule of the collection that throws fragments made it.
			if (atom.creator == nullptr)
			{
				atom.creator = OwnCreatorOf(*collection, Creator::Kind::Fragment);
			}
		}
		in.ReadValue(has);
		if (has)
		{
			atom.surface = std::make_shared<SurfaceInstance>();
			LoadMesh(in, atom.surface->still);
			LoadMesh(in, atom.surface->mesh);
			in.ReadValue(atom.surface->uvPrevious);
			in.ReadValue(atom.surface->uvLast);
			in.ReadValue(atom.surface->uvCurrent);
			// A save from before the rule's creator had a name keeps no creator for the surface. The one surface rule
			// of the collection made it.
			if (atom.creator == nullptr)
			{
				atom.creator = OwnCreatorOf(*collection, Creator::Kind::Surface);
			}
		}
		in.ReadValue(has);
		if (has)
		{
			in.ReadPlain(atom.objectGlow.emplace());
		}
		in.ReadValue(has);
		if (has)
		{
			atom.shower = std::make_shared<ObjectShower>();
			in.ReadValue(atom.shower->object);
			in.ReadValue(has);
			if (has)
			{
				in.ReadPlain(atom.shower->sparks.emplace());
			}
			in.ReadPlain(atom.shower->clock);
		}
		const auto subs = in.In().ReadSize();
		for (size_t s = 0; subs.has_value() && s < *subs && !in.Failed(); ++s)
		{
			auto sub = LoadStructure(in, &atom);
			if (!sub)
			{
				return nullptr;
			}
			atom.subCollections.push_back(std::move(sub));
		}
	}
	return in.Failed() ? nullptr : std::move(collection);
}

void Effect::SaveRuleState(const Collection& collection, EffectWriter& out) const
{
	for (const auto& slot : collection.modifiers)
	{
		out.WriteValue(slot.data.has_value());
		if (slot.data.has_value())
		{
			// Each rule's state goes with its length, so that a rule that keeps nothing of it leaves the rest readable
			const auto at = out.Out().BeginLength();
			slot.modifier->SaveSlotData(slot, out);
			out.Out().EndLength(at);
		}
	}
	for (const auto& atom : collection.atoms)
	{
		out.Out().WriteSize(atom->data.size());
		// In the order of the rules, so that the same effect always gives the same bytes
		std::vector<std::pair<int, const AtomRuleData*>> entries;
		for (const auto& [modifier, data] : atom->data)
		{
			entries.emplace_back(ModifierNumber(modifier), &data);
		}
		std::ranges::sort(entries, {}, &std::pair<int, const AtomRuleData*>::first);
		for (const auto& [number, data] : entries)
		{
			out.WriteValue(number);
			out.WriteValue(data->started);
			out.WriteValue(data->object);
			out.WriteValue(data->a);
			out.WriteValue(data->b);
			out.WriteValue(data->held != nullptr);
			if (data->held != nullptr)
			{
				const auto at = out.Out().BeginLength();
				if (const auto* modifier = ModifierAt(number))
				{
					modifier->SaveHeld(*data, out);
				}
				out.Out().EndLength(at);
			}
		}
		for (const auto& sub : atom->subCollections)
		{
			SaveRuleState(*sub, out);
		}
	}
}

void Effect::LoadRuleState(Collection& collection, EffectReader& in)
{
	// A rule that keeps nothing of its state skips the bytes of it
	const auto skipTo = [&in](size_t end) {
		if (!in.Failed() && in.In().Position() <= end)
		{
			[[maybe_unused]] const auto skipped = in.In().Take(end - in.In().Position());
		}
		else
		{
			in.In().Fail();
		}
	};
	for (auto& slot : collection.modifiers)
	{
		bool has = false;
		in.ReadValue(has);
		if (!has)
		{
			continue;
		}
		const auto length = in.In().ReadSize();
		if (!length.has_value())
		{
			return;
		}
		const auto end = in.In().Position() + *length;
		slot.modifier->LoadSlotData(slot, in);
		skipTo(end);
	}
	for (auto& atom : collection.atoms)
	{
		const auto count = in.In().ReadSize();
		for (size_t i = 0; count.has_value() && i < *count && !in.Failed(); ++i)
		{
			int number = -1;
			in.ReadValue(number);
			const auto* modifier = ModifierAt(number);
			AtomRuleData data;
			in.ReadValue(data.started);
			in.ReadValue(data.object);
			in.ReadValue(data.a);
			in.ReadValue(data.b);
			bool held = false;
			in.ReadValue(held);
			if (held)
			{
				const auto length = in.In().ReadSize();
				if (!length.has_value())
				{
					return;
				}
				const auto end = in.In().Position() + *length;
				if (modifier != nullptr)
				{
					modifier->LoadHeld(data, in);
				}
				skipTo(end);
			}
			if (modifier != nullptr)
			{
				atom->data.insert_or_assign(modifier, std::move(data));
			}
		}
		// An older save kept no creator for a particle that a rule made with a creator of its own. The rule that keeps
		// a state for the particle and has its own creator made it.
		if (atom->creator == nullptr)
		{
			atom->creator = RuleCreatorOf(*atom);
		}
		for (auto& sub : atom->subCollections)
		{
			LoadRuleState(*sub, in);
		}
	}
}
