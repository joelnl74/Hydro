#version 450

#extension GL_GOOGLE_include_directive : require

#include "frame_lighting.glsl"

// Sun shadow casters (R14, Renderer2D's SunShadows pass): sprite quads drawn into the view's shadow map,
// seen from the sun (lighting.shadowMatrix). The quad vertex layout (vertex.vert); colour, UV and texture
// slot go on for the alpha test.

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in float inTexIndex;

layout(location = 0) out float fragAlpha;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) flat out float texIndex;

void main()
{
	gl_Position = lighting.shadowMatrix * vec4(inPosition, 1.0);
	fragAlpha = inColor.a;
	fragTexCoord = inTexCoord;
	texIndex = inTexIndex;
}
