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
    std::atomic<int>                                m_num_threads;

    /* A vector containing all the threads */
    std::vector<pthread_t>                          m_threads;

    /* The queue of jobs */
    std::deque<std::shared_ptr<JobInterface> >      m_job_queue;

    /* Mutex for the job queue, r/w locks not required here */
    pthread_mutex_t                                 m_job_queue_mutex;

    /* Conditional variable for the job queue */
    pthread_cond_t                                  m_job_queue_cond;

    /* Signal to threads to exit themselves */
    bool                                            m_is_destroying;

    ThreadPool() :
        m_num_threads(0),
        m_job_queue_mutex(PTHREAD_MUTEX_INITIALIZER),
        m_job_queue_cond(PTHREAD_COND_INITIALIZER),
        m_is_destroying(false)
    {
        m_threads.reserve(8);
    }

    /* Add another worker thread */
    int addThread();

    static void* thread_start_routine(void* arg);

    void loop();
};

#endif /* #ifndef THREAD_POOL_ */