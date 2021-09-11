#include "thread_pool.h"
#include <unistd.h>
#include <cassert>
#include <cstdlib>


int ThreadPool::add_thread()
{
    pthread_t       tid;
    int             retval;

    retval = pthread_create(
                &tid,
                NULL,
                ThreadPool::thread_start_routine,
                this);
    
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

void ThreadPool::process_job_unsafe(bool& lock_held)
{
    assert(lock_held);

    if (!lock_held)
    {
        std::cerr << "lock not held " << std::endl;
        return;
    }

    if (m_job_queue.size())
    {
        auto p_job = m_job_queue.front();
        m_job_queue.pop_front();
        pthread_mutex_unlock(&m_job_queue_mutex);
        lock_held = false;

        if (m_is_debug)
        {
            std::cerr << "Running job " << p_job->get_job_id() << ": ";
            std::cerr << p_job->get_job_description() << std::endl;
        }

        p_job->run();
    }

    if (lock_held)
    {
        pthread_mutex_unlock(&m_job_queue_mutex);
        lock_held = false;
    }
}

void ThreadPool::loop()
{
    bool        lock_held       = false;
    int         err             = 0;

    while(true)
    {
        if (m_is_debug_verbose)
            std::cout << "looping " << pthread_self() << std::endl;

        if (m_is_destroying)
        {
            m_num_threads--;
            return;
        }

        if (!lock_held && (0 != (err = pthread_mutex_lock(&m_job_queue_mutex))))
        {
            std::cerr << __FILE__ << ":" << __LINE__ << " : ";
            std::cerr << "pthread_mutex_lock failed, err = " << err;
            std::cerr << " errno = " << errno << ". Exiting." << std::endl;
            m_num_threads--;
            return;
        }

        lock_held = true;

        if (m_job_queue.size())
        {
            /* This function will release the lock */
            process_job_unsafe(lock_held);
        }
        else
        {
            // wait 100 milliseconds
            struct timespec timout = {0, 100000000};

            int rc = pthread_cond_timedwait(
                &m_job_queue_cond,
                &m_job_queue_mutex,
                &timout
            );

            if (0 == rc || ETIMEDOUT == rc || ETIMEDOUT == errno)
            {
                if (rc == 0 && m_is_debug)
                {
                    std::cout << "pthread_cond_wait returned, rc = " << rc << " thread = " \
                        << pthread_self() << " q length " << m_job_queue.size() << std::endl;
                }
                if (m_job_queue.size())
                    process_job_unsafe(lock_held); // this will release the lock
                else
                {
                    pthread_mutex_unlock(&m_job_queue_mutex);
                    lock_held = false;
                }
            }
            else
            {
                assert(0);
                std::cerr << "Fatal error in pthread_cond_timedwait, rc = " << rc;
                std::cerr << " errno = " << errno << ". Terminating" << std::endl;
                exit(0);
            }
        }

    }
out:
    if (lock_held)
    {
        pthread_mutex_unlock(&m_job_queue_mutex);
        lock_held = false;
    }
}

int ThreadPool::add_job(std::shared_ptr<JobInterface> p_job)
{
    bool    lock_held   = false;
    int     rc          = 0;
    int     retval      = 0;

    if (!p_job)
        return 0;

    if (0 != pthread_mutex_lock(&m_job_queue_mutex))
    {
        std::cerr << __FILE__ << ":" << __LINE__ << ": ";
        std::cerr << "Fatal error, could not acquire lock, rc = " << rc;
        std::cerr << " errno = " << errno << std::endl;
        assert(0);
        exit(1);
    }

    lock_held = true;

    try
    {
        m_job_queue.push_back(p_job);
    }
    catch(...)
    {
        std::cerr << "Failed to push job to queue, OOM" << std::endl;
        retval = 1;
    }

    if (0 == retval)
    {
        pthread_cond_signal(&m_job_queue_cond);
    }

    pthread_mutex_unlock(&m_job_queue_mutex);

    return retval;
}

void ThreadPool::destroy()
{
    m_is_destroying = true;

    if (m_is_debug)
        std::cerr << "Waiting for threads to destroy " << std::endl;

    while (m_num_threads)
    {
        std::cerr << "Threads remaining: " << m_num_threads << std::endl;
        /*
         * TODO: Improve
         *
         * This is a sub-optimal implementation, and an ideal implemenatation
         * will use conditional signals, but for current purposes
         * this will work.
         */
        usleep(100);
    }

    if (m_is_debug)
        std::cerr << "OK." << std::endl;
}


ThreadPool* ThreadPoolFactory::create_thread_pool(
                int num_threads)
{
    ThreadPool* pool = new ThreadPool();

    if (!pool)
        return nullptr;

    for (int i = 0; i < num_threads; i++)
    {
        if (0 != pool->add_thread())
        {
            delete pool;
            std::cout << "Failed to add thread, errno = " << errno << std::endl;
            return nullptr;
        }
    }

    return pool;
}