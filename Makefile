.PHONY: clean shell.out daemon.out

shell.out: shell.cpp utils.cpp menu.cpp ringbuffer.cpp
	g++ -std=c++11 -o $@ $^

daemon.out: daemon.cpp utils.cpp
	g++ -std=c++11 -o $@ $^

clean:
	rm -rf *.o *.out
