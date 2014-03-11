prog: ftpclient.cpp ftpserver.cpp
	g++ ftpserver.cpp -Wall -lpthread -o myftpserver
	g++ ftpclient.cpp -Wall -o myftp
	
ser:
	./myftpserver 1459
	
cli:
	./myftp cf0.cs.uga.edu 1459
	
clean:
	rm myftpserver myftp
