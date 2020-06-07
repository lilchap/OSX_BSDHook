# OSX-BSDHook

This demo sets a pageguard hook on a MacOS application (via a dylib injection).

## test.cpp

test.cpp is the target application for the hook. test.cpp requests the user to input a password up to two times. If the user inputs the correct password, **1402**, then the app will print "Yeet" and exit. Otherwise, the app will simply close without that desired message.

## The Actual Hook

The first step is to find the address for our patch, add a bsd signal hook, and remove the `VM_PROT_EXECUTE` flag from the page to start the hook.

### sigbus Handler

The sigbus handler is called when the SIGBUS signal is invoked. By changing the protection of a page in memory, we are able to tell if a specific operation at a specific address is being executed.

```cpp
    if ((mach_vm_address_t)si->si_addr == address) {
        ctx->uc_mcontext->__ss.__rip = (__uint64_t)my_callback;
    }
    else {
        // Trap flag
        ctx->uc_mcontext->__ss.__rflags |= EFL_TF;
    
        // Restore original protection
        mprotect(pageof((mach_vm_address_t)si->si_addr), sysconf(_SC_PAGESIZE), VM_PROT_READ | VM_PROT_EXECUTE);
    }
```

If the address that we want to "hook" (run our callback at) happens to have the first operation run in that page, then we have to set the instruction pointer to our callback immediately. Also I forgot to reset the page permission in the code example but I'm too lazy to change that. In the most likely scenario, we have to set a trap flag on the thread so we can follow through execution to reach our desired address.

### trapflag handler

```cpp
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
        mprotect((void*)addressPage, sysconf(_SC_PAGESIZE), VM_PROT_READ);
        
        return;
    }
    
    // Check if triggered
    if ((mach_vm_address_t)si->si_addr == address) {
        
        // Unset trap flag
        ctx->uc_mcontext->__ss.__rflags &= ~EFL_TF;
        
        // Reset hook via page protection
        mprotect((void*)addressPage, sysconf(_SC_PAGESIZE), VM_PROT_READ);
        
        // Set eip/rip to our callback
        ctx->uc_mcontext->__ss.__rip = (__uint64_t)my_callback;
        
    }
}
```

In order to make sure the trapflag doesn't run forever (or hopefully not, because there could be an infinite loop in that page) we have to disable it once the address goes out of our target page's bounds. On the other case, we finally reach our address and we can go to our callback!

## Takeaways

This project was pretty fun to make because I was learning a lot of stuff when I was doing it. I did this back in the summer of 2019 but I never posted it for some reason. Anyways, if you're looking to implement this straight into whatever program you have right now, *don't*. This is just to get the general idea of the hook, and this program contains a lot of bugs now that I'm looking back on it. Luckily, you'll only need a few more lines of code to fix them.

Cheers!
