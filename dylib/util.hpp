#pragma once
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <mach/mach.h>

void* pageof(mach_vm_address_t p) {
    size_t pageSize = sysconf(_SC_PAGESIZE);
    return (void*)(p & ~(pageSize - 1));
}

vm_region_basic_info_64 getRegionInfo(vm_address_t address) {
    vm_size_t vmsize;
    struct vm_region_basic_info_64 info;
    mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
    memory_object_name_t object;
    
    kern_return_t status = vm_region_64(current_task(), &address, &vmsize, VM_REGION_BASIC_INFO_64, (vm_region_info_t)&info, &info_count, &object);
    
    if (status != KERN_SUCCESS) {
        perror("vm_region failed\n");
        return info;
    }
    
    printf("Memory protection: %c%c%c  %c%c%c\n",
           info.protection & VM_PROT_READ ? 'r' : '-',
           info.protection & VM_PROT_WRITE ? 'w' : '-',
           info.protection & VM_PROT_EXECUTE ? 'x' : '-',
           
           info.max_protection & VM_PROT_READ ? 'r' : '-',
           info.max_protection & VM_PROT_WRITE ? 'w' : '-',
           info.max_protection & VM_PROT_EXECUTE ? 'x' : '-'
           );
    
    return info;
}
