#ifndef __roCommandLine_h__
#define __roCommandLine_h__

#include "roStatus.h"

namespace ro { struct String; }

// long ::= '--' chars [ ( ' ' | '=' ) chars ];
// shorts ::= '-' ( chars )* [ [ ' ' ] chars ];
// atom ::= '(' expr ')' | '[' expr ']' | 'options' | long | shorts | argument | command;
// seq ::= ( atom [ '...' ] )*;
// expr ::= seq ( '|' seq )*;
// If options_first
//		argv ::= [ long | shorts ]* [ argument ]* [ '--' [ argument ]* ];
// else
//		argv ::= [ long | shorts | argument ]* [ '--' [ argument ]* ];

roStatus roParseCommandLine(const roUtf8* usage, const roUtf8* cmd, ro::String& outJson);

#endif	// __roCommandLine_h__
