MKPATH=mk/
include $(MKPATH)buildsys.mk

SUBDIRS = src lib
CLEAN = *.dll *.exe
DISTCLEAN = config.status config.log docs/.deps \
	mk/buildsys.mk mk/extra.mk
REPOCLEAN = aclocal.m4 autom4te.cache configure src/autoconf.h.in version

# Add the AP includes and definitions from reference project
AP_INCLUDES = -Isrc/submodules/wswrap/include\
              -Isrc/submodules/json/include\
              -Isrc/submodules/websocketpp\
              -Isrc/submodules/asio/include\
              -Isrc/submodules/valijson/include\
              -Isrc/submodules

AP_LIBS = -lwsock32 -lws2_32\
          -lssl -lcrypto -lcrypt32

AP_DEFINES = -DASIO_STANDALONE

# Add these to both C and C++ compilation flags
CFLAGS += $(AP_INCLUDES) $(AP_DEFINES)
CXXFLAGS += $(AP_INCLUDES) $(AP_DEFINES)
LDFLAGS += -lstdc++ $(AP_LIBS)

# Automatically find all .c files in src and its subdirectories
C_SRCS := $(shell find src -name '*.c')
C_OBJS := $(C_SRCS:.c=.o)

# Add your C++ files specifically
CPP_SRCS := src/apinterface.cpp
CPP_OBJS := $(CPP_SRCS:.cpp=.o)

ALL_OBJS = $(OBJS) $(CPP_OBJS)

# Pattern rules for compilation
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

TAG = angband-`cd scripts && ./version.sh`
OUT = $(TAG).tar.gz

.PHONY: check tests manual manual-optional dist

all: $(PROG)

# Link everything together
$(PROG): $(ALL_OBJS)
	$(CXX) -o $@ $(ALL_OBJS) $(LDFLAGS)

manual: manual-optional
	@if test x"$(SPHINXBUILD)" = x || test x"$(SPHINXBUILD)" = xNOTFOUND ; then \
		echo "sphinx-build was not found during configuration.  If it is not installed, you will have to install it.  You can either rerun the configuration or set SPHINXBUILD on the command line when running make to inform make how to run sphinx-build.  You may also want to set DOC_HTML_THEME to a builtin Sphinx theme to use instead of what is configured in docs/conf.py.  For instance, 'DOC_HTML_THEME=classic'." ; \
		exit 1 ; \
	fi

manual-optional:
	@if test ! x"$(SPHINXBUILD)" = x && test ! x"$(SPHINXBUILD)" = xNOTFOUND ; then \
		env DOC_HTML_THEME="$(DOC_HTML_THEME)" $(MAKE) -C docs SPHINXBUILD="$(SPHINXBUILD)" html ; \
	fi

dist:
	git checkout-index --prefix=$(TAG)/ -a
	scripts/version.sh > $(TAG)/version
	$(TAG)/autogen.sh
	rm -rf $(TAG)/autogen.sh $(TAG)/autom4te.cache
	tar --exclude .gitignore --exclude *.dll --exclude .github \
		--exclude .travis.yml -czvf $(OUT) $(TAG)
	rm -rf $(TAG)

install-extra:
	@if test x"$(NOINSTALL)" != xyes && test ! x"$(SPHINXBUILD)" = x && test ! x"$(SPHINXBUILD)" = xNOTFOUND ; then \
		for x in `find docs/_build/html -mindepth 1 \! -type d \! -name .buildinfo -print`; do \
			i=`echo $$x | sed -e s%^docs/_build/html/%%`; \
			${INSTALL_STATUS}; \
			if ${MKDIR_P} $$(dirname ${DESTDIR}${docdatadir}/$$i) && ${INSTALL} -m 644 $$x ${DESTDIR}${docdatadir}/$$i; then \
				${INSTALL_OK}; \
			else \
				${INSTALL_FAILED}; \
			fi \
		done \
	fi

pre-clean:
	@if test ! x"$(SPHINXBUILD)" = x && test ! x"$(SPHINXBUILD)" = xNOTFOUND ; then \
		$(MAKE) -C docs SPHINXBUILD="$(SPHINXBUILD)" clean ; \
	fi

pre-distclean:
	@find tests -name run.out -exec rm {} \;

repoclean: distclean
	for i in "" $(REPOCLEAN) ; do \
		test x"$$i" = x"" && continue; \
		if test -d "$$i" || test -f "$$i" ; then rm -rf "$$i" ; fi \
	done