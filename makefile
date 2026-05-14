all: server client

server: server.cpp mysql_con.cpp redis_con.cpp
	g++ -o server server.cpp mysql_con.cpp redis_con.cpp -ljsoncpp -levent -lmysqlclient -lhiredis -lcrypto -pthread

client: client.cpp
	g++ -o client client.cpp -ljsoncpp -lcrypto

debug: server.cpp mysql_con.cpp redis_con.cpp
	g++ -g -o server_debug server.cpp mysql_con.cpp redis_con.cpp -ljsoncpp -levent -lmysqlclient -lhiredis -lcrypto

clean:
	rm -f *.o server client server_debug