#include "thread_pool.h"

int ThreadPool::addThread()
{
    pthread_t       tid;
    int             retval;

    retval = pthread_create(
                &tid, NULL, ThreadPool::thread_start_routine, this);
    
    if (0 == retval)
    {
        m_num_threads++;
        m_threads.push_back(tid);
    }
    else
    {
        std::cerr << "Failed to create thread, err = ";
        std::cerr << retval << " errno = " << errno << std::endl;
    }
    return retval;
}

void* ThreadPool::thread_start_routine(void* arg)
{
    static_cast<ThreadPool*>(arg)->loop();
    return nullptr;
}

void ThreadPool::loop()
{
    while(true)
    {

    }
}