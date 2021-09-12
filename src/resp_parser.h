#ifndef RESP_OBJECT_
#define RESP_OBJECT_

#include "common_include.h"
#include <cassert>

/**
 * @brief Type of each RESP object
 * 
 */
typedef enum {
    RESP_INVALID = 0,
    RESP_STRING,
    RESP_ARRAY,
    RESP_INTEGER,
    RESP_BULK_STRING,
    RESP_ERROR
} resp_datatype_t;

/**
 * @brief Error values
 * 
 */
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

/**
 * @brief The state that is used by the parser
 * 
 */
class RespParserState
{
public:
    /**
     * @brief beginning of the input string
     * 
     */
    char*           begin;

    /**
     * @brief end of the input string
     * 
     */
    char*           end;

    /**
     * @brief current parse location in the string
     * 
     */
    char*           current;

    /**
     * @brief set in case of an error
     * 
     */
    int             parse_error;
};

/**
 * @brief abstract base class for all RESP objects
 * 
 */
class AbstractRespObject
{
public:
    /**
     * @brief Is this a simple datatype or an aggregate
     * 
     */
    bool                    m_is_aggregate;

    /**
     * @brief The type of the object
     * 
     */
    resp_datatype_t         m_datatype;

    /**
     * @brief Convert to a human readable string
     * 
     * @return std::string a string that is human readable
     */
    virtual std::string to_string() = 0;

    /**
     * @brief Serialize the object for storage
     * 
     * @return std::string serialized string
     */
    virtual std::string serialize() = 0;

    /**
     * @brief Get the type of the object
     * 
     * @return resp_datatype_t the type of the object
     */
    resp_datatype_t get_type() { return m_datatype; }
};

/**
 * @brief This class is to make objects of RESP integers
 * 
 */
class RespInteger: public AbstractRespObject
{
public:
    /**
     * @brief The integer value stored here
     * 
     */
    int                 m_value;

    RespInteger(int x)
    {
        m_datatype = RESP_INTEGER;
        m_is_aggregate = false;
        m_value = x;
    }

    /**
     * @brief convert to a human readable string
     * 
     * @return std::string a human readable string
     */
    std::string to_string()
    {
        std::stringstream ss;
        ss << m_value;
        return ss.str();
    }

    /**
     * @brief Serialize for storage
     * 
     * @return std::string a string that can be stored
     */
    std::string serialize()
    {
        std::stringstream ss;
        ss << ":" << m_value << "\r\n";
        return ss.str();
    }

    /**
     * @brief Set the value of the object
     * 
     * @param x the new value
     */
    void set_value(int x)
    {
        m_value = x;
    }
};

/**
 * @brief RESP simple string
 * 
 */
class RespString: public AbstractRespObject
{
public:
    /**
     * @brief the string value stored here
     * 
     */
    std::string             m_value;
    
    RespString(const std::string& s) :
        m_value(s)
    {
        m_is_aggregate = false;
        m_datatype = RESP_STRING;
    }
    
    /**
     * @brief convert to human readable form
     * 
     * @return std::string human readable string
     */
    std::string to_string()
    {
        return m_value;
    }

    /**
     * @brief serialize for storage
     * 
     * @return std::string serialized string
     */
    std::string serialize()
    {
        std::stringstream ss;
        ss << "+" << m_value << "\r\n";
        return ss.str();
    }
};

/**
 * @brief RESP Bulk String object
 * 
 */
class RespBulkString: public AbstractRespObject
{
public:
    /**
     * @brief The bulk string value
     * 
     */
    std::string             m_value;

    /**
     * @brief In case this is a null string, this will
     * indicate it so. This is needed for special handling
     * of bulk strings.
     * 
     */
    bool                    m_isnull;

    RespBulkString(const std::string& s):
        m_value(s)
    {
        m_is_aggregate = false;
        m_datatype = RESP_BULK_STRING;
        m_isnull = false;
    }
    
    /**
     * @brief Set whether the string is a null string or not
     * 
     * @param isnull 
     */
    void set_null(bool isnull)
    {
        m_isnull = isnull;
    }

    /**
     * @brief Convert to a human readable string
     * 
     * @return std::string human readable string
     */
    virtual std::string to_string()
    {
        return m_isnull ? "nil": m_value;
    }

    /**
     * @brief Serialize for storage or transmission
     * 
     * @return std::string serialized string
     */
    std::string serialize()
    {
        if (m_isnull)
            return "$-1\r\n";

        std::stringstream ss;
        ss << "$" << m_value.length() << "\r\n";
        ss << m_value << "\r\n";
        return ss.str();
    }
};

/**
 * @brief RESP Array object
 * 
 */
class RespArray: public AbstractRespObject
{
public:
    /**
     * @brief This is a vector which stores other RESP objects
     * that this object is an aggregation of
     * 
     */
    std::vector<std::shared_ptr<AbstractRespObject> >    m_value;

    RespArray()
    {
        m_is_aggregate = true;
        m_datatype = RESP_ARRAY;
        m_value.reserve(4);
    }

    /**
     * @brief convert to human readable string
     * 
     * @return std::string human readable string
     */
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

    /**
     * @brief Add a new RESP object to this collection
     * 
     * @param obj the RESP object to add
     * @return resp_parse_error_t ERROR_SUCCESS or
     * ERROR_NO_MEMORY
     */
    resp_parse_error_t append(
        std::shared_ptr<AbstractRespObject> obj)
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

    /**
     * @brief Serialize for transmission or storage
     * 
     * @return std::string serialized string
     */
    std::string serialize()
    {
        std::stringstream ss;

        ss << "*" << m_value.size() << "\r\n";
        for (auto p: m_value)
            ss << p->serialize();

        return ss.str();
    }

    /**
     * @brief Get the underlying vector of RESP objects
     * 
     * @return std::vector<std::shared_ptr<AbstractRespObject> > 
     * The underlying vector of RESP objects
     */
    std::vector<std::shared_ptr<AbstractRespObject> > get_array()
    {
        return m_value;
    }
};

/**
 * @brief RESP object for errors
 * 
 */
class RespError: AbstractRespObject
{
public:
    /**
     * @brief string to represent the error
     * 
     */
    std::string     m_value;

    RespError(std::string s)
    {
        m_is_aggregate = false;
        m_datatype = RESP_ERROR;
        m_value = s;
    }

    /**
     * @brief convert to a human readable string
     * 
     * @return std::string human readable string
     */
    std::string to_string()
    {
        return m_value;
    }

    /**
     * @brief serialize for transmission or storage
     * 
     * @return std::string serialized string
     */
    std::string serialize()
    {
        std::stringstream ss;
        ss << "-" << m_value << "\r\n";
        return ss.str();
    }
};

/**
 * @brief Parser that parses a string and produces a RESP object
 * 
 */
class RespParser
{
public:
    /**
     * @brief state of the parser, where it is currently in the input
     * input string
     * 
     */
    RespParserState                 m_state;

    /**
     * @brief the string to be parsed
     * 
     */
    std::string                     m_parse_string;

    RespParser(std::string parsestring)
    {
        m_parse_string        = parsestring;
        m_state.begin         = const_cast<char*>(m_parse_string.c_str());
        m_state.end           = m_state.begin + m_parse_string.length();
        m_state.current       = m_state.begin;
        m_state.parse_error   = 0;
    }

    /**
     * @brief Get the length of the curren token
     * 
     * @return std::tuple<resp_parse_error_t, int> tuple containing
     * 1. error, or success
     * 2. length
     */
    std::tuple<resp_parse_error_t, int> get_length();

    /**
     * @brief Type of object of the curren ttoken
     * 
     * @return std::tuple<resp_parse_error_t, int> tuple containing
     * 1. error, or success
     * 2. length
     */
    std::tuple<resp_parse_error_t, int> get_type();

    /**
     * @brief Skip the CR-LF
     * 
     * @return resp_parse_error_t error or success
     */
    resp_parse_error_t skip_crlf();

    /**
     * @brief parse the current token as a bulk string
     * 
     * @param stringlength the length of the string
     * @return std::tuple<resp_parse_error_t, std::string>
     * a tuple that contains
     * 1. error or success
     * 2. the string object
     */
    std::tuple<resp_parse_error_t, std::string>
        get_bulk_string_internal(int& stringlength);

    /**
     * @brief parse the current token as a bulk string
     * 
     * This is a wrapper function that first calculates the length
     * and the calls get_bulk_string_internal
     * 
     * @return std::tuple<resp_parse_error_t, std::shared_ptr<AbstractRespObject> > 
     * a tuple that contains
     * 1. error or success
     * 2. the string object
     */
    std::tuple<resp_parse_error_t, std::shared_ptr<AbstractRespObject> >
        get_bulk_string_object();

    /**
     * @brief Get the string internal object
     * TODO: This has not been implemented yet
     * 
     * @return std::tuple<resp_parse_error_t, std::string> 
     */
    std::tuple<resp_parse_error_t, std::string>
        get_string_internal();

    /**
     * @brief Get the string object object
     * TODO: This has not been implemented yet
     * 
     * @return std::tuple<resp_parse_error_t, std::shared_ptr<AbstractRespObject> > 
     */
    std::tuple<resp_parse_error_t, std::shared_ptr<AbstractRespObject> >
        get_string_object();

    /**
     * @brief parses the current token as an array object
     * It then recurses for all elements of the array
     * 
     * @return std::tuple<resp_parse_error_t, std::shared_ptr<AbstractRespObject> > 
     * A tuple containing
     * 1. error or success
     * 2. The array object
     */
    std::tuple<resp_parse_error_t, std::shared_ptr<AbstractRespObject> >
        get_array_object();

    /**
     * @brief Parse the current token regardless of type and return an object
     * 
     * @return std::tuple<resp_parse_error_t, std::shared_ptr<AbstractRespObject> > 
     * A tuple containing
     * 1. error or success
     * 2. The RESP object
     */
    std::tuple<resp_parse_error_t, std::shared_ptr<AbstractRespObject> >
        get_generic_object();

};

#endif /* #ifndef RESP_OBJECT_ */