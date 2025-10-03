PHONY.: client tsamgroup9

client: client.cpp
	g++ -std=c++23 -Wall -Werror client.cpp -o $@

tsamgroup9: tsamgroup9.cpp
	g++ -std=c++23 -Wall -Werror tsamgroup9.cpp -o $@

clean:
	rm -f client tsamgroup9
