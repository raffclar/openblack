$input v_texcoord0, v_shoreFade

#include <bgfx_shader.sh>

// the shadow's texture: alpha n / 15, CLAMP; register 11, past the terrain's 0..10 (RendererShadows.cpp)
SAMPLER2D(s_shadow, 11);

// One projected shadow over a land block, render mode 6 (the shadow's own material): texture (black) x diffuse, blended
// SRCALPHA / INVSRCALPHA over the block. The original drops the triangles whose three vertices have the code 0x400
// when it clips; here the fragments of the triangles with that code at the three vertices (z = 0 all over them)
// (aproximado: a hairline of a kept triangle's edge between two coded vertices goes too)
void main()
{
	if (v_texcoord0.z <= 0.0f)
	{
		discard;
	}
	gl_FragColor = vec4(0.0f, 0.0f, 0.0f, texture2D(s_shadow, v_texcoord0.xy).r * v_shoreFade);
}
