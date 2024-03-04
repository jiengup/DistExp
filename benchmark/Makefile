CXX = g++
CXXFLAGS = -std=c++11 -isystem benchmark/include -Lbenchmark/build/src -O1
LDFLAGS = -lbenchmark -lpthread

all: sketch_benchmark structures/*.hpp

sketch_benchmark: sketch_benchmark.cpp
	$(CXX) sketch_benchmark.cpp $(CXXFLAGS) $(LDFLAGS) -o sketch_benchmark

run: sketch_benchmark
	./sketch_benchmark
clean:
	rm -f sketch_benchmark

run_csv: sketch_benchmark
	./sketch_benchmark --benchmark_format=csv --benchmark_out=sketch_benchmark2.csv
