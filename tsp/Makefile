
load_test: load_test.cpp loader.h
	g++ -O3 -std=c++20  -Wall -Wextra -pedantic load_test.cpp -o load_test -lstdc++ -lm

normalize_tour: normalize_tour.cpp loader.h
	g++ -O3 -std=c++20  -Wall -Wextra -pedantic normalize_tour.cpp -o normalize_tour -lstdc++ -lm

greedy: greedy.cpp loader.h random_access_list.h tsp_tour.h utils.h RR_greedy.h
	g++ -O3 -std=c++20  -Wall -Wextra -pedantic greedy.cpp -o greedy -lstdc++ -lm

memopt: greedy.cpp loader.h random_access_list.h tsp_tour.h utils.h RR_greedy.h
	g++ -O3 -std=c++20  -Wall -Wextra -pedantic greedy.cpp -o greedy -lstdc++ -lm -DMEMOPT

ezxdisp:
	g++ -O3 -std=c++20  -Wall -Wextra -pedantic greedy.cpp -o greedy -lstdc++ -lm -Dezxdisp -lezx -lX11

cezxdisp:
	clang++ -O3 -std=c++20  -Wall -Wextra -pedantic greedy.cpp -o greedy -lstdc++ -lm -Dezxdisp -lezx -lX11

memoptezx:
	g++ -O3 -std=c++20  -Wall -Wextra -pedantic greedy.cpp -o greedy -lstdc++ -lm -Dezxdisp -lezx -lX11 -DMEMOPT

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

cpplint9: normalize_tour.cpp
	cpplint --filter=-legal/copyright,-runtime/references $<

cppcheck: load_test.cpp
	cppcheck --enable=all --suppress=missingIncludeSystem $<

cppcheck2: loader.h
	cppcheck --language=c++ --enable=all --suppress=missingIncludeSystem --suppress=unusedFunction $<

cppcheck3: greedy.cpp
	cppcheck --enable=all --suppress=missingIncludeSystem --suppress=useInitializationList $<

cppcheck4: random_access_list.h
	cppcheck --language=c++ --enable=all --suppress=missingIncludeSystem --suppress=useInitializationList --suppress=unusedFunction $<

cppcheck5: tsp_tour.h
	cppcheck --language=c++ --enable=all --suppress=missingIncludeSystem --suppress=unusedFunction $<

cppcheck6: utils.h
	cppcheck --language=c++ --enable=all --suppress=missingIncludeSystem --suppress=unusedFunction $<

cppcheck7: disp_utils.h
	cppcheck --language=c++ --enable=all --suppress=missingIncludeSystem --suppress=unusedFunction $<

cppcheck8: RR_greedy.h
	cppcheck --language=c++ --enable=all --suppress=missingIncludeSystem --suppress=unusedFunction $<

cppcheck9: normalize_tour.cpp
	cppcheck --enable=all --suppress=missingIncludeSystem $<

clean:
	rm -f load_test greedy
