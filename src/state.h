#ifndef STATE_H_
#define STATE_H_

#include "common_include.h"
#include "resp_parser.h"
#include <cstring>

typedef enum
{
    STATE_INVALID,
    STATE_ACCEPTED,
    STATE_WAITING_FOR_EPOLL,
    STATE_WAITING_FOR_READ_JOB,
    STATE_IN_READ_LOOP,
    STATE_WAITING_FOR_PARSING,
    STATE_PARSING,
    STATE_IN_WRITE_LOOP,
    STATE_CLOSING
} StateState;

struct State
{
    StateState                              m_state;
    std::string                             m_read_data;
    std::shared_ptr<AbstractRespObject>     m_resp_object;
    std::shared_ptr<AbstractRespObject>     m_response;
    int                                     m_socket;
    mutable std::mutex                      m_mutex;

    // In case of some unrecoverable error, this will
    // be used to communicate the error to the client
    char m_special_error[64];

    State(int fd)
    {
        m_resp_object = std::shared_ptr<AbstractRespObject>(nullptr);
        m_response = std::shared_ptr<AbstractRespObject>(nullptr);
        m_state = STATE_INVALID;
        m_socket = fd;
        m_special_error[0] = 0;
    }

    static std::shared_ptr<State>
    create_state(int fd)
    {
        auto p = new (std::nothrow) State(fd);
        return std::shared_ptr<State>(p);
    }

    void set_special_error(const char* err)
    {
        strncpy(m_special_error, err, 63);
    }

    void set_default_special_error()
    {
        set_special_error("-Unexpected Error\r\n");
    }
};

#endif /* #ifndef STATE_H_ */