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
// EXPERIMENT (not for merge): x: how much of the air's sky shows near the horizon, 0 with the experiment off; yzw: unused
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
	// EXPERIMENT (not for merge): near the horizon the sky is the air's own light, the same light as the haze over the far
	// land, so that the far land fades into the sky. Higher up the original dome shows.
	if (u_skyAtmos.x > 0.0f)
	{
		vec3 eye = mul(u_invView, vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
		vec3 dir = normalize(v_position.xyz - eye);
		vec3 air = AtmosSoftClip(AtmosSky(dir) * u_atmosSun.rgb);
		float share = (1.0f - smoothstep(-0.02f, 0.15f, dir.y)) * u_skyAtmos.x;
		dome = mix(dome, air, share);
	}
	gl_FragColor = vec4(dome, 1.0f);
}
