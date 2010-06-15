CC = gcc
CPPFLAGS = 
CFLAGS = -Wall -Wextra -O0 -g
LDFLAGS = 
LDLIBS = 

all: test.fcgi

clean:
	rm -f test.cgi

common.o: common.c common.h

filesystem.o: filesystem.c filesystem.h

graph.o: graph.c graph.h

graph_config.o: graph_config.c graph_config.h

graph_def.o: graph_def.c graph_def.h

graph_ident.o: graph_ident.c graph_ident.h

graph_instance.o: graph_instance.c graph_instance.h

graph_list.o: graph_list.c graph_list.h

utils_array.o: utils_array.c utils_array.h

utils_params.o: utils_params.c utils_params.h

action_graph.o: action_graph.c action_graph.h

action_list_graphs.o: action_list_graphs.c action_list_graphs.h

oconfig.o: oconfig.c oconfig.h

scanner.c: scanner.l
	flex --outfile=scanner.c scanner.l

scanner.o: scanner.c parser.h

parser.c parser.h: parser.y
	bison --output=parser.c --defines=parser.h parser.y

parser.o: parser.c

test: test.c utils_params.o

test.fcgi: LDLIBS = -lfcgi -lrrd
test.fcgi: test.fcgi.c common.o filesystem.o graph.o graph_config.o graph_def.o graph_ident.o graph_instance.o graph_list.o utils_array.o utils_params.o action_graph.o action_list_graphs.o scanner.o parser.o oconfig.o

.PHONY: clean

