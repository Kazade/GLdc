#include "../include/glkos.h"

void APIENTRY glVertexPackColor3fKOS(GLVertexKOS* vertex, float r, float g, float b) {
    vertex->color[3] = 255;
    vertex->color[2] = (r * 255.0f);
    vertex->color[1] = (g * 255.0f);
    vertex->color[0] = (b * 255.0f);
}

void APIENTRY glVertexPackColor4fKOS(GLVertexKOS* vertex, float r, float g, float b, float a) {
    vertex->color[3] = (a * 255.0f);
    vertex->color[2] = (r * 255.0f);
    vertex->color[1] = (g * 255.0f);
    vertex->color[0] = (b * 255.0f);
}
