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

/**
 * @brief This class stores the state associated with
 * each socket. As a socket is accepted, becomes
 * ready to read, is read, parsed, and then the response
 * is written to the client, it passes through different
 * worker queues. There must be a way to pass the state
 * through all the queues, and this class helps with that
 * 
 * An object of this class is also present in a map that
 * is available in the orchestrator object, and given an fd
 * it is always possible to lookup this structure.
 * 
 */
struct State
{
    /**
     * @brief at what stage is this socket now
     * 
     */
    StateState                              m_state;

    /**
     * @brief The data that was read from the socket
     * 
     */
    std::string                             m_read_data;

    /**
     * @brief parsed object from the data read from the
     * socket. This is the command (and not the response).
     * 
     */
    std::shared_ptr<AbstractRespObject>     m_object;

    /**
     * @brief 
     * 
     */
    std::shared_ptr<AbstractRespObject>     m_response;
    int                                     m_socket;
    mutable std::mutex                      m_mutex;

    // If this is set, then the socket must be closed
    // after writing
    bool                                    m_is_error;

    /**
     * @brief In case there is some unrecoverable error,
     * this will be used to communicate the error to the client.
     * This is really a backup mechanism.
     * 
     */
    char m_special_error[64];

    State(int fd)
    {
        m_object = std::shared_ptr<AbstractRespObject>(nullptr);
        m_response = std::shared_ptr<AbstractRespObject>(nullptr);
        m_state = STATE_INVALID;
        m_socket = fd;
        m_special_error[0] = 0;
        m_is_error = false;
    }

    /**
     * @brief Once a write has been completed, a new set of data
     * must be read.
     * This resets the state so that it can start again from a clean
     * slate.
     * 
     */
    void reset()
    {
        m_state = STATE_INVALID;
        m_read_data = std::string("");
        m_object = std::shared_ptr<AbstractRespObject>(nullptr);
        m_response = std::shared_ptr<AbstractRespObject>(nullptr);
        m_is_error = false;
        m_special_error[0] = 0;
        m_mutex.unlock();
    }

    /**
     * @brief Create a state object
     * 
     * @param fd the file descriptor for which to create a state
     * @return std::shared_ptr<State> the state
     */
    static std::shared_ptr<State>
    create_state(int fd)
    {
        auto p = new (std::nothrow) State(fd);
        return std::shared_ptr<State>(p);
    }

    /**
     * @brief Set the special error object.
     * This indicates that the error is unrecoverable and the
     * connection must be closed
     * 
     * @param err the string containing the error
     */
    void set_special_error(const char* err)
    {
        strncpy(m_special_error, err, 63);
        m_is_error = true;
    }

    /**
     * @brief Set the default special error object
     * This indicates that the error is unrecoverable and
     * the connection must be closed
     * 
     */
    void set_default_special_error()
    {
        set_special_error("-Unexpected Error\r\n");
    }
};

#endif /* #ifndef STATE_H_ */