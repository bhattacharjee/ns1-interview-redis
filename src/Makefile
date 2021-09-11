HEADERS = *.h
LDFLAGS = -lpthread
CPPFLAGS = -std=c++17 -g
CPP = c++ $(CPPFLAGS)


thread_pool_test: thread_pool.cpp thread_pool_test.cpp $(HEADERS)
	$(CPP) thread_pool_test.cpp thread_pool.cpp -o thread_pool_test $(LDFLAGS)

resp_parser_test: resp_parser.cpp resp_parser_test.cpp $(HEADERS)
	$(CPP) resp_parser.cpp  resp_parser_test.cpp -o resp_parser_test $(LDFLAGS)

ds_tests: data_store.cpp data_store_test.cpp $(HEADERS)
	$(CPP) data_store.cpp data_store_test.cpp -o ds_tests $(LDFLAGS)

server: orchestrator.cpp server.cpp data_store.cpp resp_parser.cpp thread_pool.cpp $(HEADERS)
	$(CPP) orchestrator.cpp server.cpp data_store.cpp resp_parser.cpp thread_pool.cpp -o server $(LDFLAGS)

test: ds_tests resp_parser_test thread_pool_test 

clean:
	rm -f thread_pool_test ds_tests resp_parser_test *.o