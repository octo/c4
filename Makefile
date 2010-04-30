CC = gcc
CPPFLAGS = 
CFLAGS = -Wall -Wextra -O0 -g
LDFLAGS = 
LDLIBS = 

all: test fcgi_test

clean:
	rm -f test.cgi

common.o: common.c common.h

graph_list.o: graph_list.c graph_list.h

utils_params.o: utils_params.c utils_params.h

action_list_graphs.o: action_list_graphs.c action_list_graphs.h

test: test.c utils_params.o

fcgi_test: LDLIBS = -lfcgi
fcgi_test: fcgi_test.c common.o graph_list.o utils_params.o action_list_graphs.o

.PHONY: clean

