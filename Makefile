all:
	cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=MinSizeRel && cmake --build build

install:
	cmake --install build

clean:
	rm -rf build
