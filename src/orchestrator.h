#ifndef ORCHESTRATOR_H_
#define ORCHESTRATOR_H_

#include "common_include.h"
#include "thread_pool.h"
#include "resp_parser.h"
#include "data_store.h"
#include "state.h"

#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <signal.h>
#include <sys/epoll.h>

#define NUM_DATASTORES 10
#define PORTNUM 6379
#define MAX_EPOLL_EVENTS 10

class Orchestrator
{
    /*
     * TODO: Define a lock heirarchy.
     * Need to spend more time on it to figure out if this is the
     * most logical heirarchy or something else makes more sense
     * Mostly locks should not be held simultaneously.
     * But in case they need to, this should be the order
     * 
     * 1. m_all_sockets_mtx
     * 2. State.m_mutex
     * 3. m_epoll_sockets_mtx
     * 4. m_write_sockets_mtx
     * 5. m_write_sockets_mtx
     */
public:
    int                                             m_server_socket;

    std::unordered_map<int, std::shared_ptr<State>> m_all_sockets;
    std::shared_mutex                               m_all_sockets_mtx;

    ThreadPool*                                     m_read_threadpool;
    std::unordered_set<int>                         m_epoll_sockets;
    std::shared_mutex                               m_epoll_sockets_mtx;

    ThreadPool*                                     m_processing_threadpool;
    std::unordered_set<int>                         m_processing_sockets;
    std::shared_mutex                               m_processing_sockets_mtx;

    ThreadPool*                                     m_write_threadpool;
    std::unordered_set<int>                         m_write_sockets;
    std::shared_mutex                               m_write_sockets_mtx;

    DataStore                                       m_datastore[NUM_DATASTORES];

    bool                                            m_is_destroying;
    pthread_t                                       m_accepting_thread_id;
    pthread_t                                       m_epoll_thread_id;

    int                                             m_epoll_fd;

    Orchestrator():
        m_server_socket(-1),
        m_epoll_fd(-1)
    {
        ThreadPoolFactory tfp;
        m_read_threadpool = tfp.create_thread_pool(8, false);
        m_processing_threadpool = tfp.create_thread_pool(8, false);
        m_write_threadpool = tfp.create_thread_pool(8, false);
        m_is_destroying = false;
    }

    ~Orchestrator()
    {
        /*
         * It is important to first call destroy before deleting it
         * Otherwise, it might lead to threads working on deleted objects
         */
        if (m_read_threadpool)
            m_read_threadpool->destroy();
        if (m_processing_threadpool)
            m_processing_threadpool->destroy();
        if (m_write_threadpool)
            m_write_threadpool->destroy();
        delete m_read_threadpool;
        delete m_processing_threadpool;
        delete m_write_threadpool;
    }

    void create_server_socket();
    bool spawn_accepting_thread();
    void accepting_thread_loop();
    bool spawn_epoll_thread();
    void epoll_thread_loop();
    void wakeup_epoll_thread();
    bool create_epoll_fd();
    void remove_socket(int fd);
    void epoll_rearm();
    void epoll_rearm_unsafe();
    void epoll_empty();
    void epoll_empty_unsafe();
    void create_processing_job(int fd);

    static void* accepting_thread_pthread_fn(void* arg)
    {
        Orchestrator* ptr = static_cast<Orchestrator*>(arg);
        ptr->accepting_thread_loop();
        return nullptr;
    }

    static void* epoll_thread_pthread_fn(void * arg)
    {
        Orchestrator* ptr = static_cast<Orchestrator*>(arg);
        ptr->epoll_thread_loop();
        return nullptr;
    }
};

#endif /* #ifndef ORCHESTRATOR_H_ */