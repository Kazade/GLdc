#include "parameter_equation.h"
#include "edge_equation.h"

void ParameterEquationInit(ParameterEquation* equation, float p0, float p1, float p2, const EdgeEquation* e0, const EdgeEquation* e1, const EdgeEquation* e2, float area) {

    float factor = 1.0f / (2.0f * area);

    equation->a = factor * (p0 * e0->a + p1 * e1->a + p2 * e2->a);
    equation->b = factor * (p0 * e0->b + p1 * e1->b + p2 * e2->b);
    equation->c = factor * (p0 * e0->c + p1 * e1->c + p2 * e2->c);
}


float ParameterEquationEvaluate(const ParameterEquation* equation, float x, float y) {
    return equation->a * x + equation->b * y + equation->c;
}
