# DynProf
Dyninst-based gprof equivalent

## Building
Run `make`

## Running
`export DYNINSTAPI_RT_LIB=/usr/local/lib/libdyninstAPI_RT.so`
`./dyninst /path/to/binary`
or `./dyninst --write /path/to/binary && ./dynprof_binary`

## Output
Run `make output`

## TODO
- Figure out why the elapsed time is sometimes horribly wrong.
- Figure out why g++ causes the elapsed time to be mis-calculated.
- Figure out a good way to record parent-child function relationships.
