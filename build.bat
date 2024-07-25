cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=workspace
cmake --build build --config Release 
cmake --install build --config Release