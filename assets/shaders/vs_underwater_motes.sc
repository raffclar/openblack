$input a_position, a_texcoord0
$output v_texcoord0, v_color0

#include <bgfx_shader.sh>

#include "underwater.sh"

// EXPERIMENT (not for merge): the motes in the water, plankton and grains of sediment that catch the light. Each mote
// is a small square that faces the camera. They fill a box round the eye that moves with it: a mote that leaves the box
// on one side comes back on the other, so there are always motes round the eye.

// The side of the box of motes, in units
#define MOTE_BOX 36.0f

void main()
{
	float t = u_uwParams.y;
	vec3 seed = a_position.xyz;
	// Each mote drifts slowly and sways a little, and the sediment sinks
	float kind = fract(seed.x * 13.7f + seed.z * 7.1f);
	vec3 drift = vec3(0.12f, -0.05f - 0.06f * kind, 0.08f) * t;
	drift += vec3(sin(t * 0.7f + seed.y * 40.0f), sin(t * 0.5f + seed.x * 31.0f) * 0.6f, cos(t * 0.6f + seed.z * 27.0f)) * 0.25f;
	vec3 inBox = fract(seed + (drift - u_uwEye.xyz) / MOTE_BOX) - 0.5f;
	vec3 world = u_uwEye.xyz + inBox * MOTE_BOX;

	float range = length(world - u_uwEye.xyz);
	float size = mix(0.02f, 0.05f, fract(seed.y * 91.3f)) * (kind > 0.85f ? 1.8f : 1.0f);
	// Faces the camera
	vec3 right = mul(u_invView, vec4(1.0f, 0.0f, 0.0f, 0.0f)).xyz;
	vec3 up = mul(u_invView, vec4(0.0f, 1.0f, 0.0f, 0.0f)).xyz;
	vec3 corner = world + (right * a_texcoord0.x + up * a_texcoord0.y) * size;

	// The light the mote catches: brighter in the shafts of light, and less deep down
	float depth = u_uwParams.z - world.y;
	vec3 towards = u_uwLight.xyz;
	vec2 surface = world.xz + towards.xz / towards.y * max(depth, 0.0f);
	float shaft = UwSurfaceLight(surface, t);
	vec3 light = u_uwLightColour.rgb * UwTransmittance(max(depth, 0.0f) / towards.y) * (0.25f + 1.6f * shaft);
	vec3 colour = light + u_uwScatter.rgb * 1.6f;
	// Plankton is a little green, the sediment a little brown
	colour *= kind > 0.6f ? vec3(0.85f, 1.0f, 0.8f) : vec3(1.0f, 0.95f, 0.85f);
	// They fade far away and very near, and do not show above the surface
	float alpha = (1.0f - smoothstep(6.0f, MOTE_BOX * 0.5f, range)) * smoothstep(1.5f, 3.5f, range);
	alpha *= depth > 0.05f ? 1.0f : 0.0f;
	alpha *= 0.35f + 0.65f * fract(seed.z * 57.0f);

	v_texcoord0 = vec4(a_texcoord0.xy, 0.0f, alpha);
	v_color0 = vec4(colour * UwTransmittance(range), 1.0f);
	gl_Position = alpha > 0.0f ? mul(u_viewProj, vec4(corner, 1.0f)) : vec4(0.0f, 0.0f, 2.0f, 1.0f);
}
