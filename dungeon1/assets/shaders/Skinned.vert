#version 330

uniform mat4 uWorldTransform;
uniform mat4 uViewProj;

layout(location = 0) in vec3 inPosition;
layout(location = 2) in vec3 inNormal;
layout(location = 5) in uvec4 inSkinBones;
layout(location = 6) in vec4 inSkinWeights;
layout(location = 3) in vec2 inTexCoord;

out vec2 fragTexCoord;
out vec3 fragNormal;
out vec3 fragWorldPos;

void main()
{
	vec4 pos = vec4(inPosition, 1.0);
	pos = pos * uWorldTransform;
	fragWorldPos = pos.xyz;
	gl_Position = pos * uViewProj;

	fragNormal = (vec4(inNormal, 0.0f) * uWorldTransform).xyz;
	fragTexCoord = inTexCoord;
}
