Here is some notes on my own compilation of SpiderMonkey:

A list of files that were needed only by JIT enabled compilation

/assembler/assembler/*
/assembler/jit/*
/assembler/moco/*
/assembler/wtf/Assertions.*

/methodjit/all except:
Logging.h
MethodJIT.h
MethodJIT-inl.h
MonoIC.h
PolyIC.h
Retcon.h

/nanojit/all except:
avmplus.h
nanojit.h

/tracejit/Writer.*
/vprof/vprof.*

/imacros.c.out
/jitstats.tbl
/jsanalyze.*
/jsbuiltins.cpp
/jshotloop.h
/jstracer.cpp