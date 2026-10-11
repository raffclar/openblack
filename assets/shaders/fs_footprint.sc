$input v_texcoord0

#include <bgfx_shader.sh>

// A footprint's texel painted over the land, as the game paints it: the footprint texel nearest the coordinates (kept
// inside the footprint), unfiltered, covering the land by its 4-bit alpha times 16 out of 255, so that even the most
// opaque leaves a little land showing. The footprints are laid over one another premultiplied, so that the land can
// take them all at once.

SAMPLER2D(s_footprint, 0);
// xy: the footprint texture's size in texels
uniform vec4 u_footprintSize;

void main()
{
	vec2 size = u_footprintSize.xy;
	vec2 texel = clamp(floor(v_texcoord0.xy * size + 0.5f), vec2_splat(0.0f), size - 1.0f);
	vec4 colour = texture2DLod(s_footprint, (texel + 0.5f) / size, 0.0f);
	float coverage = floor(colour.a * 15.0f + 0.5f) * 16.0f / 255.0f;
	vec3 rgb = floor(colour.rgb * 15.0f + 0.5f) / 15.0f;
	gl_FragColor = vec4(rgb * coverage, coverage);
}
