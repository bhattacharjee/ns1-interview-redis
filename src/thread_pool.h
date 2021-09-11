#ifndef THREAD_POOL_
#define THREAD_POOL_

#include "common_include.h"
#include <pthread.h>

class JobInterface
{
public:
    std::uint64_t       m_job_id;
    std::string         m_job_description;
    
    virtual std::string  get_job_description()
    {
        return m_job_description;
    }
    
    JobInterface(): m_job_id(0), m_job_description("") {}

    virtual void set_job_description(std::string description)
    {
        m_job_description = description;
    }

    std::uint64_t get_job_id() { return m_job_id; }

    virtual int run() = 0;

    virtual ~JobInterface() {}
};

class ThreadPool
{
private:
    /* Pick up a job from the queue and process it, lock must be taken by caller */
    void process_job_unsafe(bool& lock_held);

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

    /* Signal to threads to exit themselves, threads to check periodically */
    bool                                            m_is_destroying;

    /* Job ID number, ever increasing */
    std::atomic<std::uint64_t>                      m_job_sequence_number;

    /* Print additional debug logs */
    bool                                            m_is_debug;
    bool                                            m_is_debug_verbose;

    /* Constructor */
    ThreadPool() :
        m_num_threads(0),
        m_job_queue_mutex(PTHREAD_MUTEX_INITIALIZER),
        m_job_queue_cond(PTHREAD_COND_INITIALIZER),
        m_is_destroying(false),
        m_job_sequence_number(0),
        m_is_debug(false),
        m_is_debug_verbose(false)
    {
        m_threads.reserve(8);
    }


    /* Add another worker thread */
    int add_thread();

    /*
     * This will be called whenever a new pthread is created.
     * In turn, this will call loop()
     */
    static void* thread_start_routine(void* arg);

    /* The worker thread loop */
    void loop();

    /* Indicate to threads that they must destroy and wait */
    void destroy();

    /* Add a job for processing */
    int add_job(std::shared_ptr<JobInterface> p_job);

    /* Detructor */
    ~ThreadPool()
    {
        destroy();
    }
};

class ThreadPoolFactory
{
public:
    ThreadPool* create_thread_pool(int num_threads, bool is_debug);
};

#endif /* #ifndef THREAD_POOL_ */