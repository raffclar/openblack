$input v_position, v_texcoord0, v_normal, v_color0, v_haze, v_snow, v_snowLight

#include <bgfx_shader.sh>

// The sky's dome for each alignment from the larger pictures of the debug menu's sky pack, one above the other
SAMPLER2D(s_diffuse, 0);
// A square of blue noise that tiles, with a value from 0 to 255 in each texel
SAMPLER2D(s_dither, 1);
// x, y: the alignments' domes mixed. z: how much of the second there is, of 255. w: the rows of each dome
uniform vec4 u_skyAlignment;
// The colours the domes are drawn in: their pictures times the first, with the second added
uniform vec4 u_skyModulate;
uniform vec4 u_skyAdd;
// x: the side of the square of noise. y: the size of the dither, in steps of 8 bits (0 for no dither)
uniform vec4 u_skyDither;

// A point of an alignment's dome, sampled as if its third of the texture stood alone, clamped to its edges
vec3 Dome(vec2 uv, float alignment)
{
	float rows = u_skyAlignment.w;
	float v = clamp(uv.y, 0.5f / rows, 1.0f - 0.5f / rows);
	return texture2D(s_diffuse, vec2(uv.x, (alignment + v) / 3.0f)).rgb;
}

// A dome drawn in the frame's colours
vec3 Drawn(vec3 dome)
{
	return clamp(dome * u_skyModulate.rgb + u_skyAdd.rgb, 0.0f, 1.0f);
}

// The first alignment's dome with the second drawn over it. The dome keeps more than 8 bits for each channel. Thus a
// small value of blue noise goes on the colour before the screen cuts it to 8 bits, and the gradients show no bands.
void main()
{
	vec3 lower = Drawn(Dome(v_texcoord0.xy, u_skyAlignment.x));
	vec3 upper = Drawn(Dome(v_texcoord0.xy, u_skyAlignment.y));
	vec3 colour = mix(lower, upper, u_skyAlignment.z / 255.0f);
	// The noise is from -0.5 to 0.5 of a step, centred in each of its 256 values
	float noise = (texture2DLod(s_dither, gl_FragCoord.xy / u_skyDither.x, 0.0f).r * 255.0f + 0.5f) / 256.0f - 0.5f;
	gl_FragColor = vec4(colour + vec3_splat(noise * u_skyDither.y / 255.0f), 1.0f);
}
