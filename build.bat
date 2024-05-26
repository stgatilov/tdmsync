conan install . -of=build --build=missing -s build_type=RelWithDebInfo
conan install . -of=build --build=missing -s build_type=Debug
cd build
cmake .. -G "Visual Studio 17 2022" -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_POLICY_DEFAULT_CMP0091=NEW
