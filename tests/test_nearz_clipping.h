#pragma once

#include "../utils/test.h"


namespace {

class NearZClippingTests : public gldc::test::GLdcTestCase {
public:
    void test_failure() {
        assert_false(true);
    }
};

}
