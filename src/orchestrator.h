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
    /**
     * @brief invalid command
     * 
     */
    COMMAND_INVALID,
    /**
     * @brief get command
     * 
     */
    COMMAND_GET,
    /**
     * @brief del command
     * 
     */
    COMMAND_DEL,
    /**
     * @brief set command
     * 
     */
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

    /**
     * @brief removes a socket from all queues and frees
     * any data structure associated with the file descriptor
     * 
     * @param fd the file descriptor to remove
     */
    void remove_socket(int fd);

    /**
     * @brief rebuild the epoll monitor from the set of fd's
     * 
     */
    void epoll_rearm();

    /**
     * @brief rebuild the epoll monitor from the set of fd's
     * 
     * This is the unsafe version of the function. the caller
     * must hold the mutex before calling it.
     * 
     */
    void epoll_rearm_unsafe();

    /**
     * @brief remove all monitoring file descriptors from epoll
     * 
     */
    void epoll_empty();

    /**
     * @brief remove all monitoring file descriptors from epoll
     * This is the unsafe version of the function, and the caller
     * must hold the m_epoll_sockets_mtx lock before calling it
     */
    void epoll_empty_unsafe();

    /**
     * @brief creates a processing job for a ready to read fd
     * 
     * When a file descriptor becomes ready, a job is created
     * to be posted to a thread-pool.
     * The job will then read from the file descriptor and then
     * process it.
     * 
     * @param fd file descriptor to be posted for read
     */
    void create_processing_job(int fd);

    /**
     * @brief add the state associated with a file descriptor
     * to the parse queue to be parsed, and the action specified
     * by the command taken.
     * 
     * 
     * @param pstate is the state associated with the file descriptor
     * @return true on successful posting
     * @return false on failure to post
     */
    bool add_to_parse_and_run_queue(std::shared_ptr<State> pstate);

    /**
     * @brief add the state associated with a file descriptor to the
     * work queue which will send the response back to the client.
     * 
     * @param pstate is the state associated with the file descriptor
     * @return true on successful posting
     * @return false on failure to post
     */
    bool add_to_write_queue(std::shared_ptr<State> pstate);

    /**
     * @brief given a parsed command, perform the requested operations
     * 
     * @param command command after parsing, as received from client
     * @return std::tuple<bool, std::shared_ptr<AbstractRespObject> > 
     * a tuple containing two items
     * 1. has there been a fatal error, which mandates the
     *    client connection must be closed
     * 2. output of the operation as an object that can be serialized
     *    and sent to the client as response.
     */
    std::tuple<bool, std::shared_ptr<AbstractRespObject> >
        do_operation(std::shared_ptr<AbstractRespObject> command);

    /**
     * @brief In case of the GET command, perform the action
     * 
     * @param pobj command after parsing, as received from client
     * @return std::tuple<bool, std::shared_ptr<AbstractRespObject> > 
     * a tuple containing two items
     * 1. has there been a fatal error, which mandates the
     *    client connection must be closed
     * 2. output of the operation as an object that can be serialized
     *    and sent to the client as response.
     */    
    std::tuple<bool, std::shared_ptr<AbstractRespObject> >
        do_get(std::shared_ptr<AbstractRespObject> p);

    /**
     * @brief in case of a SET command, perform the action
     * 
     * @param pobj command after parsing, as received from client
     * @return std::tuple<bool, std::shared_ptr<AbstractRespObject> > 
     * a tuple containing two items
     * 1. has there been a fatal error, which mandates the
     *    client connection must be closed
     * 2. output of the operation as an object that can be serialized
     *    and sent to the client as response.
     */
    std::tuple<bool, std::shared_ptr<AbstractRespObject> >
        do_set(std::shared_ptr<AbstractRespObject> p);

    /**
     * @brief perform the DEL command
     * 
     * @param pobj command after parsing, as received from client
     * @return std::tuple<bool, std::shared_ptr<AbstractRespObject> > 
     * a tuple containing two items
     * 1. has there been a fatal error, which mandates the
     *    client connection must be closed
     * 2. output of the operation as an object that can be serialized
     *    and sent to the client as response.
     */
    std::tuple<bool, std::shared_ptr<AbstractRespObject> >
        do_del(std::shared_ptr<AbstractRespObject> p);

    /**
     * @brief delete one variable from the appropriate hash
     * 
     * @param pobj command after parsing, as received from the client
     * @return true on successful deletion
     * @return false on failure to delete for any reason, including
     * if the item was not present in the first place.
     */
    bool do_del_internal(std::shared_ptr<AbstractRespObject> pobj);


    /**
     * @brief the pthread function for the thread that accepts
     * new connections. A static glue is required because
     * pthread cannot deal object methods
     * 
     * @param arg passed by the pthread, contains the pointer
     * to the orchestrator object
     * @return void* returns nullptr
     */
    static void* accepting_thread_pthread_fn(void* arg)
    {
        Orchestrator* ptr = static_cast<Orchestrator*>(arg);
        ptr->accepting_thread_loop();
        return nullptr;
    }

    /**
     * @brief the pthread function for the thread that runs
     * epoll on connections. A static glue is required because
     * pthread cannot deal object methods
     * 
     * @param arg passed by the pthread, contains the pointer
     * to the orchestrator object
     * @return void* returns nullptr
     */
    static void* epoll_thread_pthread_fn(void * arg)
    {
        Orchestrator* ptr = static_cast<Orchestrator*>(arg);
        ptr->epoll_thread_loop();
        return nullptr;
    }


    /**
     * TODO: Refactor this function, into three different classes
     * for each command: set, get and del
     * And those classes should be responsible for both validating
     * and actual action
     */
    /**
     * @brief given an abstract object, find whether it is a valid
     * command or not.
     * 
     * When a command is received from the client, it is parsed.
     * After parsing, this function will decide whether it is a valid
     * command or not
     * 
     * @param p the parsed input in object form
     * @return std::tuple<bool, command_type_t> a tuple of two items:
     * 1. whether it is valid or not
     * 2. the type of command if it is valid
     */
    std::tuple<bool, command_type_t>
        is_valid_command(std::shared_ptr<AbstractRespObject> p);
    
    /**
     * @brief Add a file descriptor to queue for epoll
     * This queue is monitored by the thread which executes
     * epoll.
     * The thread is woken up.
     * 
     * @param fd file descriptor to monitor for changes
     */
    void add_to_epoll_queue(int fd);

    /**
     * @brief Get the partition id of the hash table, based on the
     * key.
     * 
     * For performance reasons, instead of using a single hash,
     * we use multiple hashes, and the decision to choose the
     * correct hash is taken based on the first character of the key.
     * 
     * This is done because on high load, a single hash would be
     * affected by lock contention. Using several hashes will
     * reduce the lock contention.
     * 
     * Additionally, reader-writer locks are used to increase
     * concurrency even more.
     * 
     * @param varname name of the variable
     * @return int partition id of the correct hash to use
     */
    int get_partition(const std::string& s);

    /**
     * @brief run a server
     * 
     * @return int 0 on success
     */
    int run_server();
};

#endif /* #ifndef ORCHESTRATOR_H_ */