$input a_position, i_data0, i_data1
$output v_world, v_texcoord0, v_color0, v_normal

#include <bgfx_shader.sh>

#include "underwater.sh"

// EXPERIMENT (not for merge): the jellyfish in the deep water, one instance each (see underwater::jellyfish). The bell
// beats: a quick squeeze that makes it narrower and taller and lifts it, then a slow release. The tentacles hang from
// the bell, sway with the water and turn to face the eye. The halo is a square round the middle of the bell that turns
// to face the eye; the fragment shader finds the light in the ball of water inside it.
// a_position: x along (round the shell, or down the tentacle, or -1 to 1 across the halo), y across (down the shell, or
// the side of the tentacle, or -1 to 1 up the halo), z the part (0 outer shell, 1 inner shell, 2 inner tentacle,
// 3 outer tentacle, 4 halo), w which tentacle.
// i_data0: xyz the top of the bell, w its size. i_data1: x the phase of the beat, y the colour, z how much it shows,
// w its seed. u_uwEye: xyz the eye, w the pass (0 the bodies in the scene, 1 their own light after the water).

#define JELLY_TAU 6.2831853f
// The radius of the ball of water that the light of a jellyfish makes bright (underwater::jellyfish::k_LightRadius)
#define JELLY_LIGHT_RADIUS (0.5f * UW_UNITS_PER_METRE)

void main()
{
	float t = u_uwParams.y;
	vec3 top = i_data0.xyz;
	float size = i_data0.w;
	float seed = i_data1.w;
	float part = a_position.z;

	// The beat: a quick squeeze, then a slow release
	float cycle = fract(t * (0.45f + 0.25f * seed) + i_data1.x);
	float squeeze = smoothstep(0.0f, 0.16f, cycle) * (1.0f - smoothstep(0.16f, 0.9f, cycle));
	top.y += 0.12f * size * squeeze;
	float radius = 0.5f * size * (1.0f - 0.18f * squeeze);
	float height = 0.75f * (1.0f + 0.2f * squeeze);
	// The bell's edge, a little past its side
	const float k_Edge = 1.75f;

	vec3 world;
	vec3 normal;
	if (part > 3.5f)
	{
		// The halo: a square a little larger than the ball of light, round the middle of the bell. The normal holds the
		// middle of the ball.
		vec3 middle = top - vec3(0.0f, 0.4f * radius * height, 0.0f);
		vec3 towardsEye = u_uwEye.xyz - middle;
		vec3 side = cross(vec3(0.0f, 1.0f, 0.0f), towardsEye);
		side = length(side) > 1e-4f ? normalize(side) : vec3(1.0f, 0.0f, 0.0f);
		vec3 up = normalize(cross(towardsEye, side));
		world = middle + (side * a_position.x + up * a_position.y) * (1.05f * JELLY_LIGHT_RADIUS);
		normal = middle;
	}
	else if (part < 1.5f)
	{
		// A shell: a dome with a flatter top, the inner shell smaller, the edge with a soft frill
		float angle = a_position.x * JELLY_TAU;
		float down = a_position.y;
		float shellRadius = radius * (part > 0.5f ? 0.78f : 1.0f);
		float theta = down * k_Edge;
		float across = sin(theta) * shellRadius * (1.0f + 0.05f * sin(angle * 16.0f + seed * 20.0f) * down * down);
		float rise = (cos(theta) - 1.0f) * shellRadius * height;
		world = top + vec3(cos(angle) * across, rise, sin(angle) * across);
		normal = normalize(vec3(cos(angle) * sin(theta), cos(theta) / max(height, 0.1f), sin(angle) * sin(theta)));
	}
	else
	{
		// A tentacle, from the bell's edge (the outer ones) or from under its middle (the inner ones)
		bool inner = part < 2.5f;
		float which = a_position.w;
		float along = a_position.x;
		float count = inner ? 6.0f : 13.0f;
		float angle = (which + 0.5f) / count * JELLY_TAU + seed * JELLY_TAU;
		float fromMiddle = inner ? 0.3f * radius : 0.95f * radius;
		float edgeHeight = (cos(k_Edge) - 1.0f) * radius * height * (inner ? 0.8f : 1.0f);
		float reach = size * (inner ? 1.1f : 2.6f) * (0.8f + 0.4f * fract(seed * 7.0f + which * 0.37f));
		vec3 anchor = top + vec3(cos(angle) * fromMiddle, edgeHeight, sin(angle) * fromMiddle);
		// They spread a little, and sway more the further down they are
		float loose = along * sqrt(along) * reach;
		float spread = along * reach * (inner ? 0.05f : 0.15f);
		vec2 sway = vec2(sin(t * 1.1f + along * 4.0f + which * 1.7f + seed * 9.0f),
		                 cos(t * 0.8f + along * 3.0f + which * 2.3f + seed * 5.0f)) *
		            vec2(0.22f, 0.15f) * loose;
		world = anchor + vec3(cos(angle) * spread + sway.x, -along * reach, sin(angle) * spread + sway.y);
		// A strip that turns to face the eye, thinner to its tip
		vec3 towardsEye = u_uwEye.xyz - world;
		vec3 side = cross(vec3(0.0f, 1.0f, 0.0f), towardsEye);
		side = length(side) > 1e-4f ? normalize(side) : vec3(1.0f, 0.0f, 0.0f);
		float width = size * (inner ? 0.05f : 0.012f) * (1.0f - 0.85f * along);
		world += side * width * a_position.y;
		normal = normalize(towardsEye);
	}

	v_world = world;
	v_normal = normal;
	v_texcoord0 = vec4(a_position.x, a_position.y, part, squeeze);
	v_color0 = vec4(i_data1.y, i_data1.z, cycle, seed);
	// In the pass of their own light (u_uwEye.w 1), each point moves along the line to the eye by more than the
	// jellyfish's size. It stays at the same place on the screen, and its depth is in front of the jellyfish's own
	// parts, so that only the scene in front of the jellyfish hides its light.
	vec3 drawn = world;
	if (u_uwEye.w > 0.5f && part < 3.5f)
	{
		vec3 fromEye = world - u_uwEye.xyz;
		float distance = length(fromEye);
		float pull = min(2.0f * size, 0.5f * distance);
		drawn = u_uwEye.xyz + fromEye * ((distance - pull) / max(distance, 1e-3f));
	}
	gl_Position = mul(u_viewProj, vec4(drawn, 1.0f));
}
