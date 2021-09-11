#include "thread_pool.h"

int ThreadPool::addThread()
{
    pthread_t       tid;
    int             retval;

    retval = pthread_create(
                &tid, NULL, ThreadPool::thread_start_routine, this);
    
    return retval;
}

void* ThreadPool::thread_start_routine(void* arg)
{
    static_cast<ThreadPool*>(arg)->loop();
    return nullptr;
}

void ThreadPool::loop()
{
    return;
}