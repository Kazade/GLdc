#pragma once

#include "../utils/test.h"
#include "../GL/flush.h"
#include "../containers/aligned_vector.h"

namespace {

struct VertexBuilder {
    VertexBuilder() {
        aligned_vector_init(&list_, sizeof(Vertex));
    }

    ~VertexBuilder() {
        aligned_vector_clear(&list_);
    }

    VertexBuilder& add_header() {
        Vertex v;
        v.flags = 100; // I dunno what this bit of memory would be

        aligned_vector_push_back(&list_, &v, 1);

        return *this;
    }

    VertexBuilder& add(float x, float y, float z, float w) {
        Vertex v;
        v.flags = PVR_CMD_VERTEX;
        v.xyz[0] = x;
        v.xyz[1] = y;
        v.xyz[2] = z;
        v.w = w;
        aligned_vector_push_back(&list_, &v, 1);
        return *this;
    }

    VertexBuilder& add_last(float x, float y, float z, float w) {
        add(x, y, z, w);
        Vertex* back = (Vertex*) aligned_vector_back(&list_);
        back->flags = PVR_CMD_VERTEX_EOL;
        return *this;
    }

    std::pair<Vertex*, int> done() {
        return std::make_pair((Vertex*) aligned_vector_at(&list_, 0), list_.size);
    }

private:
    AlignedVector list_;
};

#define assert_vertex_equal(v, x, y, z) \
    assert_is_not_null(v); \
    assert_close(v->xyz[0], x, 0.0001f); \
    assert_close(v->xyz[1], y, 0.0001f); \
    assert_close(v->xyz[2], z, 0.0001f); \

#define assert_is_header(v) \
    assert_false(isVertex(v)); \


class NearZClippingTests : public gldc::test::GLdcTestCase {
public:
    void test_clipping_100() {
        /* When only the first vertex is visible, we still
         * end up with 4 elements (3 vertices + 1 header). The
         * first vertex should be the same */

        VertexBuilder builder;

        auto list = builder.
            add_header().
            add(1, 1, 2, 1).
            add(1, 0, 2, -1).
            add_last(0, 1, 2, -1).done();

        ListIterator* it = _glIteratorBegin(list.first, list.second);
        Vertex* v0 = it->active;
        assert_is_not_null(v0);
        assert_false(isVertex(v0)); // Should be a header

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        Vertex* v1 = it->active;
        assert_is_not_null(v1);
        assert_true(isVertex(v1));

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        Vertex* v2 = it->active;
        assert_is_not_null(v2);
        assert_true(isVertex(v2));

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        Vertex* v3 = it->active;
        assert_is_not_null(v3);
        assert_true(isVertex(v3));

        it = _glIteratorNext(it);
        assert_is_null(it);
    }

    void test_clipping_110() {
        /* First two vertices are visible, so we need to
         * generate 2 vertices and manipulate the strip */

        VertexBuilder builder;

        auto list = builder.
            add_header().
            add(1, 1, 2, 1).
            add(1, 0, 2, 1).
            add_last(0, 1, 2, -1).done();

        ListIterator* it = _glIteratorBegin(list.first, list.second);
        Vertex* v0 = it->active;
        assert_is_not_null(v0);
        assert_false(isVertex(v0)); // Should be a header

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        Vertex* v1 = it->active;
        assert_is_not_null(v1);
        assert_true(isVertex(v1));

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        Vertex* v2 = it->active;
        assert_is_not_null(v2);
        assert_true(isVertex(v2));

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        Vertex* v3 = it->active;
        assert_is_not_null(v3);
        assert_true(isVertex(v3));

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        Vertex* v4 = it->active;
        assert_is_not_null(v4);
        assert_true(isVertex(v4));

        it = _glIteratorNext(it);
        assert_is_null(it);
    }

    void test_clipping_111() {
        /* All vertices are visible, list should be
         * totally unchanged */

        VertexBuilder builder;

        /* 2 triangle strips, with positive W coords */
        auto list = builder.
            add_header().
            add(1, 1, 2, 1).
            add(1, 0, 2, 1).
            add_last(0, 1, 2, 1).
            add_header().
            add(1, 1, 2, 1).
            add(1, 0, 2, 1).
            add_last(0, 1, 2, 1).done();

        ListIterator* it = _glIteratorBegin(list.first, list.second);
        assert_is_not_null(it);
        assert_is_header(it->active);

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        assert_vertex_equal(it->active, 1, 1, 2);

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        assert_vertex_equal(it->active, 1, 0, 2);

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        assert_vertex_equal(it->active, 0, 1, 2);

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        assert_is_header(it->active);

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        assert_vertex_equal(it->active, 1, 1, 2);

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        assert_vertex_equal(it->active, 1, 0, 2);

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        assert_vertex_equal(it->active, 0, 1, 2);

        it = _glIteratorNext(it);
        assert_is_null(it);
    }

    void test_clipping_101() {
        VertexBuilder builder;

        auto list = builder.
            add_header().
            add(1, 1, 2, 1).
            add(1, 0, 2, -1).
            add_last(0, 1, 2, 1).done();

        ListIterator* it = _glIteratorBegin(list.first, list.second);
        Vertex* v0 = it->active;
        assert_is_not_null(v0);
        assert_false(isVertex(v0)); // Should be a header

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        assert_vertex_equal(it->active, 1, 1, 2);

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        assert_vertex_equal(it->active, 0, 1, 2);

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        assert_vertex_equal(it->active, 1, 0.5f, 2);

    }

    void test_clipping_010() {

    }

    void test_clipping_001() {

    }

    void test_clipping_000() {
        /* If no vertices are visible, they should be culled
         * we (currently) leave headers as removing them before
         * submission is too costly */

        VertexBuilder builder;

        /* 2 triangle strips, with negative W coords */
        auto list = builder.
            add_header().
            add(1, 1, 2, -1).
            add(1, 0, 2, -1).
            add_last(0, 1, 2, -1).
            add_header().
            add(1, 1, 2, -1).
            add(1, 0, 2, -1).
            add_last(0, 1, 2, -1).done();

        ListIterator* it = _glIteratorBegin(list.first, list.second);
        assert_is_not_null(it);
        assert_is_header(it->active);

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        assert_is_header(it->active);

        // Done!
        it = _glIteratorNext(it);
        assert_is_null(it);
    }

    void test_clipping_011() {

    }

    void test_complex_strip() {
        VertexBuilder builder;

        auto list = builder.
            add_header().
            add(5, 0, -8, 8).
            add(2, 0, -4, 4).
            add(6, 0, -3, 3).
            add(4, 0, 5, -5).
            add(10, 0, 3, -3).
            add(11, 0, 5, -5).
            add(12, 0, 3, -3).
            add(17, 0, 5, -5).
            add(16, 0, -3, 3).
            add(19, 0, -2, 2).
            add_last(17, 0, -7, 7).done();

        ListIterator* it = _glIteratorBegin(list.first, list.second);
        assert_is_not_null(it);
        assert_is_header(it->active);

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        assert_vertex_equal(it->active, 5, 0, -8); // A
        assert_equal(it->active->flags, PVR_CMD_VERTEX);

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        assert_vertex_equal(it->active, 2, 0, -4);  // B
        assert_equal(it->active->flags, PVR_CMD_VERTEX);

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        assert_vertex_equal(it->active, 6, 0, -3);  // C
        assert_equal(it->active->flags, PVR_CMD_VERTEX);

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        assert_vertex_equal(it->active, 2.88888f, 0, 0); // BD
        assert_equal(it->active->flags, PVR_CMD_VERTEX);

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        assert_vertex_equal(it->active, 5.25f, 0, 0);  // CD
        assert_equal(it->active->flags, PVR_CMD_VERTEX_EOL);

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        assert_vertex_equal(it->active, 6, 0, -3); // C
        assert_equal(it->active->flags, PVR_CMD_VERTEX);

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        assert_vertex_equal(it->active, 5.25f, 0, 0); // CD
        assert_equal(it->active->flags, PVR_CMD_VERTEX);

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        assert_vertex_equal(it->active, 8.0f, 0, 0); // CE
        assert_equal(it->active->flags, PVR_CMD_VERTEX_EOL);

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        assert_vertex_equal(it->active, 14.0f, 0, 0);
        assert_equal(it->active->flags, PVR_CMD_VERTEX);

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        assert_vertex_equal(it->active, 16.375f, 0, 0);
        assert_equal(it->active->flags, PVR_CMD_VERTEX);

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        assert_vertex_equal(it->active, 16.0f, 0, -3); // 8
        assert_equal(it->active->flags, PVR_CMD_VERTEX_EOL);

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        assert_vertex_equal(it->active, 16.375f, 0, 0);
        assert_equal(it->active->flags, PVR_CMD_VERTEX);

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        assert_vertex_equal(it->active, 18.4286f, 0, 0);
        assert_equal(it->active->flags, PVR_CMD_VERTEX);

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        assert_vertex_equal(it->active, 16.0f, 0, -3);
        assert_equal(it->active->flags, PVR_CMD_VERTEX);

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        assert_vertex_equal(it->active, 19.0f, 0, -2);
        assert_equal(it->active->flags, PVR_CMD_VERTEX_EOL);

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        assert_vertex_equal(it->active, 16.0f, 0, -3);
        assert_equal(it->active->flags, PVR_CMD_VERTEX);

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        assert_vertex_equal(it->active, 19.0f, 0, -2);
        assert_equal(it->active->flags, PVR_CMD_VERTEX);

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        assert_vertex_equal(it->active, 17.0f, 0, -7);
        assert_equal(it->active->flags, PVR_CMD_VERTEX_EOL);

        it = _glIteratorNext(it);
        assert_is_null(it);
    }
};

}