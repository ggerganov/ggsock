# ggsock
Non-blocking sockets wrapper. Useful for building c++ socket apps with emscripten.

### Example

    [terminal 1]
    mkdir build && cd build && cmake .. && make
    ./tools/test_server
    
    [terminal 2]
    websockify 12003 127.0.0.1:12002
    
    [terminal 3]
    cd build-em
    ./compile.sh
    
    [browser]
    https://127.0.0.1/some/path/test_client.html
