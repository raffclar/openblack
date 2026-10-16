$input a_position, i_data0
$output v_texcoord0, v_texcoord1, v_color0, v_world

#include <bgfx_shader.sh>

#include "underwater.sh"

// EXPERIMENT (not for merge): the air that a wave drives under the water as it hits the foot of a cliff. Each instance
// is a point at the foot of a cliff (i_data0: xy where it is across the ground, zw the direction out to sea) with 96
// particles: 0 to 79 are bubbles that rise to the surface, the larger ones faster and with more wobble, and pop there;
// 80 to 95 are the white cloud of fine air that fills the water at the foot for a short time after the crash. From
// above the water only the cloud shows, weaker the deeper it is; from under the water all of it shows, in the
// water's fog.

SAMPLER2D(s_shore, 2);

float BubbleHash(float n)
{
	return fract(sin(n * 12.9898f) * 43758.5453f);
}

void main()
{
	vec2 foot = i_data0.xy;
	vec2 outward = i_data0.zw;
	vec2 corner = a_position.xy;
	float index = a_position.z;
	float t = u_uwParams.y;
	float level = u_uwParams.z;
	bool under = u_uwEye.w > 0.5f;
	vec4 hidden = vec4(0.0f, 0.0f, 2.0f, 1.0f);
	bool cloud = index >= 80.0f;

	vec3 eye = u_uwEye.xyz;
	vec3 forward = normalize(mul(u_invView, vec4(0.0f, 0.0f, 1.0f, 0.0f)).xyz);
	vec3 toFoot = vec3(foot.x, level, foot.y) - eye;
	float range = length(toFoot);
	bool culled = dot(toFoot, forward) < -30.0f || range > (under ? 160.0f : 600.0f) || (!under && !cloud);

	vec4 shore = texture2DLod(s_shore, UwShoreUv(foot), 0.0f);
	float cliff = UwShoreCliff(shore);
	float since;
	float crash;
	float period;
	float strength = UwCliffCrash(foot, t, since, crash, period);
	float tide = clamp((level + UW_LOWEST_TIDE) / UW_LOWEST_TIDE, 0.0f, 1.0f);
	float power = u_uwWaves.w * (0.7f + 0.3f * tide) * cliff * (strength / 0.55f);

	float seed = index * 5.71f + crash * 3.7f + dot(foot, vec2(0.0173f, 0.0291f));
	float h1 = BubbleHash(seed + 1.0f);
	float h2 = BubbleHash(seed + 2.0f);
	float h3 = BubbleHash(seed + 3.0f);
	float h4 = BubbleHash(seed + 4.0f);
	vec2 side = vec2(-outward.y, outward.x);

	vec3 world;
	float size;
	float alpha;
	if (!cloud)
	{
		// A bubble starts where the wave drove the air down, deeper for a stronger wave, and rises at the speed its
		// size gives it, on a wobbling path, carried by the current
		float radius = 0.03f + 0.22f * h3 * h3 * h3;
		float rise = min(0.25f + 4.0f * radius, 1.2f);
		float startDepth = (0.4f + 3.5f * h2) * (0.4f + power);
		float age = since - h1 * 0.8f;
		vec3 start = vec3(foot.x, level - startDepth, foot.y) + vec3(side.x, 0.0f, side.y) * (h4 - 0.5f) * 16.0f +
		             vec3(outward.x, 0.0f, outward.y) * (0.5f + 6.0f * h1);
		vec2 shift;
		vec2 flow = UwCurrent(start, t, shift);
		float wobble = radius * 6.0f;
		world = start + vec3(flow.x, rise, flow.y) * max(age, 0.0f) +
		        vec3(sin(age * 7.0f + seed), 0.0f, cos(age * 6.0f + seed)) * wobble;
		size = radius;
		float surfaced = world.y > level - radius ? 0.0f : 1.0f;
		alpha = age > 0.0f ? surfaced * smoothstep(0.0f, 0.2f, age) : 0.0f;
		alpha *= power > 0.03f ? 0.85f : 0.0f;
	}
	else
	{
		// The cloud of fine air: soft, near the surface, gone in a few seconds
		float depth = 0.3f + 1.6f * h2;
		world = vec3(foot.x, level - depth, foot.y) + vec3(side.x, 0.0f, side.y) * (h4 - 0.5f) * 14.0f +
		        vec3(outward.x, 0.0f, outward.y) * (1.0f + 5.0f * h1) +
		        vec3(outward.x, 0.0f, outward.y) * since * 0.6f;
		size = (2.0f + 3.0f * h3) * (0.6f + 0.6f * min(since, 2.0f));
		float life = 2.0f + 2.0f * power;
		alpha = since < life ? (1.0f - since / life) * smoothstep(0.0f, 0.15f, since) : 0.0f;
		alpha *= 0.45f * smoothstep(0.03f, 0.4f, power);
		// From above, the water in front of it hides it more the deeper it is
		alpha *= under ? 1.0f : 0.6f * exp(-depth * 0.8f);
	}
	alpha = culled ? 0.0f : alpha;

	vec3 right = mul(u_invView, vec4(1.0f, 0.0f, 0.0f, 0.0f)).xyz;
	vec3 up = mul(u_invView, vec4(0.0f, 1.0f, 0.0f, 0.0f)).xyz;
	vec3 placed = world + (right * corner.x + up * corner.y) * size;

	// The light the air takes: the light under the water, and the water's fog between it and the eye
	vec3 light = u_uwLightColour.rgb * 0.8f + u_uwScatter.rgb * 0.6f;
	float fog = under ? 1.0f : 0.0f;
	v_texcoord0 = vec4(corner, cloud ? 1.0f : 0.0f, alpha);
	v_texcoord1 = vec4(h1 * 5.0f, h2 * 5.0f, since * 0.2f + h3 * 5.0f, fog);
	v_color0 = vec4(light, length(placed - eye));
	v_world = placed;
	gl_Position = alpha > 0.002f ? mul(u_viewProj, vec4(placed, 1.0f)) : hidden;
}
