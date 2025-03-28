/**
 * @file hypervisor_svm_vmexit.64.c
 * @brief SVM VMEXIT handler for 64-bit x86 architecture.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <hypervisor/hypervisor_svm_vmcb_ops.h>
#include <hypervisor/hypervisor_svm_macros.h>
#include <hypervisor/hypervisor_utils.h>
#include <hypervisor/hypervisor_ept.h>
#include <memory/paging.h>
#include <cpu.h>
#include <cpu/task.h>
#include <apic.h>
#include <cpu/crx.h>
#include <logging.h>

MODULE("turnstone.hypervisor.svm");

__attribute__((naked, no_stack_protector)) static void hypervisor_svm_vm_run_single(
    task_registers_t* host_registers,
    task_registers_t* guest_registers,
    uint64_t          vmcb_frame_fa) {
    UNUSED(vmcb_frame_fa);
    asm volatile (
        "push %%rbp\n"
        "mov %%rsp, %%rbp\n"
        "// create 32 bytes for local variables\n"
        "// 8 bytes for host registers\n"
        "// 8 bytes for guest registers\n"
        "// 8 bytes for vmcb frame fa\n"
        "sub $0x20, %%rsp\n"
        "mov %%rdi, 0x0(%%rsp)\n"
        "mov %%rsi, 0x8(%%rsp)\n"
        "mov %%rdx, 0x10(%%rsp)\n"


        "// begin save host registers\n"
        "mov %%rbx, %[h_rbx]\n"
        "mov %%rcx, %[h_rcx]\n"
        "mov %%rdx, %[h_rdx]\n"
        "mov %%r8,  %[h_r8]\n"
        "mov %%r9,  %[h_r9]\n"
        "mov %%r10, %[h_r10]\n"
        "mov %%r11, %[h_r11]\n"
        "mov %%r12, %[h_r12]\n"
        "mov %%r13, %[h_r13]\n"
        "mov %%r14, %[h_r14]\n"
        "mov %%r15, %[h_r15]\n"
        "mov %%rdi, %[h_rdi]\n"
        "mov %%rsi, %[h_rsi]\n"
        "mov %%rbp, %[h_rbp]\n"
        "mov %[h_xsave_mask_lo], %%eax\n"
        "mov %[h_xsave_mask_hi], %%edx\n"
        "lea %[h_avx512f], %%rbx\n"
        "xsave (%%rbx)\n"
        "// end save host registers\n"
        : :
        [h_rbx]    "m" (host_registers->rbx),
        [h_rcx]    "m" (host_registers->rcx),
        [h_rdx]    "m" (host_registers->rdx),
        [h_r8]     "m" (host_registers->r8),
        [h_r9]     "m" (host_registers->r9),
        [h_r10]    "m" (host_registers->r10),
        [h_r11]    "m" (host_registers->r11),
        [h_r12]    "m" (host_registers->r12),
        [h_r13]    "m" (host_registers->r13),
        [h_r14]    "m" (host_registers->r14),
        [h_r15]    "m" (host_registers->r15),
        [h_rdi]    "m" (host_registers->rdi),
        [h_rsi]    "m" (host_registers->rsi),
        [h_rbp]    "m" (host_registers->rbp),
        [h_avx512f]    "m" (host_registers->avx512f),
        [h_xsave_mask_lo] "m" (host_registers->xsave_mask_lo),
        [h_xsave_mask_hi] "m" (host_registers->xsave_mask_hi)
        );

    asm volatile (
        "// begin save host vmcb frame\n"
        "mov 0x10(%rsp), %rax\n"
        "addq $0x1000, %rax\n"
        "vmsave\n"
        "// end save host vmcb frame\n"
        );

    asm volatile (
        "// begin load guest registers\n"
        "mov %[g_rcx], %%rcx\n"
        "mov %[g_r8],  %%r8\n"
        "mov %[g_r9],  %%r9\n"
        "mov %[g_r10], %%r10\n"
        "mov %[g_r11], %%r11\n"
        "mov %[g_r12], %%r12\n"
        "mov %[g_r13], %%r13\n"
        "mov %[g_r14], %%r14\n"
        "mov %[g_r15], %%r15\n"
        "mov %[g_rdi], %%rdi\n"
        "mov %[g_rbp], %%rbp\n"
        "lea %[g_avx512f], %%rbx\n"
        "mov %[g_xsave_mask_lo], %%eax\n"
        "mov %[g_xsave_mask_hi], %%edx\n"
        "xrstor (%%rbx)\n"
        "mov %[g_rbx], %%rbx\n"
        "mov %[g_rdx], %%rdx\n"
        "mov %[g_rsi], %%rsi\n"
        "// end load guest registers\n"
        : :
        [g_rbx]    "m" (guest_registers->rbx),
        [g_rcx]    "m" (guest_registers->rcx),
        [g_rdx]    "m" (guest_registers->rdx),
        [g_r8]     "m" (guest_registers->r8),
        [g_r9]     "m" (guest_registers->r9),
        [g_r10]    "m" (guest_registers->r10),
        [g_r11]    "m" (guest_registers->r11),
        [g_r12]    "m" (guest_registers->r12),
        [g_r13]    "m" (guest_registers->r13),
        [g_r14]    "m" (guest_registers->r14),
        [g_r15]    "m" (guest_registers->r15),
        [g_rdi]    "m" (guest_registers->rdi),
        [g_rsi]    "m" (guest_registers->rsi),
        [g_rbp]    "m" (guest_registers->rbp),
        [g_avx512f]    "m" (guest_registers->avx512f),
        [g_xsave_mask_lo] "m" (guest_registers->xsave_mask_lo),
        [g_xsave_mask_hi] "m" (guest_registers->xsave_mask_hi)
        );

    asm volatile (
        "// begin load guest vmcb frame\n"
        "mov 0x10(%rsp), %rax\n"
        "vmload\n"
        "// end load guest vmcb frame\n"

        "// begin vmrun\n"
        "vmrun\n"
        "// end vmrun\n"

        "// begin save guest vmcb frame\n"
        "vmsave\n"
        "// end save guest vmcb frame\n"
        );

    asm volatile (
        "// now tricky part we need to save guest registers however rsi is guest rsi\n"
        "xchg 0x8(%%rsp), %%rsi\n"

        "// begin save guest registers\n"
        "mov %%rbx, %[g_rbx]\n"
        "mov %%rcx, %[g_rcx]\n"
        "mov %%rdx, %[g_rdx]\n"
        "mov %%r8,  %[g_r8]\n"
        "mov %%r9,  %[g_r9]\n"
        "mov %%r10, %[g_r10]\n"
        "mov %%r11, %[g_r11]\n"
        "mov %%r12, %[g_r12]\n"
        "mov %%r13, %[g_r13]\n"
        "mov %%r14, %[g_r14]\n"
        "mov %%r15, %[g_r15]\n"
        "mov %%rdi, %[g_rdi]\n"
        "mov %%rbp, %[g_rbp]\n"
        "mov %[g_xsave_mask_lo], %%eax\n"
        "mov %[g_xsave_mask_hi], %%edx\n"
        "lea %[g_avx512f], %%rbx\n"
        "xsave (%%rbx)\n"
        "// move guest rsi to rax\n"
        "mov 0x8(%%rsp), %%rax\n"
        "// and save it\n"
        "mov %%rax, %[g_rsi]\n"
        "// end save guest registers\n"
        : :
        [g_rbx]    "m" (guest_registers->rbx),
        [g_rcx]    "m" (guest_registers->rcx),
        [g_rdx]    "m" (guest_registers->rdx),
        [g_r8]     "m" (guest_registers->r8),
        [g_r9]     "m" (guest_registers->r9),
        [g_r10]    "m" (guest_registers->r10),
        [g_r11]    "m" (guest_registers->r11),
        [g_r12]    "m" (guest_registers->r12),
        [g_r13]    "m" (guest_registers->r13),
        [g_r14]    "m" (guest_registers->r14),
        [g_r15]    "m" (guest_registers->r15),
        [g_rdi]    "m" (guest_registers->rdi),
        [g_rsi]    "m" (guest_registers->rsi),
        [g_rbp]    "m" (guest_registers->rbp),
        [g_avx512f]    "m" (guest_registers->avx512f),
        [g_xsave_mask_lo] "m" (guest_registers->xsave_mask_lo),
        [g_xsave_mask_hi] "m" (guest_registers->xsave_mask_hi)
        );

    asm volatile (
        "// now restore host parametes rsi is already host rsi\n"
        "mov 0x0(%%rsp), %%rdi\n"

        "// begin restore host registers\n"
        "mov %[h_rcx],  %%rcx\n"
        "mov %[h_r8],   %%r8\n"
        "mov %[h_r9],   %%r9\n"
        "mov %[h_r10],  %%r10\n"
        "mov %[h_r11],  %%r11\n"
        "mov %[h_r12],  %%r12\n"
        "mov %[h_r13],  %%r13\n"
        "mov %[h_r14],  %%r14\n"
        "mov %[h_r15],  %%r15\n"
        "mov %[h_rdi],  %%rdi\n"
        "mov %[h_rsi],  %%rsi\n"
        "mov %[h_rbp],  %%rbp\n"
        "mov %[h_xsave_mask_lo], %%eax\n"
        "mov %[h_xsave_mask_hi], %%edx\n"
        "lea %[h_avx512f], %%rbx\n"
        "xrstor (%%rbx)\n"
        "mov %[h_rbx],  %%rbx\n"
        "mov %[h_rdx],  %%rdx\n"
        "// end restore host registers\n"

        "mov 0x10(%%rsp), %%rax\n"
        "addq $0x1000, %%rax\n"
        "vmload\n"
        "add $0x20, %%rsp\n"
        "pop %%rbp\n"
        "retq\n"
        : :
        [h_rbx]    "m" (host_registers->rbx),
        [h_rcx]    "m" (host_registers->rcx),
        [h_rdx]    "m" (host_registers->rdx),
        [h_r8]     "m" (host_registers->r8),
        [h_r9]     "m" (host_registers->r9),
        [h_r10]    "m" (host_registers->r10),
        [h_r11]    "m" (host_registers->r11),
        [h_r12]    "m" (host_registers->r12),
        [h_r13]    "m" (host_registers->r13),
        [h_r14]    "m" (host_registers->r14),
        [h_r15]    "m" (host_registers->r15),
        [h_rdi]    "m" (host_registers->rdi),
        [h_rsi]    "m" (host_registers->rsi),
        [h_rbp]    "m" (host_registers->rbp),
        [h_avx512f]    "m" (host_registers->avx512f),
        [h_xsave_mask_lo] "m" (host_registers->xsave_mask_lo),
        [h_xsave_mask_hi] "m" (host_registers->xsave_mask_hi)
        );
}

int8_t hypervisor_svm_vm_run(uint64_t hypervisor_vm_ptr) {
    hypervisor_vm_t* vm = (hypervisor_vm_t*)hypervisor_vm_ptr;

    if(vm == NULL) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "invalid vm");
        return -1;
    }

    uint64_t guest_vmcb = vm->vmcb_frame_fa;
    svm_vmcb_t* vmcb = (svm_vmcb_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(guest_vmcb);

    while(true) {
        if(hypervisor_svm_vmcb_set_running(vm) != 0) {
            PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot set running");
            return -1;
        }

        hypervisor_svm_vm_run_single(vm->host_registers, vm->guest_registers, guest_vmcb);

        if(hypervisor_svm_vmcb_set_stopped(vm) != 0) {
            PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot set stopped");
            return -1;
        }

        PRINTLOG(HYPERVISOR, LOG_DEBUG, "vmexit occurred exit code: 0x%llx 0x%llx 0x%llx 0x%llx",
                 vmcb->control_area.exit_code, vmcb->control_area.exit_info_1, vmcb->control_area.exit_info_2, vmcb->control_area.exit_int_info.bits);


        while(true) {
            asm volatile ("hlt");
        }
    }



}
