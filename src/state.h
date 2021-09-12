#ifndef STATE_H_
#define STATE_H_

#include "common_include.h"
#include "resp_parser.h"

typedef enum
{
    STATE_INVALID,
    STATE_ACCEPTED,
    STATE_WAITING_FOR_EPOLL,
    STATE_WAITING_FOR_READ_JOB,
    STATE_IN_READ_LOOP,
    STATE_PARSING,
    STATE_IN_WRITE_LOOP,
    STATE_CLOSING
} StateState;

struct State
{
    StateState                              m_state;
    std::string                             m_read_data;
    std::shared_ptr<AbstractRespObject>     m_resp_object;
    int                                     m_socket;
    mutable std::mutex                      m_mutex;

    State(int fd)
    {
        m_resp_object = std::shared_ptr<AbstractRespObject>(nullptr);
        m_state = STATE_INVALID;
        m_socket = fd;
    }

    static std::shared_ptr<State>
    create_state(int fd)
    {
        auto p = new (std::nothrow) State(fd);
        return std::shared_ptr<State>(p);
    }
};

#endif /* #ifndef STATE_H_ */