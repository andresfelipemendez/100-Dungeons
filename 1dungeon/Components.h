#pragma once
#include "Math.h"
#include "glm.hpp"

struct Camera {
	float fov;
	float aspect;
	float nearPlane;
	float farPlane;
	Matrix4 viewMatrix;
	Matrix4 projectionMatrix;
};

struct AmbientLightColor {
	Vector3 color;
};

struct DirectionalLight {
	Vector3 direction;
	Vector3 diffuseColor;
	Vector3 specColor;
};

struct Texture {
	unsigned int textureID;
	int texWidth;
	int texHeight;
	float scale = 1;
};


struct Sprite {
	unsigned int textureID;
	int texWidth;
	int texHeight;
	float scale = 1;
};

struct Mesh {
	unsigned int arrayID;
	size_t numVerts;
};

struct SkinnedMesh {
	unsigned int arrayID;
	size_t numVerts;
};

struct MultiMesh {
	std::vector<Mesh> meshes;
};

struct VertexArray {
	unsigned int arrayID;
	size_t numVerts;
};

struct Shader {
	unsigned int programID;
};

struct Translation {
	Vector3 position;
};

struct Rotation {
	Quaternion rotation;
};

struct Scale {
	float scale;
};

struct Shoot {
  int shootKey;
  float shootCooldown;
};

struct DestroyTag {};

struct Laser {
  float life;
  float radius;
};

struct Move {
  float angularSpeed;
  float forwardSpeed;
};

struct Circle {
  float radius;
};

struct Input {
  float maxFwdSpeed;
  float maxAngSpeed;
  int forwardKey;
  int backKey;
  int clockwiseKey;
  int counterClockwiseKey;
};

struct BoneTransform
{
	Quaternion rotation;
	Vector3 translation;
};

struct Bone {
	BoneTransform localBindBose;
	std::string name;
	int parent;
};

struct Skeleton {
	std::vector<Bone> bones;
	std::vector<Matrix4> globalInvertBindBones;
};

struct SkinnedVertex
{
	glm::vec3 m_Position;  // layout(location = 0)
	glm::vec4 m_Color;     // layout(location = 1)
	glm::vec3 m_Normal;    // layout(location = 2)
	glm::vec2 m_UV;        // layout(location = 3)
	glm::vec3 m_Tangent;   // layout(location = 4)
	glm::ivec4 m_JointIds; // layout(location = 5)
	glm::vec4 m_Weights;   // layout(location = 6)
};

struct Submesh
{
	uint32_t m_FirstIndex;
	uint32_t m_FirstVertex;
	uint32_t m_IndexCount;
	uint32_t m_VertexCount;
	uint32_t m_InstanceCount = 0;
	/*Material m_Material;
	Resources m_Resources;*/
};