#include "resp_parser.h"

std::tuple<resp_parse_error_t, int>
RespParser::get_type()
{
    resp_datatype_t         type    = RESP_INVALID;
    resp_parse_error_t      err     = ERROR_SUCCESS;

    if (m_state.current >= m_state.end)
        return std::make_tuple(ERROR_CURRENT_BEYOND_END, RESP_INVALID);
    
    switch (*m_state.current)
    {
        case '+':
            type = RESP_STRING;
            break;
        case '-':
            type = RESP_ERROR;
            break;
        case ':':
            type = RESP_INTEGER;
            break;
        case '$':
            type = RESP_BULK_STRING;
            break;
        case '*':
            type = RESP_ARRAY;
            break;
        default:
            type = RESP_INVALID;
            err = ERROR_INVALID_TYPE;
    }

    if (RESP_INVALID != type && ERROR_SUCCESS == err)
        m_state.current++;

    return std::make_tuple(err, type);
}

std::tuple<resp_parse_error_t, int>
RespParser::get_length()
{
    char*           endptr          = nullptr;
    long int        thenum          = 0;

    if (m_state.current >= m_state.end)
        return std::make_tuple(ERROR_CURRENT_BEYOND_END, 0);
        
    thenum = strtol(m_state.current, &endptr, 10);

    if (thenum == 0 && (endptr == m_state.current || \
            (*m_state.current && endptr && *endptr == 0)))
        return std::make_tuple(ERROR_INVALID_NUMBER, 0);

    m_state.current = endptr;
    return std::make_tuple(ERROR_SUCCESS, (int)thenum);
}

resp_parse_error_t
RespParser::skip_crlf()
{
    char* current = m_state.current;
    if (current >= m_state.end)
        return ERROR_CURRENT_BEYOND_END;
    
    if ('\r' != *current++)
        return ERROR_CRLF_MISSING;

    if (current >= m_state.end)
        return ERROR_CURRENT_BEYOND_END;

    if ('\n' != *current++)
        return ERROR_CRLF_MISSING;

    m_state.current = current;

    return ERROR_SUCCESS;
}

std::tuple<resp_parse_error_t, std::string>
RespParser::get_bulk_string_internal(int& stringlength)
{
    std::string retval;

    auto [err2, length] = get_length();
    if (ERROR_SUCCESS != err2)
        return std::make_tuple(err2, retval);
    
    stringlength = length;
    
    auto err = skip_crlf();
    if (ERROR_SUCCESS != err)
    {
        return std::make_tuple(err, retval);
    }

    // "$-1\r\n" is interpreted as NULL string, we are just going
    // to treat it as an empty string for now
    if (length < 0)
        return std::make_tuple(ERROR_SUCCESS, retval);

    // "$0\r\n\r\n" is treated as an empty string
    if (!length)
        return std::make_tuple(skip_crlf(), retval);

    char* current = m_state.current;
    char* save_current = current;

    for (int i = 0; i < length; i++, current++)
    {
        if (current >= m_state.end)
            return std::make_tuple(ERROR_CURRENT_BEYOND_END, retval);
        if ('\r' == *current || '\n' == *current)
            return std::make_tuple(ERROR_STRING_CONTAINS_CRLF, retval);
    }

    m_state.current = current;
    err = skip_crlf();
    if (ERROR_SUCCESS != err)
    {
        m_state.current = save_current;
        return make_tuple(err, retval);
    }

    // Overwrite the string temporarily here to
    // optimize duplication
    auto savechar = *current;
    *current = 0;
    retval = save_current;
    *current = savechar;

    return std::make_tuple(ERROR_SUCCESS, retval);
}

std::tuple<resp_parse_error_t, std::shared_ptr<AbstractRespObject> >
RespParser::get_bulk_string_object()
{
    int stringlength = 0;

    resp_parse_error_t error = ERROR_SUCCESS;
    auto [err, thestring] = get_bulk_string_internal(stringlength);
    if (ERROR_SUCCESS != err)
    {
        return std::make_tuple(
            err,
            std::shared_ptr<AbstractRespObject>(nullptr)
        );
    }

    RespBulkString* p = new (std::nothrow) RespBulkString(thestring);
    if (!p)
        error = ERROR_NO_MEMORY;
    
    if (stringlength < 0)
        p->set_null(true);
        
    return std::make_tuple(
        error,
        std::shared_ptr<AbstractRespObject>(p)
    );
}

std::tuple<resp_parse_error_t, std::shared_ptr<AbstractRespObject> >
RespParser::get_array_object()
{

    auto [err, length] = get_length();
    if (ERROR_SUCCESS != err)
        return std::make_tuple(
            ERROR_INVALID_ARRAY_LENGTH,
            std::shared_ptr<AbstractRespObject>(nullptr));

    auto arrp = new (std::nothrow) RespArray();
    if (!arrp)
        return std::make_tuple(
            ERROR_NO_MEMORY,
            std::shared_ptr<AbstractRespObject>(nullptr));
    
    // Empty array
    if (length < 0)
        return std::make_tuple(
            skip_crlf(),
            std::shared_ptr<AbstractRespObject>(arrp));
    
    err = skip_crlf();
    if (ERROR_SUCCESS != err)
        return std::make_tuple(
            err,
            std::shared_ptr<AbstractRespObject>(nullptr));

    for (int i = 0; i < length; i++)
    {
        auto [err, type] = get_type();
        if (ERROR_SUCCESS != err || RESP_INVALID == type)
        {
            return std::make_tuple(
                err,
                std::shared_ptr<AbstractRespObject>(nullptr)
            );
        }

        switch(type)
        {
            case RESP_BULK_STRING:
                {
                    auto [err, obj1] = get_bulk_string_object();
                    if (ERROR_SUCCESS != err)
                        return std::make_tuple(
                            err,
                            std::shared_ptr<AbstractRespObject>(nullptr)
                        );
                    arrp->append(obj1);
                }
                break;
            case RESP_ARRAY:
                {
                    auto [err, obj] = get_array_object();
                    if (ERROR_SUCCESS != err)
                        return std::make_tuple(
                            err,
                            std::shared_ptr<AbstractRespObject>(nullptr)
                        );
                    arrp->append(obj);
                }
                break;
            default:
                {
                    std::cerr << "Type " << type << " Not implemented. "\
                            << std::endl;
                    return std::make_tuple(
                        ERROR_NOT_IMPLEMENTED,
                        std::shared_ptr<AbstractRespObject>(nullptr)
                    );
                }
        }
    }

    return std::make_tuple(
        ERROR_SUCCESS, 
        std::shared_ptr<AbstractRespObject>(arrp));
}

std::tuple<resp_parse_error_t, std::shared_ptr<AbstractRespObject> >
RespParser::get_generic_object()
{
    auto [err, type] = get_type();
    if (ERROR_SUCCESS != err || RESP_INVALID == type)
    {
        return std::make_tuple(
            err,
            std::shared_ptr<AbstractRespObject>(nullptr)
        );
    }

    switch(type)
    {
        case RESP_BULK_STRING:
            return get_bulk_string_object();
            break;
        case RESP_ARRAY:
            return get_array_object();
            break;
        default:
            {
                std::cerr << "Type " << type << " Not implemented. "\
                        << std::endl;
                return std::make_tuple(
                    ERROR_NOT_IMPLEMENTED,
                    std::shared_ptr<AbstractRespObject>(nullptr)
                );
            }
    }
}
