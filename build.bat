rem Builds and installs bootpd into .\workspace, see README.md
conan install . --build=missing -s compiler.cppstd=20 -o "&:with_tests=False" || exit /b 1
cmake --preset conan-default || exit /b 1
cmake --build --preset conan-release || exit /b 1
cmake --install build --config Release --prefix workspace
