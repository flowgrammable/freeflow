
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

Under full load (i.e., filling the input pipe), performance can be affected
by non-obvious factors. 

For example, the failure of the compiler to inline a
member function that is called repeatedly can lead to suprisingly large
overheads. In particular, in the Select_set the contains() and can_read()
calls can also potentially impact cache locality: indirection through
the this pointer may suppress obvious optimizations.

