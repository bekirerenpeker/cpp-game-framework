#pragma once

namespace Engine {

namespace Math {

#define PI      3.14159265359
#define PI2     PI / 2
#define PI4     PI / 4
#define TAU     PI * 2
#define DEG2RAD PI / 180
#define RAD2DEG 180 / PI
#define E_CONST 2.71828182845

float abs(float x);
float sign(float x);
float exp(float x);
float pow(float base, float pow);
float sqrt(float x);
float mod(float x, float y);

float ln(float x);      // natrual logarithm
float log10(float x);   // base 10 logarithm
float log2(float x);    // base 2  logarithm

float trunc(float x);
float ceil(float x);
float floor(float x);
float round(float x, int decimal = 1);

float min(float x, float y);
float max(float x, float y);
float clamp(float x, float minVal, float maxVal);

float lerp(float a, float b, float t);
float inverseLerp(float a, float b, float val);
float moveTowards(float a, float b, float step);

float pingPong(float x, float start, float end);

float deltaAngle(float a, float b);
float lerpAngle(float a, float b, float t);
float moveTowardsAngle(float a, float b, float step);

float perlin2D(float x, float y, int seed = 0);
float perlin3D(float x, float y, float z, int seed = 0);

float sin(float radians);
float cos(float radians);
float tan(float radians);

float asin(float radians);
float acos(float radians);
float atan(float radians);
float atan2(float y, float x);

}   // namespace Math

}   // namespace Engine
