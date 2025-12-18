AM_CFLAGS = $(GLOBAL_CFLAGS) $(libfuse_CFLAGS) $(libgeneral_CFLAGS) 
AM_CXXFLAGS = $(AM_CFLAGS) $(GLOBAL_CXXFLAGS)
AM_LDFLAGS = $(libfuse_LIBS) $(libgeneral_LIBS)

bin_PROGRAMS = orbisFSTool

orbisFSTool_CFLAGS = $(AM_CFLAGS)
orbisFSTool_CXXFLAGS = $(AM_CXXFLAGS)
orbisFSTool_LDFLAGS = $(AM_LDFLAGS)
orbisFSTool_SOURCES = main.cpp \
                      utils.cpp \
                      OrbisFSBlockAllocator.cpp \
                      OrbisFSException.cpp \
                      OrbisFSFile.cpp \
                      OrbisFSImage.cpp \
                      OrbisFSInodeDirectory.cpp \
                      OrbisFSFuse.cpp