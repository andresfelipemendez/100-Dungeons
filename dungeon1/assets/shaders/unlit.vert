#version 450 core
uniform mat4 uWorldTransform;
uniform mat4 uViewProj;
layout (location = 0) in vec3 aPos;
out vec3 fragWorldPos;
void main()
{
	vec4 pos = vec4(aPos, 1.0);
	pos = uWorldTransform * pos;
	fragWorldPos = pos.xyz;
	gl_Position = uViewProj * pos;
};