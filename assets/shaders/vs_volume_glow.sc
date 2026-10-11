$input a_position, a_normal, a_texcoord0
$output v_texcoord0, v_color0

// Only the model matrix is used: a draw uploads every bone the shader declares
#define BGFX_CONFIG_MAX_BONES 1
#include <bgfx_shader.sh>

// x: how far the outer edge is pushed out along the normals, in the model's units. y: 1 for a model moved by bones.
// zw: how much the projection widens the camera's frame across and up
uniform vec4 u_glowShape;
// The shell's colour and alpha, 0 to 255
uniform vec4 u_glowColour;
// xy: how far the texture has slid, zw: how much of it the shell spans across and up
uniform vec4 u_glowTexture;

// A corner of a glowing shell over a model: a_texcoord0.x is 0 on the model and 1 pushed out. The inner corners glow in
// the shell's colour the more side on the camera sees them, scaled in whole bytes; the outer ones are dark and clear.
// Across, the texture follows how much the normal points up as the camera sees it.
void main()
{
	float outer = a_texcoord0.x;
	vec3 position = a_position.xyz + a_normal * (u_glowShape.x * outer);
	gl_Position = mul(u_modelViewProj, vec4(position, 1.0f));

	// The normal in the camera's frame, by the model's axes as the camera sees them, widened across and up by the
	// projection, each then its own length taken away
	vec3 widen = vec3(u_glowShape.z, u_glowShape.w, 1.0f);
	vec3 axisX = normalize(widen * mul(u_modelView, vec4(1.0f, 0.0f, 0.0f, 0.0f)).xyz);
	vec3 axisY = normalize(widen * mul(u_modelView, vec4(0.0f, 1.0f, 0.0f, 0.0f)).xyz);
	vec3 axisZ = normalize(widen * mul(u_modelView, vec4(0.0f, 0.0f, 1.0f, 0.0f)).xyz);
	vec3 normal = axisX * a_normal.x + axisY * a_normal.y + axisZ * a_normal.z;

	float sideOn = u_glowShape.y > 0.5f ? abs(normal.x) + abs(normal.y)
	                                    : (normal.x * normal.x + normal.y * normal.y) / sqrt(2.0f);
	float level = (sideOn - 0.57f) * (1.0f / (2.0f - 0.57f));
	level = level > 0.0f ? min(floor(level * 255.0f + 0.5f), 255.0f) : 0.0f;
	vec3 rgb = level >= 255.0f ? u_glowColour.rgb : floor(u_glowColour.rgb * level / 256.0f);
	vec4 inner = vec4(rgb / 255.0f, u_glowColour.a / 255.0f);
	v_color0 = inner * (1.0f - outer);

	v_texcoord0 = vec4(normal.y * u_glowTexture.z + u_glowTexture.x, u_glowTexture.y - u_glowTexture.w * outer, 0.0f,
	                   0.0f);
}
