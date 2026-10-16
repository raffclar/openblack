$input a_position, a_texcoord0, i_data0, i_data1
$output v_world, v_texcoord0, v_color0, v_normal

#include <bgfx_shader.sh>

#include "underwater.sh"

// EXPERIMENT (not for merge): the reef's coral and plants, one instance each. The coral are crossed quads with an
// image of a coral grown by an L-system; the plants are crossed strips. Both bend with the sea's current: more at the
// top than at the foot, the plants much more than the coral, so a gust in the current passes through them in turn.
// i_data0: xyz the foot, w the size. i_data1: x the angle about the up axis, y the coral's cell or the plant's kind, z
// which colour, w 0 for coral and 1 for a plant.

void main()
{
	vec3 foot = i_data0.xyz;
	float size = i_data0.w;
	float turn = i_data1.x;
	bool plant = i_data1.w > 0.5f;
	float c = cos(turn);
	float s = sin(turn);
	vec3 local = a_position.xyz * size;
	vec3 placed = vec3(local.x * c - local.z * s, local.y, local.x * s + local.z * c);

	// The current at the foot, and how far the water there has moved, which gives each the phase of its flutter
	float t = u_uwParams.y;
	vec2 shift;
	vec2 flow = UwCurrent(foot, t, shift);
	float h = a_position.y;
	float bend = plant ? 1.4f : 0.05f;
	vec2 lean = flow * (h * h) * size * bend;
	float flutter = sin(t * 2.3f + dot(shift, vec2(1.7f, 1.3f)) + h * 4.0f) * (0.05f + 0.3f * length(flow));
	vec2 across = length(flow) > 1e-3f ? normalize(vec2(-flow.y, flow.x)) : vec2(1.0f, 0.0f);
	lean += across * flutter * h * size * (plant ? 0.25f : 0.02f);
	// A plant that leans far is lower at its top
	vec3 world = foot + placed + vec3(lean.x, -length(lean) * 0.35f * h, lean.y);

	vec2 cell = vec2(mod(i_data1.y, 3.0f), floor(i_data1.y / 3.0f));
	v_texcoord0 = vec4((cell + a_texcoord0.xy) / 3.0f, plant ? a_texcoord0.y : h, plant ? 1.0f : 0.0f);
	// The soft colours of coral, and the greens and browns of the plants
	float pick = i_data1.z;
	vec3 colour;
	if (plant)
	{
		colour = mix(vec3(0.20f, 0.42f, 0.16f), vec3(0.42f, 0.36f, 0.14f), pick);
	}
	else
	{
		colour = pick < 0.2f ? vec3(0.92f, 0.56f, 0.56f)
		       : pick < 0.4f ? vec3(0.95f, 0.64f, 0.42f)
		       : pick < 0.6f ? vec3(0.74f, 0.62f, 0.84f)
		       : pick < 0.8f ? vec3(0.93f, 0.88f, 0.72f)
		                     : vec3(0.58f, 0.78f, 0.72f);
	}
	v_color0 = vec4(colour, 1.0f);
	v_normal = vec3(-s, 0.0f, c);
	v_world = world;
	gl_Position = mul(u_viewProj, vec4(world, 1.0f));
}
