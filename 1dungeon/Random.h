#pragma once
#include <random>
#include "Math.h"

class Random
{
public:
	static void Init();
	static void Seed(unsigned int seed);
	static Vector2 GetVector(const Vector2& min, const Vector2& max);
	static float GetFloat();
	static float GetFloatRange(float min, float max);
private:
	static std::mt19937 sGenerator;
};

