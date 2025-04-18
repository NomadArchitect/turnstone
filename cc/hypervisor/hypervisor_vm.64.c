/**
 * @file hypervisor_vm.64.c
 * @brief Hypervisor Virtual Machine Management
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#include <hypervisor/hypervisor_vm.h>
#include <hypervisor/hypervisor_ipc.h>
#include <hypervisor/hypervisor_utils.h>
#include <list.h>
#include <cpu/task.h>
#include <cpu.h>
#include <memory.h>
#include <memory/paging.h>
#include <logging.h>
#include <time.h>
#include <linker_utils.h>

MODULE("turnstone.hypervisor");

list_t* hypervisor_vm_list = NULL;

extern volatile uint64_t time_timer_rdtsc_delta;

int8_t hypervisor_vm_init(void) {
    if (hypervisor_vm_list != NULL) {
        return 0;
    }

    hypervisor_vm_list = list_create_list();

    if (hypervisor_vm_list == NULL) {
        return -1;
    }

    return 0;
}

static int8_t hypervisor_vm_readonly_section_cmp(const void* a, const void* b) {
    linker_metadata_at_memory_t* ma = (linker_metadata_at_memory_t*)a;
    linker_metadata_at_memory_t* mb = (linker_metadata_at_memory_t*)b;

    uint64_t a_start = ma->section.virtual_start;
    uint64_t b_start = mb->section.virtual_start;

    uint64_t a_end = a_start + ma->section.size;
    uint64_t b_end = b_start + mb->section.size;

    if(a_end < b_start) {
        return -1;
    }

    if(b_end < a_start) {
        return 1;
    }

    return 0;
}

int8_t hypervisor_vm_create_and_attach_to_task(hypervisor_vm_t* vm) {
    if (hypervisor_vm_list == NULL) {
        return -1;
    }

    task_set_interruptible();

    list_t* mq_list = list_create_queue();

    if(mq_list == NULL) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot create message queue");
        return -1;
    }

    task_add_message_queue(mq_list);

    buffer_t* output_buffer = task_get_output_buffer();


    vm->heap = memory_get_heap(NULL);
    vm->ipc_queue = mq_list;
    vm->task_id = task_get_id();
    vm->last_tsc = rdtsc();
    vm->output_buffer = output_buffer;
    vm->msr_map = map_integer();
    vm->ept_frames = list_create_list();
    vm->loaded_module_ids = hashmap_integer(128);
    vm->read_only_frames = list_create_list();
    vm->released_pages = list_create_queue();

    list_set_equality_comparator(vm->read_only_frames, hypervisor_vm_readonly_section_cmp);

    vm->mapped_pci_devices = list_create_list();
    vm->mapped_io_ports = list_create_list();
    vm->mapped_interrupts = list_create_list();
    vm->interrupt_queue = list_create_queue();

    vm->lapic.timer_masked = true;

    list_list_insert(hypervisor_vm_list, vm);

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "vmcs frame fa: 0x%llx", vm->vmcs_frame_fa);
    task_set_vmcs_physical_address(vm->vmcs_frame_fa);
    task_set_vm(vm);

    return 0;
}

void hypervisor_vm_destroy(hypervisor_vm_t* vm) {
    if (vm == NULL) {
        return;
    }

    list_list_delete(hypervisor_vm_list, vm);

    list_destroy(vm->ipc_queue);
    map_destroy(vm->msr_map);
    hashmap_destroy(vm->loaded_module_ids);

    list_destroy(vm->mapped_pci_devices);
    list_destroy(vm->mapped_io_ports);
    hypervisor_cleanup_mapped_interrupts(vm);
    list_destroy(vm->mapped_interrupts);
    list_destroy(vm->interrupt_queue);
    list_destroy(vm->released_pages);

    if(vm->host_registers) {
        memory_free_ext(vm->heap, vm->host_registers);
    }

    if(vm->guest_registers) {
        memory_free_ext(vm->heap, vm->guest_registers);
    }

    frame_t self_frame = vm->owned_frames[HYPERVISOR_VM_FRAME_TYPE_SELF];

    for(int32_t i = HYPERVISOR_VM_FRAME_TYPE_NR - 1; i > 0; i--) {
        frame_t* frame = &vm->owned_frames[i];

        PRINTLOG(HYPERVISOR, LOG_TRACE, "released 0x%llx 0x%llx", frame->frame_address, frame->frame_count);

        if(frame->frame_address != 0) {
            uint64_t frame_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(frame->frame_address);
            memory_memclean((void*)frame_va, FRAME_SIZE * frame->frame_count);

            if(memory_paging_delete_va_for_frame_ext(NULL, frame_va, frame) != 0 ) {
                PRINTLOG(TASKING, LOG_ERROR, "cannot remove pages for stack at va 0x%llx", frame_va);
            }

            if(frame_get_allocator()->release_frame(frame_get_allocator(), frame) != 0) {
                PRINTLOG(TASKING, LOG_ERROR, "cannot release stack with frames at 0x%llx with count 0x%llx",
                         frame->frame_address, frame->frame_count);
            }

        }
    }


    for(uint64_t fi = 0; fi < list_size(vm->ept_frames); fi++) {
        frame_t* ept_frame = (frame_t*)list_get_data_at_position(vm->ept_frames, fi);

        PRINTLOG(HYPERVISOR, LOG_TRACE, "released 0x%llx 0x%llx", ept_frame->frame_address, ept_frame->frame_count);

        uint64_t frame_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(ept_frame->frame_address);
        memory_memclean((void*)frame_va, FRAME_SIZE * ept_frame->frame_count);

        if(memory_paging_delete_va_for_frame_ext(NULL, frame_va, ept_frame) != 0 ) {
            PRINTLOG(TASKING, LOG_ERROR, "cannot remove pages for stack at va 0x%llx", frame_va);
        }

        if(frame_get_allocator()->release_frame(frame_get_allocator(), ept_frame) != 0) {
            PRINTLOG(TASKING, LOG_ERROR, "cannot release stack with frames at 0x%llx with count 0x%llx",
                     ept_frame->frame_address, ept_frame->frame_count);
        }
    }

    list_destroy(vm->ept_frames);

    uint64_t got_address = vm->got_physical_address;
    uint64_t got_size = vm->got_size;
    uint64_t got_frame_count = (got_size + FRAME_SIZE - 1) / FRAME_SIZE;

    if(got_address != 0) {
        frame_t got_frame = {.frame_address = got_address, .frame_count = got_frame_count};

        PRINTLOG(HYPERVISOR, LOG_TRACE, "released 0x%llx 0x%llx", got_frame.frame_address, got_frame.frame_count);

        uint64_t frame_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(got_frame.frame_address);

        memory_memclean((void*)frame_va, FRAME_SIZE * got_frame.frame_count);

        if(memory_paging_delete_va_for_frame_ext(NULL, frame_va, &got_frame) != 0 ) {
            PRINTLOG(TASKING, LOG_ERROR, "cannot remove pages for stack at va 0x%llx", frame_va);
        }

        if(frame_get_allocator()->release_frame(frame_get_allocator(), &got_frame) != 0) {
            PRINTLOG(TASKING, LOG_ERROR, "cannot release stack with frames at 0x%llx with count 0x%llx",
                     got_frame.frame_address, got_frame.frame_count);
        }
    }

    PRINTLOG(HYPERVISOR, LOG_TRACE, "released 0x%llx 0x%llx", self_frame.frame_address, self_frame.frame_count);

    uint64_t frame_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(self_frame.frame_address);
    memory_memclean((void*)frame_va, FRAME_SIZE * self_frame.frame_count);

    if(memory_paging_delete_va_for_frame_ext(NULL, frame_va, &self_frame) != 0 ) {
        PRINTLOG(TASKING, LOG_ERROR, "cannot remove pages for stack at va 0x%llx", frame_va);
    }

    if(frame_get_allocator()->release_frame(frame_get_allocator(), &self_frame) != 0) {
        PRINTLOG(TASKING, LOG_ERROR, "cannot release stack with frames at 0x%llx with count 0x%llx",
                 self_frame.frame_address, self_frame.frame_count);
    }

    memory_heap_t* dheap = memory_get_default_heap();

    memory_free_ext(dheap, (void*)vm->entry_point_name);
}

void hypervisor_vm_notify_timers(void) {
    if (hypervisor_vm_list == NULL) {
        return;
    }

    for(uint64_t i = 0; i < list_size(hypervisor_vm_list); i++) {
        hypervisor_vm_t* vm = (hypervisor_vm_t*)list_get_data_at_position(hypervisor_vm_list, i);

        if(vm == NULL) {
            continue;
        }

        if(vm->lapic.timer_masked) {
            continue;
        }

        uint64_t tsc = rdtsc();

        uint64_t delta = tsc - vm->last_tsc;
        delta /= time_timer_rdtsc_delta;
        delta *= vm->lapic.timer_divider_realvalue;

        vm->last_tsc = tsc;

        boolean_t timer_expired = false;

        if(vm->lapic.timer_current_value > delta) {
            vm->lapic.timer_current_value -= delta; // delta;
        } else {
            vm->lapic.timer_current_value = vm->lapic.timer_initial_value;
            timer_expired = true;
        }

        if(timer_expired && !vm->lapic.timer_masked && !vm->lapic.timer_exits) {
            hypervisor_ipc_send_timer_interrupt(vm);
        }
    }
}
