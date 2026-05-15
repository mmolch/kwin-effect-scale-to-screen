all:
	cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=MinSizeRel && cmake --build build

clean:
	rm -rf build
