/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include "setup.h"
#include <map.h>
#include <strings.h>
#include <utils.h>
#include <bplustree.h>
#include <xxhash.h>

uint32_t main(uint32_t argc, char_t** argv) {

    UNUSED(argc);
    UNUSED(argv);

    map_t* map = map_integer();

    if(map == NULL) {
        print_error("cannot create map");
        return -1;
    }

    map_insert(map, (void*)3, "elma");
    map_insert(map, (void*)5, "armut");

    if(map_size(map) != 2) {
        print_error("map size failed");
        map_destroy(map);

        return -1;
    }

    const char_t* test;

    test = map_get(map, (void*)3);

    if(test == NULL || strcmp(test, "elma") != 0) {
        print_error("cannot get data");
        map_destroy(map);

        return -1;
    }

    test = map_insert(map, (void*)5, "ayva");

    if(test == NULL || strcmp(test, "armut") != 0) {
        print_error("cannot replace data");
        map_destroy(map);

        return -1;
    }

    map_delete(map, (void*)3);

    if(map_exists(map, (void*)3)) {
        print_error("cannot delete data");
        map_destroy(map);

        return -1;
    }

    if(map_size(map) != 1) {
        print_error("map size should be 1");
        printf("map size %lli\n", map_size(map));
        map_destroy(map);

        return -1;
    }

    map_destroy(map);

    map = map_string();

    map_insert(map, "elma", "armut");
    map_insert(map, "ayva", "kel mahmut");

    test = map_get(map, "elma");

    if(strcmp(test, "armut") != 0) {
        print_error("cannot get data");
        map_destroy(map);

        return -1;
    }

    iterator_t* iter = map_create_iterator(map);

    while(iter->end_of_iterator(iter) != 0) {
        const char_t* data = iter->get_item(iter);

        printf("data: %s\n", data);

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    map_destroy(map);


    print_success("TESTS PASSED");

    return 0;
}
