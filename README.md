DistSysProj2
============

Multithreaded FTP (Project 2 for Distributed Systems)

Tasks:
1. Add second port and connection on server and on client (data port)
2. Handle "&" parameter in user command for get and put
3. Fix get() and put() so that if running in background, they will run on the data connection
4. Add terminate() (thread_cancel function will be useful)
5. Write some data structure to keep track of whether or not a file is locked (unordered_map)s