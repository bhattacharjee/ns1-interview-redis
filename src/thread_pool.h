#ifndef THREAD_POOL_
#define THREAD_POOL_

#include "common_include.h"
#include <pthread.h>

/**
 * @brief Jobs posted to the thread pool must
 * derive from this class
 * 
 */
class JobInterface
{
public:
    /**
     * @brief The job id
     * 
     */
    std::uint64_t       m_job_id;

    /**
     * @brief The job description
     * 
     */
    std::string         m_job_description;
    
    /**
     * @brief Get the job description object
     * 
     * @return std::string job description
     */
    virtual std::string  get_job_description()
    {
        return m_job_description;
    }
    
    JobInterface(): m_job_id(0), m_job_description("") {}

    /**
     * @brief Set the job description object
     * 
     * @param description the job description
     */
    virtual void set_job_description(std::string description)
    {
        m_job_description = description;
    }

    /**
     * @brief Get the job id object
     * 
     * @return std::uint64_t the job id
     */
    std::uint64_t get_job_id() { return m_job_id; }

    /**
     * @brief The actual function which is run when this job is scheduled
     * All sub-classes must implement this.
     * 
     * @return int 
     */
    virtual int run() = 0;

    virtual ~JobInterface() {}
};

/**
 * @brief A thread pool that runs jobs. The thread-pool can have
 * multiple worker threads, and there is one job queue.
 * 
 * Priorities are not supported at the moment.
 * 
 */
class ThreadPool
{
private:
    /**
     * @brief pick up a job from the queue and process it
     * lock must be taken by the caller
     * 
     * @param lock_held 
     */
    void process_job_unsafe(bool& lock_held);

public:
    /**
     * @brief current number of worker threads
     * 
     */
    std::atomic<int>                                m_num_threads;

    /**
     * @brief A vector containing all threads
     * 
     */
    std::vector<pthread_t>                          m_threads;

    /**
     * @brief the queue of new jobs
     * 
     */
    std::deque<std::shared_ptr<JobInterface> >      m_job_queue;

    /**
     * @brief Mutex for the job queue
     * 
     */
    pthread_mutex_t                                 m_job_queue_mutex;

    /**
     * @brief conditional variable for the job queue to
     * wake up threads on new jobs
     * 
     */
    pthread_cond_t                                  m_job_queue_cond;

    /**
     * @brief Signal to the theads that this pool is being destroyed
     * and they have to terminate
     * 
     */
    bool                                            m_is_destroying;

    /**
     * @brief auto-incrementin job id
     * 
     */
    std::atomic<std::uint64_t>                      m_job_sequence_number;

    /**
     * @brief print additional debug logs if this is true
     * 
     */
    bool                                            m_is_debug;

    /**
     * @brief print very verbose debug logs if this is true
     * 
     */
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


    /**
     * @brief Add another worker thread
     * 
     * @return int 0 on success
     */
    int add_thread();

    /**
     * @brief whenever a new pthread is created, it will call
     * this function. 
     * a static glue is required for the pthread interface
     * 
     * @param arg the pointer to this thread pool
     * @return void* nullptr
     */
    static void* thread_start_routine(void* arg);

    /**
     * @brief The loop of the worker thread runs
     * In this loop, it monitors the queue for work, and when
     * one is available it performs the job
     * 
     */
    void loop();

    /**
     * @brief Destroy the pool, and ask all threads to stop processing
     * 
     */
    void destroy();

    /**
     * @brief add a new job for processing
     * 
     * @param p_job the job for processing, must derive
     * from JobInterface
     * @return int 0 on success
     */
    int add_job(std::shared_ptr<JobInterface> p_job);

    /* Detructor */
    ~ThreadPool()
    {
        destroy();
    }
};

/**
 * @brief This class is a helper method to create a new thread-pool
 * 
 */
class ThreadPoolFactory
{
public:
    /**
     * @brief Create a thread pool object with the specified number
     * of threads
     * 
     * @param num_threads number of threads
     * @param is_debug turn on verbose debug messages
     * @return ThreadPool* a thread pool
     */
    ThreadPool* create_thread_pool(int num_threads, bool is_debug);
};

#endif /* #ifndef THREAD_POOL_ */