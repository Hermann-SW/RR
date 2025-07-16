
load_test: load_test.cpp loader.h
	g++ -O3 -std=c++20  -Wall -Wextra -pedantic load_test.cpp -o load_test -lstdc++ -lm

greedy: greedy.cpp loader.h
	g++ -O3 -std=c++20  -Wall -Wextra -pedantic greedy.cpp -o greedy -lstdc++ -lm

ezxdisp:
	g++ -O3 -std=c++20  -Wall -Wextra -pedantic greedy.cpp -o greedy -lstdc++ -lm -Dezxdisp -lezx -lX11

cpplint: load_test.cpp
	cpplint --filter=-legal/copyright,-runtime/references $<

cpplint2: loader.h
	cpplint --filter=-legal/copyright,-runtime/references $<

cpplint3: greedy.cpp
	cpplint --filter=-legal/copyright,-runtime/references $<

cppcheck: load_test.cpp
	cppcheck --enable=all --suppress=missingInclude $<

cppcheck2: loader.h
	cppcheck --language=c++ --enable=all --suppress=missingInclude $<

cppcheck3: greedy.cpp
	cppcheck --enable=all --suppress=missingInclude $<

clean:
	rm -f load_test greedy
