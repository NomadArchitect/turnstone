/**
 * @file cpu_state.h
 * @brief cpu state stored at each cpu at gs segment
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___CPU_STATE_H
/*! prevent duplicate header error macro */
#define ___CPU_STATE_H 0

#include <cpu/task.h>
#include <list.h>
#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cpu_state_t {
    uint64_t  local_apic_id; ///< local apic id
    task_t*   current_task; ///< current task
    task_t*   idle_task; ///< idle task
    boolean_t tasking_enabled; ///< tasking enabled
    boolean_t task_switch_paramters_need_eoi; ///< task switch parameters need eoi
    list_t*   task_queue; ///< task list
    list_t*   task_sleep_queue; ///< task sleep list
    list_t*   task_wait_queue; ///< task wait list
    list_t*   task_cleanup_queue; ///< task cleanup list
} cpu_state_t;

#ifdef __cplusplus
}
#endif

#endif
