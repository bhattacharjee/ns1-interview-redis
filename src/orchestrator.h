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
#include <fcntl.h>

#define NUM_DATASTORES 10
#define PORTNUM 6379
#define MAX_EPOLL_EVENTS 10

class Orchestrator;
class SocketReadJob;
class ParseAndRunJob;

/**
 * @brief enum defines the different types of commands
 * 
 * Commands can be get, set or del
 * 
 */
typedef enum
{
    COMMAND_INVALID,
    COMMAND_GET,
    COMMAND_DEL,
    COMMAND_SET
} command_type_t;

/**
 * @brief Job to read from a socket
 * 
 * The responsibility of this class is to read
 * from a ready socket, and then pass this on to the
 * thread-pool that schedules jobs that parse and do
 * the actual work.
 * 
 */
class SocketReadJob: public JobInterface
{
public:
    /**
     * @brief the state associated with the socket
     * 
     */
    std::shared_ptr<State>      m_pstate;

    /**
     * @brief the orchestrator object
     * 
     */
    Orchestrator*               m_porchestrator;

    SocketReadJob(
        Orchestrator*           porch,
        std::shared_ptr<State>  pstate)
    {
        m_porchestrator = porch;
        m_pstate = pstate;
    }

    /**
     * @brief Reads from a socket and stores the values the state
     * 
     * It runs as a part of the read worker pool. When done,
     * it then invokes the parser worker pool.
     * 
     * @return int on success it returns 0, otherwise a number
     * to indicate the error
     */
    int run();
};

/**
 * @brief job to parse the string that is already been
 * read, and then take the appropriate actions.
 * 
 * This class runs in a worker pool
 * 
 */
class ParseAndRunJob: public JobInterface
{
public:
    /**
     * @brief The state associated with the socket
     * 
     */
    std::shared_ptr<State>      m_pstate;

    /**
     * @brief Pointer to the orchestrator object
     * 
     */
    Orchestrator*               m_porchestrator;

    ParseAndRunJob(
        Orchestrator*           porch,
        std::shared_ptr<State>  pstate)
    {
        m_porchestrator = porch;
        m_pstate = pstate;
    }
    /**
     * @brief The job in the work-queue which parses the input
     * that is received from the user, and then performs the
     * appropriate action
     * 
     * @return int 0 on success
     */
    int run();
};

/**
 * @brief class to send the response back to the client
 * It runs in a worker queue
 * 
 */
class SocketWriteJob: public JobInterface
{
public:
    /**
     * @brief state associated with the socket
     * 
     */
    std::shared_ptr<State>      m_pstate;

    /**
     * @brief pointer to the orchestrator object
     * 
     */
    Orchestrator*               m_porchestrator;

    SocketWriteJob(
        Orchestrator*           porch,
        std::shared_ptr<State>  pstate)
    {
        m_porchestrator = porch;
        m_pstate = pstate;
    }

    /**
     * @brief the job for the work-queue which actually sends
     * the response back to the client
     * 
     * @return int 0 on success
     */
    int run();
};

/**
 * @brief This class orchestrates the entire server
 * 
 * This class tries to maximize parallelism
 * 
 * There are three different thread-pools, which start off
 * with 8 threads in each. Threads can be added to the thread
 * pool later, if the load increases, but cannot be removed.
 * 
 * The three different thread pools perform the following functions:
 * 1. A thread pool to read input from a connection when it is ready
 * 2. A thread pool to parse the data read from the client and take
 *    action on it
 * 3. A thread pool to send the response to the client.
 * 
 * In addition to this, there are two other threads:
 * 1. A thread that constantly listens on the socket, and accepts
 *    connections.
 * 2. A thread that runs epoll() on all accepted sockets to see
 *    which sockets are ready for reading.
 * 
 */
class Orchestrator
{
    /**
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
    /**
     * @brief The server socket on which the thread is listening
     * for new incoming connections
     * 
     */
    int                                             m_server_socket;

    /**
     * @brief A map of all valid sockets, to its associated state
     * This state gets passed to all worker threads, but
     * this is the master table in case we need it.
     * 
     */
    std::unordered_map<int, std::shared_ptr<State>> m_all_sockets;

    /**
     * @brief shared mutex to synchronize m_all_sockets
     * 
     */
    std::shared_mutex                               m_all_sockets_mtx;

    /**
     * @brief Thread pool to schedule jobs to read from the socket
     * 
     */
    ThreadPool*                                     m_read_threadpool;

    /**
     * @brief Sockets which are currently not ready and are being
     * monitored if they become ready at any point of time
     * 
     */
    std::unordered_set<int>                         m_epoll_sockets;

    /**
     * @brief lock to synchronize m_epoll_sockets
     * 
     */
    std::shared_mutex                               m_epoll_sockets_mtx;

    /**
     * @brief Thread pool to schedule jobs to parse the data
     * already read from the socket and then take the appropriate action
     * 
     */
    ThreadPool*                                     m_processing_threadpool;
 
    /**
     * @brief Not really used
     * 
     */
    std::unordered_set<int>                         m_processing_sockets;
 
    /**
     * @brief Not really used
     * 
     */
    std::shared_mutex                               m_processing_sockets_mtx;

    /**
     * @brief The thread pool to parse and run all jobs
     * 
     */
    ThreadPool*                                     m_parse_and_run_threadpool;

    /**
     * @brief Thread pool to send teh response back to the client
     * 
     */
    ThreadPool*                                     m_write_threadpool;

    /**
     * @brief Not really used
     * 
     */
    std::unordered_set<int>                         m_write_sockets;

    /**
     * @brief Not really used
     * 
     */
    std::shared_mutex                               m_write_sockets_mtx;

    /**
     * @brief Datastores are the hash tables, we use 10 hash-tables
     * partitioned by the first character for greater parallelism
     * 
     */
    DataStore                                       m_datastore[NUM_DATASTORES];

    /**
     * @brief In case the server is asked to shut down, all threads
     * should look at this.
     * 
     * Currently unimplemented.
     * 
     */
    bool                                            m_is_destroying;

    /**
     * @brief The thread id that listens for new connections and accepts
     * them.
     * 
     */
    pthread_t                                       m_accepting_thread_id;

    /**
     * @brief The thread id that executes epoll to look for connections
     * that are ready to read
     * 
     */
    pthread_t                                       m_epoll_thread_id;

    /**
     * @brief File descriptor for epoll
     * 
     */
    int                                             m_epoll_fd;

    Orchestrator():
        m_server_socket(-1),
        m_epoll_fd(-1)
    {
        ThreadPoolFactory tfp;
        m_read_threadpool = tfp.create_thread_pool(8, false);
        m_processing_threadpool = tfp.create_thread_pool(8, false);
        m_write_threadpool = tfp.create_thread_pool(8, false);
        m_parse_and_run_threadpool = tfp.create_thread_pool(8, false);
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
        if (m_parse_and_run_threadpool)
            m_parse_and_run_threadpool->destroy();
        delete m_read_threadpool;
        delete m_processing_threadpool;
        delete m_write_threadpool;
        delete m_parse_and_run_threadpool;
    }

    /**
     * @brief create a server socket to listen on
     * 
     */
    void create_server_socket();

    /**
     * @brief spawn a thread that loops to accpept new connections
     * 
     * @return true on successful launch
     * @return false on unsuccessful launch
     */   
    bool spawn_accepting_thread();

    /**
     * @brief loop and accept connections on the listening socket
     * Once a connection is received, it then adds it to the queue
     * which the epoll thread uses, and wakes up the epoll thread.
     * 
     */
    void accepting_thread_loop();

    /**
     * @brief spawn the thread that calls epoll for ready sockets
     * 
     * @return true on successful launch
     * @return false on failure to launch
     */
    bool spawn_epoll_thread();

    /**
     * @brief Loop and call epoll, and send ready sockets to workers
     * 
     * Loops and calls epoll. When a ready socket is found, it is
     * posted to the thread pool which processes it.
     * 
     */
    void epoll_thread_loop();

    /**
     * @brief Wake up the epoll thread by sending it a signal
     * 
     */  
    void wakeup_epoll_thread();

    /**
     * @brief creates the file descriptor on which epoll is run
     * 
     * @return true on success
     * @return false on failure
     */   
    bool create_epoll_fd();
    void remove_socket(int fd);
    void epoll_rearm();
    void epoll_rearm_unsafe();
    void epoll_empty();
    void epoll_empty_unsafe();
    void create_processing_job(int fd);
    bool add_to_parse_and_run_queue(std::shared_ptr<State> pstate);
    bool add_to_write_queue(std::shared_ptr<State> pstate);


    std::tuple<bool, std::shared_ptr<AbstractRespObject> >
        do_operation(std::shared_ptr<AbstractRespObject> command);
    
    std::tuple<bool, std::shared_ptr<AbstractRespObject> >
        do_get(std::shared_ptr<AbstractRespObject> p);

    std::tuple<bool, std::shared_ptr<AbstractRespObject> >
        do_set(std::shared_ptr<AbstractRespObject> p);

    std::tuple<bool, std::shared_ptr<AbstractRespObject> >
        do_del(std::shared_ptr<AbstractRespObject> p);

    bool do_del_internal(std::shared_ptr<AbstractRespObject> pobj);


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


    std::tuple<bool, command_type_t>
        is_valid_command(std::shared_ptr<AbstractRespObject> p);
    
    void add_to_epoll_queue(int fd);

    int get_partition(const std::string& s);
};

#endif /* #ifndef ORCHESTRATOR_H_ */