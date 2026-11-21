/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The rules of the hand's gestures: the trail a recognised gesture leaves on the land, and the chain that follows the
// hand while it draws one

#include <cmath>

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <ParticleFile.h>

#include "GestureTrail.h"
#include "LightSheet.h"
#include "ParticleArchive.h"
#include "ParticleClassRegistry.h"

using namespace openblack::particles;
using openblack::psys::ParticleObject;
namespace trail = openblack::particles::gesture_trail;

namespace
{
/// The in-game bank's sound of a gesture recognised
constexpr uint32_t k_RecognisedSample = 36;
constexpr float k_ByteMax = 255.0f;

/// What the trail rule keeps for each of its atoms, one atom a trail
struct TrailAtom
{
	std::shared_ptr<GestureTrail> trail;
	std::shared_ptr<LightSheet> sheet;
	/// Its particles have been made, when its collection was this old
	bool started {false};
	float startAge {0.0f};
	/// The particles' size, from the shape's length
	float scale {1.0f};
	/// The box round the shape once raised
	glm::vec3 boxMin {0.0f};
	glm::vec3 boxMax {0.0f};
	/// The drawing player's colour, 0xRRGGBB
	uint32_t colour {0};
	/// The particles in a shuffled order, so that the wiggles of neighbours differ
	std::vector<int32_t> order;
};
using TrailAtoms = std::unordered_map<const Atom*, TrailAtom>;

/// A recognised gesture's trail: an atom for each trail the gesture system lays out, with the gesture's recognised sound,
/// a collection of particles that flow from the path the hand drew to the gesture's shape over the land, wiggling and
/// flashing in the player's colour before they disperse, and a sheet of light rising along the shape. The hand glows the
/// player's colour as the particles flash.
class GesturingRecognised final: public Modifier
{
public:
	void SaveSlotData(const Collection::Slot& slot, EffectWriter& out) const override
	{
		WriteAtomMap(out, std::any_cast<const TrailAtoms&>(slot.data), [&out](const TrailAtom& trail) {
			out.WriteShared(trail.trail);
			out.WriteShared(trail.sheet);
			out.Write(trail.started);
			out.Write(trail.startAge);
			out.Write(trail.scale);
			out.Write(trail.boxMin);
			out.Write(trail.boxMax);
			out.Write(trail.colour);
			out.WritePlainList(trail.order);
		});
	}
	void LoadSlotData(Collection::Slot& slot, EffectReader& in) const override
	{
		TrailAtoms trails;
		ReadAtomMap(in, trails, [&in](TrailAtom& trail) {
			trail.trail = in.ReadShared<GestureTrail>();
			trail.sheet = in.ReadShared<LightSheet>();
			in.Read(trail.started);
			in.Read(trail.startAge);
			in.Read(trail.scale);
			in.Read(trail.boxMin);
			in.Read(trail.boxMax);
			in.Read(trail.colour);
			in.ReadPlainList(trail.order);
		});
		slot.data = std::move(trails);
	}

	explicit GesturingRecognised(const ParticleObject& object)
	    : _creator(object.String("PCreator"))
	    , _spriteCreator(object.String("SpriteCreator"))
	    , _nextGroups(object.IntArray("NextGroups"))
	    , _sheetHeightScale(object.Float("LightSheetHeightScale", 0.0f))
	    , _timeToIdeal(object.Float("TimeToIdeal", 0.0f))
	    , _interpGain(object.Float("InterpGain", 0.0f))
	    , _dieAge(object.Float("DieAge", 0.0f))
	    , _sheetDieAge(object.Float("LightSheetDieAge", 0.0f))
	    , _maxAlpha(object.Float("MaxAlpha", 0.0f))
	    , _numAtoms(object.Int("NumAtoms", 0))
	    , _wiggleFreq(object.Float("WiggleFreq", 0.0f))
	    , _wiggleMag(object.Float("WiggleMag", 0.0f))
	    , _wiggleMagY(object.Float("WiggleMagY", 0.0f))
	    , _wiggleSpeed(object.Float("WiggleSpeed", 0.0f))
	    , _wigglePhaseSpeed(object.Float("WigglePhaseSpeed", 0.0f))
	    , _dispersalTime(object.Float("DispersalTime", 0.0f))
	    , _collectionAlphaPulse(object.Int("CollectionAlphaPulse", 0))
	    , _collectionAlphaInit(object.Int("CollectionAlphaInit", 0))
	    , _shrinkTime(object.Float("ShrinkTimeAfterDispersal", 0.0f))
	    , _handPulseDuration(object.Float("HandPulseDuration", 0.0f))
	    , _sparkleGroup(object.Int("SparkleGroup", 0))
	    , _goToIdeal(object.Bool("GoToIdeal", false))
	    , _doTransition(object.Bool("DoTransition", false))
	{
	}

	[[nodiscard]] bool Creates() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& slot) const override
	{
		const auto* pointCreator = effect.FindCreator(_creator);
		if (pointCreator == nullptr)
		{
			return false;
		}
		if (!slot.data.has_value())
		{
			slot.data = TrailAtoms {};
		}
		auto& trails = *std::any_cast<TrailAtoms>(&slot.data);
		auto& world = effect.Services().world;

		// One waiting trail a step becomes an atom, heard as it appears
		if (auto waiting = world.TakeGestureTrail())
		{
			auto& atom = effect.NewAtom(collection, pointCreator, _nextGroups);
			// The gesture of this computer's player is heard with no position, another player's at the place of its hand
			if (waiting->heardAt.has_value())
			{
				world.PlayPointSound(k_RecognisedSample, *waiting->heardAt);
			}
			else
			{
				world.PlayListenerSound(k_RecognisedSample, false);
			}
			trails[&atom].trail = std::move(waiting);
		}

		// A trail ends when its atom has lived its time, and the hand's glow goes with it
		std::erase_if(collection.atoms, [&](const AtomPtr& atom) {
			if (effect.AtomAge(*atom) <= _dieAge)
			{
				return false;
			}
			// Only the trail of this computer's player made the hand glow
			const auto found = trails.find(atom.get());
			if (found != trails.end())
			{
				const bool own = found->second.trail != nullptr && !found->second.trail->heardAt.has_value();
				trails.erase(found);
				if (own)
				{
					world.SetHandGlow(0);
				}
			}
			return true;
		});

		// The newest first
		for (auto it = collection.atoms.rbegin(); it != collection.atoms.rend(); ++it)
		{
			auto& atom = **it;
			const auto found = trails.find(&atom);
			if (found == trails.end() || found->second.trail == nullptr)
			{
				continue;
			}
			auto& data = found->second;
			for (auto& sub : atom.subCollections)
			{
				if (sub->group == _sparkleGroup && data.started)
				{
					CarryRound(effect, atom, *sub, data);
				}
				else
				{
					ModifyParticles(effect, *sub, data);
				}
			}
			const float age = effect.AtomAge(atom);
			if (data.sheet == nullptr)
			{
				std::vector<glm::vec3> points;
				points.reserve(trail::k_SheetPoints);
				const auto step = 1.0f / static_cast<float>(trail::k_SheetPoints - 1);
				for (int i = 0; i < trail::k_SheetPoints; ++i)
				{
					points.push_back(data.trail->ideal.At(static_cast<float>(i) * step));
				}
				data.sheet = std::make_shared<LightSheet>();
				data.sheet->Start(std::move(points), data.colour, data.scale * _sheetHeightScale, trail::k_SheetShiftSeconds);
				world.AddLightSheet(data.sheet);
			}
			data.sheet->SetStrength(trail::SheetStrength(age, _sheetDieAge));
		}
		return true;
	}

private:
	/// The trail's atom goes round the shape, fading out over the end of its life
	void CarryRound(Effect& effect, Atom& atom, Collection& sub, const TrailAtom& data) const
	{
		const float age = effect.AtomAge(atom);
		const float round = std::fmod(age / trail::k_CircuitSeconds, 1.0f);
		atom.position = data.trail->ideal.At(round);
		const float left = _dieAge - age;
		if (left < trail::k_CircuitFadeSeconds)
		{
			const float alpha = std::clamp(left * (k_ByteMax / trail::k_CircuitFadeSeconds), 0.0f, k_ByteMax);
			const auto level = static_cast<uint8_t>(static_cast<int>(alpha));
			atom.rgba[3] = level;
			sub.alpha = level;
		}
	}

	/// The particles flowing from the drawn path to the shape
	void ModifyParticles(Effect& effect, Collection& sub, TrailAtom& data) const
	{
		auto& world = effect.Services().world;
		if (!data.started)
		{
			Start(effect, sub, data);
		}
		const float age = effect.CollectionAge(sub);

		// From the drawn path to the shape over a time, eased by the gain
		const float grown = std::min(age - data.startAge, _timeToIdeal) / _timeToIdeal;
		const auto transition = trail::Transition(grown, _interpGain);
		const auto width = data.boxMax.x - data.boxMin.x;
		const auto depth = data.boxMax.z - data.boxMin.z;
		const auto wiggle = trail::WiggleAmount(age, _wigglePhaseSpeed, _dispersalTime, _shrinkTime);

		// A while in, the particles flash and the hand glows in the player's colour, both dying away. Only the trail of
		// this computer's player makes the hand glow: the trail of another player has a place where it is heard.
		const float sinceFlash = age - trail::k_FlashStart;
		if (sinceFlash > 0.0f)
		{
			const float flash = static_cast<float>(_collectionAlphaPulse) * trail::FlashLeft(sinceFlash, trail::k_FlashSeconds);
			sub.alpha = static_cast<uint8_t>(static_cast<int>(flash));
		}
		if (sinceFlash > 0.0f && !data.trail->heardAt.has_value())
		{
			const auto glow =
			    static_cast<uint8_t>(static_cast<int>(trail::FlashLeft(sinceFlash, _handPulseDuration) * k_ByteMax));
			world.SetHandGlow(trail::Dimmed(data.colour, glow));
		}

		const auto count = sub.atoms.size();
		const float step = 1.0f / (static_cast<float>(count) - 1.0f);
		const auto perSecond = 1.0f / effect.GetDt();
		const auto& noise = effect.Services().noise;
		// The newest first
		for (size_t i = 0; i < count; ++i)
		{
			auto& atom = *sub.atoms[count - 1 - i];
			const float along = static_cast<float>(i) * step;
			atom.rgba[3] = trail::RevealAlpha(along, grown, _maxAlpha);

			glm::vec3 position;
			if (_doTransition)
			{
				const auto drawn = data.trail->drawn.At(along);
				const auto ideal = data.trail->ideal.At(along);
				position = drawn + ((ideal - drawn) * transition);
			}
			else
			{
				position = _goToIdeal ? data.trail->ideal.At(along) : data.trail->drawn.At(along);
			}

			// Each wiggles by its own place in the shuffled order, across the shape's box
			const float place = static_cast<float>(data.order.at(i)) * step;
			const float sweep = age * _wiggleSpeed;
			const auto phase = [&](float offset) { return sweep + ((place + offset) * _wiggleFreq); };
			const float acrossX = noise.Smooth(phase(0.0f)) * width * _wiggleMag;
			const float acrossZ = noise.Smooth(phase(trail::k_WiggleOffsetZ)) * depth * _wiggleMag;
			const float up = (noise.Smooth(phase(trail::k_WiggleOffsetY)) + 1.0f) * 0.5f * _wiggleMagY * depth;
			position.x = (acrossX * wiggle) + position.x;
			position.y = position.y + (up * wiggle);
			position.z = position.z + (acrossZ * wiggle);

			atom.velocity = (position - atom.position) * perSecond;
			atom.position = position;
		}
	}

	/// The first step of a trail's particles: the shape is raised towards the camera, the particles are made in the
	/// player's colour, sized by the shape's length, and put in a shuffled order
	void Start(Effect& effect, Collection& sub, TrailAtom& data) const
	{
		auto& world = effect.Services().world;
		data.startAge = effect.CollectionAge(sub);
		data.started = true;
		sub.alpha = static_cast<float>(static_cast<uint8_t>(_collectionAlphaInit));
		data.scale = data.trail->ideal.Length() * trail::k_ScalePerLength;

		const auto camera = world.CameraPosition();
		auto& ideal = data.trail->ideal;
		for (size_t i = 0; i < ideal.Size(); ++i)
		{
			ideal.SetPoint(i, trail::Lift(ideal.Points()[i], camera, data.scale));
		}

		data.colour = world.PlayerColour(effect.GetPlayer()) & 0xFFFFFFu;
		if (const auto* sprite = effect.FindCreator(_spriteCreator))
		{
			for (int i = 0; i < _numAtoms; ++i)
			{
				auto& atom = effect.NewAtom(sub, sprite, {});
				atom.rgba[0] = static_cast<uint8_t>(data.colour >> 16u);
				atom.rgba[1] = static_cast<uint8_t>(data.colour >> 8u);
				atom.rgba[2] = static_cast<uint8_t>(data.colour);
				atom.baseScale *= data.scale;
			}
		}
		if (sub.parent != nullptr)
		{
			sub.parent->ruleScale *= data.scale;
		}

		data.boxMin = data.boxMax = ideal.Points().front();
		for (const auto& point : ideal.Points())
		{
			data.boxMin = glm::min(data.boxMin, point);
			data.boxMax = glm::max(data.boxMax, point);
		}

		data.order.resize(static_cast<size_t>(std::max(_numAtoms, 0)));
		for (size_t i = 0; i < data.order.size(); ++i)
		{
			data.order[i] = static_cast<int32_t>(i);
		}
		for (int k = 0; k < _numAtoms * 2; ++k)
		{
			const auto a = static_cast<size_t>(effect.Rand(_numAtoms));
			const auto b = static_cast<size_t>(effect.Rand(_numAtoms));
			std::swap(data.order.at(a), data.order.at(b));
		}
	}

	std::string _creator;
	std::string _spriteCreator;
	std::vector<int> _nextGroups;
	float _sheetHeightScale;
	float _timeToIdeal;
	float _interpGain;
	float _dieAge;
	float _sheetDieAge;
	float _maxAlpha;
	int _numAtoms;
	float _wiggleFreq;
	float _wiggleMag;
	float _wiggleMagY;
	float _wiggleSpeed;
	float _wigglePhaseSpeed;
	float _dispersalTime;
	int _collectionAlphaPulse;
	int _collectionAlphaInit;
	float _shrinkTime;
	float _handPulseDuration;
	int _sparkleGroup;
	bool _goToIdeal;
	bool _doTransition;
};

/// What the chain rule keeps for each of its atoms
struct ChainAtom
{
	/// It follows the hand until the hand stops gesturing, then waits a while to go
	bool active {true};
	bool first {true};
	float stoppedAt {0.0f};
	/// Where the hand was at the last step, and where the last joint was laid
	glm::vec3 last {0.0f};
	glm::vec3 lastJoint {0.0f};
	/// How many joints the hand's movement has asked for so far, and how many have been laid
	float wanted {0.0f};
	float laid {0.0f};

	/// Gives each field to a visitor, so that a save writes zero into the gaps between the fields
	template <typename Visitor>
	static void VisitPlainFields(const ChainAtom& self, Visitor&& visit)
	{
		visit(self.active, self.first, self.stoppedAt, self.last, self.lastJoint, self.wanted, self.laid);
	}
};
struct ChainState
{
	bool gesturing {false};
	std::unordered_map<const Atom*, ChainAtom> atoms;
};

/// The chain that follows the hand while it draws a gesture: each time the hand starts gesturing, whatever chain there
/// was goes and an atom is made whose collection of joints is the chain. The joints are laid at the hand as it moves,
/// one each time the hand has gone the emit distance from the last (no faster than the whole chain each lifetime), the
/// older joints moving down the chain. Once the hand stops gesturing, the chain is left to fade and goes a few seconds
/// later.
class ChainGesture final: public Modifier
{
public:
	void SaveSlotData(const Collection::Slot& slot, EffectWriter& out) const override
	{
		const auto& state = std::any_cast<const ChainState&>(slot.data);
		out.Write(state.gesturing);
		WriteAtomMap(out, state.atoms, [&out](const ChainAtom& atom) { out.WritePlain(atom); });
	}
	void LoadSlotData(Collection::Slot& slot, EffectReader& in) const override
	{
		ChainState state;
		in.Read(state.gesturing);
		ReadAtomMap(in, state.atoms, [&in](ChainAtom& atom) { in.ReadPlain(atom); });
		slot.data = std::move(state);
	}

	/// A chain the hand has stopped drawing goes this long after
	static constexpr float k_LeftSeconds = 5.0f;

	explicit ChainGesture(const ParticleObject& object)
	    : _creator(object.String("PCreator"))
	    , _nextGroups(object.IntArray("NextGroups"))
	    , _dieAge(object.Float("DieAge", 0.0f))
	    , _minEmitDist(object.Float("MinEmitDist", 0.0f))
	    , _adjustInitialScale(object.String("AdjustInitialScale"))
	    , _inTestMode(object.Bool("InTestMode", false))
	{
	}

	[[nodiscard]] bool Creates() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& slot) const override
	{
		const auto* pointCreator = effect.FindCreator(_creator);
		if (pointCreator == nullptr)
		{
			return false;
		}
		if (!slot.data.has_value())
		{
			slot.data = ChainState {};
		}
		auto& state = *std::any_cast<ChainState>(&slot.data);
		const bool was = state.gesturing;
		state.gesturing = _inTestMode || effect.GetProcessInfo().enabled;
		if (state.gesturing && !was)
		{
			collection.atoms.clear();
			state.atoms.clear();
			effect.NewAtom(collection, pointCreator, _nextGroups);
		}

		// The newest first
		for (size_t n = collection.atoms.size(); n-- > 0;)
		{
			auto& atom = *collection.atoms[n];
			auto& data = state.atoms[&atom];
			if (!data.active && effect.AtomAge(atom) - data.stoppedAt > k_LeftSeconds)
			{
				state.atoms.erase(&atom);
				collection.atoms.erase(collection.atoms.begin() + static_cast<std::ptrdiff_t>(n));
				continue;
			}
			if (!data.active)
			{
				continue;
			}
			if (!state.gesturing)
			{
				data.active = false;
				data.stoppedAt = effect.AtomAge(atom);
				continue;
			}
			if (atom.subCollections.empty())
			{
				return true;
			}
			Lay(effect, data, *atom.subCollections.back(), atom.position);
		}
		return true;
	}

private:
	/// The hand at a point lays the joints its movement asks for, each between where the hand was and where it is
	void Lay(Effect& effect, ChainAtom& data, Collection& chain, glm::vec3 hand) const
	{
		if (chain.atoms.empty())
		{
			return;
		}
		if (data.first)
		{
			data.lastJoint = hand;
			data.last = hand;
			for (auto& joint : chain.atoms)
			{
				joint->position = hand;
				joint->ruleScale = 0.0f;
			}
		}
		const float headScale = _adjustInitialScale.empty() ? 1.0f : effect.FloatProvider(_adjustInitialScale, 1.0f);
		const float before = data.wanted;
		const float most = static_cast<float>(chain.atoms.size()) / _dieAge * effect.GetDt();
		float asked = 1.0f;
		if (data.laid != 0.0f)
		{
			const auto away = data.lastJoint - hand;
			asked = std::sqrt((away.z * away.z) + (away.y * away.y) + (away.x * away.x)) / _minEmitDist;
		}
		if (!(most > asked))
		{
			asked = most;
		}
		data.wanted = asked + before;
		for (bool more = data.wanted - 1.0f > data.laid; more; more = data.wanted - 1.0f > data.laid)
		{
			data.laid += 1.0f;
			// Each joint takes the place, age and size of the one newer than it
			for (size_t j = 0; j + 1 < chain.atoms.size(); ++j)
			{
				auto& joint = *chain.atoms[j];
				const auto& newer = *chain.atoms[j + 1];
				joint.position = newer.position;
				joint.birth = newer.birth;
				joint.baseScale = newer.baseScale;
			}
			auto& head = *chain.atoms.back();
			glm::vec3 point = hand;
			if (data.wanted != before)
			{
				const float t = (data.laid - before) / (data.wanted - before);
				point = data.last + ((hand - data.last) * t);
				head.birth = effect.GetAge() - (effect.GetDt() * t);
			}
			head.position = point;
			data.lastJoint = point;
			head.baseScale = headScale;
			head.ruleScale = 1.0f;
		}
		data.first = false;
		data.last = hand;
	}

	std::string _creator;
	std::vector<int> _nextGroups;
	float _dieAge;
	float _minEmitDist;
	std::string _adjustInitialScale;
	bool _inTestMode;
};

/// The joints of a chain, all made at once, and the rule is done
class MakeChain final: public Modifier
{
public:
	explicit MakeChain(const ParticleObject& object)
	    : creator(object.String("PCreator"))
	    , numAtoms(object.Int("NumAtoms", 0))
	{
	}

	[[nodiscard]] bool Creates() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		if (const auto* joint = effect.FindCreator(creator))
		{
			for (int i = 0; i < numAtoms; ++i)
			{
				effect.NewAtom(collection, joint, {});
			}
		}
		return false;
	}

	std::string creator;
	int numAtoms;
};
} // namespace

void openblack::particles::RegisterGestureRules(ParticleClassRegistry& registry)
{
	registry.AddModifier("UR_GesturingRecognised", ParticleClassRegistry::Make<GesturingRecognised>);
	registry.AddModifier("ZR_ChainGesture", ParticleClassRegistry::Make<ChainGesture>);
	registry.AddModifier("CreateRuleMakeChain", ParticleClassRegistry::Make<MakeChain>);
}
