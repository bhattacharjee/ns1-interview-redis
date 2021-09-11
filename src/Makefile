HEADERS = *.h
LDFLAGS = -lpthread
CPPFLAGS = -std=c++17
CPP = c++ $(CPPFLAGS)


thread_pool_test: thread_pool.cpp $(HEADERS)
	$(CPP) thread_pool_test.cpp thread_pool.cpp -o thread_pool_test $(LDFLAGS)

test: thread_pool_test

clean:
	rm -f thread_pool_test *.o