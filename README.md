Simple Epoll HTTP Server

Overview

This project is a basic HTTP server built using non-blocking sockets and epoll for event-driven I/O.
It demonstrates handling multiple concurrent connections efficiently on Linux using a simple worker thread model.

The server is not production-grade, but it works fine for testing and learning purposes.

Architecture

Single-threaded epoll loop monitors client connections.
Worker threads handle request processing and responses.
Connections are wrapped with RAII helpers to ensure proper cleanup.
Simple HTTP request parser and response builder are included.

Performance

Tested on a 4-core CPU with up to 1000 concurrent connections:
The server handled all requests successfully.
Throughput: ~5,700 requests/sec at 1000 concurrent clients.
Median latency: ~40 ms, 99th percentile ~57 ms.
This shows that even a basic event-driven server can handle high concurrency reasonably well.

Features

Non-blocking sockets with epoll.
Basic worker thread pool for request handling.
RAII-based connection management.
Keep-alive support.

Notes / Future Work

This is a learning project — not optimized for production.
Could be extended with:
Custom memory pools
Zero-copy response handling
Better request parsing and routing
Pretty much almost everything ... 
