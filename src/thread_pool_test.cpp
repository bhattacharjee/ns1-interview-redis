#include "thread_pool.h"
#include <cstdlib>
#include <unistd.h>
#include <pthread.h>

#define TEST(x, y) {\
    if (!(x))\
    {\
        std::cout << y << std::endl;\
        exit(1);\
    }\
}

class BasicTest: public JobInterface
{
public:
    int run()
    {
        std::cout << "XXX: Running job in thread " << pthread_self << std::endl;
        return 0;
    }

    ~BasicTest()
    {
        std::cout << "XXX: Destroying job " << this << std::endl;
    }
};

int main(int argc, char** argv)
{
    TEST(true, "should be true");
    auto tpf = ThreadPoolFactory();
    auto tp = tpf.create_thread_pool(4);
    if (tp)
    {
        tp->m_is_debug = true;
        tp->add_job(std::shared_ptr<JobInterface>(new BasicTest()));
        sleep(1);
        delete tp;
    }
}