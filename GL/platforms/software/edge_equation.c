#include "edge_equation.h"

void EdgeEquationInit(EdgeEquation* edge, const float* v0, const float* v1) {
    edge->a = v0[1] - v1[1];
    edge->b = v1[0] - v0[0];
    edge->c = -(edge->a * (v0[0] + v1[0]) + edge->b * (v0[1] + v1[1])) / 2;
    edge->tie = edge->a != 0 ? edge->a > 0 : edge->b > 0;
}

float EdgeEquationEvaluate(const EdgeEquation* edge, float x, float y) {
    return edge->a * x + edge->b * y + edge->c;
}

bool EdgeEquationTestValue(const EdgeEquation* edge, float value) {
    return (value >= 0 || (value == 0 && edge->tie));
}

bool EdgeEquationTestPoint(const EdgeEquation* edge, float x, float y) {
    return EdgeEquationTestValue(edge, EdgeEquationEvaluate(edge, x, y));
}
