/**
 * @file hypervisor.64.c
 * @brief Hypervisor for 64-bit x86 architecture.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#include <hypervisor/hypervisor.h>
#include <hypervisor/hypervisor_vmx_macros.h>
#include <hypervisor/hypervisor_vmx_utils.h>
#include <hypervisor/hypervisor_vmx_vmcs_ops.h>
#include <hypervisor/hypervisor_vmx_ops.h>
#include <hypervisor/hypervisor_vm.h>
#include <hypervisor/hypervisor_svm_macros.h>
#include <hypervisor/hypervisor_svm_ops.h>
#include <hypervisor/hypervisor_svm_vmcb_ops.h>
#include <cpu.h>
#include <cpu/crx.h>
#include <cpu/descriptor.h>
#include <cpu/task.h>
#include <cpu/sync.h>
#include <memory/paging.h>
#include <memory/frame.h>
#include <logging.h>
#include <utils.h>
#include <strings.h>

MODULE("turnstone.hypervisor");

uint64_t hypervisor_next_vm_id = 0;
lock_t* hypervisor_vm_lock = NULL;

static int32_t hypervisor_vmx_vm_task(uint64_t argc, void** args) {
    if(argc != 1) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "invalid argument count");
        return -1;
    }

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "args pointer: 0x%llx", (uint64_t)args);

    hypervisor_vm_t* vm = args[0];

    const char_t* entry_point_name = vm->entry_point_name;

    if(strlen(entry_point_name) == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "invalid entry point name");
        return -1;
    }

    uint64_t vmcs_frame_fa = vm->vmcs_frame_fa;

    if(vmcs_frame_fa == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "invalid vmcs frame fa");
        return -1;
    }

    if(vmx_vmptrld(vmcs_frame_fa) != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "vmptrld failed");
        return -1;
    }

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "vmptrld success");
    PRINTLOG(HYPERVISOR, LOG_INFO, "vm (0x%llx) starting...", vmcs_frame_fa);


    if(hypervisor_vm_create_and_attach_to_task(vm) != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot create vm and attach to task");
        return -1;
    }

    if(hypevisor_deploy_program(vm, entry_point_name) != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot deploy program");
        return -1;
    }

    if(vmx_vmlaunch() != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "vmxlaunch/vmresume failed");
        hypervisor_vmx_vmcs_dump();

        return -1;
    }

    return 0;
}

static int8_t hypervisor_svm_vm_task(uint64_t argc, void** args) {
    if(argc != 1) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "invalid argument count");
        return -1;
    }

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "args pointer: 0x%llx", (uint64_t)args);

    hypervisor_vm_t* vm = args[0];

    const char_t* entry_point_name = vm->entry_point_name;

    if(strlen(entry_point_name) == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "invalid entry point name");
        return -1;
    }

    uint64_t vmcb_frame_fa = vm->vmcb_frame_fa;

    if(vmcb_frame_fa == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "invalid vmcb frame fa");
        return -1;
    }

    if(svm_vmload(vmcb_frame_fa) != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "vmload failed");
        return -1;
    }

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "vmload success");
    PRINTLOG(HYPERVISOR, LOG_INFO, "vm (0x%llx) starting...", vmcb_frame_fa);

    if(hypervisor_vm_create_and_attach_to_task(vm) != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot create vm and attach to task");
        return -1;
    }

    if(hypevisor_deploy_program(vm, entry_point_name) != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot deploy program");
        return -1;
    }

    if(svm_vmrun(vmcb_frame_fa) != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "vmrun failed");
        hypervisor_vmx_vmcs_dump();

        return -1;
    }

    return 0;
}

static int8_t hypervisor_init_intel(void) {
    cpu_reg_cr4_t cr4;

    cr4 = cpu_read_cr4();

    cr4.fields.vmx_enable = 1;

    cpu_write_cr4(cr4);

    uint64_t feature_control = cpu_read_msr(CPU_MSR_IA32_FEATURE_CONTROL);

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "Feature control: 0x%llx", feature_control);

    uint64_t required = FEATURE_CONTROL_LOCKED | FEATURE_CONTROL_VMXON_OUTSIDE_SMX;

    if((feature_control & required) != required) {
        feature_control |= required;
        cpu_write_msr(CPU_MSR_IA32_FEATURE_CONTROL, feature_control);
    }

    cpu_reg_cr0_t cr0 = cpu_read_cr0();

    cr0.bits &= cpu_read_msr(CPU_MSR_IA32_VMX_CR0_FIXED1);
    cr0.bits |= cpu_read_msr(CPU_MSR_IA32_VMX_CR0_FIXED0);

    cpu_write_cr0(cr0);

    cr4 = cpu_read_cr4();

    cr4.bits &= cpu_read_msr(CPU_MSR_IA32_VMX_CR4_FIXED0);
    cr4.bits |= cpu_read_msr(CPU_MSR_IA32_VMX_CR4_FIXED1);

    cpu_write_cr4(cr4);

    frame_t* vmxon_frame = NULL;

    uint64_t vmxon_frame_va = hypervisor_allocate_region(&vmxon_frame, FRAME_SIZE);

    if(vmxon_frame_va == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot allocate vmxon frame");
        return -1;
    }

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "vmxon frame va: 0x%llx", vmxon_frame_va);

    uint32_t revision_id = hypervisor_vmx_vmcs_revision_id();

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "VMCS revision id: 0x%x", revision_id);

    *(uint32_t*)vmxon_frame_va = revision_id;

    uint8_t err = 0;

    err = vmx_vmxon(vmxon_frame->frame_address);

    if(err) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "vmxon failed");
        return -1;
    }

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "vmxon success");

    return 0;
}

static int8_t hypervisor_init_amd(void) {
    uint64_t msr_efer = cpu_read_msr(CPU_MSR_EFER);
    msr_efer |= 1 << 12;
    cpu_write_msr(CPU_MSR_EFER, msr_efer);

    frame_t* svm_ha_frame = NULL;

    uint64_t svm_ha_frame_va = hypervisor_allocate_region(&svm_ha_frame, FRAME_SIZE);

    if(svm_ha_frame_va == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot allocate svm ha frame");
        return -1;
    }

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "svm ha frame va: 0x%llx", svm_ha_frame_va);

    cpu_write_msr(SVM_MSR_VM_HSAVE_PA, svm_ha_frame->frame_address);

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "svm success");

    return 0;
}


int8_t hypervisor_init(void) {
    logging_set_level(HYPERVISOR, LOG_DEBUG);
    if(hypervisor_vm_lock == NULL) { // thread safe, first creator is main thread, ap threads will soon call
        hypervisor_vm_lock = lock_create();

        if(hypervisor_vm_lock == NULL) {
            PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot create vm lock");
            return -1;
        }
    }

    if(hypervisor_init_interrupt_mapped_vms() != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot initialize vmcall interrupt mapped vms");
        return -1;
    }

    cpu_cpuid_regs_t query;
    cpu_cpuid_regs_t result;

    if(cpu_get_type() == CPU_TYPE_INTEL) {
        query.eax = 0x1;
        query.ebx = 0;
        query.ecx = 0;
        query.edx = 0;

        cpu_cpuid(query, &result);

        if(!(result.ecx & (1 << HYPERVISOR_INTEL_ECX_HYPERVISOR_BIT))) {
            PRINTLOG(HYPERVISOR, LOG_ERROR, "Hypervisor not supported");
            return -1;
        }

        if(hypervisor_init_intel() != 0) {
            PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot initialize intel hypervisor");
            return -1;
        }
    } else if(cpu_get_type() == CPU_TYPE_AMD) {
        query.eax = 0x80000001;
        query.ebx = 0;
        query.ecx = 0;
        query.edx = 0;

        cpu_cpuid(query, &result);

        if(!(result.ecx & (1 << HYPERVISOR_AMD_ECX_HYPERVISOR_BIT))) {
            PRINTLOG(HYPERVISOR, LOG_ERROR, "Hypervisor not supported");
            return -1;
        }

        if(hypervisor_init_amd() != 0) {
            PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot initialize amd hypervisor");
            return -1;
        }

    } else {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "Hypervisor not supported");
        return -1;
    }

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "Hypervisor supported");

    if(hypervisor_vm_init() != 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot initialize hypervisor vm");
        return -1;
    }

    return 0;
}

int8_t hypervisor_vm_create(const char_t* entry_point_name) {
    return 0;
    if(strlen(entry_point_name) == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "invalid entry point name");
        return -1;
    }

    hypervisor_vm_t* vm = NULL;

    void* entry_point = NULL;

    if(cpu_get_type() == CPU_TYPE_INTEL) {
        entry_point = hypervisor_vmx_vm_task;

        if(hypervisor_vmx_vmcs_prepare(&vm) != 0) {
            PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot prepare vm");
            return -1;
        }
    } else if(cpu_get_type() == CPU_TYPE_AMD) {
        entry_point = hypervisor_svm_vm_task;

        if(hypervisor_svm_vmcb_prepare(&vm) != 0) {
            PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot prepare vm");
            return -1;
        }
    } else {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "Hypervisor not supported");
        return -1;
    }

    vm->entry_point_name = entry_point_name;

    memory_heap_t* heap = memory_get_default_heap();

    void** args = memory_malloc_ext(heap, sizeof(void*) * 2, 0);

    if(args == NULL) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot allocate args");

        return -1;
    }

    args[0] = vm;

    char_t* vm_name = strprintf("vm%08llx", ++hypervisor_next_vm_id);

    if(task_create_task(heap, 2 << 20, 16 << 10, entry_point, 1, args, vm_name) == -1ULL) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot create vm task");
        memory_free(args);
        memory_free(vm_name);

        return -1;
    }

    memory_free(vm_name);

    return 0;
}

int8_t hypervisor_stop(void) {
    if(cpu_get_type() == CPU_TYPE_INTEL) {
        asm volatile ("vmxoff" ::: "cc");
        PRINTLOG(HYPERVISOR, LOG_TRACE, "vmxoff success");
    } else if(cpu_get_type() == CPU_TYPE_AMD) {
        uint64_t msr_efer = cpu_read_msr(CPU_MSR_EFER);
        msr_efer &= ~(1 << 12);
        cpu_write_msr(CPU_MSR_EFER, msr_efer);
        PRINTLOG(HYPERVISOR, LOG_TRACE, "svm off success");
    } else {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "Hypervisor not supported");
        return -1;
    }

    return 0;
}
