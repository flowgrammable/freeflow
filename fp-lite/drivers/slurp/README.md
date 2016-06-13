
# Experimental data plane processing variants

## Single-threaded/Sequential

This is a non-asynchronous dataplane. It accepts connections from all of
its intended ports, and then processes. If ports disconnect, then the
all remaining packets are flushed through the pipeline, and the program
returns to its initial state, accepting more connections.

### Single-threaded/select

The select system call is used to determine if connections or data are
available.


## Notes

Our full-load testing may not paint an accurate portrait of efficiency. When
we test this way, the input queue almost always has data available.
When we're testing with an asynchronous model (select, epoll, kqueue), we're
issuing system calls to determine if more data is available: add a context
switch. Then we call recv() to get the data: add another context switch.

Furthermore, this kind of testing exacerbates performance concerns. In 
particular, we can use callgrind to find instances where a failure to inline
code results in a 4% performance penalty. This kind of optimization is nearly
impossible to hand-tune at the source code level.

Note that some performance penalties may also be assessed by failures to
optimize around the `this` pointer. Certain optimizations may be avoided
because of aliasing problems.
