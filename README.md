# dynprof
Dyninst-based gprof equivalent

## Building
Run `make`

## Running
`export DYNINSTAPI_RT_LIB=/usr/local/lib/libdyninstAPI_RT.so`
`./dyninst $pid`

## TODO
- Handle errors from system functions, especially malloc
- Use more c++ as opposed to c functionality.
- Don't leak memory
- Dynamically loaded code?
- insertInitCallback/insertFiniCallback
