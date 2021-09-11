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