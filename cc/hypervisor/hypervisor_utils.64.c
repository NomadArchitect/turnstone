/**
 * @file hypervisor_utils.64.c
 * @brief Hypervisor Utilities
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#include <hypervisor/hypervisor_utils.h>
#include <hypervisor/hypervisor_ept.h>
#include <hypervisor/hypervisor_guestlib.h>
#include <memory/paging.h>
#include <memory/frame.h>
#include <logging.h>
#include <cpu.h>
#include <cpu/task.h>
#include <tosdb/tosdb_manager.h>
#include <linker.h>
#include <pci.h>
#include <apic.h>

MODULE("turnstone.hypervisor");



uint64_t hypervisor_allocate_region(frame_t** frame, uint64_t size) {
    if(frame_get_allocator()->allocate_frame_by_count(frame_get_allocator(),
                                                      size / FRAME_SIZE,
                                                      FRAME_ALLOCATION_TYPE_USED | FRAME_ALLOCATION_TYPE_BLOCK,
                                                      frame, NULL) != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot allocate region frame");
        return 0;
    }

    PRINTLOG(HYPERVISOR, LOG_TRACE, "allocated 0x%llx 0x%llx", (*frame)->frame_address, (*frame)->frame_count);

    uint64_t frame_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA((*frame)->frame_address);

    if(memory_paging_add_va_for_frame(frame_va, *frame, MEMORY_PAGING_PAGE_TYPE_4K) != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot map region frame");
        return 0;
    }

    memory_memclean((void*)frame_va, size);

    return frame_va;
}

uint64_t hypervisor_create_stack(hypervisor_vm_t* vm, uint64_t stack_size) {
    frame_t* stack_frames;
    uint64_t stack_frames_cnt = (stack_size + FRAME_SIZE - 1) / FRAME_SIZE;
    stack_size = stack_frames_cnt * FRAME_SIZE;

    if(frame_get_allocator()->allocate_frame_by_count(frame_get_allocator(), stack_frames_cnt, FRAME_ALLOCATION_TYPE_USED | FRAME_ALLOCATION_TYPE_BLOCK, &stack_frames, NULL) != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot allocate stack with frame count 0x%llx", stack_frames_cnt);

        return -1;
    }

    vm->owned_frames[HYPERVISOR_VM_FRAME_TYPE_VMEXIT_STACK] = *stack_frames;

    uint64_t stack_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(stack_frames->frame_address);

    if(memory_paging_add_va_for_frame(stack_va, stack_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot add stack va 0x%llx for frame at 0x%llx with count 0x%llx", stack_va, stack_frames->frame_address, stack_frames->frame_count);

        cpu_hlt();
    }

    memory_memclean((void*)stack_va, stack_size);

    PRINTLOG(HYPERVISOR, LOG_TRACE, "stack va 0x%llx[0x%llx]", stack_va, stack_size);

    return stack_va + stack_size - 16;
}

static void hypervisor_cleanup_unused_modules(hypervisor_vm_t * vm, uint64_t got_fa, uint64_t got_size){
    uint64_t got_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(got_fa);
    uint64_t got_entry_count = got_size / sizeof(linker_global_offset_table_entry_t);
    linker_global_offset_table_entry_t* got_entries = (linker_global_offset_table_entry_t*)got_va;

    PRINTLOG(HYPERVISOR, LOG_TRACE, "got 0x%llx 0x%llx", got_fa, got_size);

    for(uint64_t i = 2; i < got_entry_count; i++) {
        linker_global_offset_table_entry_t* got_entry = &got_entries[i];

        if(got_entry->module_id == 0) {
            break;
        }

        PRINTLOG(HYPERVISOR, LOG_TRACE, "got entry 0x%llx 0x%x 0x%x", got_entry->module_id, got_entry->symbol_type, got_entry->resolved);

        if(!got_entry->resolved) {
            PRINTLOG(HYPERVISOR, LOG_TRACE, "unresolved global object 0x%llx 0x%x", got_entry->module_id, got_entry->symbol_type);
        }

        if(hashmap_get(vm->loaded_module_ids, (void*)got_entry->module_id) == NULL) {
            if(got_entry->resolved) {
                PRINTLOG(HYPERVISOR, LOG_TRACE, "cleaning up unused module 0x%llx", got_entry->module_id);
                got_entry->resolved = false;
            }
        }
    }
}

int8_t hypevisor_deploy_program(hypervisor_vm_t* vm, const char_t* entry_point_name) {
    tosdb_manager_ipc_t ipc = {0};

    ipc.type = TOSDB_MANAGER_IPC_TYPE_PROGRAM_LOAD;
    ipc.program_build.entry_point_name = entry_point_name;
    ipc.program_build.for_vm = true;

    if(tosdb_manager_ipc_send_and_wait(&ipc) != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot send program build ipc");
        return -1;
    }

    if(!ipc.is_response_done) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "program build ipc response not done");
        return -1;
    }

    if(!ipc.is_response_success) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "program build ipc response failed");
        return -1;
    }

    vm->program_entry_point_virtual_address = ipc.program_build.program_entry_point_virtual_address;

    hashmap_put(vm->loaded_module_ids, (void*)ipc.program_build.module.module_handle, (void*)true);

    hypervisor_cleanup_unused_modules(vm, ipc.program_build.got_physical_address, ipc.program_build.got_size);

    hypervisor_vm_module_load_t ml = {0};

    ml.old_got_physical_address = vm->got_physical_address;
    ml.old_got_size = vm->got_size;
    ml.new_got_physical_address = ipc.program_build.got_physical_address;
    ml.new_got_size = ipc.program_build.got_size;
    ml.module_physical_address = ipc.program_build.module.module_physical_address;
    ml.module_virtual_address = ipc.program_build.module.module_virtual_address;
    ml.module_size = ipc.program_build.module.module_size;
    ml.metadata_physical_address = ipc.program_build.module.metadata_physical_address;
    ml.metadata_virtual_address = ipc.program_build.module.metadata_virtual_address;
    ml.metadata_size = ipc.program_build.module.metadata_size;

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "module id 0x%llx loaded", ipc.program_build.module.module_handle);
    PRINTLOG(HYPERVISOR, LOG_TRACE, "old got 0x%llx 0x%llx", ml.old_got_physical_address, ml.old_got_size);
    PRINTLOG(HYPERVISOR, LOG_TRACE, "new got 0x%llx 0x%llx", ml.new_got_physical_address, ml.new_got_size);
    PRINTLOG(HYPERVISOR, LOG_TRACE, "module 0x%llx 0x%llx", ml.module_physical_address, ml.module_size);

    if(hypervisor_ept_merge_module(vm, &ml) != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot merge module");
        return -1;
    }

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "deployed program entry point is at 0x%llx", ipc.program_build.program_entry_point_virtual_address);

    return 0;
}

int8_t hypervisor_load_module(hypervisor_vm_t* vm, uint64_t got_entry_address) {
    uint64_t got_fa = vm->got_physical_address;
    uint64_t got_size = vm->got_size;
    uint64_t got_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(got_fa);

    if(got_entry_address > got_size) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "module id 0x%llx is out of got size 0x%llx", got_entry_address, got_size);
        return -1;
    }

    got_va += got_entry_address;

    linker_global_offset_table_entry_t* got_entry = (linker_global_offset_table_entry_t*)got_va;

    uint64_t module_id = got_entry->module_id;

    if(module_id == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "module id 0x%llx is not valid", module_id);
        return -1;
    }

    if(got_entry->resolved) {
        PRINTLOG(HYPERVISOR, LOG_WARNING, "module id 0x%llx is already resolved", module_id);
        return 0;
    }

    tosdb_manager_ipc_t ipc = {0};

    ipc.type = TOSDB_MANAGER_IPC_TYPE_MODULE_LOAD;
    ipc.program_build.module.module_handle = module_id;
    ipc.program_build.for_vm = true;

    if(tosdb_manager_ipc_send_and_wait(&ipc) != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot send program build ipc");
        return -1;
    }

    if(!ipc.is_response_done) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "program build ipc response not done");
        return -1;
    }

    if(!ipc.is_response_success) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "program build ipc response failed");
        return -1;
    }

    hashmap_put(vm->loaded_module_ids, (void*)ipc.program_build.module.module_handle, (void*)true);

    hypervisor_cleanup_unused_modules(vm, ipc.program_build.got_physical_address, ipc.program_build.got_size);

    hypervisor_vm_module_load_t ml = {0};

    ml.old_got_physical_address = vm->got_physical_address;
    ml.old_got_size = vm->got_size;
    ml.new_got_physical_address = ipc.program_build.got_physical_address;
    ml.new_got_size = ipc.program_build.got_size;
    ml.module_physical_address = ipc.program_build.module.module_physical_address;
    ml.module_virtual_address = ipc.program_build.module.module_virtual_address;
    ml.module_size = ipc.program_build.module.module_size;
    ml.metadata_physical_address = ipc.program_build.module.metadata_physical_address;
    ml.metadata_virtual_address = ipc.program_build.module.metadata_virtual_address;
    ml.metadata_size = ipc.program_build.module.metadata_size;

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "module id 0x%llx loaded", module_id);
    PRINTLOG(HYPERVISOR, LOG_TRACE, "old got 0x%llx 0x%llx", ml.old_got_physical_address, ml.old_got_size);
    PRINTLOG(HYPERVISOR, LOG_TRACE, "new got 0x%llx 0x%llx", ml.new_got_physical_address, ml.new_got_size);
    PRINTLOG(HYPERVISOR, LOG_TRACE, "module 0x%llx 0x%llx", ml.module_physical_address, ml.module_size);

    if(hypervisor_ept_merge_module(vm, &ml) != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot merge module");
        return -1;
    }

    return 0;
}

uint64_t hypervisor_attach_pci_dev(hypervisor_vm_t* vm, uint32_t pci_address) {
    uint8_t group = (pci_address >> 24) & 0xff;
    uint8_t bus = (pci_address >> 16) & 0xff;
    uint8_t device = (pci_address >> 8) & 0xff;
    uint8_t function = pci_address & 0xff;

    const pci_dev_t* pci_dev = pci_find_device_by_address(group, bus, device, function);

    if(pci_dev == NULL) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot find pci device 0x%x 0x%x 0x%x 0x%x", group, bus, device, function);
        return -1;
    }

    uint64_t pci_va = hypervisor_ept_map_pci_device(vm, pci_dev);

    if(pci_va != -1ULL){
        list_list_insert(vm->mapped_pci_devices, pci_dev);
    }

    return pci_va;
}

void video_text_print(const char* str);

list_t** hypervisor_vmcall_interrupt_mapped_vms = NULL;

int8_t hypervisor_init_interrupt_mapped_vms(void) {
    if(hypervisor_vmcall_interrupt_mapped_vms != NULL) {
        return 0;
    }

    hypervisor_vmcall_interrupt_mapped_vms = memory_malloc_ext(NULL, 256 * sizeof(list_t*), 0);

    if(hypervisor_vmcall_interrupt_mapped_vms == NULL) {
        return -1;
    }

    for(uint64_t i = 0; i < 256; i++) {
        hypervisor_vmcall_interrupt_mapped_vms[i] = list_create_list();

        if(hypervisor_vmcall_interrupt_mapped_vms[i] == NULL) {
            return -1;
        }
    }

    return 0;
}

static int8_t hypervisor_vmcall_interrupt_mapped_isr(interrupt_frame_ext_t* frame) {
    uint64_t interrupt_number = frame->interrupt_number;

    list_t* vms = hypervisor_vmcall_interrupt_mapped_vms[interrupt_number];

    if(list_size(vms)) {
        for(size_t i = 0; i < list_size(vms); i++) {
            video_text_print("interrupt mapped\n");
            hypervisor_vm_t* vm = (hypervisor_vm_t*)list_get_data_at_position(vms, i);

            list_queue_push(vm->interrupt_queue, (void*)interrupt_number);

            task_set_interrupt_received(vm->task_id);
        }

        apic_eoi();

        return 0;
    } else {
        char_t buf[64] = {0};
        utoh_with_buffer(buf, interrupt_number);
        video_text_print("interrupt not mapped: 0x");
        video_text_print(buf);
        video_text_print("list size 0x");
        utoh_with_buffer(buf, list_size(vms));
        video_text_print(buf);
        video_text_print(" list 0x");
        utoh_with_buffer(buf, (uint64_t)vms);
        video_text_print(buf);
        video_text_print("\n");
    }

    return -1;
}

int16_t  hypervisor_attach_interrupt(hypervisor_vm_t* vm, uint64_t pci_dev_address, vm_guest_interrupt_type_t interrupt_type, uint8_t interrupt_number) {
    pci_generic_device_t* pci_dev = (pci_generic_device_t*)pci_dev_address;

    pci_capability_msi_t* msi_cap = NULL;
    pci_capability_msix_t* msix_cap = NULL;

    if(pci_dev->common_header.status.capabilities_list) {
        pci_capability_t* pci_cap = (pci_capability_t*)(((uint8_t*)pci_dev) + pci_dev->capabilities_pointer);


        while(pci_cap->capability_id != 0xFF) {
            if(pci_cap->capability_id == PCI_DEVICE_CAPABILITY_MSI) {
                msi_cap = (pci_capability_msi_t*)pci_cap;
            } else if(pci_cap->capability_id == PCI_DEVICE_CAPABILITY_MSIX) {
                msix_cap = (pci_capability_msix_t*)pci_cap;
            }

            if(pci_cap->next_pointer == NULL) {
                break;
            }

            pci_cap = (pci_capability_t*)(((uint8_t*)pci_dev) + pci_cap->next_pointer);
        }
    }

    if(interrupt_type == VM_GUEST_INTERRUPT_TYPE_MSI && !msi_cap) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "pci device does not support msi");
        return -1;
    } else if(interrupt_type == VM_GUEST_INTERRUPT_TYPE_MSIX && !msix_cap) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "pci device does not support msix");
        return -1;
    }

    uint8_t intnum = 0;

    if(interrupt_type == VM_GUEST_INTERRUPT_TYPE_MSI) {
        uint32_t msg_addr = 0xFEE00000;
        uint32_t apic_id = apic_get_local_apic_id();
        apic_id <<= 12;
        msg_addr |= apic_id;

        if(msi_cap->ma64_support) {
            msi_cap->ma64.message_address = msg_addr; // | (1 << 3) | (0 << 2);

            if(!msi_cap->ma64.message_data){
                intnum = interrupt_get_next_empty_interrupt();
                msi_cap->ma64.message_data = intnum;
            } else {
                intnum = msi_cap->ma64.message_data;
            }

        } else {
            msi_cap->ma32.message_address = msg_addr; // | (1 << 3) | (0 << 2);

            if(!msi_cap->ma32.message_data){
                intnum = interrupt_get_next_empty_interrupt();
                msi_cap->ma32.message_data = intnum;
            } else {
                intnum = msi_cap->ma32.message_data;
            }
        }

        uint8_t isrnum = intnum - INTERRUPT_IRQ_BASE;
        interrupt_irq_set_handler(isrnum, &hypervisor_vmcall_interrupt_mapped_isr);

        msi_cap->enable = 1;
    } else if(interrupt_type == VM_GUEST_INTERRUPT_TYPE_MSIX) {
        intnum = pci_msix_set_isr(pci_dev, msix_cap, interrupt_number, &hypervisor_vmcall_interrupt_mapped_isr);
        intnum += INTERRUPT_IRQ_BASE;
    } else {
        intnum = INTERRUPT_IRQ_BASE + pci_dev->interrupt_line;
        apic_ioapic_enable_irq(pci_dev->interrupt_line);
        uint8_t isrnum = intnum - INTERRUPT_IRQ_BASE;
        interrupt_irq_set_handler(isrnum, &hypervisor_vmcall_interrupt_mapped_isr);
    }

    interrupt_number = intnum;

    list_list_insert(hypervisor_vmcall_interrupt_mapped_vms[interrupt_number], vm);

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "interrupt number 0x%x mapped. list size 0x%llx list 0x%p",
             interrupt_number, list_size(hypervisor_vmcall_interrupt_mapped_vms[interrupt_number]),
             hypervisor_vmcall_interrupt_mapped_vms[interrupt_number]);

    list_list_insert(vm->mapped_interrupts, (void*)(uint64_t)(interrupt_number));

    return interrupt_number;
}

void hypervisor_cleanup_mapped_interrupts(hypervisor_vm_t* vm) {
    while(list_size(vm->mapped_interrupts)) {
        uint64_t interrupt_number = (uint64_t)list_queue_pop(vm->mapped_interrupts);
        list_list_delete(hypervisor_vmcall_interrupt_mapped_vms[interrupt_number], vm);

        if(!list_size(hypervisor_vmcall_interrupt_mapped_vms[interrupt_number])) {
            interrupt_irq_remove_handler(interrupt_number - INTERRUPT_IRQ_BASE, &hypervisor_vmcall_interrupt_mapped_isr);
        }
    }

    while(list_size(vm->interrupt_queue)) {
        list_queue_pop(vm->interrupt_queue);
    }
}
