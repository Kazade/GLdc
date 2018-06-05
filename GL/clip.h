#ifndef CLIP_H
#define CLIP_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CLIP_RESULT_ALL_IN_FRONT,
    CLIP_RESULT_ALL_BEHIND,
    CLIP_RESULT_ALL_ON_PLANE,
    CLIP_RESULT_FRONT_TO_BACK,
    CLIP_RESULT_BACK_TO_FRONT
} ClipResult;

ClipResult clipLineToNearZ(const float* v1, const float* v2, const float dist, float* vout, float* t);

#ifdef __cplusplus
}
#endif

#endif // CLIP_H
