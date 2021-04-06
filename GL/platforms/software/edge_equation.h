#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct EdgeEquation {
    float a;
    float b;
    float c;
    bool tie;
} EdgeEquation;

void EdgeEquationInit(EdgeEquation* edge, const float* v0, const float* v1);
float EdgeEquationEvaluate(const EdgeEquation* edge, float x, float y);
bool EdgeEquationTestValue(const EdgeEquation* edge, float value);
bool EdgeEquationTestPoint(const EdgeEquation* edge, float x, float y);

