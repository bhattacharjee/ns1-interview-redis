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
    ERROR_INVALID_NUMBER,
    ERROR_CRLF_MISSING,
    ERROR_STRING_CONTAINS_CRLF,
    ERROR_NO_MEMORY,
    ERROR_INVALID_ARRAY_LENGTH,
    ERROR_NOT_IMPLEMENTED,
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

    RespBulkString(std::string s):
        m_value(s)
    {
        m_is_aggregate = false;
        m_datatype = RESP_BULK_STRING;
    }
    
    virtual std::string to_string()
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

    resp_parse_error_t append(std::shared_ptr<AbstractRespObject> obj)
    {
        try
        {
            m_value.push_back(obj);
        }
        catch(...)
        {
            return ERROR_NO_MEMORY;
        }
        
        return ERROR_SUCCESS;
    }
};

class RespParser
{
public:
    RespParserState                 m_state;
    std::string                     m_parse_string;

    RespParser(std::string parsestring)
    {
        m_parse_string        = parsestring;
        m_state.begin         = const_cast<char*>(m_parse_string.c_str());
        m_state.end           = m_state.begin + m_parse_string.length();
        m_state.current       = m_state.begin;
        m_state.parse_error   = 0;
    }

    std::tuple<resp_parse_error_t, int> get_length();

    std::tuple<resp_parse_error_t, int> get_type();

    resp_parse_error_t skip_crlf();


    std::tuple<resp_parse_error_t, std::string>
        get_bulk_string_internal();

    std::tuple<resp_parse_error_t, std::shared_ptr<AbstractRespObject> >
        get_bulk_string_object();

    // TODO: Implement this
    std::tuple<resp_parse_error_t, std::string>
        get_string_internal();

    // TODO: Implement this
    std::tuple<resp_parse_error_t, std::shared_ptr<AbstractRespObject> >
        get_string_object();

    std::tuple<resp_parse_error_t, std::shared_ptr<AbstractRespObject> >
        get_array_object();

};

#endif /* #ifndef RESP_OBJECT_ */