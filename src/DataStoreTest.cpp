#include <cstdlib>
#include <unistd.h>
#include <pthread.h>
#include "DataStore.h"

#define TEST(x, y) {\
    if (!(x))\
    {\
        std::cout << "FAILED: " << y << std::endl;\
        exit(1);\
    }\
    else\
    {\
        std::cout << "PASSED: " << y << std::endl;\
    }\
}

void basic_tests()
{
    std::cout << std::endl << "Runnin basic tests " << std::endl;

    {
        DataStore m;
        auto r = m.set("foo", "bar");
        TEST(r, "Should be able to set value");

        {
            auto [succ, readValue] = m.get("foo");
            TEST(succ, "reading existing value should succeed");
            TEST(readValue == std::string("bar"), "Should read the correct set value");
        }
        
        auto e = m.del("foo");
        TEST(e, "Should be able to delete existing value");
        e = m.del("foo");
        TEST(!e, "deleting non-existent value should fail");

        {
            auto [succ, readValue] = m.get("foo");
            TEST(false == succ, "reading non-existing value should fail");
        }
        
    }
}

int main(int argc, char** argv)
{
    basic_tests();

    std::cout << std::endl << "All tests passed" << std::endl;
}