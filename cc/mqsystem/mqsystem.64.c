/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <mqsystem/mqsystem.h>
#include <map.h>
#include <strings.h>
#include <linkedlist.h>
#include <memory.h>
#include <cpu/task.h>

map_t mqsystem_queues = NULL;
extern linkedlist_t task_queue;

typedef struct {
    char_t*      queue_name;
    uint64_t     task_id;
    linkedlist_t queue;
} mqsystem_queue_item_t;


int8_t mqsystem_init(void) {

    mqsystem_queues = map_string();

    if(mqsystem_queues == NULL) {
        return -1;
    }



    return 0;
}
