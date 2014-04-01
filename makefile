prog: ftpclient.cpp ftpserver.cpp
	g++ -std=c++0x ftpserver.cpp -Wall -pthread -o myftpserver
	g++ ftpclient.cpp -Wall -o myftp
	
ser:
	./myftpserver 1459 1460
	
cli:
	./myftp cf0.cs.uga.edu 1459 1460
	
clean:
	rm myftpserver myftp
