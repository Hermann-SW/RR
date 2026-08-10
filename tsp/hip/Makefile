
all: load_test gen.ml100K.UINT16_L

load_test: load_test.cpp ../loader.h
	g++ -O3 -std=c++20  -Wall -Wextra -pedantic load_test.cpp -o load_test -lstdc++ -lm

gen.ml100K.UINT16_L: gen.ml100K.UINT16_L.cpp ../loader.h
	g++ -O3 -std=c++20  -Wall -Wextra -pedantic gen.ml100K.UINT16_L.cpp -o gen.ml100K.UINT16_L -lstdc++ -lm

clean:
	rm -f load_test gen.ml100K.UINT16_L ml100K.UINT16_L.bin
