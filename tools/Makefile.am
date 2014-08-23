

noinst_PROGRAMS = client-test proxy-server

client_test_SOURCES = client-test.c
client_test_CFLAGS = $(GST_RTMP_CFLAGS) $(GST_CFLAGS) -I../rtmp
client_test_LDADD = ../rtmp/libgstrtmp-1.0.la $(GST_LIBS)

proxy_server_SOURCES = proxy-server.c
proxy_server_CFLAGS = $(GST_RTMP_CFLAGS) $(GST_CFLAGS) -I../rtmp
proxy_server_LDADD = ../rtmp/libgstrtmp-1.0.la $(GST_LIBS)

