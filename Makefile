.PHONY: clean

SRCS := $(wildcard *.cpp) \
		$(wildcard lib/util/*.cpp)
OBJS := $(patsubst %.cpp,%.o,$(SRCS))

CXX := g++
CXXHEADERS := -I. -Ilib/util
CXXFLAGS := $(CXXHEADERS) -std=c++11 -Os -ffast-math -fno-rtti -fno-exceptions -Wno-null-dereference
LDFLAGS := -Llib/SOIL -Llib/GLFW -Llib -L.

ifeq '$(findstring ;,$(PATH))' ';'
    OS := Windows
else
    OS := $(shell uname 2>/dev/null || echo Unknown)
    OS := $(patsubst CYGWIN%,Cygwin,$(OS))
    OS := $(patsubst MSYS%,MSYS,$(OS))
    OS := $(patsubst MINGW%,MinGW,$(OS))
endif

ifeq (${OS}, Linux)
	LDLIBS := -lglfw -lGL -ldl
	LIBNAME := libdraw.so
endif

ifeq (${OS}, MSYS)
	LDLIBS := -lopengl32 -lgdi32 -lkernel32 -luser32 -lcomdlg32 -lglfw3.dll -lglfw3
	LDFLAGS += -static -static-libgcc -static-libstdc++ -L/mingw64/lib 
	LIBNAME := libdraw.dll
endif

ifeq (${OS}, MinGW)
	LDLIBS := -lopengl32 -lgdi32 -lkernel32 -luser32 -lcomdlg32 -lglfw3.dll -lglfw3
	LDFLAGS += -static -static-libgcc -static-libstdc++ -L/mingw64/lib 
	LIBNAME := libdraw.dll
endif

clean:
	rm -f *.o lib/GLAD/*.o lib/util/*.o test/*.exe libdraw.dll libdraw.so libdraw.a

libdraw.a: $(OBJS) lib/GLAD/glad.o lib/util/hash.o lib/util/io.o lib/util/str.o
	mkdir __make_tmp
	cd __make_tmp; ar xv ../lib/SOIL/libSOIL.a; cd ..
	ar rcs libdraw.a $^ __make_tmp/*.o
	rm -r __make_tmp

libdraw.so: $(OBJS) lib/GLAD/glad.o lib/util/hash.o lib/util/io.o lib/util/str.o
	${CXX} -fPIC -shared ${LDFLAGS} $^ -o $@ -lSOIL ${LDLIBS}

libdraw.dll: $(OBJS) lib/GLAD/glad.o lib/util/hash.o lib/util/io.o lib/util/str.o
	${CXX} -fPIC -shared ${LDFLAGS} $^ -o $@ -lSOIL ${LDLIBS}
	
test/%: test/%.cpp $(LIBNAME)
	$(CXX) $(CXXFLAGS) -Wl,-rpath=$$(pwd) -L. $< -o $@ -ldraw

lib/GLAD/glad.o: lib/GLAD/glad.c
	$(CXX) $(CXXFLAGS) -fPIC -c $< -o $@

%.o: %.cpp %.h
	$(CXX) $(CXXFLAGS) -fPIC -c $< -o $@
