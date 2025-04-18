/**
 * @file helloworld.h
 * @brief hello world header for long mode
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___HELLO_WORLD_H
/*! prevent duplicate header error macro */
#define ___HELLO_WORLD_H 0

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief returns hello world string
 * @return "Hello World\r\n" string
 */
const char_t* hello_world(void);

void hello_world_cpp_test(void);

#ifdef __cplusplus
}
#endif

#endif
