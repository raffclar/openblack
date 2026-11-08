$input v_texcoord0, v_color0

#include <bgfx_shader.sh>

// A full-screen Bink video, from its three decoded planes as the game's video library and renderer show it:
// - each pixel's colour from its luma and its 2x2 block's chroma through the library's integer tables;
// - optionally through the game's 16-bit surface, 555 or 565 (see Graphics/FilmSurface.h);
// - drawn in the game's tiles of 256x256 texels. The texture coordinates are texels of the picture, from half a texel
//   in to half a texel before the end of each tile. The bilinear filter stays in the tile, as the game clamped each
//   tile texture, thus no tile mixes with the next.

SAMPLER2D(s_lumaPlane, 0);
SAMPLER2D(s_uPlane, 1);
SAMPLER2D(s_vPlane, 2);
SAMPLER2D(s_colourTables, 3);

// x, y: the picture's size in pixels; z, w: the luma plane texture's size
uniform vec4 u_videoSize;
// x, y: the chroma plane textures' size
uniform vec4 u_videoChroma;
// x: 0 for full colour, 1 for the 555 surface, 2 for the 565 surface; y: 1 when there is a picture, 0 for black
uniform vec4 u_videoOptions;

float PlaneValue(sampler2D plane, vec2 texel, vec2 size)
{
	return floor(texture2D(plane, (texel + vec2_splat(0.5)) / size).r * 255.0 + 0.5);
}

// Row 0 by Y or V: luma, red from V, green from V. Row 1 by U: green from U, blue from U
vec4 Table(float value, float row)
{
	return texture2DLod(s_colourTables, vec2((value + 0.5) / 256.0, (row + 0.5) / 2.0), 0.0);
}

vec3 TexelColour(vec2 texel)
{
	float y = PlaneValue(s_lumaPlane, texel, u_videoSize.zw);
	vec2 chroma = floor(texel * 0.5);
	float u = PlaneValue(s_uPlane, chroma, u_videoChroma.xy);
	float v = PlaneValue(s_vPlane, chroma, u_videoChroma.xy);
	vec4 byY = Table(y, 0.0);
	vec4 byV = Table(v, 0.0);
	vec4 byU = Table(u, 1.0);
	vec3 colour = clamp(vec3(byY.x + byV.y, byY.x + byU.x + byV.z, byY.x + byU.y), 0.0, 255.0);
	if (u_videoOptions.x > 0.5)
	{
		// Keep the top bits of each channel. The display card reads a level n of m bits as n / (2^m - 1)
		vec3 step = u_videoOptions.x > 1.5 ? vec3(8.0, 4.0, 8.0) : vec3_splat(8.0);
		vec3 levels = 256.0 / step - vec3_splat(1.0);
		return floor(colour / step) / levels;
	}
	return colour / 255.0;
}

void main()
{
	if (u_videoOptions.y < 0.5)
	{
		gl_FragColor = vec4(0.0, 0.0, 0.0, v_color0.a);
		return;
	}
	// The tile of the point, and its first and last texels
	vec2 lowest = floor(v_texcoord0.xy / 256.0) * 256.0;
	vec2 highest = min(lowest + vec2_splat(255.0), u_videoSize.xy - vec2_splat(1.0));
	vec2 base = v_texcoord0.xy - vec2_splat(0.5);
	vec2 low = floor(base);
	vec2 f = base - low;
	vec2 t0 = clamp(low, lowest, highest);
	vec2 t1 = clamp(low + vec2_splat(1.0), lowest, highest);
	vec3 c00 = TexelColour(vec2(t0.x, t0.y));
	vec3 c10 = TexelColour(vec2(t1.x, t0.y));
	vec3 c01 = TexelColour(vec2(t0.x, t1.y));
	vec3 c11 = TexelColour(vec2(t1.x, t1.y));
	vec3 colour = mix(mix(c00, c10, f.x), mix(c01, c11, f.x), f.y);
	gl_FragColor = vec4(colour * v_color0.rgb, v_color0.a);
}
