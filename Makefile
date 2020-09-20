# smtp-js-http

# Copyright (c) 2020 James Kinnaird
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

CXXFLAGS=--std=c++11 -DDUK_USE_CPP_EXCEPTIONS -Ithirdparty/concurrentqueue-1.0.1 -Ithirdparty/spdlog-1.6.1/include -Ithirdparty/tclap-1.2.2/include -Ithirdparty/duktape-2.5.0/src -Ithirdparty/inih-r51 -Ithirdparty/inih-r51/cpp -Ithirdparty/dukglue-master/include
LIBS=`pkg-config --libs libsystemd` -lpthread `pkg-config --libs libcurl`

DEPS = src/%.hpp

OBJDIR = obj
_OBJ = smtp-js-http.o smtp.o scriptvm.o webrequest.o duktape.o ini.o inireader.o
OBJ = $(patsubst %,$(OBJDIR)/%,$(_OBJ))

$(OBJDIR)/%.o: src/%.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

smtp-js-http: $(OBJ)
	mkdir -p obj
	$(CXX) -o $@ $^ $(LIBS)

obj/smtp-js-http.o: src/smtp-js-http.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
obj/smtp.o: src/smtp.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
obj/scriptvm.o: src/scriptvm.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
obj/webrequest.o: src/webrequest.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

obj/duktape.o: thirdparty/duktape-2.5.0/src/duktape.c
	$(CXX) $(CXXFLAGS) -c -o $@ $<

obj/ini.o: thirdparty/inih-r51/ini.c
	$(CXX) $(CXXFLAGS) -c -o $@ $<
obj/inireader.o: thirdparty/inih-r51/cpp/INIReader.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

.PHONY: clean

clean:
	rm -f $(OBJDIR)/*.o *~ smtp-js-http
