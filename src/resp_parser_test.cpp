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
        TEST(1 == t1.m_state.current - t1.m_state.begin, "Should move current by 1 position");
    }
    {
        RespParser t1("-");
        auto [err, type] = t1.get_type();
        TEST(RESP_ERROR == type, "- should match type error");
        TEST(1 == t1.m_state.current - t1.m_state.begin, "Should move current by 1 position");
    }
    {
        RespParser t1(":");
        auto [err, type] = t1.get_type();
        TEST(RESP_INTEGER == type, ": should match type integer");
        TEST(1 == t1.m_state.current - t1.m_state.begin, "Should move current by 1 position");
    }
    {
        RespParser t1("$");
        auto [err, type] = t1.get_type();
        TEST(RESP_BULK_STRING == type, "$ should match type bulk string");
        TEST(1 == t1.m_state.current - t1.m_state.begin, "Should move current by 1 position");
    }
    {
        RespParser t1("*");
        auto [err, type] = t1.get_type();
        TEST(RESP_ARRAY == type, "* should match type array");
        TEST(1 == t1.m_state.current - t1.m_state.begin, "Should move current by 1 position");
    }
}

void length_test()
{
    std::cout << std::endl << "Tests to validate the length";

    {
        RespParser t1("*2M");
        auto [err, type] = t1.get_type();
        TEST(RESP_ARRAY == type, "* should match type array");
        TEST(1 == t1.m_state.current - t1.m_state.begin, "Should move current by 1 position");
        auto [err2, length] = t1.get_length();
        TEST(ERROR_SUCCESS == err2 && 2 == length, "Length should be 2");
        TEST('M' == *t1.m_state.current, "current should move to character after length");
    }
    {
        RespParser t1("*22M");
        auto [err, type] = t1.get_type();
        TEST(RESP_ARRAY == type, "* should match type array");
        TEST(1 == t1.m_state.current - t1.m_state.begin, "Should move current by 1 position");
        auto [err2, length] = t1.get_length();
        TEST(ERROR_SUCCESS == err2 && 22 == length, "Length should be 22");
        TEST('M' == *t1.m_state.current, "current should move to character after length");
    }
    {
        RespParser t1("*-1M");
        auto [err, type] = t1.get_type();
        TEST(RESP_ARRAY == type, "* should match type array");
        TEST(1 == t1.m_state.current - t1.m_state.begin, "Should move current by 1 position");
        auto [err2, length] = t1.get_length();
        TEST(ERROR_SUCCESS == err2 && -1 == length, "Length should be -1");
        TEST('M' == *t1.m_state.current, "current should move to character after length");
    }
    {
        RespParser t1("*0M");
        auto [err, type] = t1.get_type();
        TEST(RESP_ARRAY == type, "* should match type array");
        TEST(1 == t1.m_state.current - t1.m_state.begin, "Should move current by 1 position");
        auto [err2, length] = t1.get_length();
        TEST(ERROR_SUCCESS == err2 && 0 == length, "Length should be 0");
        TEST('M' == *t1.m_state.current, "current should move to character after length");
    }
    {
        RespParser t1("*M");
        auto [err, type] = t1.get_type();
        TEST(RESP_ARRAY == type, "* should match type array");
        TEST(1 == t1.m_state.current - t1.m_state.begin, "Should move current by 1 position");
        auto [err2, length] = t1.get_length();
        TEST(ERROR_SUCCESS != err2, "Length not present, expect failure to parse");
   }
}

int main(int argc, char** argv)
{
    basic_tests();
    length_test();
}