#include "thread_pool.h"
#include <cstdlib>

#define TEST(x, y) {\
    if (!(x))\
    {\
        std::cout << y << std::endl;\
        exit(1);\
    }\
}

int main(int argc, char** argv)
{
    TEST(true, "should be true");
}