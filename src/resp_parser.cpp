#include "resp_parser.h"

std::tuple<resp_parse_error_t, int> RespParser::get_type()
{
    resp_datatype_t         type    = RESP_INVALID;
    resp_parse_error_t      err     = ERROR_SUCCESS;

    if (state.current >= state.end)
        return std::make_tuple(ERROR_CURRENT_BEYOND_END, RESP_INVALID);
    
    switch (*state.current)
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
        state.current++;

    return std::make_tuple(err, type);
}

