/**
 * @file vm_test_program.64.c
 * @brief VM Test Program
 *
 * This is a sample program that will run in the VM.
 * It has minimal dependencies and is used to test the VM.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#include <types.h>
#include <hypervisor/hypervisor_guestlib.h>
#include <cpu.h>
#include <ports.h>
#include <buffer.h>
#include <stdbufs.h>

MODULE("turnstone.user.programs.vmedu");

_Noreturn void vmedu(void);


_Noreturn void vmedu(void) {
    vm_guest_print("VM EDU Passthrough Test Program\n");
    uint64_t heap_base = 4ULL << 40;
    uint64_t heap_size = 16ULL << 20;
    memory_heap_t* heap = memory_create_heap_simple(heap_base, heap_base + heap_size);

    if(heap == NULL) {
        vm_guest_print("Failed to create heap\n");
        vm_guest_halt();
    }

    memory_set_default_heap(heap);

    stdbufs_init_buffers(vm_guest_print);

    printf("base init done\n");

    uint64_t hpa = vm_guest_get_host_physical_address((uint64_t)heap);

    printf("Heap HPA: 0x%llx\n", hpa);

    vm_guest_exit();

}
