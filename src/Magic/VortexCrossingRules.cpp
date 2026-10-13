/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VortexCrossingRules.h"

#include <cmath>

#include <algorithm>
#include <type_traits>

#include <fmt/printf.h>

namespace openblack::vortex::crossing
{

namespace
{
/// The share of the way a thing has blended, 0 to 1
float Blend(float age, float time)
{
	const float share = age / time;
	if (share <= 0.0f)
	{
		return 0.0f;
	}
	return share >= 1.0f ? 1.0f : share;
}

/// The swing round the middle is this many times the setting's speed
constexpr float k_SwingScale = 30.0f;
/// Once it has blended this far, a thing that can't be destroyed may go from the middle
constexpr float k_IndestructibleGoesAt = 0.5f;

/// A place across the land in metres, as the game turns a map position into one
double Metres(int32_t fixed)
{
	return static_cast<double>(fixed) * 10.0 / 65536.0;
}
} // namespace

float PileShare(float random)
{
	constexpr float k_Least = 0.3f;
	constexpr float k_Spread = 0.4f;
	return random * k_Spread + k_Least;
}

std::optional<AttractStep> Attract(const AttractSettings& settings, const AttractAtom& atom, glm::vec3 origin, float dt,
                                   const GroundHeight& ground)
{
	const float dx = atom.position.x - origin.x;
	const float dz = atom.position.z - origin.z;
	float radius = std::sqrt(dx * dx + dz * dz);
	const float angle = std::atan2(dz, dx);
	const float blend = Blend(atom.age, settings.blendTime);
	const float velocityBlend = Blend(atom.age, settings.blendTimeVelocity);
	radius -= blend * settings.radialSpeed * dt * radius;
	if (!(settings.minRadius < radius))
	{
		radius = settings.minRadius;
	}
	const float swung = blend * settings.thetaSpeed * dt * k_SwingScale / radius + angle;
	// At the middle, once blended in, it is gone; a thing that can't be destroyed goes from half way blended
	if (radius < settings.deletionRadius && (blend >= 1.0f || (atom.indestructible && blend >= k_IndestructibleGoesAt)))
	{
		return std::nullopt;
	}
	const float invDt = 1.0f / dt;
	const double targetX = std::cos(static_cast<double>(swung)) * radius + origin.x;
	const float sinRadius = static_cast<float>(std::sin(static_cast<double>(swung))) * radius;
	const float targetY = 0.0f + origin.y;
	const float targetZ = sinRadius + origin.z;
	// The velocity that would put it there this step, blended in from the one it was taken with
	const glm::vec3 wanted {
	    static_cast<float>((targetX - atom.position.x) * invDt) - atom.takenVelocity.x,
	    (targetY - atom.position.y) * invDt - atom.takenVelocity.y,
	    (targetZ - atom.position.z) * invDt - atom.takenVelocity.z,
	};
	const glm::vec3 velocity = atom.takenVelocity + wanted * velocityBlend;
	const float x = velocity.x * dt + atom.position.x;
	const float z = velocity.z * dt + atom.position.z;
	// Its height rides from where it was taken onto the funnel over the land under it
	const float funnelSquare = settings.yOffset * settings.yOffset / settings.rZeroHeight;
	const float funnel = std::sqrt(radius * funnelSquare) + settings.yOffset;
	const float y = (funnel - atom.startHeight) * blend + ground({x, z}) + atom.startHeight;
	const glm::vec3 position {x, y, z};
	return AttractStep {.position = position, .velocity = (position - atom.position) * invDt};
}

ThrowOut ThrowOutOf(float angle, float outRandom, float upRandom, float scale, float height)
{
	constexpr float k_LeastOut = 8.0f;
	constexpr float k_LeastUp = 10.0f;
	constexpr float k_HalfHeight = 0.5f;
	const float out = (outRandom + k_LeastOut) * scale;
	const glm::vec3 velocity {std::sin(angle) * out, (upRandom + k_LeastUp) * scale, -(std::cos(angle) * out)};
	const float turn = 1.0f / (height * k_HalfHeight);
	return {.velocity = velocity, .spin = {(0.0f - velocity.z) * turn, turn * 0.0f, (velocity.x - 0.0f) * turn}};
}

std::string CommandFormat(std::string_view name, std::string_view parameters)
{
	std::string format(name);
	format += '(';
	const auto opened = format.size();
	for (const char letter : parameters)
	{
		switch (letter)
		{
		case 'A':
			format += "%s, ";
			break;
		case 'F':
			format += "%f, ";
			break;
		case 'L':
			format += "\"%s\", ";
			break;
		case 'N':
			format += "%d, ";
			break;
		default:
			break;
		}
	}
	// The last separator goes
	if (format.size() != opened)
	{
		format.resize(format.size() - 2);
	}
	format += ")\n";
	return format;
}

std::string PlaceText(const map_coords::MapCoords& offset)
{
	return fmt::sprintf("\"%0.2f,%0.2f\"", Metres(offset.x), Metres(offset.z));
}

std::string Text(const Line& line)
{
	return std::visit(
	    [](const auto& l) -> std::string {
		    using T = std::decay_t<decltype(l)>;
		    const auto place = PlaceText(l.offset);
		    if constexpr (std::is_same_v<T, VillagerLine>)
		    {
			    return fmt::sprintf(CommandFormat("CREATE_TOWN_VILLAGER", "NAAN"), l.town, place, l.villagerType, l.age);
		    }
		    else if constexpr (std::is_same_v<T, SpecialVillagerLine>)
		    {
			    return fmt::sprintf(CommandFormat("CREATE_SPECIAL_TOWN_VILLAGER", "NANN"), l.town, place, l.row, l.age);
		    }
		    else if constexpr (std::is_same_v<T, AnimalLine>)
		    {
			    return fmt::sprintf(CommandFormat("CREATE_NEW_ANIMAL", "ANNNN"), place, l.type, l.flock, l.town, l.age);
		    }
		    else if constexpr (std::is_same_v<T, PotLine>)
		    {
			    return fmt::sprintf(CommandFormat("CREATE_POT", "ANNN"), place, l.row, l.resource, l.amount);
		    }
		    else if constexpr (std::is_same_v<T, TreeLine>)
		    {
			    return fmt::sprintf(CommandFormat("CREATE_NEW_TREE", "NANNFFF"), l.forest, place, l.row, l.nonScenic ? 1 : 0,
			                        static_cast<double>(l.yAngle), static_cast<double>(l.scale),
			                        static_cast<double>(l.maxScale));
		    }
		    else if constexpr (std::is_same_v<T, MobileStaticLine>)
		    {
			    return fmt::sprintf(CommandFormat("CREATE_MOBILE_STATIC", "ANFFFFF"), place, l.row,
			                        static_cast<double>(l.offset.altitude), static_cast<double>(l.xAngle),
			                        static_cast<double>(l.yAngle), static_cast<double>(l.zAngle), static_cast<double>(l.scale));
		    }
		    else if constexpr (std::is_same_v<T, MobileObjectLine>)
		    {
			    // The angle and the size in thousandths, truncated
			    constexpr float k_Thousand = 1000.0f;
			    return fmt::sprintf(CommandFormat("CREATE_MOBILEOBJECT", "ANNN"), place, l.row,
			                        map_coords::FtoL(l.yAngle * k_Thousand), map_coords::FtoL(l.scale * k_Thousand));
		    }
		    else
		    {
			    static_assert(std::is_same_v<T, DeadTreeLine>);
			    return fmt::sprintf(CommandFormat("CREATE_DEAD_TREE", "ALNFFFF"), place, l.player, l.row,
			                        static_cast<double>(l.life), static_cast<double>(l.xAngle), static_cast<double>(l.yAngle),
			                        static_cast<double>(l.zAngle));
		    }
	    },
	    line);
}

} // namespace openblack::vortex::crossing
