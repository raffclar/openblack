$input a_position, a_texcoord0, a_color0
$output v_texcoord0, v_color0

#include <bgfx_shader.sh>

// The curtain of an influence circle, from the static buffer of the circle:
// u_borderCurtain.xy: the scroll of the texture in this frame
// u_borderCurtain.z: the strength of the border in this frame, 0..1. The alpha of a vertex is 1 where the border shows
// and 0 where it does not.
// The values are the same as those of vs_blob with the scroll and the strength in the vertices, thus the pixels are
// the same.
uniform vec4 u_borderCurtain;

void main()
{
	v_texcoord0 = vec4(a_texcoord0 + u_borderCurtain.xy, 0.0f, 0.0f);
	v_color0 = vec4(a_color0.rgb, a_color0.a * u_borderCurtain.z);
	gl_Position = mul(u_viewProj, vec4(a_position.xyz, 1.0f));
}
