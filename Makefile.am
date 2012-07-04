EXTRA_DIST = \
	autogen.sh \
	gst-autogen.sh \
	m4/as-version.m4 \
	m4/as-compiler-flag.m4

plugin_LTLIBRARIES = libgstdtapi.la

# sources used to compile this plug-in
libgstdtapi_la_SOURCES = \
	src/gstdtapi.c \
	src/gstdtapisink.cpp

libgstdtapi_la_CPPFLAGS = $(GST_CFLAGS) $(GST_BASE_CFLAGS) $(DTAPI_CFLAGS)
libgstdtapi_la_LIBADD   = $(GST_LIBS)   $(GST_BASE_LIBS)   $(DTAPI_LIBS)

# headers we need but don't want installed
noinst_HEADERS = \
	src/gstdtapisink.h
