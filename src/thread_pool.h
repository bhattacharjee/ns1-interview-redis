#ifndef THREAD_POOL_
#define THREAD_POOL_

#include "common_include.h"
#include <pthread.h>

class JobInterface
{
public:
    virtual int run() = 0;
};

class ThreadPool
{
public:
    /* Current number of worker threads */
    int                         mNumThreads;

    /* A vector containing all the threads */
    std::vector<pthread_t>      mThreads;
    ThreadPool() : mNumThreads(0)
    {
        mThreads.reserve(8);
    }

    /* Add another worker thread */
    int addThread();

    static void* thread_start_routine(void* arg);

    void loop();
};

#endif /* #ifndef THREAD_POOL_ */