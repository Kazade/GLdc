#ifndef CLIP_H
#define CLIP_H

#ifdef __cplusplus
extern "C" {
#endif

/* If we're not on the Dreamcast then define pvr_vertex_t
 * (this if for testing only)
 */
#ifndef _arch_dreamcast
typedef struct {
    uint32  flags;              /**< \brief TA command (vertex flags) */
    float   x;                  /**< \brief X coordinate */
    float   y;                  /**< \brief Y coordinate */
    float   z;                  /**< \brief Z coordinate */
    float   u;                  /**< \brief Texture U coordinate */
    float   v;                  /**< \brief Texture V coordinate */
    uint32  argb;               /**< \brief Vertex color */
    uint32  oargb;              /**< \brief Vertex offset color */
} pvr_vertex_t;
#else
#include <dc/pvr.h>
#endif

typedef enum {
    CLIP_RESULT_ALL_IN_FRONT,
    CLIP_RESULT_ALL_BEHIND,
    CLIP_RESULT_ALL_ON_PLANE,
    CLIP_RESULT_FRONT_TO_BACK,
    CLIP_RESULT_BACK_TO_FRONT
} ClipResult;

ClipResult clipLineToNearZ(const float* v1, const float* v2, const float dist, float* vout, float* t);


/* There are 4 possible situations we'll hit when clipping triangles:
 *
 * 1. The entire triangle was in front of the near plane, so we do nothing
 * 2. The entire triangle was behind the near plane, so we drop it completely
 * 3. One vertex was behind the clip plane. In this case we need to create a new vertex and an additional triangle
 * 4. Two vertices were behind the clip plane, we can simply move them so the triangle no longer intersects
 */
typedef enum {
    TRIANGLE_CLIP_RESULT_NO_CHANGE,
    TRIANGLE_CLIP_RESULT_DROP_TRIANGLE,
    TRIANGLE_CLIP_RESULT_ALTERED_AND_CREATED_VERTEX,
    TRIANGLE_CLIP_RESULT_ALTERED_VERTICES
} TriangleClipResult;

/* Clips a triangle from a triangle strip to a near-z plane. Alternating triangles in a strip switch vertex ordering
 * so the number of the triangle must be passed in (vn - 2).
 *
 * Note that clipping a triangle with a plane may create a quadrilateral, so this function must have
 * a space to output the 4th vertex.
 *
 * The outputs can be the same as the inputs.
 */
TriangleClipResult clipTriangleToNearZ(
    const float plane_dist,
    const unsigned short triangle_n, const pvr_vertex_t* v1, const pvr_vertex_t* v2, const pvr_vertex_t *v3,
    pvr_vertex_t* v1out, pvr_vertex_t* v2out, pvr_vertex_t* v3out, pvr_vertex_t* v4out, unsigned char* visible
);

#ifdef __cplusplus
}
#endif

#endif // CLIP_H
