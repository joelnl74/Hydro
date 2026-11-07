#version 450

// Applies the light buffer to the scene. The pipeline multiplies the target by this colour twice over
// (BlendMode::Multiply2x), so the light is halved here: 1 in the buffer leaves the scene as it is,
// 2 makes it twice as bright, 0 makes it black.
//
// The descriptor set binds an array of images (see VulkanDescriptorBuilder::BindImage); every element
// is the light buffer, and only the first is read.
layout(binding = 0) uniform sampler2D inLightBuffer[32];

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

void main()
{
	vec3 light = texture(inLightBuffer[0], fragUV).rgb;
	outColor = vec4(light * 0.5, 1.0);
}
