#include "orchestrator.h"

/**
 * @brief close a file descriptor and remove all associated data
 * 
 * @param fd the file descriptor to remove
 * @param m_pstate the state structure
 * @param m_porchestrator pionter to the orchestrator object
 */
void close_and_cleanup(
    int fd,
    std::shared_ptr<State> m_pstate,
    Orchestrator* m_porchestrator)
{
    m_pstate->m_mutex.unlock();
    m_porchestrator->remove_socket(fd);
    close(fd);
}

/**
 * @brief create a server socket to listen on
 * 
 */
void Orchestrator::create_server_socket()
{
    int                     opt         = 1;
    struct sockaddr_in      address;
    m_server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_server_socket <= 0)
    {
        perror("socket");
        std::cerr << "Could not create socket, errno = " << errno << std::endl;
        assert(0);
        exit(1);
    }

    if (setsockopt(
            m_server_socket,
            SOL_SOCKET,
            SO_REUSEADDR | SO_REUSEPORT,
            &opt,
            sizeof(opt)))
    {
        perror("setsockopt");
        std::cerr << "setsockopt failed with error = " << errno << std::endl;
        assert(0);
        exit(1);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORTNUM);

    if (bind(
            m_server_socket,
            (struct sockaddr*)&address,
            sizeof(address)))
    {
        perror("bind");
        std::cerr << "bind failed with error = " << errno << std::endl;
        assert(0);
        exit(1);
    }

    if (listen(m_server_socket, 10) < 0)
    {
        perror("listen");
        std::cerr << "listen failed with error = " << errno << std::endl;
        exit(1);
    }
}

/**
 * @brief spawn a thread that loops to accpept new connections
 * 
 * @return true on successful launch
 * @return false on unsuccessful launch
 */
bool Orchestrator::spawn_accepting_thread()
{
    int retval;
    if (0 != (retval = pthread_create(
        &m_accepting_thread_id,
        NULL,
        Orchestrator::accepting_thread_pthread_fn,
        this)))
    {
        std::cerr << "pthread_create failed with rc = " << retval \
                << " errno = " << errno << std::endl;
        return false;
    }

    return true;
}

/**
 * @brief signal handler used to trap SIGUSR1
 * The signal mechanism is used to interrupt the epoll_wait
 * system call. In case a new connection is accepted, it
 * must be added to the epoll structure immediately.
 * 
 * @param signo the signal 
 * @param info 
 * @param context 
 */
void sigusr1_handler(int signo, siginfo_t *info, void *context)
{
    std::cout << "sigusr1 delivered" << std::endl;
    return;
}

/**
 * @brief spawn the thread that calls epoll for ready sockets
 * 
 * @return true on successful launch
 * @return false on failure to launch
 */
bool Orchestrator::spawn_epoll_thread()
{
    int retval;

    if (0 != (retval = pthread_create(
        &m_epoll_thread_id,
        NULL,
        Orchestrator::epoll_thread_pthread_fn,
        this)))
    {
        std::cerr << "pthread_create failed with rc = " << retval \
                << " errno = " << errno << std::endl;
        return false;
    }

    return true;
}
/**
 * @brief loop and accept connections on the listening socket
 * Once a connection is received, it then adds it to the queue
 * which the epoll thread uses, and wakes up the epoll thread.
 * 
 */
void Orchestrator::accepting_thread_loop()
{
    struct sockaddr_in address;
    int addrlen;

    while(true)
    {
        addrlen = sizeof(address);
        int new_socket = accept(
                            m_server_socket,
                            (struct sockaddr*)&address,
                            (socklen_t*)&addrlen);
        if (new_socket < 0)
        {
            perror("accept");
            std::cerr << "accept failed with rc = " << new_socket;
            std::cerr << " errno = " << errno << std::endl;
            continue;
        }

        int flags = fcntl(new_socket, F_GETFL, 0);
        if (0 != fcntl(new_socket, F_SETFL, flags | O_NONBLOCK))
        {
            std::cerr << new_socket << \
                    ": could not set nonblocking" << std::endl;
        }

        std::unique_lock    lock(m_all_sockets_mtx);
        try
        {
            auto state = State::create_state(new_socket);
            if (state)
            {
                state->m_state = STATE_ACCEPTED;
                m_all_sockets[new_socket] = state;
            }
        }
        catch (...)
        {
            std::cerr << "Unknown exception" << std::endl;
            assert(0);
            exit(1);
        }
        lock.unlock();

        std::unique_lock    lock2(m_epoll_sockets_mtx);
        try
        {
            m_epoll_sockets.insert(new_socket);
        }
        catch(const std::exception& e)
        {
            std::cerr << "Unknown exception" << std::endl;
            assert(0);
            exit(1);
        }
        lock2.unlock();

        std::cerr << new_socket << \
            ": Accepted, waking up epoll thread" << std::endl;
        
        wakeup_epoll_thread();
    }
}

/**
 * @brief Add a file descriptor to queue for epoll
 * This queue is monitored by the thread which executes
 * epoll.
 * The thread is woken up.
 * 
 * @param fd file descriptor to monitor for changes
 */
void Orchestrator::add_to_epoll_queue(int fd)
{
    std::unique_lock lock(m_epoll_sockets_mtx);
    try
    {
        m_epoll_sockets.insert(fd);
    }
    catch(...)
    {
        std::cerr << "Unknown exception" << std::endl;
        assert(0);
        exit(1);
    }
    lock.unlock();
    std::cerr << fd << ": Added to epoll queue" << std::endl;
    wakeup_epoll_thread();
}

/**
 * @brief Wake up the epoll thread by sending it a signal
 * 
 */
void Orchestrator::wakeup_epoll_thread()
{
    // force the epoll thread to wakeup by sending SIGUSR1
    pthread_kill(m_epoll_thread_id, SIGUSR1);
}

/**
 * @brief remove all monitoring file descriptors from epoll
 * This is the unsafe version of the function, and the caller
 * must hold the m_epoll_sockets_mtx lock before calling it
 */
void Orchestrator::epoll_empty_unsafe()
{
    for (auto fd: m_epoll_sockets)
    {
        struct epoll_event event;
        event.data.fd = fd;
        event.events = EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLERR | EPOLLET;
        if (epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fd, &event))
        {
            if (ENOENT != errno)
            {
                perror("epoll_ctl");
                std::cerr << "epoll_ctl failed fd = " << fd \
                    <<" errno = " << errno << std::endl;
            }
        }
    }
}

/**
 * @brief remove all monitoring file descriptors from epoll
 * 
 */
void Orchestrator::epoll_empty()
{
    std::shared_lock lock(m_epoll_sockets_mtx);
    return epoll_empty_unsafe();
}

/**
 * @brief rebuild the epoll monitor from the set of fd's
 * 
 * This is the unsafe version of the function. the caller
 * must hold the mutex before calling it.
 * 
 */
void Orchestrator::epoll_rearm_unsafe()
{
    for (auto fd: m_epoll_sockets)
    {
        struct epoll_event event;
        event.data.fd = fd;
        if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, fd, &event))
        {
            if (ENOENT != errno)
            {
                perror("epoll_ctl");
                std::cerr << "epoll_ctl failed, errno = " \
                    << errno << std::endl;
            }
        }
    }
}

/**
 * @brief rebuild the epoll monitor from the set of fd's
 * 
 */
void Orchestrator::epoll_rearm()
{
    std::unique_lock lock(m_epoll_sockets_mtx);
    return epoll_rearm_unsafe();
}

/**
 * @brief Loop and call epoll, and send ready sockets to workers
 * 
 * Loops and calls epoll. When a ready socket is found, it is
 * posted to the thread pool which processes it.
 * 
 */
void Orchestrator::epoll_thread_loop()
{
    int retval;
    struct epoll_event events[MAX_EPOLL_EVENTS + 1];

    struct sigaction action;
    action.sa_sigaction = &sigusr1_handler;
    action.sa_flags = SA_SIGINFO  | SA_RESTART;
    if (retval = sigaction(SIGUSR1, &action, NULL))
    {
        perror("sigaction");
        std::cerr << "Fatal: sigaction failed with rc = " << retval;
        std::cerr << " errno = " << errno << std::endl;
        exit(1);
    }

    while(true)
    {
        //std::cerr << "epoll looping" << std::endl;
        std::unordered_set<int> ready_fds;
        epoll_empty();
        epoll_rearm();
        bzero(events, sizeof(events));
        int n_fd = epoll_wait(
            m_epoll_fd,
            events,
            MAX_EPOLL_EVENTS,
            1000);
        if (n_fd)
        {
            // Take both locks to maintain lock heirarchy
            std::shared_lock lock0(m_all_sockets_mtx);
            std::unique_lock lock(m_epoll_sockets_mtx);
            try
            {    
                for (int i = 0; i < n_fd; i++)
                {
                    if (0 == events[i].events)
                        continue;
                    ready_fds.insert(events[i].data.fd);
                    std::cerr << events[i].data.fd << \
                        ": ePOll, ready for read" << std::endl;
                }
            }
            catch(...)
            {
                std::cerr << __FILE__ << ":" << __LINE__;
                std::cerr << " Error inserting into set" << std::endl;
            }

            epoll_empty_unsafe();
            for (auto fd: ready_fds)
            {
                auto it = m_epoll_sockets.find(fd);
                if (it != m_epoll_sockets.end())
                    m_epoll_sockets.erase(it);
            }

            if (ready_fds.size())
            {
                std::unique_lock lock2(m_processing_sockets_mtx);
                try
                {
                    for (auto fd: ready_fds)
                        m_processing_sockets.insert(fd);
                }
                catch(...)
                {
                    std::cerr << __FILE__ << ":" << __LINE__;
                    std::cerr << " Error inserting into set" << std::endl;
                }
            }

            lock0.unlock();
            lock.unlock();
            for (auto fd: ready_fds)
                create_processing_job(fd);
        }
    }
}

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
void Orchestrator::create_processing_job(int fd)
{
    std::shared_ptr<State>      p(nullptr);

    {
        std::shared_mutex(m_all_sockets_mtx);
        assert(m_all_sockets.find(fd) != m_all_sockets.end());
        p = m_all_sockets[fd];
        assert(!!p);
        p->m_mutex.lock();
    }

    p->m_state = STATE_WAITING_FOR_READ_JOB;

    SocketReadJob* job = new (std::nothrow) SocketReadJob(this, p);
    if (!job)
    {
        std::cerr << "Out of memory" << std::endl;
        return;
    }

    if (0 != m_processing_threadpool->add_job(\
                            std::shared_ptr<JobInterface>(job)))
    {
        std::cerr << "Error adding job to processing threadpool" << std::endl;
    }
    else
        std::cerr << fd << ": Added a job to read the data" << std::endl;
}

/**
 * @brief creates the file descriptor on which epoll is run
 * 
 * @return true on success
 * @return false on failure
 */
bool Orchestrator::create_epoll_fd()
{
    m_epoll_fd = epoll_create1(0);
    if (m_epoll_fd < 0)
    {
        perror("epoll_create");
        std::cerr << "epoll create failed, errno = " << errno;
        return false;
    }
    return true;
}

/**
 * @brief removes a socket from all queues and frees
 * any data structure associated with the file descriptor
 * 
 * @param fd the file descriptor to remove
 */
void Orchestrator::remove_socket(int fd)
{
    std::cerr << fd << ": removing from all queues" << std::endl;
    {
        std::unique_lock lock(m_all_sockets_mtx);
        auto it = m_all_sockets.find(fd);
        if (it != m_all_sockets.end())
        {
            auto state = m_all_sockets[fd];
            state->m_mutex.lock();
            m_all_sockets.erase(it);
            state->m_mutex.unlock();
        }
    }

    {
        std::unique_lock lock(m_epoll_sockets_mtx);
        auto it = m_epoll_sockets.find(fd);
        if (it != m_epoll_sockets.end())
            m_epoll_sockets.erase(it);
    }

    {
        std::unique_lock lock(m_processing_sockets_mtx);
        auto it = m_processing_sockets.find(fd);
        if (it != m_processing_sockets.end())
            m_processing_sockets.erase(it);
    }

    {
        std::unique_lock lock(m_write_sockets_mtx);
        auto it = m_write_sockets.find(fd);
        if (it != m_write_sockets.end())
            m_write_sockets.erase(it);
    }
}

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
bool Orchestrator::add_to_parse_and_run_queue(
                    std::shared_ptr<State> pstate)
{
    ParseAndRunJob* job = new (std::nothrow) ParseAndRunJob(this, pstate);
    if (!job)
    {
        std::cerr << "Out of memory" << std::endl;
        return false;
    }

    if (0 != m_parse_and_run_threadpool->add_job(\
                            std::shared_ptr<JobInterface>(job)))
        return false;
    
    return true;
}

/**
 * @brief add the state associated with a file descriptor to the
 * work queue which will send the response back to the client.
 * 
 * @param pstate is the state associated with the file descriptor
 * @return true on successful posting
 * @return false on failure to post
 */
bool Orchestrator::add_to_write_queue(std::shared_ptr<State> pstate)
{
    SocketWriteJob* job = new (std::nothrow) SocketWriteJob(this, pstate);
    if (!job)
    {
        std::cerr << "Out of memory" << std::endl;
        return false;
    }

    if (0 != m_write_threadpool->add_job(std::shared_ptr<JobInterface>(job)))
        return false;
    
    return true;
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
Orchestrator::is_valid_command(std::shared_ptr<AbstractRespObject> p)
{
    if (! p->m_is_aggregate)
        return std::make_tuple(false, COMMAND_INVALID);
    
    RespArray* p_array_obj = static_cast<RespArray*>(p.get());
    auto array = p_array_obj->get_array();

    if (array.size() <= 1)
        return std::make_tuple(false, COMMAND_INVALID);

    auto command_string = array[0]->to_string();
    command_type_t type = COMMAND_INVALID;

    if (command_string == std::string("get") || 
        command_string == std::string("del"))
    {
        if (command_string == std::string("get"))
            type = COMMAND_GET;
        else
            type = COMMAND_DEL;

        if (array.size() >= 2 && 
            (RESP_BULK_STRING == array[1]->m_datatype ||
                RESP_STRING == array[1]->m_datatype))
            return std::make_tuple(true, type);
        else
            return std::make_tuple(false, COMMAND_INVALID);
    }
    else if (command_string == std::string("set"))
    {
        if (array.size() >= 3 &&
            (RESP_BULK_STRING == array[1]->m_datatype ||
                RESP_STRING == array[1]->m_datatype))
            return std::make_tuple(true, COMMAND_SET);
        else
            return std::make_tuple(false, COMMAND_INVALID);
    }
    else
        return std::make_tuple(false, COMMAND_INVALID);
}

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
int Orchestrator::get_partition(const std::string& varname)
{
    if (0 == varname.length())
        return 0;
    char x = varname[0];
    return x % NUM_DATASTORES;
}

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
Orchestrator::do_operation(std::shared_ptr<AbstractRespObject> command)
{
    // TODO: Fill this up
    auto [is_valid, cmd_type] = is_valid_command(command);
    if (!is_valid)
    {
        RespError* error = new (std::nothrow) RespError("Invalid command");
        if (!error)
        {
            std::cerr << "Out of memory, exiting" << std::endl;
            exit(1);
        }
        return std::make_tuple(
            false,
            std::shared_ptr<AbstractRespObject>((AbstractRespObject*)error));
    }

    if (COMMAND_GET == cmd_type)
        return do_get(command);
    else if (COMMAND_SET == cmd_type)
        return do_set(command);
    else if (COMMAND_DEL == cmd_type)
        return do_del(command);

    RespError* error = \
               new (std::nothrow) RespError(std::string("generic error"));
    if (!error)
    {
        std::cerr << "Out of memory. exiting" << std::endl;
        exit(1);
    }
    std::shared_ptr<AbstractRespObject> p((AbstractRespObject*)error);
    return std::make_tuple(false, p);
}

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
Orchestrator::do_set(std::shared_ptr<AbstractRespObject> pobj)
{
    RespArray* p_array_obj = static_cast<RespArray*>(pobj.get());
    auto array = p_array_obj->get_array();
    auto varname = array[1]->to_string();
    auto partition = get_partition(varname);
    auto value = array[2]->serialize();

    auto success = m_datastore[partition].set(varname.c_str(), value.c_str());
    if (success)
    {
        auto *p = new (std::nothrow) RespString(std::string("OK"));
        if (!p)
        {
            std::cerr << "Outo of memory" << std::endl;
            exit(1);
        }
        return std::make_tuple(
            false, 
            std::shared_ptr<AbstractRespObject>(\
                static_cast<AbstractRespObject*>(p)));
    }

    RespError* err_obj = new RespError("Failed to set the value");
    if (!err_obj)
    {
        std::cerr << "Out of memory " << std::endl;
        exit(1);
    }

    return std::make_tuple(
        false,
        std::shared_ptr<AbstractRespObject>((AbstractRespObject*)err_obj));
}

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
Orchestrator::do_get(std::shared_ptr<AbstractRespObject> pobj)
{
    RespArray* p_array_obj = static_cast<RespArray*>(pobj.get());
    auto array = p_array_obj->get_array();
    auto varname = array[1]->to_string();
    auto partition = get_partition(varname);
    
    auto [found, value] = m_datastore[partition].get(varname.c_str());

    if (!found)
    {
        auto *p = new (std::nothrow) RespBulkString(std::string(""));
        if (!p)
        {
            std::cerr << "Outo of memory" << std::endl;
            exit(1);
        }

        p->set_null(true);
        return std::make_tuple(
            false, 
            std::shared_ptr<AbstractRespObject>(\
                static_cast<AbstractRespObject*>(p)));
    }

    RespParser parser(value);
    auto [err, ret] = parser.get_generic_object();
    if (ERROR_SUCCESS != err)
    {
        std::cerr << "IMPORTANT: could not parse value from hash '"\
            << value << "'" << std::endl;

        auto *p = new (std::nothrow) RespBulkString(std::string(""));
        if (!p)
        {
            std::cerr << "Outo of memory" << std::endl;
            exit(1);
        }

        p->set_null(true);
        return std::make_tuple(
            false, 
            std::shared_ptr<AbstractRespObject>(\
                static_cast<AbstractRespObject*>(p)));
    }

    return std::make_tuple(true, ret);
}

/**
 * @brief delete one variable from the appropriate hash
 * 
 * @param pobj command after parsing, as received from the client
 * @return true on successful deletion
 * @return false on failure to delete for any reason, including
 * if the item was not present in the first place.
 */
bool Orchestrator::do_del_internal(std::shared_ptr<AbstractRespObject> pobj)
{
    auto key = pobj->to_string();
    auto partition = get_partition(key);
    return m_datastore[partition].del(key.c_str());
}

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
Orchestrator::do_del(std::shared_ptr<AbstractRespObject> pobj)
{
    RespArray* p_array_obj = static_cast<RespArray*>(pobj.get());
    auto array = p_array_obj->get_array();
    
    int del_count = 0;
    for (int i = 1; i < array.size(); i++)
    {
        if (do_del_internal(array[i]))
            del_count++;
    }

    RespInteger* ret = new (std::nothrow) RespInteger(del_count);
    if (!ret)
    {
        std::cerr << "Out of memory " << std::endl;
        exit(1);
    }

    return std::make_tuple(
        false,
        std::shared_ptr<AbstractRespObject>(
            static_cast<AbstractRespObject*>(ret)));

}

/**
 * @brief start the server
 * 
 * @return int 0 on success
 */
int Orchestrator::run_server()
{
    create_server_socket();
    if (!create_epoll_fd())
    {
        std::cerr << "failed to create epoll socket." << std::endl;
        return -1;
    }
    if (!spawn_epoll_thread())
    {
        std::cerr << "Failed to spawn thread that polls for ready sockets" << std::endl;
        return -1;
    }
    if (!spawn_accepting_thread())
    {
        std::cerr << "Failed to spawn thread that accepts connections" << std::endl;
        return -1;
    }

    return 0;
}

#define BUFSIZE 513

/**
 * @brief Reads from a socket and stores the values the state
 * 
 * It runs as a part of the read worker pool. When done,
 * it then invokes the parser worker pool.
 * 
 * @return int on success it returns 0, otherwise a number
 * to indicate the error
 */
int SocketReadJob::run()
{
    m_pstate->m_state = STATE_IN_READ_LOOP;
    auto fd = m_pstate->m_socket;
    std::cerr << fd << ": Picked up for reading" << std::endl;
    char buffer[BUFSIZE];

    int read_bytes;
    int save_errno;
    
    do 
    {
        bzero(buffer, sizeof(buffer));
        read_bytes = read(fd, buffer, BUFSIZE);
        save_errno = errno;
        if (read_bytes > 0)
            m_pstate->m_read_data += buffer;
    } while (read_bytes > 0);

    if ((-1 == read_bytes && EAGAIN != save_errno) ||
        m_pstate->m_read_data == std::string(""))
    {
        perror("read");
        std::cerr << fd << ": error, read " << read_bytes << \
            " bytes, err = " << errno << std::endl;
        close_and_cleanup(fd, m_pstate, m_porchestrator);
        return read_bytes;
    }

    if (false == m_porchestrator->add_to_parse_and_run_queue(m_pstate))
    {
        std::cerr << fd << ": Adding to parse queue failed" << std::endl;
        close_and_cleanup(fd, m_pstate, m_porchestrator);
        return -1;
    }

    std::cerr << fd << ": Added to parse queue" << std::endl;

    return 0;
}

/**
 * @brief The job in the work-queue which parses the input
 * that is received from the user, and then performs the
 * appropriate action
 * 
 * @return int 0 on success
 */
int ParseAndRunJob::run()
{
    m_pstate->m_state = STATE_PARSING;
    auto fd = m_pstate->m_socket;
    std::cerr << fd << ": Picked up for parsing" << std::endl;


    RespParser parser(m_pstate->m_read_data);
    auto [err, parsed_obj] = parser.get_generic_object();
    if (ERROR_SUCCESS != err)
    {
        std::cerr << fd << ": Could not parse command '" \
            << m_pstate->m_read_data << "'" << std::endl;

        m_pstate->m_is_error = true;

        RespError* e = new RespError(
            std::string("Unable to parse '") 
                + std::string(m_pstate->m_read_data)
                + std::string("'. Try again."));

        m_pstate->m_response = \
               std::shared_ptr<AbstractRespObject>((AbstractRespObject*)e);

        if (false == m_porchestrator->add_to_write_queue(m_pstate))
        {
            std::cerr << fd << ": Add to write queue failed" << std::endl;
            close_and_cleanup(fd, m_pstate, m_porchestrator);
            return -1;
        }

        return -1;
    }

    m_pstate->m_object = parsed_obj;

    auto [is_fatal, response] = m_porchestrator->do_operation(
                                    m_pstate->m_object);
    m_pstate->m_response = response;

    if (is_fatal)
    {
        m_pstate->m_is_error = true;
        if (!response)
            m_pstate->set_default_special_error();
    }

    if (false == m_porchestrator->add_to_write_queue(m_pstate))
    {
        std::cerr << fd << ": Add to write queue failed" << std::endl;
        close_and_cleanup(fd, m_pstate, m_porchestrator);
        return -1;    
    }

    std::cerr << fd << ": Added to write queue" << std::endl;


    return 0;

}

/**
 * @brief the job for the work-queue which actually sends
 * the response back to the client
 * 
 * @return int 0 on success
 */
int SocketWriteJob::run()
{
    m_pstate->m_state = STATE_PARSING;
    auto fd = m_pstate->m_socket;

    std::cerr << fd << ": Picked up write job";

    char default_buffer[] = "-ERROR\r\n";
    char* buffer;
    std::string response_string;

    if (m_pstate->m_special_error[0])
        buffer = m_pstate->m_special_error;
    else if (m_pstate->m_response)
    {
        response_string = m_pstate->m_response->serialize();
        buffer = const_cast<char*>(response_string.c_str());
    }
    else
    {
        buffer = default_buffer;
    }

    int buflen = strlen(buffer);

    auto bytes_written = write(fd, buffer, buflen);

    if (bytes_written < 0)
    {
        perror("write");
        std::cout << fd << ": Write failed with rc = " << bytes_written \
            << " error = " << errno << std::endl;
        close_and_cleanup(fd, m_pstate, m_porchestrator);
        return -1;
    }

    if (m_pstate->m_is_error)
        close_and_cleanup(fd, m_pstate, m_porchestrator);
    else
    {
        std::cerr << fd << ": Adding back to epoll queue" << std::endl;
        m_pstate->reset();
        m_porchestrator->add_to_epoll_queue(fd);
    }

    return 0;
}
