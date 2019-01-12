#!/bin/bash

em++ -O3 -std=c++11 \
    -DCG_OUTPUT -DCG_LOGGING=10 \
    -s WASM=1 \
    -s ASSERTIONS=1 \
    -s USE_PTHREADS=1 \
    -s PTHREAD_POOL_SIZE=4 \
    -s TOTAL_MEMORY=134217728 \
    -s EXPORTED_FUNCTIONS='["_main"]' \
    -s EXTRA_EXPORTED_RUNTIME_METHODS='["ccall", "cwrap"]' \
    -I ../include \
    ../src/Communicator.cpp \
    ../tools/test_client.cpp \
    -o test_client.html

cp -v test_client*     ../../ggerganov.github.io/private/
cp -v pthread-main.js  ../../ggerganov.github.io/private/
