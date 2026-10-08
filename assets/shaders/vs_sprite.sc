$input a_position
$output v_texcoord0

#include <bgfx_shader.sh>

uniform vec4 u_sampleRect;

// The sprite draw's mode A (flag 0x40 clear), the billboard::Screen mode of src/3D/Billboard.h, on the GPU: the
// plane -1..1 (Primitive::CreatePlane) is scaled and turned by u_model (billboard::ScreenSpriteModel: T(pos)
// Rz(-angle) S(half width, half height, 1)), taken into the world by the camera's rotation (u_invView) and moved to
// the translation. So a corner is pos + R (c x + s y) + U (-s x + c y), local x going to (cos, -sin) on the screen
// as in the original, with v = 0 at the top. The sprite's origin offset is 0 for every
// user of this shader (components::Sprite and the chimney smoke have none); the near test is made on the
// CPU before the draw.
void main()
{
	// Plane position to UV
	v_texcoord0.xy = vec2(a_position.x * 0.5f + 0.5f, 0.5f - a_position.y * 0.5f);
	// Zoom on section of sprite to render
	v_texcoord0.xy = v_texcoord0.xy * u_sampleRect.xy + u_sampleRect.zw;

	vec3 translation = mul(u_model[0], vec4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
	vec4 position = a_position;
	// Apply scaling
	position.xyz = mul(u_model[0], vec4(position.xyz, 0.0)).xyz;
	// Undo camera rotation so sprite faces camera
	position.xyz = mul(u_invView, vec4(position.xyz, 0.0)).xyz;
	// Apply translation
	position.xyz += translation;
	gl_Position = mul(u_viewProj, position);
}
