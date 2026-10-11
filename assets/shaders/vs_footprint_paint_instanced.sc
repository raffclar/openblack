$input a_position, a_texcoord0, i_data0, i_data1, i_data2, i_data3, i_data4
$output v_texcoord0

// Only the model matrix is used: a draw uploads every bone the shader declares
#define BGFX_CONFIG_MAX_BONES 1
#include <bgfx_shader.sh>

// x: land texels a unit of the world
uniform vec4 u_footprintPaint;

// A footprint painted into the land's texels, as the game paints it: each corner moves to the nearest corner of a land
// texel and the footprint's coordinates spread over the moved triangle. Each land texel takes the coordinates at its
// corner, so the triangle is drawn half a texel on, where the texel's centre is.
void main()
{
	mat4 model;
	model[0] = i_data0;
	model[1] = i_data1;
	model[2] = i_data2;
	model[3] = i_data3;

	vec4 world = instMul(model, mul(u_model[0], vec4(a_position.x, 0.0f, a_position.y, 1.0f)));
	float texelsPerUnit = u_footprintPaint.x;
	vec2 corner = floor(world.xz * texelsPerUnit + 0.5f);
	vec2 painted = (corner + 0.5f) / texelsPerUnit;
	v_texcoord0 = vec4(a_texcoord0, 0.0f, 0.0f);
	gl_Position = mul(u_viewProj, vec4(painted.x, world.y, painted.y, 1.0f));
	gl_Position.z = 0.0f;
}
