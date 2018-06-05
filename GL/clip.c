#include <float.h>
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
