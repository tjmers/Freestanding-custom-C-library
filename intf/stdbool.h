#ifndef __STD__BOOL_H
#define __STD__BOOL_H

// C++ has bool, true, and false built in
#ifndef __cplusplus

// Use the built in _Bool type
typedef _Bool bool;
#define true 1
#define false 0

#endif

#endif
