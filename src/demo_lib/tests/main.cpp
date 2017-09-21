
#include <math/math.hpp>

void test_components();
void test_make_cnt();
void test_transactions();

int main() {
    math::init();

    test_components();
    test_make_cnt();
    test_transactions();
}