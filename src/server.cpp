#include "orchestrator.h"


int main(int argc, char** argv)
{
    std::cout << "Starting server ..." << std::endl;
    
    Orchestrator orchestrator;
    orchestrator.create_server_socket();
    if (!orchestrator.spawn_epoll_thread())
    {
        std::cerr << "Failed to spawn thread that polls for ready sockets" << std::endl;
        goto out;
    }
    if (!orchestrator.spawn_accepting_thread())
    {
        std::cerr << "Failed to spawn thread that accepts connections" << std::endl;
        goto out;
    }
    while (true)
    {
        sleep(5);
        if (orchestrator.m_is_destroying)
            break;
    }
out:
    return 0;
}