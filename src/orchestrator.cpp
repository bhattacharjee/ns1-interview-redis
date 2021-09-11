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
            m_all_sockets.insert(new_socket);
        }
        catch (...)
        {
            std::cerr << "Unknown exception" << std::endl;
            assert(0);
            exit(1);
        }
        lock.unlock();

        std::unique_lock    lock2(m_waiting_for_read_sockets_mtx);
        try
        {
            m_waiting_for_read_sockets.insert(new_socket);
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

void Orchestrator::epoll_thread_loop()
{
    int retval;

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
        sleep(5);
    }
}

