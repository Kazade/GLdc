#include "private.h"

void APIENTRY glVertexPackColor3fKOS(GLVertexKOS* vertex, float r, float g, float b) {
    vertex->bgra[3] = 255;
    vertex->bgra[2] = (r * 255.0f);
    vertex->bgra[1] = (g * 255.0f);
    vertex->bgra[0] = (b * 255.0f);
}

void APIENTRY glVertexPackColor4fKOS(GLVertexKOS* vertex, float r, float g, float b, float a) {
    vertex->bgra[3] = (a * 255.0f);
    vertex->bgra[2] = (r * 255.0f);
    vertex->bgra[1] = (g * 255.0f);
    vertex->bgra[0] = (b * 255.0f);
}
