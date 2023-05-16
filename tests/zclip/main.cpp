
#include <cstdint>
#include <vector>
#include <cstdio>
#include <cmath>
#include <stdexcept>
#include <cassert>

#define SQ_BASE_ADDRESS 0
#define SPAN_SORT_CFG 0
#define PVR_SET(x, y) (void)(x); (void)(y)

struct Vertex  {
    uint32_t flags;
    float xyz[3];
    float uv[2];
    float w;
    uint8_t bgra[4];
};

struct {
    float hwidth;
    float x_plus_hwidth;
    float hheight;
    float y_plus_hheight;
} VIEWPORT = {320, 320, 240, 240};


struct VideoMode {
    float height;
};

static VideoMode* GetVideoMode() {
    static VideoMode mode = {320.0f};
    return &mode;
}

enum GPUCommand {
    GPU_CMD_POLYHDR = 0x80840000,
    GPU_CMD_VERTEX = 0xe0000000,
    GPU_CMD_VERTEX_EOL = 0xf0000000,
    GPU_CMD_USERCLIP = 0x20000000,
    GPU_CMD_MODIFIER = 0x80000000,
    GPU_CMD_SPRITE = 0xA0000000
};

static std::vector<Vertex> sent;

static inline void interpolateColour(const uint32_t* a, const uint32_t* b, const float t, uint32_t* out) {
    const static uint32_t MASK1 = 0x00FF00FF;
    const static uint32_t MASK2 = 0xFF00FF00;

    const uint32_t f2 = 256 * t;
    const uint32_t f1 = 256 - f2;

    *out = (((((*a & MASK1) * f1) + ((*b & MASK1) * f2)) >> 8) & MASK1) |
            (((((*a & MASK2) * f1) + ((*b & MASK2) * f2)) >> 8) & MASK2);
}

static inline void _glClipEdge(const Vertex* v1, const Vertex* v2, Vertex* vout) {
    /* Clipping time! */
    const float d0 = v1->w + v1->xyz[2];
    const float d1 = v2->w + v2->xyz[2];
    const float sign = ((2.0f * (d1 < d0)) - 1.0f);
    const float epsilon = -0.00001f * sign;
    const float n = (d0 - d1);
    const float r = (1.f / sqrtf(n * n)) * sign;
    float t = fmaf(r, d0, epsilon);

    vout->xyz[0] = fmaf(v2->xyz[0] - v1->xyz[0], t, v1->xyz[0]);
    vout->xyz[1] = fmaf(v2->xyz[1] - v1->xyz[1], t, v1->xyz[1]);
    vout->xyz[2] = fmaf(v2->xyz[2] - v1->xyz[2], t, v1->xyz[2]);
    vout->w = fmaf(v2->w - v1->w, t, v1->w);

    vout->uv[0] = fmaf(v2->uv[0] - v1->uv[0], t, v1->uv[0]);
    vout->uv[1] = fmaf(v2->uv[1] - v1->uv[1], t, v1->uv[1]);

    interpolateColour((uint32_t*) v1->bgra, (uint32_t*) v2->bgra, t, (uint32_t*) vout->bgra);
}

bool glIsVertex(const uint32_t flags) {
    return flags == GPU_CMD_VERTEX_EOL || flags == GPU_CMD_VERTEX;
}

bool glIsLastVertex(const uint32_t flags) {
    return flags == GPU_CMD_VERTEX_EOL;
}

void _glSubmitHeaderOrVertex(volatile uint32_t*, Vertex* vtx) {
    sent.push_back(*vtx);
}

float _glFastInvert(float x) {
    return (1.f / __builtin_sqrtf(x * x));
}

void _glPerspectiveDivideVertex(Vertex* vertex, const float h) {
    const float f = _glFastInvert(vertex->w);

    /* Convert to NDC and apply viewport */
    vertex->xyz[0] = __builtin_fmaf(
        VIEWPORT.hwidth, vertex->xyz[0] * f, VIEWPORT.x_plus_hwidth
    );

    vertex->xyz[1] = h - __builtin_fmaf(
        VIEWPORT.hheight, vertex->xyz[1] * f, VIEWPORT.y_plus_hheight
    );

    /* Orthographic projections need to use invZ otherwise we lose
    the depth information. As w == 1, and clip-space range is -w to +w
    we add 1.0 to the Z to bring it into range. We add a little extra to
    avoid a divide by zero.
    */

    vertex->xyz[2] = (vertex->w == 1.0f) ? _glFastInvert(1.0001f + vertex->xyz[2]) : f;
}


void memcpy_vertex(Vertex* dst, Vertex* src) {
    *dst = *src;
}

/* Zclipping is so difficult to get right, that self sample tests all the cases of clipping and makes sure that things work as expected */

#ifdef __DREAMCAST__
static volatile int *pvrdmacfg = (int*)0xA05F6888;
static volatile int *qacr = (int*)0xFF000038;
#else
static int pvrdmacfg[2];
static int qacr[2];
#endif

void SceneListSubmit(void* src, int n) {
    /* You need at least a header, and 3 vertices to render anything */
    if(n < 4) {
        return;
    }

    const float h = GetVideoMode()->height;

    PVR_SET(SPAN_SORT_CFG, 0x0);

    //Set PVR DMA registers
    pvrdmacfg[0] = 1;
    pvrdmacfg[1] = 1;

    //Set QACR registers
    qacr[1] = qacr[0] = 0x11;

    volatile uint32_t *d = SQ_BASE_ADDRESS;

    int8_t queue_head = 0;
    int8_t queue_tail = 0;

    /* The most vertices ever in the queue is 5 (as some clipping operations
     * produce and additional couple of vertice, but we add one more so the ring buffer doesn't
     * trip over itself (e.g. if tail == head we can guarantee it's empty, not full) */
    Vertex __attribute__((aligned(32))) queue[4];
    const int queue_capacity = sizeof(queue) / sizeof(Vertex);

    Vertex* vertex = (Vertex*) src;
    uint32_t visible_mask = 0;

#if CLIP_DEBUG
    for(int i = 0; i < n; ++i) {
        fprintf(stderr, "{%f, %f, %f, %f}, // %x (%x)\n", vertex[i].xyz[0], vertex[i].xyz[1], vertex[i].xyz[2], vertex[i].w, vertex[i].flags, &vertex[i]);
    }

    fprintf(stderr, "----\n");
#endif
    while(n--) {
        bool last_vertex = false;
        memcpy_vertex(queue + queue_tail, vertex);
        ++vertex;
        switch(queue[queue_tail].flags) {
            case GPU_CMD_POLYHDR:
                _glSubmitHeaderOrVertex(d, &queue[queue_tail]);
            break;
            case GPU_CMD_VERTEX_EOL:
                last_vertex = true;  // fallthru
            case GPU_CMD_VERTEX:
                visible_mask = (visible_mask >> 1) | (queue[queue_tail].xyz[2] >= -queue[queue_tail].w) << 2;
                assert(visible_mask < 15);
                queue_tail = (queue_tail + 1) % queue_capacity;
            default:
            break;
        }

        int counter = (queue_tail - queue_head + queue_capacity) % queue_capacity;
        if(counter < 3) {
            continue;
        }

#if CLIP_DEBUG
        fprintf(stderr, "%d\n", visible_mask);
#endif
        Vertex __attribute__((aligned(32))) a, b;  // Scratch vertices
        switch(visible_mask) {
            case 0:
            break;
            case 7:
                /* All visible, push the first vertex and move on */
                _glPerspectiveDivideVertex(&queue[queue_head], h);
                _glSubmitHeaderOrVertex(d, &queue[queue_head]);

                if(last_vertex) {
                    /* If this was the last vertex in the strip, we need to flush the queue and then
                       restart it again */

                    int v1 = (queue_head + 1) % queue_capacity;
                    int v2 = (queue_head + 2) % queue_capacity;

                    _glPerspectiveDivideVertex(&queue[v1], h);
                    _glSubmitHeaderOrVertex(d, &queue[v1]);

                    _glPerspectiveDivideVertex(&queue[v2], h);
                    _glSubmitHeaderOrVertex(d, &queue[v2]);
                }
            break;
            case 1:
                /* First vertex was visible */
                {
                        Vertex* v0 = &queue[queue_head];
                        Vertex* v1 = &queue[(queue_head + 1) % queue_capacity];
                        Vertex* v2 = &queue[(queue_head + 2) % queue_capacity];

                        _glClipEdge(v0, v1, &a);
                        _glClipEdge(v2, v0, &b);
                        a.flags = GPU_CMD_VERTEX;

                        /* If v2 was the last in the strip, then b should be. If it wasn't
                        we'll create a degenerate triangle by adding b twice in a row so that the
                        strip processing will continue correctly after crossing the plane so it can
                        cross back*/
                        b.flags = v2->flags;

                        _glPerspectiveDivideVertex(v0, h);
                        _glPerspectiveDivideVertex(&a, h);
                        _glPerspectiveDivideVertex(&b, h);

                        _glSubmitHeaderOrVertex(d, v0);
                        _glSubmitHeaderOrVertex(d, &a);
                        _glSubmitHeaderOrVertex(d, &b);
                        _glSubmitHeaderOrVertex(d, &b);
                }
            break;
            case 2:
                /* Second vertex was visible. In self case we need to create a triangle and produce
                two new vertices: 1-2, and 2-3. */
                {
                        Vertex* v0 = &queue[queue_head];
                        const Vertex* v1 = &queue[(queue_head + 1) % queue_capacity];
                        const Vertex* v2 = &queue[(queue_head + 2) % queue_capacity];

                        _glClipEdge(v0, v1, &a);
                        _glClipEdge(v1, v2, &b);
                        a.flags = GPU_CMD_VERTEX;
                        b.flags = v2->flags;

                        _glPerspectiveDivideVertex(v0, h);
                        _glPerspectiveDivideVertex(&a, h);
                        _glPerspectiveDivideVertex(&b, h);

                        _glSubmitHeaderOrVertex(d, &a);
                        _glSubmitHeaderOrVertex(d, v0);
                        _glSubmitHeaderOrVertex(d, &b);
                }
            break;
            case 3:  /* First and second vertex were visible */
                    {
                        Vertex* v0 = &queue[queue_head];
                        Vertex __attribute__((aligned(32))) v1 = queue[(queue_head + 1) % queue_capacity];
                        Vertex* v2 = &queue[(queue_head + 2) % queue_capacity];

                        _glClipEdge(&v1, v2, &a);
                        _glClipEdge(v2, v0, &b);

                        a.flags = v2->flags;
                        b.flags = GPU_CMD_VERTEX;

                        _glPerspectiveDivideVertex(v0, h);
                        _glPerspectiveDivideVertex(&v1, h);
                        _glPerspectiveDivideVertex(&a, h);
                        _glPerspectiveDivideVertex(&b, h);

                        _glSubmitHeaderOrVertex(d, v0);
                        _glSubmitHeaderOrVertex(d, &v1);
                        _glSubmitHeaderOrVertex(d, &b);
                        _glSubmitHeaderOrVertex(d, &v1);
                        _glSubmitHeaderOrVertex(d, &a);
                }
            break;
            case 4:
                /* Third vertex was visible. */
                {
                        Vertex* v0 = &queue[queue_head];
                        Vertex* v1 = &queue[(queue_head + 1) % queue_capacity];
                        Vertex __attribute__((aligned(32))) v2 = queue[(queue_head + 2) % queue_capacity];

                        _glClipEdge(&v2, v0, &a);
                        _glClipEdge(v1, &v2, &b);
                        a.flags = GPU_CMD_VERTEX;
                        b.flags = GPU_CMD_VERTEX;

                        _glPerspectiveDivideVertex(&v2, h);
                        _glPerspectiveDivideVertex(&a, h);
                        _glPerspectiveDivideVertex(&b, h);

                        _glSubmitHeaderOrVertex(d, &a);
                        _glSubmitHeaderOrVertex(d, &a);
                        _glSubmitHeaderOrVertex(d, &b);
                        _glSubmitHeaderOrVertex(d, &v2);
                }
            break;
            case 5:  /* First and third vertex were visible */
                {
                        Vertex* v0 = &queue[queue_head];
                        Vertex* v1 = &queue[(queue_head + 1) % queue_capacity];
                        Vertex __attribute__((aligned(32))) v2 = queue[(queue_head + 2) % queue_capacity];

                        _glClipEdge(v0, v1, &a);
                        _glClipEdge(v1, &v2, &b);
                        a.flags = GPU_CMD_VERTEX;
                        b.flags = GPU_CMD_VERTEX;

                        _glPerspectiveDivideVertex(v0, h);
                        _glPerspectiveDivideVertex(&v2, h);
                        _glPerspectiveDivideVertex(&a, h);
                        _glPerspectiveDivideVertex(&b, h);

                        _glSubmitHeaderOrVertex(d, v0);
                        _glSubmitHeaderOrVertex(d, &a);
                        uint32_t v2_flags = v2.flags;
                        v2.flags = GPU_CMD_VERTEX;
                        _glSubmitHeaderOrVertex(d, &v2);
                        v2.flags = v2_flags;
                        _glSubmitHeaderOrVertex(d, &b);
                        _glSubmitHeaderOrVertex(d, &v2);
                }
            break;
            case 6:  /* Second and third vertex were visible */
                {
                        Vertex* v0 = &queue[queue_head];
                        Vertex __attribute__((aligned(32))) v1 = queue[(queue_head + 1) % queue_capacity];
                        Vertex __attribute__((aligned(32))) v2 = queue[(queue_head + 2) % queue_capacity];

                        _glClipEdge(v0, &v1, &a);
                        _glClipEdge(&v2, v0, &b);

                        a.flags = GPU_CMD_VERTEX;
                        b.flags = GPU_CMD_VERTEX;

                        _glPerspectiveDivideVertex(&v1, h);
                        _glPerspectiveDivideVertex(&v2, h);
                        _glPerspectiveDivideVertex(&a, h);
                        _glPerspectiveDivideVertex(&b, h);

                        _glSubmitHeaderOrVertex(d, &a);
                        _glSubmitHeaderOrVertex(d, &v1);
                        _glSubmitHeaderOrVertex(d, &b);
                        _glSubmitHeaderOrVertex(d, &v1);
                        _glSubmitHeaderOrVertex(d, &v2);
                }
            break;
            default:
                break;
        }

        if(last_vertex) {
            visible_mask = queue_head = queue_tail = 0;
        } else {
            queue_head = (queue_head + 1) % queue_capacity;
        }
    }
}


struct VertexTmpl {
    VertexTmpl(float x, float y, float z, float w):
        x(x), y(y), z(z), w(w) {}

    float x, y, z, w;
};

std::vector<Vertex> make_vertices(const std::vector<VertexTmpl>& verts) {
    std::vector<Vertex> result;
    Vertex r;

    r.flags = GPU_CMD_POLYHDR;
    result.push_back(r);

    for(auto& v: verts) {
        r.flags = GPU_CMD_VERTEX;
        r.xyz[0] = v.x;
        r.xyz[1] = v.y;
        r.xyz[2] = v.z;
        r.uv[0] = 0.0f;
        r.uv[1] = 0.0f;
        r.w = v.w;

        result.push_back(r);
    }

    result.back().flags = GPU_CMD_VERTEX_EOL;
    return result;
}

template<typename T, typename U>
void check_equal(const T& lhs, const U& rhs) {
    if(lhs != rhs) {
        throw std::runtime_error("Assertion failed");
    }
}

template<>
void check_equal(const Vertex& lhs, const Vertex& rhs) {
    if(lhs.xyz[0] != rhs.xyz[0] ||
       lhs.xyz[1] != rhs.xyz[1] ||
       lhs.xyz[2] != rhs.xyz[2] ||
       lhs.w != rhs.w) {
        throw std::runtime_error("Assertion failed");
    }
}


bool test_clip_case_001() {
    /* The first vertex is visible only */
    sent.clear();

    auto data = make_vertices({
        {0.000000, -2.414213, 3.080808, 5.000000},
        {-4.526650, -2.414213, -7.121212, -5.000000},
        {4.526650, -2.414213, -7.121212, -5.000000}
    });

    SceneListSubmit(&data[0], data.size());

    check_equal(sent.size(), 5);
    check_equal(sent[0].flags, GPU_CMD_POLYHDR);
    check_equal(sent[1].flags, GPU_CMD_VERTEX);
    check_equal(sent[2].flags, GPU_CMD_VERTEX);

    // Because we're sending a single triangle, we end up sending a
    // degenerate final vert. But if we were sending more than one triangle
    // this would be GPU_CMD_VERTEX twice
    check_equal(sent[3].flags, GPU_CMD_VERTEX_EOL);
    check_equal(sent[4].flags, GPU_CMD_VERTEX_EOL);
    check_equal(sent[3], sent[4]);
    return true;
}

bool test_clip_case_010() {
    /* The third vertex is visible only */
    sent.clear();

    auto data = make_vertices({
        {-4.526650, -2.414213, -7.121212, -5.000000},
        {0.000000, -2.414213, 3.080808, 5.000000},
        {4.526650, -2.414213, -7.121212, -5.000000}
    });

    SceneListSubmit(&data[0], data.size());

    check_equal(sent.size(), 4);
    check_equal(sent[0].flags, GPU_CMD_POLYHDR);
    check_equal(sent[1].flags, GPU_CMD_VERTEX);
    check_equal(sent[2].flags, GPU_CMD_VERTEX);
    check_equal(sent[3].flags, GPU_CMD_VERTEX_EOL);
    return true;
}

bool test_clip_case_100() {
    /* The third vertex is visible only */
    sent.clear();

    auto data = make_vertices({
        {-4.526650, -2.414213, -7.121212, -5.000000},
        {4.526650, -2.414213, -7.121212, -5.000000},
        {0.000000, -2.414213, 3.080808, 5.000000}
    });

    SceneListSubmit(&data[0], data.size());

    check_equal(sent.size(), 5);
    check_equal(sent[0].flags, GPU_CMD_POLYHDR);
    check_equal(sent[1].flags, GPU_CMD_VERTEX);
    check_equal(sent[2].flags, GPU_CMD_VERTEX);

    // Because we're sending a single triangle, we end up sending a
    // degenerate final vert. But if we were sending more than one triangle
    // this would be GPU_CMD_VERTEX twice
    check_equal(sent[3].flags, GPU_CMD_VERTEX);
    check_equal(sent[4].flags, GPU_CMD_VERTEX_EOL);
    check_equal(sent[1], sent[2]);
    return true;
}

bool test_clip_case_110() {
    /* 2nd and 3rd visible */
    sent.clear();

    auto data = make_vertices({
        {0.0, -2.414213, -7.121212, -5.000000},
        {-4.526650, -2.414213, 3.080808, 5.000000},
        {4.526650, -2.414213, 3.080808, 5.000000}
    });

    SceneListSubmit(&data[0], data.size());

    check_equal(sent.size(), 6);
    check_equal(sent[0].flags, GPU_CMD_POLYHDR);
    check_equal(sent[1].flags, GPU_CMD_VERTEX);
    check_equal(sent[2].flags, GPU_CMD_VERTEX);
    check_equal(sent[3].flags, GPU_CMD_VERTEX);
    check_equal(sent[4].flags, GPU_CMD_VERTEX);
    check_equal(sent[5].flags, GPU_CMD_VERTEX_EOL);
    check_equal(sent[2], sent[4]);
    return true;
}

bool test_clip_case_011() {
    /* 1st and 2nd visible */
    sent.clear();

    auto data = make_vertices({
        {-4.526650, -2.414213, 3.080808, 5.000000},
        {4.526650, -2.414213, 3.080808, 5.000000},
        {0.0, -2.414213, -7.121212, -5.000000}
    });

    SceneListSubmit(&data[0], data.size());

    check_equal(sent.size(), 6);
    check_equal(sent[0].flags, GPU_CMD_POLYHDR);
    check_equal(sent[1].flags, GPU_CMD_VERTEX);
    check_equal(sent[2].flags, GPU_CMD_VERTEX);
    check_equal(sent[3].flags, GPU_CMD_VERTEX);
    check_equal(sent[4].flags, GPU_CMD_VERTEX);
    check_equal(sent[5].flags, GPU_CMD_VERTEX_EOL);
    check_equal(sent[2], sent[4]);
    return true;
}

bool test_clip_case_101() {
    /* 1st and 3rd visible */
    sent.clear();

    auto data = make_vertices({
        {-4.526650, -2.414213, 3.080808, 5.000000},
        {0.0, -2.414213, -7.121212, -5.000000},
        {4.526650, -2.414213, 3.080808, 5.000000},
    });

    SceneListSubmit(&data[0], data.size());

    check_equal(sent.size(), 6);
    check_equal(sent[0].flags, GPU_CMD_POLYHDR);
    check_equal(sent[1].flags, GPU_CMD_VERTEX);
    check_equal(sent[2].flags, GPU_CMD_VERTEX);
    check_equal(sent[3].flags, GPU_CMD_VERTEX);
    check_equal(sent[4].flags, GPU_CMD_VERTEX);
    check_equal(sent[5].flags, GPU_CMD_VERTEX_EOL);
    check_equal(sent[3], sent[5]);
    return true;
}

bool test_clip_case_111() {
    /* 1st and 3rd visible */
    sent.clear();

    auto data = make_vertices({
        {-4.526650, -2.414213, 3.080808, 5.000000},
        {0.0, -2.414213, -7.121212, 8.000000},
        {4.526650, -2.414213, 3.080808, 5.000000},
    });

    SceneListSubmit(&data[0], data.size());

    check_equal(sent.size(), 4);
    check_equal(sent[0].flags, GPU_CMD_POLYHDR);
    check_equal(sent[1].flags, GPU_CMD_VERTEX);
    check_equal(sent[2].flags, GPU_CMD_VERTEX);
    check_equal(sent[3].flags, GPU_CMD_VERTEX_EOL);
    return true;
}


bool test_start_behind() {
    /* Triangle behind the plane, but the strip continues in front */
    sent.clear();

    auto data = make_vertices({
      {-3.021717, -2.414213, -10.155344, -9.935254},
      {5.915236, -2.414213, -9.354721, -9.136231},
      {-5.915236, -2.414213, -0.264096, -0.063767},
      {3.021717, -2.414213, 0.536527, 0.735255},
      {-7.361995, -2.414213, 4.681529, 4.871976},
      {1.574958, -2.414213, 5.482152, 5.670999},
    });

    SceneListSubmit(&data[0], data.size());

    return true;
}

bool test_longer_strip() {
    sent.clear();

    auto data = make_vertices({
        {-4.384623, -2.414213, -5.699644, -5.488456},
        {4.667572, -2.414213, -5.621354, -5.410322},
        {-4.667572, -2.414213, 4.319152, 4.510323},
        {4.384623, -2.414213, 4.397442, 4.588456},
        {-4.809045, -2.414213, 9.328549, 9.509711},
        {4.243149, -2.414213, 9.406840, 9.587846},
    });

    SceneListSubmit(&data[0], data.size());

    return true;
}

int main(int argc, char* argv[]) {
    // test_clip_case_000();
    test_clip_case_001();
    test_clip_case_010();
    test_clip_case_100();
    test_clip_case_110();
    test_clip_case_011();
    test_clip_case_101();
    test_clip_case_111();

    test_start_behind();
    test_longer_strip();

    return 0;
}
