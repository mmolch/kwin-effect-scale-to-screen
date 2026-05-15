all:
	cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Debug -DPROJECT_VERSION_SUFFIX=ABCDE1234 && cmake --build build

clean:
	rm -rf build
