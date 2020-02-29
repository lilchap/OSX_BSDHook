#include <stdio.h>

#include <errno.h>
#include <signal.h>
#include <sys/mman.h>

#include <mach/mach.h>
#include <mach-o/dyld.h>
#include <mach-o/dyld_images.h>

#include "util.hpp"

static mach_vm_address_t baseAddress;
static mach_vm_address_t patchAddress;
static const mach_vm_address_t patchOffset = 0xDFC;

void my_callback() {
    
    printf("Hello from callback! Press any key to continue...\n");
    getchar();
    getchar();
    
    // Make it return true so we can login with anything!
    asm __volatile ("int3");
    asm (".intel_syntax;"
         "mov eax, 0;");
}

static void sigbus_handler(int signal, siginfo_t* si, void* arg) {
    ucontext_t* ctx = (ucontext_t*)arg;
    
    mach_vm_address_t address = patchAddress;
    
    // Check if triggered - very rare, but possible
    if ((mach_vm_address_t)si->si_addr == address) {
        ctx->uc_mcontext->__ss.__rip = (__uint64_t)my_callback;
    }
    else {
        // Trap flag
        ctx->uc_mcontext->__ss.__rflags |= EFL_TF;
    
        // Restore original protection
        mprotect(pageof((mach_vm_address_t)si->si_addr), sysconf(_SC_PAGESIZE), VM_PROT_READ | VM_PROT_EXECUTE);
    }
}

static void trapflag_handler(int signal, siginfo_t* si, void* arg) {
    ucontext_t* ctx = (ucontext_t*)arg;
    mach_vm_address_t address = patchAddress;
    mach_vm_address_t addressPage = (mach_vm_address_t)pageof(address);
    
    // Check if outside of target page
    // NOTE: This is my only solution to prevent deadlocks from non async-safe functions
    //       or if it the program goes outside of the target page
    //       It's hacky, but it works... for now.
    if ((mach_vm_address_t)pageof((mach_vm_address_t)si->si_addr) != addressPage) {
        
        // Unset trap flag
        ctx->uc_mcontext->__ss.__rflags &= ~EFL_TF;
        
        // Reset hook via page protection
        // If you want the call back to only trigger once, comment this out
        mprotect((void*)addressPage, sysconf(_SC_PAGESIZE), VM_PROT_READ);
        
        return;
    }
    
    // Check if triggered
    if ((mach_vm_address_t)si->si_addr == address) {
        
        // Unset trap flag
        ctx->uc_mcontext->__ss.__rflags &= ~EFL_TF;
        
        // Reset hook via page protection
        // If you want the call back to only trigger once, comment this out
        mprotect((void*)addressPage, sysconf(_SC_PAGESIZE), VM_PROT_READ);
        
        // Set eip/rip to our callback
        ctx->uc_mcontext->__ss.__rip = (__uint64_t)my_callback;
        
    }
}

// New BSD signal handler
void set_exception_handler(void (*handler)(int, struct __siginfo*, void*), int signal) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = handler;
    sa.sa_flags = SA_SIGINFO;
    
    if (sigaction(signal, &sa, NULL) == -1) {
        printf("Error while calling sigaction! %s\n", strerror(errno));
    }
}

// Dylib entry point
__attribute__((constructor)) void load() {
    printf("Patching... \n");
    struct task_vm_info info;
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    kern_return_t err = task_info(current_task(), TASK_VM_INFO, (task_info_t)&info, &count);
    if (err != KERN_SUCCESS) {
        return;
    }
    
    baseAddress = info.min_address;
    patchAddress = baseAddress + patchOffset;
    
    printf("Base Address: 0x%llx\n", baseAddress);
    
    // Attatch exception handler for SIGBUS signals
    set_exception_handler(sigbus_handler, SIGBUS);
    set_exception_handler(trapflag_handler, SIGTRAP);
    
    // Set protection to later trigger our hook
    if (!mprotect(pageof(patchAddress), sysconf(_SC_PAGESIZE), VM_PROT_READ)) {
        getRegionInfo(patchAddress);
    }
    else {
        printf("mprotect failed! %s\n", strerror(errno));
    }
}
