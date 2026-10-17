$input v_position, v_texcoord0, v_normal, v_color0, v_haze, v_snow, v_snowLight

#include <bgfx_shader.sh>

#include "atmosphere.sh"
#include "air_sky.sh"

// The sky's dome for each alignment, blended for the time of day, one above the other from evil to good
SAMPLER2D(s_diffuse, 0);
// x, y: the alignments' domes mixed. z: how much of the second there is, of 255
uniform vec4 u_skyAlignment;
// The colours the domes are drawn in: their pictures times the first, with the second added
uniform vec4 u_skyModulate;
uniform vec4 u_skyAdd;
// EXPERIMENT (not for merge): x: how much of the air's sky shows, 0 with the experiment off; y: the sine of the height of
// the air's sun over the horizon; z: 1 when the aurora's pass at half the screen's size is for this view, 0 to march the
// aurora here (as in the faces of the far cube); w: the steps of that march, under 0 for a march with no probes
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
	// that the sky follows the air's sun through the day and the far land fades into it. The stars, the sun above the
	// air and the aurora are in it too (air_sky.sh). The original dome shows only with the experiment off.
	if (u_skyAtmos.x > 0.0f)
	{
		vec3 eye = mul(u_invView, vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
		vec3 dir = normalize(v_position.xyz - eye);
		vec3 aurora = u_skyAtmos.z > 0.5f ? AirAuroraOnScreen(gl_FragCoord.xy * u_viewTexel.xy)
		                                  : AirAurora(eye, dir, 1e9f, gl_FragCoord.xy, int(abs(u_skyAtmos.w)), u_skyAtmos.w > 0.0f);
		dome = mix(dome, AirSkyColour(eye, dir, u_skyAtmos.y, gl_FragCoord.xy) + aurora, u_skyAtmos.x);
	}
	gl_FragColor = vec4(dome, 1.0f);
}
