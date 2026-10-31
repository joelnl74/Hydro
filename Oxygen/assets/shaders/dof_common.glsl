// Shared by the depth of field passes (#135; Renderer2D::RecordDepthOfField), all on composite.vert's fullscreen
// triangle. Included, not compiled on its own.
//
// The circle of confusion (CoC) is signed and in pixels of the full-size view: negative in front of the focus point
// (towards the camera), positive behind it, 0 inside the sharp band.

// Must match DepthOfFieldPushConstants in VulkanGPUData.h (std430, 112 bytes).
layout(push_constant) uniform DepthOfFieldPushConstants {
	mat4 inverseViewProjection; // clip space -> world
	vec4 focus; // xyz: the view direction (unit length); w: dot(focus point, direction)
	vec4 band;  // x: near start, y: far start (metres), z: 1 / range, w: largest radius in full-size pixels
	vec4 texel; // xy: one texel of the pass's source in UV; z: largest radius in this pass's pixels; w: the gather's step
} dof;
