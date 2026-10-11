/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The rule that lays a glowing shell over the models of the objects its effect is given, as the magic over a worthy
// artefact: an atom kept on each object, whose shell reaches further out the stronger the effect is, in its player's
// colour, fading with its collection

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include <ParticleFile.h>

#include "ParticleClassRegistry.h"
#include "ParticleEffect.h"
#include "VolumeGlow.h"

using namespace openblack::particles;
using openblack::psys::ParticleObject;

namespace
{
class VolumeGlowOnObject final: public Modifier
{
public:
	explicit VolumeGlowOnObject(const ParticleObject& object)
	    : _creator(object.String("PCreator"))
	    , _nextGroups(object.IntArray("NextGroups"))
	{
		const volume_glow::Settings defaults;
		_settings = {
		    .texture =
		        std::clamp(object.Int("MatInd", defaults.texture), 0, static_cast<int>(volume_glow::k_Textures.size()) - 1),
		    .animSpeed = {object.Float("AnimSpeedU", defaults.animSpeed.x), object.Float("AnimSpeedV", defaults.animSpeed.y)},
		    .zoomMin = object.Float("ZoomProportionMin", defaults.zoomMin),
		    .zoomMax = object.Float("ZoomProportionMax", defaults.zoomMax),
		    .textureFraction = {object.Float("TextFracU", defaults.textureFraction.x),
		                        object.Float("TextFracV", defaults.textureFraction.y)},
		    .alpha = object.Int("Alpha", defaults.alpha),
		    .usePlayerColour = object.Bool("UsePlayerColor", defaults.usePlayerColour),
		    .alphaFromStrength = object.Bool("SetAlphaFromStrength", defaults.alphaFromStrength),
		};
	}
	[[nodiscard]] bool Creates() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		auto& world = effect.Services().world;
		const auto* creator = effect.FindCreator(_creator);
		// An atom on each object given; anything given that isn't an object in the world ends the rule's step
		while (effect.TargetCount() > 0)
		{
			const auto target = effect.TakeTarget();
			if (!target.has_value() || !world.ObjectPosition(*target).has_value() || creator == nullptr)
			{
				return true;
			}
			auto& atom = effect.NewAtom(collection, creator, _nextGroups);
			atom.data[this].object = *target;
		}

		const float power = effect.GetProcessInfo().power;
		const auto player = effect.GetPlayer();
		const auto playerRgb = player >= 0 ? std::optional<uint32_t>(world.PlayerColour(player)) : std::nullopt;
		const auto alpha = volume_glow::Alpha(_settings, power);
		const auto collectionAlpha = static_cast<uint8_t>(std::clamp(collection.alpha, 0.0f, 255.0f));
		const auto argb = volume_glow::Argb(_settings, alpha, collectionAlpha, playerRgb);
		for (size_t i = 0; i < collection.atoms.size();)
		{
			auto& atom = *collection.atoms[i];
			const auto found = atom.data.find(this);
			const auto object = found != atom.data.end() ? found->second.object : entt::null;
			// An atom whose object has gone goes with it
			if (object == entt::null || !world.ObjectPosition(object).has_value())
			{
				collection.atoms.erase(collection.atoms.begin() + static_cast<std::ptrdiff_t>(i));
				continue;
			}
			atom.objectGlow = ObjectGlow {
			    .object = object,
			    .zoom = volume_glow::Zoom(_settings, power, atom.baseScale * atom.ruleScale),
			    .argb = argb,
			    .texture = _settings.texture,
			    .animSpeed = _settings.animSpeed,
			    .textureFraction = _settings.textureFraction,
			};
			++i;
		}
		return true;
	}

private:
	std::string _creator;
	std::vector<int> _nextGroups;
	volume_glow::Settings _settings;
};
} // namespace

void openblack::particles::RegisterVolumeGlowRules(ParticleClassRegistry& registry)
{
	registry.AddModifier("UR_VolFXOnObject", ParticleClassRegistry::Make<VolumeGlowOnObject>);
}
