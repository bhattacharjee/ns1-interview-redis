# BASIC REDIS IMPLEMENTATION

## Overview
The goal was to maximize concurrency as much as possible, while at the same time,
avoiding too many threads. the following was done
1. A thread that accepts new connections
2. A thread that monitors non-ready connections via epoll to see when they become readable.
3. A thread pool with 8 threads which performs reads on ready sockets. Threads can be added to this thread poll dynamically if required.
4. A thread pool with 8 threads that parses the data read from the socket and then performs the desired command. Again this can grow dynamically.
5. A thread pool with 8 threads that sends the response back to the client. Again this can grow dynamically.

## Data Store
The data store is a hash-map. Since there are multiple threads, the hash-map must be synchronized. Reader-writer locks were used to increase parallelism.
To further increase parallelism, a **partitioning scheme** was used. Instead of a single hash-map, 10 different hash-maps were used.
Each key maps to one hash-map, and the decision is taken based on the first character of the key.

## Extended Documentation
To address the documentation is available in the documentation folder.
To access it, please open the file **documentation/html/index.html** file in a browser. Firefox is recommended.

To access the documentation, click on one of the two tabs in the main page:
1. Classes
2. Files


