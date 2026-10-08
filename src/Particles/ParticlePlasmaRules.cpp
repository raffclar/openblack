/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The plasma beam, as a temple heart fires it at what takes a blow for it: each beam the effect is asked for becomes an
// atom of its own carrying a few ribbons, each a collection of joints coloured by the effect's player. Every step each
// ribbon is laid afresh along a curve that leaves the start and arrives at the end along the beam's tangents, nudged at
// random each step; a few key points along it are pushed aside by noise drifting along the ribbon, most at its middle,
// and the joints are spread along a smooth curve through them, thickest at the middle. The ribbons fade in and out over
// the beam's life, and the beam goes a fifth of a second after its life is over.

#include <cstddef>

#include <algorithm>
#include <memory>
#include <ranges>
#include <string>
#include <vector>

#include <ParticleFile.h>

#include "BeamMaths.h"
#include "ParticleClassRegistry.h"
#include "ParticleDrawFrame.h"
#include "PlasmaCommand.h"

using namespace openblack::particles;
using openblack::psys::ParticleObject;

namespace
{
/// A beam stays this long after its life is over before it goes
constexpr float k_PlasmaLinger = 0.2f;

class Plasma final: public Modifier
{
public:
	explicit Plasma(const ParticleObject& object)
	    : creator(object.String("PCreator"))
	    , chainCreator(object.String("ChainCreator"))
	    , scaleTangents(object.Float("ScaleTangents", 1.0f))
	    , randomTangents(object.Float("RandomTangents", 0.2f))
	    , joints(std::max(0, object.Int("JointsPerArc", 19)))
	    , keyPoints(object.Int("NumSplinePoints", 5))
	    , beams(std::max(0, object.Int("NumBeams", 3)))
	    , beamGroup(object.Int("BeamGroup", -1))
	    , maxAlpha(object.Int("MaxAlpha", 255))
	    , wiggleFrequency(object.Float("WiggleFreq", 4.0f))
	    , wiggleSpeed(object.Float("WiggleSpeed", 1.0f))
	    , speedV(object.Float("SpeedV", 1.0f))
	    , randomFrac(object.Float("RandomFrac", 0.1f))
	    , forkScaleMin(object.Float("ForkScaleMin", 1.0f))
	    , forkScaleMax(object.Float("ForkScaleMax", 1.0f))
	{
	}

	/// Waits for beams for as long as the effect lasts
	[[nodiscard]] bool KeepsAlive() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		// Without a creator for its atoms it does nothing at all
		const auto* pointCreator = effect.FindCreator(creator);
		if (pointCreator == nullptr)
		{
			return false;
		}
		while (const auto command = effect.TakePlasma())
		{
			Begin(effect, collection, *pointCreator, *command);
		}
		for (size_t i = 0; i < collection.atoms.size();)
		{
			auto& atom = *collection.atoms[i];
			const auto found = atom.data.find(this);
			if (found == atom.data.end() || found->second.held == nullptr)
			{
				++i;
				continue;
			}
			const auto& command = *std::static_pointer_cast<const PlasmaCommand>(found->second.held);
			if (command.life + k_PlasmaLinger < effect.AtomAge(atom))
			{
				collection.atoms.erase(collection.atoms.begin() + static_cast<std::ptrdiff_t>(i));
				continue;
			}
			Lay(effect, atom, command);
			++i;
		}
		return true;
	}

private:
	/// A new atom for the beam, carrying its ribbons and their joints in the effect's player's colour, white for none or
	/// the neutral player
	void Begin(Effect& effect, Collection& collection, const Creator& pointCreator, const PlasmaCommand& command) const
	{
		const std::vector<int> groups(static_cast<size_t>(beams), beamGroup);
		auto& atom = effect.NewAtom(collection, &pointCreator, groups);
		atom.data[this].held = std::make_shared<const PlasmaCommand>(command);
		const auto* jointCreator = effect.FindCreator(chainCreator);
		if (jointCreator == nullptr || jointCreator->kind != Creator::Kind::Chain)
		{
			return;
		}
		const int player = effect.GetPlayer();
		uint32_t rgb = 0xFFFFFFu;
		if (player >= 0 && player != draw::k_NeutralPlayer)
		{
			rgb = effect.Services().world.PlayerColour(player) & 0xFFFFFFu;
		}
		// The newest ribbon first, as the game keeps its lists
		for (auto& ribbon : std::ranges::reverse_view(atom.subCollections))
		{
			ribbon->textureSpeed = command.speed * speedV;
			for (int j = 0; j < joints; ++j)
			{
				auto& joint = effect.NewAtom(*ribbon, jointCreator, {});
				joint.rgba[0] = static_cast<uint8_t>((rgb >> 16u) & 0xFFu);
				joint.rgba[1] = static_cast<uint8_t>((rgb >> 8u) & 0xFFu);
				joint.rgba[2] = static_cast<uint8_t>(rgb & 0xFFu);
			}
		}
	}

	/// Each ribbon laid along the beam's curve, numbered from the newest so that no two wiggle alike, its joints from the
	/// newest at the start to the first made at the end
	void Lay(Effect& effect, Atom& atom, const PlasmaCommand& command) const
	{
		const auto& services = effect.Services();
		const auto noise = [&services](float x) { return services.noise.Smooth(x); };
		const auto count = atom.subCollections.size();
		for (size_t s = 0; s < count; ++s)
		{
			auto& ribbon = *atom.subCollections[count - 1 - s];
			const float age = effect.CollectionAge(ribbon);
			ribbon.alpha = static_cast<float>(maths::PlasmaAlpha(age, command.life, command.alpha, maxAlpha));
			// Both nudges are drawn every step, nudging or not, the start's first
			const auto startNudge = effect.RandomInBall();
			const auto endNudge = effect.RandomInBall();
			const auto [startTangent, endTangent] =
			    maths::PlasmaTangents(command.start, command.end, command.startTangent, command.endTangent, startNudge,
			                          endNudge, randomTangents, scaleTangents);
			const auto curve = maths::PlasmaCurve(command.start, command.end, startTangent, endTangent);
			const float drift = age * command.speed * wiggleSpeed;
			const auto keys =
			    maths::PlasmaKeyPoints(curve, keyPoints, wiggleFrequency, randomFrac, drift, static_cast<int>(s), noise);
			const auto laid = maths::BeamJoints(keys, ribbon.atoms.size(), forkScaleMin, forkScaleMax);
			const auto n = std::min(laid.size(), ribbon.atoms.size());
			for (size_t j = 0; j < n; ++j)
			{
				auto& joint = *ribbon.atoms[ribbon.atoms.size() - 1 - j];
				joint.position = laid[j].position;
				joint.ruleScale = laid[j].scale;
			}
		}
	}

	std::string creator;
	std::string chainCreator;
	float scaleTangents;
	float randomTangents;
	int joints;
	int keyPoints;
	int beams;
	int beamGroup;
	int maxAlpha;
	float wiggleFrequency;
	float wiggleSpeed;
	/// How fast the ribbons' texture slides, times the beam's speed
	float speedV;
	/// How far the key points are pushed aside
	float randomFrac;
	float forkScaleMin;
	float forkScaleMax;
};
} // namespace

void openblack::particles::RegisterPlasmaRules(ParticleClassRegistry& registry)
{
	registry.AddModifier("UR_Plasma", ParticleClassRegistry::Make<Plasma>);
}
