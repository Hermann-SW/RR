
load_test: load_test.cpp loader.h
	g++ -O3 -std=c++20  -Wall -Wextra -pedantic load_test.cpp -o load_test -lstdc++ -lm

greedy: greedy.cpp loader.h random_access_list.h tsp_tour.h utils.h RR_greedy.h
	g++ -O3 -std=c++20  -Wall -Wextra -pedantic greedy.cpp -o greedy -lstdc++ -lm

ezxdisp:
	g++ -O3 -std=c++20  -Wall -Wextra -pedantic greedy.cpp -o greedy -lstdc++ -lm -Dezxdisp -lezx -lX11

cpplint: load_test.cpp
	cpplint --filter=-legal/copyright,-runtime/references $<

cpplint2: loader.h
	cpplint --filter=-legal/copyright,-runtime/references $<

cpplint3: greedy.cpp
	cpplint --filter=-legal/copyright,-runtime/references $<

cpplint4: random_access_list.h
	cpplint --filter=-legal/copyright,-runtime/references $<

cpplint5: tsp_tour.h
	cpplint --filter=-legal/copyright,-runtime/references $<

cpplint6: utils.h
	cpplint --filter=-legal/copyright,-runtime/references $<

cpplint7: disp_utils.h
	cpplint --filter=-legal/copyright,-runtime/references $<

cpplint8: RR_greedy.h
	cpplint --filter=-legal/copyright,-runtime/references $<

cppcheck: load_test.cpp
	cppcheck --enable=all --suppress=missingInclude $<

cppcheck2: loader.h
	cppcheck --language=c++ --enable=all --suppress=missingInclude --suppress=unusedFunction $<

cppcheck3: greedy.cpp
	cppcheck --enable=all --suppress=missingInclude $<

cppcheck4: random_access_list.h
	cppcheck --language=c++ --enable=all --suppress=missingInclude $<

cppcheck5: tsp_tour.h
	cppcheck --language=c++ --enable=all --suppress=missingInclude --suppress=unusedFunction $<

cppcheck6: utils.h
	cppcheck --language=c++ --enable=all --suppress=missingInclude --suppress=unusedFunction $<

cppcheck7: disp_utils.h
	cppcheck --language=c++ --enable=all --suppress=missingInclude --suppress=unusedFunction $<

cppcheck8: RR_greedy.h
	cppcheck --language=c++ --enable=all --suppress=missingInclude --suppress=unusedFunction $<

clean:
	rm -f load_test greedy
