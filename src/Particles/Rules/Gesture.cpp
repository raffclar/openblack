/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The gesture effects' rules: UR_GesturingRecognised (SF_Gesture, the sparkles of a recognised gesture), ZR_ChainGesture
// and CreateRuleMakeChain (SF_GestureChain, the trail at the hand, drawn by ParticleChainCreator).
// Wiki: docs/bw1-notes/magic.md, "Efectos de utilidad".

#include <cmath>

#include <algorithm>
#include <numeric>
#include <unordered_map>
#include <vector>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include "Audio/Audio.h"
#include "Camera/Camera.h"
#include "Locator.h"
#include "Magic/Gestures/GestureShapes.h"
#include "Particles/Noise.h"
#include "Particles/PSys.h"
#include "Particles/PSysFile.h"
#include "Particles/PSysRegistry.h"
#include "Particles/Utility.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
/// The player colour of this interface's player, from the player colour table by its remapped number (the remap is the
/// identity here, as in Particles/TownBelief.cpp; the local player is number 0). (inferred) every record is drawn in this
/// colour: the record's own player and the remap are not used
constexpr uint32_t k_LocalPlayerColour = 0xFF4646;
/// When the collection's pulse starts, and how long it lasts
constexpr float k_PulseStart = 2.4f;
constexpr float k_PulseLength = 2.1f;
/// the light sheet's points
constexpr int k_LightSheetPoints = 50;

/// UR_GesturingRecognised: one
/// atom per recognised gesture record, whose sub-collection holds NumAtoms sprites that rise from the drawn stroke onto
/// the gesture's ideal shape lifted towards the camera, wiggle and fade
class GesturingRecognised final: public Modifier
{
public:
	explicit GesturingRecognised(const Object& object)
	    : creator(object.String("PCreator"))
	    , nextGroups(object.Array("NextGroups"))
	    , lightSheetHeightScale(object.Float("LightSheetHeightScale", 0.0f))
	    , timeToIdeal(object.Float("TimeToIdeal", 0.0f))
	    , interpGain(object.Float("InterpGain", 0.0f))
	    , dieAge(object.Float("DieAge", 0.0f))
	    , lightSheetDieAge(object.Float("LightSheetDieAge", 0.0f))
	    , maxAlpha(object.Float("MaxAlpha", 0.0f))
	    , numAtoms(object.Int("NumAtoms", 10))
	    , spriteCreator(object.String("SpriteCreator"))
	    , wiggleFreq(object.Float("WiggleFreq", 0.0f))
	    , wiggleMag(object.Float("WiggleMag", 0.0f))
	    , wiggleMagY(object.Float("WiggleMagY", 0.0f))
	    , wiggleSpeed(object.Float("WiggleSpeed", 0.0f))
	    , wigglePhaseSpeed(object.Float("WigglePhaseSpeed", 0.0f))
	    , dispersalTime(object.Float("DispersalTime", 0.0f))
	    , collectionAlphaPulse(object.Int("CollectionAlphaPulse", 0))
	    , collectionAlphaInit(object.Int("CollectionAlphaInit", 0))
	    , shrinkTimeAfterDispersal(object.Float("ShrinkTimeAfterDispersal", 0.0f))
	    , sparkleGroup(object.Int("SparkleGroup", -1))
	    , goToIdeal(object.Bool("GoToIdeal", false))
	    , doTransition(object.Bool("DoTransition", false))
	{
		// (inferred) the defaults above: the original's were not read (SF_Gesture sets DieAge, TimeToIdeal and the rest)
		// HeightOffset, ExplodeFactor, ExplodePause and HandPulseDuration are properties too; the first three are not
		// used by the rule, the last one only by the hand pulse (not ported)
	}

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		auto& pending = utility::PendingRecognised();
		// a new record: one atom of PCreator (its NextGroups sub-collection gets the sprites) and the recognition sound
		if (!pending.empty())
		{
			const auto* pointCreator = effect.FindCreator(creator);
			if (pointCreator == nullptr)
			{
				return false;
			}
			auto& atom = effect.NewAtom(collection, pointCreator, nextGroups);
			auto& data = _data[&atom];
			data.record = std::move(pending.back());
			pending.pop_back();
			PlayRecognisedSound(data);
		}
		// the atoms older than DieAge go (with their sprites)
		std::erase_if(collection.atoms, [&](const std::unique_ptr<Atom>& atom) {
			if (effect.AtomAge(*atom) > dieAge)
			{
				_data.erase(atom.get());
				return true;
			}
			return false;
		});
		for (auto& atom : collection.atoms)
		{
			auto found = _data.find(atom.get());
			if (found == _data.end())
			{
				continue;
			}
			auto& data = found->second;
			for (auto& sub : atom->subCollections)
			{
				if (sub->group == sparkleGroup && data.initialised)
				{
					// the sparkle group (none in SF_Gesture): the atom runs along the ideal once every 2 s and fades out in
					// its last 2 s (x 127.5: from 255 to 0)
					atom->position = data.record.ideal.At(std::fmod(effect.AtomAge(*atom) / 2.0f, 1.0f));
					const float left = dieAge - effect.AtomAge(*atom);
					if (left < 2.0f)
					{
						const auto alpha = static_cast<uint8_t>(std::clamp(left * 127.5f, 0.0f, 255.0f));
						atom->colour[3] = alpha;
						sub->alpha = static_cast<float>(alpha);
					}
				}
				else
				{
					ModifySubCollection(effect, *sub, data);
				}
			}
			// the light sheet: 50 points along the ideal, the player's colour, height = scale x LightSheetHeightScale, a
			// width of 0.03, and its alpha 1 - (2f - 1)^2 over LightSheetDieAge. The data is kept; the light sheet's draw
			// is not ported.
			if (!data.lightSheetMade)
			{
				data.lightSheetMade = true;
				data.lightSheet.clear();
				for (int i = 0; i < k_LightSheetPoints; ++i)
				{
					data.lightSheet.push_back(data.record.ideal.At(static_cast<float>(i) * (1.0f / 49.0f)));
				}
				data.lightSheetHeight = data.scale * lightSheetHeightScale;
			}
			const float f = std::clamp(effect.AtomAge(*atom) / lightSheetDieAge, 0.0f, 1.0f);
			const float g = 2.0f * f - 1.0f;
			data.lightSheetAlpha = 1.0f - g * g;
		}
		return true;
	}

private:
	/// What the rule keeps for one atom
	struct AtomData
	{
		magic::gestures::RecognisedGesture record;
		bool initialised {false};
		float start {0.0f};   ///< the collection's age when it began
		float scale {1.0f};   ///< the ideal's length x 0.01
		glm::vec3 min {0.0f}; ///< the ideal's box
		glm::vec3 max {0.0f};
		std::vector<int> order; ///< the atoms' shuffled wiggle phases
		bool lightSheetMade {false};
		std::vector<glm::vec3> lightSheet;
		float lightSheetHeight {0.0f};
		float lightSheetAlpha {0.0f};
		/// the atom data as a channel owner, for another interface's recognition sound
		uint32_t soundOwner {0};
	};

	/// A record of this computer's interface (openblack has only that one, RecognisedGesture::fromInterface) plays
	/// LH_SAMPLE_G_SPELLGESTURERECOGNISE (0x24) in 2D: no owner, mode 3, no loops, not 3D, the in-game bank; another
	/// interface's one plays at its hand: the in-game bank, owned by the atom data, 3D, not tracked, at the record's hand
	/// position
	static void PlayRecognisedSound(AtomData& data)
	{
		if (data.record.fromInterface)
		{
			audio::PlaySoundEffect(audio::Owner::None(), 0x24, 3, 0, false, false, audio::SfxBank::InGame);
			return;
		}
		if (data.soundOwner == 0)
		{
			data.soundOwner = audio::NewObjectId();
		}
		audio::PlayOptions options;
		options.sample = {audio::Bank(audio::SfxBank::InGame), 0x24};
		options.owner = audio::Owner::Object(data.soundOwner);
		options.is3D = true;
		options.track = false;
		options.position = data.record.handPosition;
		audio::PlaySoundEffect(options);
	}

	void ModifySubCollection(Effect& effect, Collection& sub, AtomData& data) const
	{
		auto& stroke = data.record.stroke;
		auto& ideal = data.record.ideal;
		if (!data.initialised)
		{
			data.start = effect.CollectionAge(sub);
			data.initialised = true;
			ideal.Measure();
			sub.alpha = static_cast<float>(static_cast<uint8_t>(collectionAlphaInit));
			// the scale's factor is never set: the scale is always the ideal's length / 100
			data.scale = ideal.Length() * 0.01f;
			// every ideal point comes towards the camera until it is `scale` higher (at most half-way)
			const glm::vec3 camera = Locator::camera::has_value() ? Locator::camera::value().GetOrigin() : glm::vec3(0.0f);
			for (size_t k = 0; k < ideal.Size(); ++k)
			{
				glm::vec3 d = camera - ideal[k];
				float length = 0.0f;
				if (d != glm::vec3(0.0f))
				{
					length = glm::length(d);
					d /= length;
				}
				if (d.y > 0.0f)
				{
					const float s = std::min(data.scale / d.y, length * 0.5f);
					ideal[k] += d * s;
				}
			}
			stroke.Measure();
			ideal.Measure();
			// the sprites: SpriteCreator's atoms in the player's colour (the alpha is set below), their scale x `scale`
			const auto* sprites = effect.FindCreator(spriteCreator);
			for (int i = 0; sprites != nullptr && i < numAtoms; ++i)
			{
				auto& atom = effect.NewAtom(sub, sprites, {});
				atom.colour[0] = static_cast<uint8_t>(k_LocalPlayerColour >> 16);
				atom.colour[1] = static_cast<uint8_t>(k_LocalPlayerColour >> 8);
				atom.colour[2] = static_cast<uint8_t>(k_LocalPlayerColour);
				atom.baseScale *= data.scale;
			}
			if (sub.parent != nullptr)
			{
				sub.parent->ruleScale *= data.scale;
			}
			data.min = ideal.Size() > 0 ? ideal[0] : glm::vec3(0.0f);
			data.max = data.min;
			for (size_t k = 0; k < ideal.Size(); ++k)
			{
				data.min = glm::min(data.min, ideal[k]);
				data.max = glm::max(data.max, ideal[k]);
			}
			// a shuffled order: identity, then 2 x NumAtoms swaps of two Rand(NumAtoms) slots
			data.order.resize(static_cast<size_t>(std::max(numAtoms, 0)));
			std::iota(data.order.begin(), data.order.end(), 0);
			for (int i = 0; !data.order.empty() && i < 2 * numAtoms; ++i)
			{
				const auto a = static_cast<size_t>(effect.Rand(numAtoms));
				const auto b = static_cast<size_t>(effect.Rand(numAtoms));
				std::swap(data.order[a], data.order[b]);
			}
		}
		const float age = effect.CollectionAge(sub);
		// t: how far towards the ideal (0..1 over TimeToIdeal), e: t pulled towards smoothstep by InterpGain
		const float t = std::min(age - data.start, timeToIdeal) / timeToIdeal;
		const float e = t + ((3.0f - 2.0f * t) * t * t - t) * interpGain;
		const float width = data.max.x - data.min.x;
		const float depth = data.max.z - data.min.z;
		float wiggle = (std::cos(age * wigglePhaseSpeed) + 1.0f) * 0.5f;
		const float peak = t * maxAlpha;
		if (age - dispersalTime >= 0.0f)
		{
			wiggle *= std::clamp(1.0f - (age - dispersalTime) / shrinkTimeAfterDispersal, 0.0f, 1.0f);
		}
		// after 2.4 s the collection pulses from CollectionAlphaPulse to 0 over 2.1 s; the hand of this interface pulses in
		// the player's colour over HandPulseDuration (not ported)
		if (const float h = age - k_PulseStart; h > 0.0f)
		{
			const float f = std::clamp(1.0f - h / k_PulseLength, 0.0f, 1.0f);
			sub.alpha =
			    static_cast<float>(static_cast<uint8_t>(static_cast<int>(static_cast<float>(collectionAlphaPulse) * f)));
		}
		const float step = sub.atoms.size() > 1 ? 1.0f / static_cast<float>(sub.atoms.size() - 1) : 0.0f;
		// the max(dt, eps) is a port guard: the original multiplies by 1/dt directly
		const float perSecond = 1.0f / std::max(effect.GetDt(), 1e-4f);
		for (size_t i = 0; i < sub.atoms.size(); ++i)
		{
			auto& atom = *sub.atoms[i];
			const float u = static_cast<float>(i) * step;
			// alpha: the two ends of the line appear first and meet in the middle as t grows
			float alpha = 0.0f;
			if (u < t)
			{
				alpha += peak - peak * (u / t);
			}
			if (1.0f - u < t)
			{
				alpha += peak - peak * ((1.0f - u) / t);
			}
			atom.colour[3] = static_cast<uint8_t>(static_cast<int>(std::clamp(alpha, 0.0f, 255.0f)));
			glm::vec3 position;
			if (doTransition)
			{
				const auto from = stroke.At(u);
				position = from + (ideal.At(u) - from) * e;
			}
			else
			{
				position = goToIdeal ? ideal.At(u) : stroke.At(u);
			}
			// the wiggle: value noise at the atom's shuffled phase, x and z by the ideal's box, y upwards only (the
			// phase offsets +0.3 and +0.7)
			const float phase = static_cast<float>(i < data.order.size() ? data.order[i] : 0) * step;
			const float nx = noise::SignedValueNoise(phase * wiggleFreq + age * wiggleSpeed) * width * wiggleMag;
			const float nz = noise::SignedValueNoise((phase + 0.3f) * wiggleFreq + age * wiggleSpeed) * depth * wiggleMag;
			const float ny =
			    (noise::SignedValueNoise((phase + 0.7f) * wiggleFreq + age * wiggleSpeed) + 1.0f) * 0.5f * wiggleMagY * depth;
			position += glm::vec3(nx, ny, nz) * wiggle;
			atom.velocity = (position - atom.position) * perSecond;
			atom.position = position;
		}
	}

	std::string creator;
	std::vector<int> nextGroups;
	float lightSheetHeightScale;
	float timeToIdeal;
	float interpGain;
	float dieAge;
	float lightSheetDieAge;
	float maxAlpha;
	int numAtoms;
	std::string spriteCreator;
	float wiggleFreq;
	float wiggleMag;
	float wiggleMagY;
	float wiggleSpeed;
	float wigglePhaseSpeed;
	float dispersalTime;
	int collectionAlphaPulse;
	int collectionAlphaInit;
	float shrinkTimeAfterDispersal;
	int sparkleGroup;
	bool goToIdeal;
	bool doTransition;
	/// the atoms' AtomData (the original keeps it in the atom's modifier-data list)
	mutable std::unordered_map<const Atom*, AtomData> _data;
};

/// ZR_ChainGesture: while the effect is enabled one head atom at the gesture position trails a chain: its
/// NextGroups sub-collection's joints are a shift register, a new joint at the head every MinEmitDist of movement (at
/// most count / DieAge joints per second). When the effect stops being enabled the head stops emitting and goes 5 s
/// later.
class ChainGesture final: public Modifier
{
public:
	explicit ChainGesture(const Object& object)
	    : creator(object.String("PCreator"))
	    , nextGroups(object.Array("NextGroups"))
	    , dieAge(object.Float("DieAge", 0.0f))
	    , minEmitDist(object.Float("MinEmitDist", 0.0f))
	    , adjustInitialScale(object.String("AdjustInitialScale"))
	    , inTestMode(object.Bool("InTestMode", false))
	{
		// (inferred) the defaults above: the original's were not read
		// PredictNextPosition and DrawOnFirstUpdate are properties too; this rule does not read them
	}

	[[nodiscard]] bool Creates() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& slot) const override
	{
		const auto* head = effect.FindCreator(creator);
		if (head == nullptr)
		{
			return false;
		}
		// the slot keeps whether it was emitting last time; a start of emission begins a new chain
		const bool emitting = inTestMode || effect.GetProcessInfo().enabled;
		const bool was = slot.state.x != 0.0f;
		slot.state.x = emitting ? 1.0f : 0.0f;
		if (emitting != was && !was)
		{
			for (const auto& atom : collection.atoms)
			{
				_data.erase(atom.get());
			}
			collection.atoms.clear();
			effect.NewAtom(collection, head, nextGroups);
		}
		for (auto it = collection.atoms.begin(); it != collection.atoms.end();)
		{
			auto& atom = **it;
			auto& data = _data[&atom];
			if (!data.emitting && effect.AtomAge(atom) - data.stopTime > 5.0f) // (inferred) the offset of 5 s
			{
				_data.erase(&atom);
				it = collection.atoms.erase(it);
				continue;
			}
			if (data.emitting)
			{
				if (!emitting)
				{
					data.emitting = false;
					data.stopTime = effect.AtomAge(atom);
				}
				else
				{
					if (atom.subCollections.empty())
					{
						return false;
					}
					Emit(effect, data, *atom.subCollections.front(), atom.position);
				}
			}
			++it;
		}
		return true;
	}

private:
	/// What the rule keeps for one head atom
	struct AtomData
	{
		glm::vec3 previousHead {0.0f}; ///< the head's position last step
		glm::vec3 lastEmitted {0.0f};  ///< where the last joint came out
		float wanted {0.0f};           ///< joints owed so far
		float emitted {0.0f};          ///< joints emitted
		float stopTime {0.0f};         ///< the head's age when the emission stopped
		bool emitting {true};
		bool first {true};
	};

	/// The emission: the chain's joints come out of the head as it moves
	void Emit(Effect& effect, AtomData& data, Collection& chain, const glm::vec3& head) const
	{
		const size_t count = chain.atoms.size();
		if (count == 0)
		{
			return;
		}
		if (data.first)
		{
			data.lastEmitted = head;
			data.previousHead = head;
			for (auto& joint : chain.atoms)
			{
				joint->position = head;
				joint->ruleScale = 0.0f;
			}
		}
		const float initialScale = adjustInitialScale.empty() ? 1.0f : effect.FloatProvider(adjustInitialScale, 1.0f);
		const float before = data.wanted;
		const float dt = effect.GetDt();
		const float maxStep = static_cast<float>(count) / dieAge * dt;
		float step = data.emitted == 0.0f ? 1.0f : glm::distance(data.lastEmitted, head) / minEmitDist;
		if (!(maxStep > step))
		{
			step = maxStep;
		}
		if (!data.emitting)
		{
			step = 0.0f;
		}
		data.wanted += step;
		const float after = data.wanted;
		while (data.wanted - 1.0f > data.emitted)
		{
			data.emitted += 1.0f;
			// every joint takes the state of the one before it (position, age, initial scale)
			glm::vec3 position(0.0f);
			float age = 0.0f;
			float scale = 1.0f;
			for (size_t i = 0; i < count; ++i)
			{
				auto& joint = *chain.atoms[i];
				const glm::vec3 p = joint.position;
				const float a = effect.AtomAge(joint);
				const float s = joint.baseScale;
				if (i != 0)
				{
					joint.position = position;
					joint.birth = effect.GetAge() - age; // sets the joint's age
					joint.baseScale = scale;
				}
				position = p;
				age = a;
				scale = s;
			}
			// the new joint at the head, between last step's head and this one's
			auto& first = *chain.atoms.front();
			glm::vec3 at = head;
			if (after != before)
			{
				const float fraction = (data.emitted - before) / (after - before);
				at = data.previousHead + (head - data.previousHead) * fraction;
				first.birth = effect.GetAge() - dt * fraction;
			}
			first.position = at;
			data.lastEmitted = at;
			first.baseScale = initialScale;
			first.ruleScale = 1.0f;
		}
		data.first = false;
		data.previousHead = head;
	}

	std::string creator;
	std::vector<int> nextGroups;
	float dieAge;
	float minEmitDist;
	std::string adjustInitialScale;
	bool inTestMode;
	/// the head atoms' AtomData (the original keeps it in the atom's modifier-data list)
	mutable std::unordered_map<const Atom*, AtomData> _data;
};

/// CreateRuleMakeChain (a once-only create rule): NumAtoms joints of PCreator (the chain creator numbers them), once
class MakeChain final: public Modifier
{
public:
	explicit MakeChain(const Object& object)
	    : creator(object.String("PCreator"))
	    , numAtoms(object.Int("NumAtoms", 0)) // (inferred) default: the original's was not read
	{
	}
	[[nodiscard]] bool Creates() const override { return true; }
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		const auto* joints = effect.FindCreator(creator);
		for (int i = 0; joints != nullptr && i < numAtoms; ++i)
		{
			effect.NewAtom(collection, joints, {});
		}
		return false; // once
	}
	std::string creator;
	int numAtoms;
};
} // namespace

void openblack::psys::RegisterGestureRules()
{
	RegisterModifier("UR_GesturingRecognised", MakeModifierOf<GesturingRecognised>);
	RegisterModifier("ZR_ChainGesture", MakeModifierOf<ChainGesture>);
	RegisterModifier("CreateRuleMakeChain", MakeModifierOf<MakeChain>);
}
