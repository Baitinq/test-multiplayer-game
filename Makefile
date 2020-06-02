CXX = g++

CXXFLAGS = -Wall
LIBS = -lncurses

main:
	$(CXX) $(CXXFLAGS) $(LIBS) server.cpp -o server
	$(CXX) $(CXXFLAGS) $(LIBS) client.cpp -o client
