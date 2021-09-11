#include <cstdlib>
#include <unistd.h>
#include <pthread.h>
#include "resp_parser.h"

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
    std::cout << std::endl << "Basic tests" << std::endl;

    {
        RespParser t1("+");
        auto [err, type] = t1.get_type();
        TEST(RESP_STRING == type, "+ should match type string");
        TEST(1 == t1.state.current - t1.state.begin, "Should move current by 1 position");
    }
    {
        RespParser t1("-");
        auto [err, type] = t1.get_type();
        TEST(RESP_ERROR == type, "- should match type error");
        TEST(1 == t1.state.current - t1.state.begin, "Should move current by 1 position");
    }
    {
        RespParser t1(":");
        auto [err, type] = t1.get_type();
        TEST(RESP_INTEGER == type, ": should match type integer");
        TEST(1 == t1.state.current - t1.state.begin, "Should move current by 1 position");
    }
    {
        RespParser t1("$");
        auto [err, type] = t1.get_type();
        TEST(RESP_BULK_STRING == type, "$ should match type bulk string");
        TEST(1 == t1.state.current - t1.state.begin, "Should move current by 1 position");
    }
    {
        RespParser t1("*");
        auto [err, type] = t1.get_type();
        TEST(RESP_ARRAY == type, "* should match type array");
        TEST(1 == t1.state.current - t1.state.begin, "Should move current by 1 position");
    }
}

int main(int argc, char** argv)
{
    basic_tests();
}