Here is some notes on my own compilation of SpiderMonkey:

--------------------------------------------------
Hyper linked JS opt code reference
http://mxr.mozilla.org/mozilla-central/source/js/src/jsopcode.tbl

--------------------------------------------------
A good documentation on the TraceMonkey
http://developer.mozilla.org/En/SpiderMonkey/Internals/Tracing_JIT

--------------------------------------------------
Talk about the property cache
http://www.masonchang.com/blog/2008/5/28/spidermonkeys-secret-object-sauce.html

--------------------------------------------------
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