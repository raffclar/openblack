$input v_texcoord0

// River channel footprint (data\river.l3d) into the land alpha target, blended with MIN (the block
// texture's alpha nibble becomes min(dst, src)). Only the footprint's alpha is used, nearest texel. The footprint is
// a BGRA4 texture (L3DMesh), so the texel is already one of the 16 levels of the original's ARGB4444; it is created
// with Filter::Linear, though, so the value is rounded to its level again: that absorbs any error of the hardware's
// bilinear weights at the snapped texel centre and changes nothing when the sample is exact.

#include <bgfx_shader.sh>

SAMPLER2D(s_footprint, 0);
uniform vec4 u_footprintSize; // xy: footprint texture size in texels

void main()
{
	vec2 texel = (floor(v_texcoord0.xy * u_footprintSize.xy) + 0.5f) / u_footprintSize.xy;
	float alpha = floor(texture2D(s_footprint, texel).a * 15.0f + 0.5f) / 15.0f;
	gl_FragColor = vec4(alpha, alpha, alpha, alpha);
}
