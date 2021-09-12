#include "orchestrator.h"


void close_and_cleanup(
    int fd,
    std::shared_ptr<State> m_pstate,
    Orchestrator* m_porchestrator)
{
    m_pstate->m_mutex.unlock();
    m_porchestrator->remove_socket(fd);
    close(fd);
}

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

bool Orchestrator::spawn_accepting_thread()
{
    int retval;
    if (0 != (retval = pthread_create(
        &m_accepting_thread_id,
        NULL,
        Orchestrator::accepting_thread_pthread_fn,
        this)))
    {
        std::cerr << "pthread_create failed with rc = " << retval << " errno = " << errno << std::endl;
        return false;
    }

    return true;
}

void sigusr1_handler(int signo, siginfo_t *info, void *context)
{
    std::cout << "sigusr1 delivered" << std::endl;
    return;
}
bool Orchestrator::spawn_epoll_thread()
{
    int retval;

    if (0 != (retval = pthread_create(
        &m_epoll_thread_id,
        NULL,
        Orchestrator::epoll_thread_pthread_fn,
        this)))
    {
        std::cerr << "pthread_create failed with rc = " << retval << " errno = " << errno << std::endl;
        return false;
    }

    return true;
}

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
            std::cerr << new_socket << ": could not set nonblocking" << std::endl;
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

void Orchestrator::wakeup_epoll_thread()
{
    // force the epoll thread to wakeup by sending SIGUSR1
    pthread_kill(m_epoll_thread_id, SIGUSR1);
}

void Orchestrator::epoll_empty_unsafe()
{
    for (auto fd: m_epoll_sockets)
    {
        struct epoll_event event;
        event.data.fd = fd;
        event.events = EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLERR | EPOLLET;
        if (epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fd, &event))
        {
            perror("epoll_ctl");
            std::cerr << "epoll_ctl failed fd = " << fd \
                <<" errno = " << errno << std::endl;
        }
    }
}
void Orchestrator::epoll_empty()
{
    std::shared_lock lock(m_epoll_sockets_mtx);
    return epoll_empty_unsafe();
}

void Orchestrator::epoll_rearm_unsafe()
{
    for (auto fd: m_epoll_sockets)
    {
        struct epoll_event event;
        event.data.fd = fd;
        if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, fd, &event))
        {
            perror("epoll_ctl");
            std::cerr << "epoll_ctl failed, errno = " << errno << std::endl;
        }
    }
}
void Orchestrator::epoll_rearm()
{
    std::unique_lock lock(m_epoll_sockets_mtx);
    return epoll_rearm_unsafe();
}

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
        std::cerr << "epoll looping" << std::endl;
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

    if (0 != m_processing_threadpool->add_job(std::shared_ptr<JobInterface>(job)))
    {
        std::cerr << "Error adding job to processing threadpool" << std::endl;
    }
    else
        std::cerr << fd << ": Added a job to read the data" << std::endl;
}

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

bool Orchestrator::add_to_parse_queue(std::shared_ptr<State> pstate)
{
    ParseAndRunJob* job = new (std::nothrow) ParseAndRunJob(this, pstate);
    if (!job)
    {
        std::cerr << "Out of memory" << std::endl;
        return false;
    }

    if (0 != m_parse_and_run_threadpool->add_job(std::shared_ptr<JobInterface>(job)))
        return false;
    
    return true;
}

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

std::tuple<bool, std::shared_ptr<AbstractRespObject> >
Orchestrator::do_operation(std::shared_ptr<AbstractRespObject> command)
{
    // TODO: Fill this up

    RespError* error = new RespError(std::string("generic error"));
    std::shared_ptr<AbstractRespObject> p((AbstractRespObject*)error);
    return std::make_tuple(false, p);
}
#define BUFSIZE 513
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

    if (-1 == read_bytes && EAGAIN != save_errno)
    {
        perror("read");
        std::cerr << fd << ": error, read " << read_bytes << \
            " bytes, err = " << errno << std::endl;
        close_and_cleanup(fd, m_pstate, m_porchestrator);
        return read_bytes;
    }

    if (false == m_porchestrator->add_to_parse_queue(m_pstate))
    {
        std::cerr << fd << ": Adding to parse queue failed" << std::endl;
        close_and_cleanup(fd, m_pstate, m_porchestrator);
        return -1;
    }

    std::cerr << fd << ": Added to parse queue" << std::endl;

    return 0;
}


int ParseAndRunJob::run()
{
    m_pstate->m_state = STATE_PARSING;
    auto fd = m_pstate->m_socket;
    std::cerr << fd << ": Picked up for parsing" << std::endl;


    RespParser parser(m_pstate->m_read_data);
    auto [err, parsed_obj] = parser.get_generic_object();
    if (ERROR_SUCCESS != err)
    {
        std::cerr << fd << ": Could not parse" << std::endl;
        m_pstate->m_is_error = true;
        RespError* e = new RespError(std::string("Unable to parse"));
        m_pstate->m_response = std::shared_ptr<AbstractRespObject>((AbstractRespObject*)e);

        if (false == m_porchestrator->add_to_write_queue(m_pstate))
        {
            std::cerr << fd << ": Add to write queue failed" << std::endl;
            close_and_cleanup(fd, m_pstate, m_porchestrator);
            return -1;
        }

        return -1;
    }

    m_pstate->m_resp_object = parsed_obj;

    auto [is_fatal, response] = m_porchestrator->do_operation(
                                    m_pstate->m_resp_object);
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

    if (-bytes_written < 0)
    {
        perror("write");
        std::cout << fd << ": Write failed with rc = " << bytes_written \
            << " error = " << errno << std::endl;
        close_and_cleanup(fd, m_pstate, m_porchestrator);
        return -1;
    }

    // TODO: clear the state and add to the polling queue
    close_and_cleanup(fd, m_pstate, m_porchestrator);

    return 0;
}
