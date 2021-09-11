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
    std::cout << std::endl << "Tests to validate the length" << std::endl;

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

void test_crlf()
{
    std::cout << std::endl << "Tests to validate the CRLF parsing" << std::endl;
    {
        RespParser t1("\r\nM");
        auto err = t1.skip_crlf();
        TEST(ERROR_SUCCESS == err, "CRLF should be found where it is available");
        TEST('M' == *t1.m_state.current, "current should be updated properly");
    }
    {
        RespParser t1("\rM");
        auto err = t1.skip_crlf();
        TEST(ERROR_SUCCESS != err, "CRLF should not be found where it is unavailable");
    }
}

void test_get_string()
{
    std::cout << std::endl << "Tests to validate string parsing" << std::endl;

    {
        RespParser t1("3\r\ndel\r\nM");
        auto [err, ret] = t1.get_bulk_string_internal();
        TEST(ERROR_SUCCESS == err, "Parsing a valid string should succeed");
        TEST(ret == std::string("del"), "The actual string should be retrieved");
        TEST('M' == *t1.m_state.current, "current should be updated properly");
    }
    {
        RespParser t1("2\r\ndel\r\nM");
        auto [err, ret] = t1.get_bulk_string_internal();
        TEST(ERROR_SUCCESS != err, "Parsing a invalid string should fail");
    }
    {
        RespParser t1("4\r\ndel\r\nM");
        auto [err, ret] = t1.get_bulk_string_internal();
        TEST(ERROR_SUCCESS != err, "Parsing a invalid string should fail");
    }
    {
        RespParser t1("0\r\ndel\r\nM");
        auto [err, ret] = t1.get_bulk_string_internal();
        TEST(ERROR_SUCCESS != err, "Parsing a invalid string should fail");
    }
    {
        RespParser t1("0\r\n\r\nM");
        auto [err, ret] = t1.get_bulk_string_internal();
        TEST(ERROR_SUCCESS == err, "Parsing a valid string should succeed");
        TEST(ret == std::string(""), "The actual empty string should be retrieved");
        TEST('M' == *t1.m_state.current, "current should be updated properly");
    }
    {
        RespParser t1("-1\r\nM");
        auto [err, ret] = t1.get_bulk_string_internal();
        TEST(ERROR_SUCCESS == err, "Parsing a valid string should succeed");
        TEST(ret == std::string(""), "The actual empty string should be retrieved");
        TEST('M' == *t1.m_state.current, "current should be updated properly");
    }
    {
        RespParser t1("3\r\ndel\r\nM");
        auto [err, ret] = t1.get_bulk_string_object();
        TEST(ERROR_SUCCESS == err, "OBJ: Parsing a valid string should succeed");
        TEST(ret, "OBJ: on success, the object should be returned");
        TEST(ret->to_string() == std::string("del"), "OBJ: The actual string should be retrieved");
        TEST('M' == *t1.m_state.current, "OBJ: current should be updated properly");
    }
}

void test_array()
{
    std::cout << std::endl << "Tests to validate array parsing" << std::endl;

    {
        RespParser t1("3\r\n$3\r\nset\r\n$1\r\nx\r\n$1\r\n1\r\nM");
        auto [err, ret] = t1.get_array_object();
        TEST(ERROR_SUCCESS == err, "Valid array should be parsed");
        TEST(ret->to_string() == std::string("[set, x, 1]"), "Correct array should be returned");
        TEST('M' == *t1.m_state.current, "current object should be properly updated.");
    }
}

void test_generic()
{
    std::cout << std::endl << "Tests to validate a generic data type" << std::endl;

    {
        RespParser t1("*3\r\n$3\r\nset\r\n$1\r\nx\r\n$1\r\n1\r\nM");
        auto [err, ret] = t1.get_generic_object();
        TEST(ERROR_SUCCESS == err, "Valid object should be parsed");
        TEST(ret->to_string() == std::string("[set, x, 1]"), "Correct array should be returned");
        TEST('M' == *t1.m_state.current, "current object should be properly updated.");
    }
}

int main(int argc, char** argv)
{
    basic_tests();
    length_test();
    test_crlf();
    test_get_string();
    test_array();
    test_generic();
}