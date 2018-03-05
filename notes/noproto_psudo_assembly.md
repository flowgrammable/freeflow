The following protocol-independent packet processing pseudo-assembly language
was inspired by NVIDIA's PTX.  The style of defining types and state space (locality) have been mimicked.  Types have been expanded to support arbitrary bit-widths.  State Space (Locality) has been re-defined for use within a packet processing environment.

There are no limits to the number of variables.  Every variable is defined with a {type, locality} pair.  The type is officially unbounded and intended to match the packet field's size.  Providing arbitrary bit-widths is intended to allow the translation later to combines (packs) multiple fields if target machine can efficiently support packing/unpacking to preserve memory footprint.  Similarly, fields larger than native words on target machine can be supported through bitwise concatenation (for un-typed bit-fields), or potentially big integer emulation (for .u or .s).  Floating

## Types
All types are defined in used widths: {n}.  Translation layer will find optimal
mapping for target machine.  This also allows for
```
.b{n} -- un-typed bit-field
.u{n} -- unsigned type
.s{n} -- signed (2's complement) type
.f{16,32,64,128} -- IEEE floating point standards
.pred -- 1-bit predicate
```

## Memory Regions

Temporary
```
.reg
.const
.special
```

Packet Context
```
.local
.global
```

Data Plane Memory
```
.shared
.statistic
```


Defines the lifetime requirements of the variable and designates ownership
for write permissions and resource management.  Declaring locality potentially
allows the translator to optimally decide the location for the variable.  This
gives hints as to how critical the data is.  i.e.:

- Temporary (try to keep as physical register)
- Memory within packet context
- Packet Field vs. Statistic
- Constant (Potentially can exist entirely in immediate instructions)

Persists with life of program block (np_program):
```
.reg -- fast, not persistent after np_program returns
.const -- fast, not writable, may not actually exist
.special -- special processor/thread specific information (may not be needed)
```

Persists with life of Packet Context:
```
.local -- [not atomic] private within packet context (e.g. key)
.global -- [not atomic] visible to entire data plane but only modifiable by
  current owner of packet context (i.e. an np_program)
```

Persists with life of Data Plane:
```
.shared -- [atomic] modifiable by entire data plane (e.g. configuration)
.statistic -- [relative atomic] non-blocking write with increment or addition
  not based on writing an absolute value (order independent).  Value at time of
  read is not guaranteed to be precise (with respect to exact pipeline state).
```

NOTE: Need mechanism to manage ownership / lifetime of shared variables.

## Instructions


Load/Stores against fields in packet




## Example
```
.np_program extract (.param .b32 N, .param .b32 buffer[32]) {
 .reg.u32 $n, $r;
 .reg.f32 $f;
 .reg.pred $p;
 
 ld.param.u32 $n, [N];
 mov.u32 $r, buffer; // forces buffer to .local state space
 
Loop:
 setp.eq.u32 $p, $n, 0;
@p: bra Done;
 ld.local.f32 $f, [$r];
 ...
 add.u32 $r, $r, 4;
 sub.u32 $n, $n, 1;
 bra Loop;
Done:
 ...
```
 
