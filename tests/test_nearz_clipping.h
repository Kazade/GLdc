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
        Vertex* v0 = it->it;
        assert_is_not_null(v0);
        assert_false(isVertex(v0)); // Should be a header

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        Vertex* v1 = it->it;
        assert_is_not_null(v1);
        assert_true(isVertex(v1));

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        Vertex* v2 = it->it;
        assert_is_not_null(v2);
        assert_true(isVertex(v2));

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        Vertex* v3 = it->it;
        assert_is_not_null(v3);
        assert_true(isVertex(v3));

        it = _glIteratorNext(it);
        assert_is_null(it);
    }

    void test_clipping_110() {

    }

    void test_clipping_111() {

    }

    void test_clipping_101() {

    }

    void test_clipping_010() {

    }

    void test_clipping_001() {

    }

    void test_clipping_000() {

    }

    void test_clipping_011() {

    }
};

}
