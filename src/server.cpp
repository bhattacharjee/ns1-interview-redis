#include "orchestrator.h"


int main(int argc, char** argv)
{
    std::cout << "Starting server ..." << std::endl;

    Orchestrator orchestrator;
    if (orchestrator.run_server())
    {
        std::cerr << "could not start server " << std::endl;
        exit(1);
    }
    while (true)
    {
        sleep(5);
        if (orchestrator.m_is_destroying)
            break;
    }
    return 0;
}