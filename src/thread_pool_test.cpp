#include "thread_pool.h"
#include <cstdlib>
#include <unistd.h>
#include <pthread.h>

#define TEST(x, y) {\
    if (!(x))\
    {\
        std::cout << "FAILED: " << y << std::endl;\
        exit(1);\
    }\
    else\
    {\
        std::cout << "PASSED: " << y << std::endl;\
    }\
}


class BasicTest: public JobInterface
{
public:
    std::atomic<int>*   p_constructor_count;
    std::atomic<int>*   p_destructor_count;
    std::atomic<int>*   p_run_count;
    bool                m_is_debug;
    int run()
    {
        ++(*p_run_count);
        if (m_is_debug)
            std::cout << "XXX: Running job in thread " << pthread_self() << std::endl;
        sleep(1);
        return 0;
    }

    ~BasicTest()
    {
        ++(*p_destructor_count);
        if (m_is_debug)
            std::cout << "XXX: Destroying job " << this << std::endl;
    }

    BasicTest(
        std::atomic<int>*   pcons_count,
        std::atomic<int>*   pdest_count,
        std::atomic<int>*   prun_count
    )
    {
        p_constructor_count = pcons_count;
        p_destructor_count = pdest_count;
        p_run_count = prun_count;

        ++(*p_constructor_count);
    }
};

void test_jobs()
{
    auto tpf = ThreadPoolFactory();
    auto tp = tpf.create_thread_pool(4, false);

    std::cout << std::endl << __FILE__ << ":" << __LINE__ << " Executing test 1" << std::endl;

    const int NUM_JOBS = 8;

    if (tp)
    {
        std::atomic<int>        job_constructor_count   = 0;
        std::atomic<int>        job_destructor_count    = 0;
        std::atomic<int>        job_run_count           = 0;


        tp->m_is_debug = false;

        for (int i = 0; i < NUM_JOBS; i++)
        {
            auto bt = new BasicTest(&job_constructor_count, &job_destructor_count, &job_run_count);
            bt->m_is_debug = false;
            tp->add_job(std::shared_ptr<JobInterface>(bt));
        }

        sleep(3);

        TEST(job_constructor_count == NUM_JOBS, "All constructors must be called.");
        TEST(job_destructor_count == NUM_JOBS, "All destructors must be called.");
        TEST(job_run_count == NUM_JOBS, "All jobs must be run.");

    }
}


void test_jobs2()
{
    auto tpf = ThreadPoolFactory();
    auto tp = tpf.create_thread_pool(4, false);

    const int NUM_JOBS = 200;

    std::cout << std::endl << __FILE__ << ":" << __LINE__ << " Executing test 2" << std::endl;

    if (tp)
    {
        std::atomic<int>        job_constructor_count   = 0;
        std::atomic<int>        job_destructor_count    = 0;
        std::atomic<int>        job_run_count           = 0;


        tp->m_is_debug = false;

        for (int i = 0; i < NUM_JOBS; i++)
        {
            auto bt = new BasicTest(&job_constructor_count, &job_destructor_count, &job_run_count);
            bt->m_is_debug = false;
            tp->add_job(std::shared_ptr<JobInterface>(bt));
        }

        sleep(3);
        delete tp;
        sleep(2);

        //std::cout << job_constructor_count << " " << job_destructor_count << " " << job_run_count << std::endl;
        TEST(job_constructor_count && job_constructor_count == NUM_JOBS, "All constructors must be called.");
        TEST(job_destructor_count && job_destructor_count == NUM_JOBS, "All destructors must be called.");
        TEST(job_run_count && job_run_count != NUM_JOBS, "All jobs cannot run within the time frame.");

    }
}

int main(int argc, char** argv)
{
    test_jobs();
    test_jobs2();
}