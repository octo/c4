AUTOMAKE_OPTIONS = foreign no-dependencies

AM_CPPFLAGS = -DPREFIX='"${prefix}"'
AM_CPPFLAGS += -DCONFIGFILE='"${sysconfdir}/collection.conf"'

if COMPILER_IS_GCC
AM_CFLAGS = -Wall -Wextra
endif

AM_YFLAGS = -d

BUILT_SOURCES = parser.h

bin_PROGRAMS = collection.fcgi

collection_fcgi_SOURCES = main.c \
			  oconfig.c oconfig.h aux_types.h scanner.l parser.y \
			  action_graph.c action_graph.h \
			  action_instance_data_json.c action_instance_data_json.h \
			  action_graph_def_json.c action_graph_def_json.h \
			  action_list_graphs.c action_list_graphs.h \
			  action_list_hosts.c action_list_hosts.h \
			  action_search.c action_search.h \
			  action_search_json.c action_search_json.h \
			  action_show_graph.c action_show_graph.h \
			  action_show_graph_json.c action_show_graph_json.h \
			  action_show_instance.c action_show_instance.h \
			  common.c common.h \
			  data_provider.c data_provider.h \
			  dp_rrdtool.c dp_rrdtool.h \
			  filesystem.c filesystem.h \
			  graph_types.h \
			  graph.c graph.h \
			  graph_config.c graph_config.h \
			  graph_def.c graph_def.h \
			  graph_ident.c graph_ident.h \
			  graph_instance.c graph_instance.h \
			  graph_list.c graph_list.h \
			  rrd_args.c rrd_args.h \
			  utils_array.c utils_array.h \
			  utils_cgi.c utils_cgi.h \
			  utils_search.c utils_search.h
