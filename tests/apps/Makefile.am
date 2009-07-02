### Makefile.am for kerrighed tests
###
### Copyright 2007 INRIA, All rights reserved
### Copyright 2009 Kerlabs, All rights reserved
###
### Authors:
###   Jean Parpaillon <jean.parpaillon@kerlabs.com>
###
bin_PROGRAMS = bi bi-cr bi-cr-static bi-file bi-signal bi-double bi-clone-files bi-clone-fs bi-clone-semundo bi-thread bi-server-socket ipcshm-tool
lib_LTLIBRARIES = libbi.la
include_HEADERS = libbi.h

libbi_la_SOURCES = libbi.c

bi_SOURCES = bi.c

bi_cr_SOURCES = bi-cr.c
bi_cr_LDADD = -lbi
bi_cr_DEPENDENCIES = libbi.la

bi_cr_static_SOURCES = bi-cr.c
bi_cr_static_CFLAGS = -static
bi_cr_static_LDADD = -lbi
bi_cr_static_DEPENDENCIES = libbi.la

bi_file_SOURCES = bi-file.c
bi_file_LDADD = -lbi
bi_file_DEPENDENCIES = libbi.la

bi_signal_SOURCES = bi-signal.c
bi_signal_LDADD = -lbi
bi_signal_DEPENDENCIES = libbi.la

bi_double_SOURCES = bi-double.c
bi_double_LDADD = -lbi
bi_double_DEPENDENCIES = libbi.la

bi_clone_files_SOURCES = bi-clone-files.c
bi_clone_files_LDADD = -lbi
bi_clone_files_DEPENDENCIES = libbi.la

bi_clone_fs_SOURCES = bi-clone-fs.c
bi_clone_fs_LDADD = -lbi
bi_clone_fs_DEPENDENCIES = libbi.la

bi_clone_semundo_SOURCES = bi-clone-semundo.c
bi_clone_semundo_LDADD = -lbi
bi_clone_semundo_DEPENDENCIES = libbi.la

bi_thread_SOURCES = bi-thread.c
bi_thread_LDADD = -lbi -lpthread
bi_thread_DEPENDENCIES = libbi.la

bi_server_socket_SOURCES = bi-server-socket.c
bi_server_socket_CFLAGS = -I@top_srcdir@/libs/include
bi_server_socket_LDADD = @top_builddir@/libs/libkrgcb/libkrgcb.la @top_builddir@/libs/libkerrighed/libkerrighed.la -lbi
bi_server_socket_DEPENDENCIES = libbi.la

ipcshm_tool_SOURCES = ipcshm-tool.c
ipcshm_tool_LDADD = -lbi
ipcshm_tool_DEPENDENCIES = libbi.la
