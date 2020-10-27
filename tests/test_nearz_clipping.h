#pragma once

#include "../utils/test.h"
#include "../GL/flush.h"

namespace {

struct VertexBuilder {
    VertexBuilder& add_header() {
        Vertex v;
        v.flags = 100; // I dunno what this bit of memory would be
        list_.push_back(std::move(v));
        return *this;
    }

    VertexBuilder& add(float x, float y, float z, float w) {
        Vertex v;
        v.xyz[0] = x;
        v.xyz[1] = y;
        v.xyz[2] = z;
        v.w = w;
        return *this;
    }

    std::vector<Vertex> done() {
        return list_;
    }

private:
    std::vector<Vertex> list_;
};

class NearZClippingTests : public gldc::test::GLdcTestCase {
public:
    void test_clipping_100() {
        /* When only the first vertex is visible, we still
         * end up with 4 elements (3 vertices + 1 header). The
         * first vertex should be the same */

        VertexBuilder builder;

        std::vector<Vertex> list = builder.
            add_header().
            add(1, 1, 2, 1).
            add(1, 0, 2, -1).
            add(0, 1, 2, -1).done();

        ListIterator* it = _glIteratorBegin(&list[0], list.size());
        Vertex* v0 = it->current;
        assert_is_not_null(v0);
        assert_false(isVertex(v0)); // Should be a header

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        Vertex* v1 = it->current;
        assert_is_not_null(v1);

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        Vertex* v2 = it->current;
        assert_is_not_null(v2);

        it = _glIteratorNext(it);
        assert_is_not_null(it);
        Vertex* v3 = it->current;
        assert_is_not_null(v3);

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
