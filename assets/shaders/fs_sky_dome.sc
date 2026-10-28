$input v_texcoord0

#include <bgfx_shader.sh>

// The pictures of the sky, a layer for each alignment and, in each, for each time of day
SAMPLER2DARRAY(s_diffuse, 0);
// x: the alignment's first layer. y, z: the times of day blended. w: how much of the second, of 255, or -1 to copy
// every second texel of the picture of time y into a dome of half the size
uniform vec4 u_skyDome;

// A texel of a picture as its 5-bit channels
vec3 Channels(vec2 uv, float time)
{
	return floor(texture2DArrayLod(s_diffuse, vec3(uv, u_skyDome.x + time), 0.0f).rgb * 31.0f + 0.5f);
}

// A row of the sky's dome for an alignment, blended for a time of day in whole steps of its channels. The two weights
// only add up to 255 of 256, so even a whole picture comes out a step darker at its brightest.
void main()
{
	// The low resolution textures of the lowest detail levels: the dome takes every second texel of one picture as it is
	if (u_skyDome.w < 0.0f)
	{
		// The pictures are 256 texels square
		vec2 size = vec2_splat(256.0f);
		vec2 texel = floor(v_texcoord0.xy * size * 0.5f) * 2.0f;
		gl_FragColor = vec4(Channels((texel + 0.5f) / size, u_skyDome.y) / 31.0f, 1.0f);
		return;
	}
	vec3 lower = Channels(v_texcoord0.xy, u_skyDome.y);
	vec3 upper = Channels(v_texcoord0.xy, u_skyDome.z);
	vec3 blended = floor(lower * (255.0f - u_skyDome.w) / 256.0f) + floor(upper * u_skyDome.w / 256.0f);
	gl_FragColor = vec4(blended / 31.0f, 1.0f);
}
