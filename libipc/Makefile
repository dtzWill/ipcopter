CC=gcc
CXX=g++

AR=ar
ARFLAGS=rcs

STATIC_LIB := libipc.a
DYNAMIC_LIB := libipc.so

SHARED_LDFLAGS = -shared $(WRAP_FLAGS) -ldl

OPTFLAGS := -O3
CFLAGS := $(OPTFLAGS) -fPIC -Wall -Wextra -Werror
CXXFLAGS := $(OPTFLAGS) -fPIC -Wall -Wextra -Werror

C_DEPS := $(wildcard *.c)
CXX_DEPS := $(wildcard *.cpp)
H_DEPS := $(wildcard *.h)

OBJS := $(C_DEPS:.c=.o) $(CXX_DEPS:.cpp=.o)

# Don't build STATIC_LIBRARY by default,
# targeting use with LD_PRELOAD and common
# benchmarks/programs
all: $(DYNAMIC_LIB)

$(STATIC_LIB): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

$(DYNAMIC_LIB): $(OBJS)
	$(CXX) $(SHARED_LDFLAGS) $^ -o $@

%.o: %.c $(H_DEPS) Makefile
	$(CC) $< $(CFLAGS) -c -o $@

%.o: %.cpp $(H_DEPS) Makefile
	$(CXX) $< $(CXXFLAGS) -c -o $@

clean:
	-rm -f $(OBJS) $(STATIC_LIB) $(DYNAMIC_LIB)
