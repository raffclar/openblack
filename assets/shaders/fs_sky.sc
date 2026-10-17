$input v_position, v_texcoord0, v_normal, v_color0, v_haze, v_snow, v_snowLight

#include <bgfx_shader.sh>

#include "atmosphere.sh"

// The sky's dome for each alignment, blended for the time of day, one above the other from evil to good
SAMPLER2D(s_diffuse, 0);
// x, y: the alignments' domes mixed. z: how much of the second there is, of 255
uniform vec4 u_skyAlignment;
// The colours the domes are drawn in: their pictures times the first, with the second added
uniform vec4 u_skyModulate;
uniform vec4 u_skyAdd;
// EXPERIMENT (not for merge): x: how much of the air's sky shows, 0 with the experiment off; y: the sine of the height of
// the air's sun over the horizon; zw: unused
uniform vec4 u_skyAtmos;

// A point of an alignment's dome, sampled as if its third of the texture stood alone, clamped to its edges
vec3 Dome(vec2 uv, float alignment)
{
	const float rows = 256.0f;
	float v = clamp(uv.y, 0.5f / rows, 1.0f - 0.5f / rows);
	return texture2D(s_diffuse, vec2(uv.x, (alignment + v) / 3.0f)).rgb;
}

// A dome drawn in the frame's colours
vec3 Drawn(vec3 dome)
{
	return clamp(dome * u_skyModulate.rgb + u_skyAdd.rgb, 0.0f, 1.0f);
}

// The first alignment's dome with the second drawn over it
void main()
{
	vec3 lower = Drawn(Dome(v_texcoord0.xy, u_skyAlignment.x));
	vec3 upper = Drawn(Dome(v_texcoord0.xy, u_skyAlignment.y));
	vec3 dome = mix(lower, upper, u_skyAlignment.z / 255.0f);
	// EXPERIMENT (not for merge): the whole sky is the air's own light, the same light as the haze over the far land, so
	// that the sky follows the air's sun through the day and the far land fades into it. Where the air is dark at night,
	// small stars show. The original dome shows only with the experiment off.
	if (u_skyAtmos.x > 0.0f)
	{
		vec3 eye = mul(u_invView, vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
		vec3 dir = normalize(v_position.xyz - eye);
		vec3 air = AtmosSky(dir) * u_atmosSun.rgb;
		// By day the air's sky high up is lighter than the plain model gives, as the original dome is, so the sky does
		// not go to a deep blue over the eye. The sky at dusk and at night keeps the model's own brightness.
		const vec3 luma = vec3(0.2126f, 0.7152f, 0.0722f);
		float lift = 1.0f + 0.8f * smoothstep(0.05f, 0.6f, dir.y) * smoothstep(-0.02f, 0.2f, u_atmosLight.y);
		air = AtmosSoftClip(air * lift);
		// After sunset and in the moon's light the model gives a deep, strong blue. The eye sees the dim sky with less
		// colour, so the sky goes part of the way to grey while the air's sun is low.
		float grey = dot(air, luma);
		air = mix(vec3_splat(grey), air, mix(0.25f, 1.0f, smoothstep(0.0f, 0.35f, u_skyAtmos.y)));
		float dark = 1.0f - smoothstep(0.02f, 0.12f, dot(air, luma));
		vec3 cell = floor(dir * 260.0f);
		vec3 q = fract(cell * vec3(0.1031f, 0.1030f, 0.0973f));
		q += dot(q, q.yzx + 33.33f);
		vec3 h = fract((q.xxy + q.yzz) * q.zyx);
		vec3 star = cell + 0.2f + 0.6f * h - dir * 260.0f;
		float glint = h.x > 0.985f ? smoothstep(0.06f, 0.0f, dot(star, star)) * (0.4f + 0.6f * h.y) : 0.0f;
		air += vec3(0.85f, 0.9f, 1.0f) * glint * dark * smoothstep(0.02f, 0.2f, dir.y);
		dome = mix(dome, air, u_skyAtmos.x);
	}
	gl_FragColor = vec4(dome, 1.0f);
}
