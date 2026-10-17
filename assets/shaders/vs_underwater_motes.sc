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
	float kind = fract(seed.x * 13.7f + seed.z * 7.1f);
	// Each mote drifts with the sea's current, and the sediment sinks slowly
	vec3 home = u_uwEye.xyz + (fract(seed - u_uwEye.xyz / MOTE_BOX) - 0.5f) * MOTE_BOX;
	vec2 shift;
	vec2 flow = UwCurrent(home, t, shift);
	vec3 drift = vec3(u_uwCurrent.x * t + shift.x, (-0.05f - 0.06f * kind) * t, u_uwCurrent.y * t + shift.y);
	drift += vec3(sin(t * 0.7f + seed.y * 40.0f), sin(t * 0.5f + seed.x * 31.0f) * 0.6f, cos(t * 0.6f + seed.z * 27.0f)) * 0.1f;
	vec3 inBox = fract(seed + (drift - u_uwEye.xyz) / MOTE_BOX) - 0.5f;
	vec3 world = u_uwEye.xyz + inBox * MOTE_BOX;

	float range = length(world - u_uwEye.xyz);
	// The debris pass draws the few bits of weed and leaf among the motes, larger, lit and not glowing, and turned
	// along the current that carries them
	bool debrisPass = u_uwFlags2.z > 0.5f;
	bool debris = kind > 0.9f;
	float size = mix(0.02f, 0.05f, fract(seed.y * 91.3f)) * (kind > 0.85f ? 1.8f : 1.0f);
	// Faces the camera
	vec3 right = mul(u_invView, vec4(1.0f, 0.0f, 0.0f, 0.0f)).xyz;
	vec3 up = mul(u_invView, vec4(0.0f, 1.0f, 0.0f, 0.0f)).xyz;
	vec3 corner = world + (right * a_texcoord0.x + up * a_texcoord0.y) * size;
	if (debrisPass)
	{
		// A tenth of the size they had: 1 to 2.5 cm, as small bits of weed and leaf are
		size = mix(0.008f, 0.022f, fract(seed.y * 53.7f));
		vec3 along = length(flow) > 1e-3f ? normalize(vec3(flow.x, 0.15f * sin(t + seed.z * 9.0f), flow.y)) : right;
		vec3 across = normalize(cross(along, mul(u_invView, vec4(0.0f, 0.0f, 1.0f, 0.0f)).xyz));
		corner = world + (along * a_texcoord0.x * 1.8f + across * a_texcoord0.y * 0.6f) * size;
	}

	// The light the mote catches: brighter in the shafts of light, and less deep down
	float depth = u_uwParams.z - world.y;
	vec3 towards = u_uwLight.xyz;
	vec2 surface = world.xz + towards.xz / towards.y * max(depth, 0.0f);
	float shaft = UwSurfaceLight(surface, t);
	vec3 light = u_uwLightColour.rgb * UwTransmittance(max(depth, 0.0f) / towards.y) * (0.25f + 1.6f * shaft);
	vec3 colour = light + u_uwScatter.rgb * 1.6f;
	// Plankton is a little green, the sediment a little brown
	colour *= kind > 0.6f ? vec3(0.85f, 1.0f, 0.8f) : vec3(1.0f, 0.95f, 0.85f);
	if (debrisPass)
	{
		// Bits of weed and leaf, lit by the light and the water's own
		vec3 leaf = mix(vec3(0.22f, 0.30f, 0.10f), vec3(0.40f, 0.30f, 0.16f), fract(seed.z * 17.0f));
		colour = leaf * (light * 0.6f + u_uwScatter.rgb * 1.5f);
	}
	// They fade far away and very near, and do not show above the surface
	float alpha = (1.0f - smoothstep(6.0f, MOTE_BOX * 0.5f, range)) * smoothstep(1.5f, 3.5f, range);
	alpha *= depth > 0.05f ? 1.0f : 0.0f;
	alpha *= 0.35f + 0.65f * fract(seed.z * 57.0f);
	alpha *= fract(seed.x * 71.3f + seed.y * 3.1f) < 0.55f + 0.45f * u_uwWaves.y ? 1.0f : 0.0f;
	alpha *= debrisPass == debris ? 1.0f : 0.0f;

	v_texcoord0 = vec4(a_texcoord0.xy, 0.0f, alpha);
	v_color0 = vec4(colour * UwTransmittance(range), debrisPass ? 1.0f : 0.0f);
	gl_Position = alpha > 0.0f ? mul(u_viewProj, vec4(corner, 1.0f)) : vec4(0.0f, 0.0f, 2.0f, 1.0f);
}
