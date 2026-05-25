CXX      = g++
CXXFLAGS = -std=c++17 -O2 -I include
LDFLAGS  = -pthread -lssl -lcrypto

sprachmeister: src/main.cpp
	$(CXX) $(CXXFLAGS) src/main.cpp -o $@ $(LDFLAGS)

clean:
	rm -f sprachmeister
