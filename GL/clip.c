#include <float.h>
#include <stdio.h>

#include "clip.h"

ClipResult clipLineToNearZ(const float* v1, const float* v2, const float dist, float* vout, float* t) {
    if(v1[2] < dist && v2[2] < dist) {
        // Both behind, no clipping
        return CLIP_RESULT_ALL_BEHIND;
    }

    if(v1[2] > dist && v2[2] > dist) {
        return CLIP_RESULT_ALL_IN_FRONT;
    }

    float vec [] = {v2[0] - v1[0], v2[1] - v1[1], v2[2] - v1[2]};

    /*
     * The plane normal will always be pointing down the negative Z so we can simplify the dot products as x and y will always be zero
     * the resulting calculation will result in simply -z of the vector
    */
    float vecDotP = -vec[2];

    /* If the dot product is zero there is no intersection */
    if(vecDotP > FLT_MIN || vecDotP < -FLT_MIN) {
        *t = (-(dist - v1[2])) / vecDotP;

        vout[0] = v1[0] + (vec[0] * (*t));
        vout[1] = v1[1] + (vec[1] * (*t));
        vout[2] = v1[2] + (vec[2] * (*t));

        return (v1[2] >= dist) ? CLIP_RESULT_FRONT_TO_BACK : CLIP_RESULT_BACK_TO_FRONT;
    } else {
        return CLIP_RESULT_ALL_ON_PLANE;
    }
}


TriangleClipResult clipTriangleToNearZ(
    const float plane_dist,
    const unsigned short triangle_n, const pvr_vertex_t* v1, const pvr_vertex_t* v2, const pvr_vertex_t *v3,
    pvr_vertex_t* v1out, pvr_vertex_t* v2out, pvr_vertex_t* v3out, pvr_vertex_t* v4out
) {

    /* Fast out. Let's just see if everything is in front of the clip plane (and as in OpenGL Z comes out of the screen
     * we check to see if they are all < -dist
     */

    typedef unsigned char uint8;
    uint8 visible = (v1->z >= plane_dist) ? 1 : 0 | (v2->z >= plane_dist) ? 2 : 0 | (v3->z >= plane_dist) ? 4 : 0;

    switch(visible) {
    case 0b000:
        /* If behind is zero, then none of the vertices are visible */
        return TRIANGLE_CLIP_RESULT_DROP_TRIANGLE;
    case 0b111:
        /* If behind is zero, then none of the vertices are visible */
        return TRIANGLE_CLIP_RESULT_NO_CHANGE;
    case 0b101:
    case 0b110:
    case 0b011: {
        /* Two vertices are visible */
        /* Tricky case. If two vertices are visible then manipulating the other one is going to change the shape of the
         * triangle. So we have to clip both lines, and output a new vertex.
         */

        return TRIANGLE_CLIP_RESULT_ALTERED_AND_CREATED_VERTEX;
    } break;
    default: {
        /* One vertex is visible */
        /* This is the "easy" case, we simply find the vertex which is visible, and clip the lines to the other 2 against the plane */
        fprintf(stderr, "Clipping triangle\n");

        pvr_vertex_t tmp1, tmp2;
        float t1, t2;

        if(visible == 0b001) {
            ClipResult l1 = clipLineToNearZ(&v1->x, &v2->x, plane_dist, &tmp1.x, &t1);
            ClipResult l2 = clipLineToNearZ(&v1->x, &v3->x, plane_dist, &tmp2.x, &t2);

            fprintf(stderr, "New V2: %f, %f, %f. T: %f\n", tmp1.x, tmp1.y, tmp1.z, t1);
            fprintf(stderr, "New V3: %f, %f, %f. T: %f\n", tmp2.x, tmp2.y, tmp2.z, t2);

            *v1out = *v1;
            *v2out = tmp1;
            *v3out = tmp2;
        } else if(visible == 0b010) {
            ClipResult l1 = clipLineToNearZ(&v2->x, &v1->x, plane_dist, &tmp1.x, &t1);
            ClipResult l2 = clipLineToNearZ(&v2->x, &v3->x, plane_dist, &tmp2.x, &t2);

            *v1out = tmp1;
            *v2out = *v2;
            *v3out = tmp2;
        } else {
            ClipResult l1 = clipLineToNearZ(&v3->x, &v1->x, plane_dist, &tmp1.x, &t1);
            ClipResult l2 = clipLineToNearZ(&v3->x, &v2->x, plane_dist, &tmp2.x, &t2);

            *v1out = tmp1;
            *v2out = tmp2;
            *v3out = *v3;
        }

        return TRIANGLE_CLIP_RESULT_ALTERED_VERTICES;
    }
    }
}
