#include "orchestrator.h"

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
            std::cerr << "epoll_ctl failed, errno = " << errno << std::endl;
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

#define MAX_EVENTS 10
void Orchestrator::epoll_thread_loop()
{
    int retval;
    struct epoll_event events[MAX_EVENTS + 1];

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
            MAX_EVENTS,
            1000);
        if (n_fd)
        {
            // Take both locks to maintain lock heirarchy
            std::shared_lock lock0(m_all_sockets_mtx);
            std::unique_lock lock(m_epoll_sockets_mtx);
            for (int i = 0; i < n_fd; i++)
            {
                if (0 == events[i].events)
                    continue;
                ready_fds.insert(events[i].data.fd);
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
                for (auto fd: ready_fds)
                    m_processing_sockets.insert(fd);
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