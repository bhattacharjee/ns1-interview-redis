#ifndef RESP_OBJECT_
#define RESP_OBJECT_

#include "common_include.h"

typedef enum {
    RESP_INVALID = 0,
    RESP_STRING,
    RESP_ARRAY,
    RESP_INTEGER,
    RESP_BULK_STRING,
    RESP_ERROR
} resp_datatype_t;

typedef enum {
    ERROR_SUCCESS,
    ERROR_CURRENT_BEYOND_END,
    ERROR_INVALID_TYPE,
} resp_parse_error_t;

class RespParserState
{
public:
    char*           begin;
    char*           end;
    char*           current;
    int             parse_error;
};

class AbstractRespObject
{
public:
    /* Is this an aggregation or a simple data type */
    bool                    m_is_aggregate;

    /* Type of this object */
    resp_datatype_t         m_datatype;

    virtual std::string to_string() = 0;

    resp_datatype_t get_type() { return m_datatype; }
};

class RespString: public AbstractRespObject
{
public:
    /* The string value */
    std::string             m_value;
    
    RespString(const std::string& s) :
        m_value(s)
    {
        m_is_aggregate = false;
        m_datatype = RESP_STRING;
    }
    
    std::string to_string()
    {
        return m_value;
    }
};

class RespBulkString: public AbstractRespObject
{
public:
    /* The bulk string value */
    std::string             m_value;

    RespBulkString(const std::string& s):
        m_value(s)
    {
        m_is_aggregate = false;
        m_datatype = RESP_BULK_STRING;
    }
    
    std::string to_string()
    {
        return m_value;
    }   
};

class RespArray: public AbstractRespObject
{
public:
    /* The vector to store all the resp objects */
    std::vector<std::shared_ptr<AbstractRespObject> >    m_value;

    RespArray()
    {
        m_is_aggregate = true;
        m_datatype = RESP_ARRAY;
        m_value.reserve(4);
    }

    std::string to_string()
    {
        std::string result = "[";

        bool firstitem = true;
        for (auto pobj: m_value)
        {
            if (!firstitem)
                result += ", ";
            firstitem = false;
            result += pobj->to_string();
        }

        result += "]";

        return result;
    }
};

class RespParser
{
public:
    RespParserState                 state;
    std::string                     parse_string;

    RespParser(std::string parsestring)
    {
        parse_string        = parsestring;
        state.begin         = const_cast<char*>(parse_string.c_str());
        state.end           = state.begin + parse_string.length();
        state.current       = state.begin;
        state.parse_error   = 0;
    }

    std::tuple<resp_parse_error_t, int> get_length();

    std::tuple<resp_parse_error_t, int> get_type();

};

#endif /* #ifndef RESP_OBJECT_ */