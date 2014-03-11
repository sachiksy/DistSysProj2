DistSysProj2
============

Multithreaded FTP (Project 2 for Distributed Systems)

Tasks:
1. Rework Project 1 to allow multiple simultaneous clients
2. Add second port and connection on server and on client (data port)
3. Handle "&" parameter in user command for get and put
4. Fix get() and put() so that if running in background, they will run on the data connection
5. Add terminate() (thread_cancel function will be useful)
6. Write some data structure to keep track of whether or not a file is locked
(Maybe a linked list w/filename and mutex for each node?)