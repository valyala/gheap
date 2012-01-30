CPP_COMPILER=g++
C_COMPILER=gcc

COMMON_CFLAGS=-Wall -Wextra -Werror -pedantic

DEBUG_CFLAGS=-g
OPT_CFLAGS=-DNDEBUG -O2

C_CFLAGS=$(COMMON_CFLAGS) -std=c99
CPP03_CFLAGS=$(COMMON_CFLAGS)
CPP11_CFLAGS=$(COMMON_CFLAGS) -std=c++0x -DGHEAP_CPP11

all: tests perftests ops_count_test

tests:
	$(C_COMPILER) tests.c $(C_CFLAGS) $(DEBUG_CFLAGS) -o tests_c
	$(CPP_COMPILER) tests.cpp $(CPP03_CFLAGS) $(DEBUG_CFLAGS) -o tests_cpp03
	$(CPP_COMPILER) tests.cpp $(CPP11_CFLAGS) $(DEBUG_CFLAGS) -o tests_cpp11

perftests:
	$(C_COMPILER) perftests.c $(C_CFLAGS) $(OPT_CFLAGS) -o perftests_c
	$(CPP_COMPILER) perftests.cpp $(CPP03_CFLAGS) $(OPT_CFLAGS) -o perftests_cpp03
	$(CPP_COMPILER) perftests.cpp $(CPP11_CFLAGS) $(OPT_CFLAGS) -o perftests_cpp11

ops_count_test:
	$(CPP_COMPILER) ops_count_test.cpp $(CPP03_CFLAGS) $(OPT_CFLAGS) -o ops_count_test_cpp03
	$(CPP_COMPILER) ops_count_test.cpp $(CPP11_CFLAGS) $(OPT_CFLAGS) -o ops_count_test_cpp11

clean:
	rm -f ./tests_c
	rm -f ./tests_cpp03
	rm -f ./tests_cpp11
	rm -f ./perftests_c
	rm -f ./perftests_cpp03
	rm -f ./perftests_cpp11
	rm -f ./ops_count_test_cpp03
	rm -f ./ops_count_test_cpp11
