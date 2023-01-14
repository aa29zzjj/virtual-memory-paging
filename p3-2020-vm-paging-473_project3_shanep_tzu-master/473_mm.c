// Starter code for the page replacement project
#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <signal.h>
#include "473_mm.h"

/*
Link list of the memory page
phy_addr: the address return the specific memory page
page_spot: the spot of the page.
logic_addr: to easily count the offset (will never be used)
ref_bit: for read/write implement
mod_bit: make sure the memory page is modified or not
operator: the type for write back 
chance: for third chance replacement
*/
typedef struct Page{
    void* phy_addr;
    int logic_addr;
    int page_spot;
    int ref_bit;
    int mod_bit;
    int operator;
    int chance;
    struct Page *next;
}Page;

/*void mm_logger(int virt_page, int fault_type, int evicted_page, int write_back, unsigned int phy_addr)
{
  stats[statCounter].virt_page     = virt_page;
  stats[statCounter].fault_type    = fault_type;
  stats[statCounter].evicted_page  = evicted_page;
  stats[statCounter].write_back    = write_back;
  stats[statCounter].phy_addr      = phy_addr;
  statCounter++;
}*/

/*tools for needs*/
Page *get_frame(int page_num);
Page *find_head();
void FIFO_replacement(void *error_addr, int page_num, int offset, int error_num);
void third_chance_replacement(void *error_addr, int page_num, int offset, int error_num);
void sighandler(int signalnumber, siginfo_t* siginfo, void * context);

/*pulic variable*/
Page *head_frame;
int pagesize;
int pagepolicy;
void *start_of_vm;

//setups struct to create own sigaction
struct sigaction sigact;

void mm_init(void* vm, int vm_size, int n_frames, int page_size, int policy)
{
    //initialize the memory pages
    start_of_vm = vm;
    pagesize = page_size;
    mprotect(start_of_vm, vm_size, PROT_NONE);
    pagepolicy = policy;
    head_frame = (Page *)malloc(sizeof(Page));
    Page *current = head_frame;
    //reset the details of the memory page
    current->phy_addr = start_of_vm;
    current->logic_addr = 0;
    current->page_spot = -1;
    current->ref_bit = 0;
    current->mod_bit = 0;
    current->operator = 0;
    current->chance = 0;

    int i = 1;
    while(i < n_frames){

        current->next = (Page *)malloc(sizeof(Page));
        current->next->phy_addr = current->phy_addr + page_size;
        current->next->logic_addr = current->logic_addr + page_size;
        current->next->page_spot = -1;
        current->next->ref_bit = 0;
        current->next->operator = 0;
        current->next->chance = 0;
        current = current->next;
        i++;    
    }
    // build into circular link list 
    current->next = head_frame;
    sigact.sa_handler = sighandler;
    sigact.sa_flags = SA_SIGINFO;

    sigaction(SIGSEGV, &sigact, NULL);

    /*mprotect(vm,vm_size,PROT_WRITE);

    sigact.sa_flags = SA_SIGINFO;

    sigact.sa_handler = &sighandler;
    sigemptyset(sigaction.sa_mask);
    sigaction();*/
}
    
Page *get_frame(int page_num){

    Page *current = head_frame;
    if (current->page_spot == page_num)
    {
        return current;
    }

    current = current->next;
    //Goes through linked list until the frame with the page num is 
    //found or exits when getting back to the head frame.
    while (current != head_frame)
    {
        /*if the page number equals to the current page number, return the current*/
        if (current->page_spot == page_num)
        {
            return current;
        }
        current = current->next;
    }
    return NULL;

}
/*use find_head to get the start of the three chance*/
Page *find_head(){
    Page *current = head_frame;
    while(current->ref_bit == 1 || current->mod_bit == 1){
        //case3: mod_bit and ref_bit = 1
        if(current->chance == 2)
            break;
        //case1 and case2
        if(current->mod_bit == 1){
            if(current->ref_bit == 1){
                current->ref_bit = 0;
                current->chance = 1;
                if(mprotect(current->phy_addr, pagesize, PROT_NONE) == -1)
                    printf("mprotect fail\n");
                current = current->next;
            }
            else{
                    current->chance = 2;
                    current = current->next;
                }
        }
        else{
            current->ref_bit = 0;
            current->chance = 1;
            if(mprotect(current->phy_addr, pagesize, PROT_NONE) == -1)
                printf("mprotect fail\n");
            current = current->next;
        }
        
    }
    head_frame = current;
    return current;

}
/*
  error_num: 0 = read error, 1 = write error
  error_addr: the address for replace
  page_num: the number to be replaced 
*/
void FIFO_replacement(void *error_addr, int page_num, int offset, int error_num){
    //read error
    if(error_num == 0){

        if(mprotect(head_frame->phy_addr, pagesize, PROT_NONE) == -1)
            printf("mprotect fail\n");
        head_frame->phy_addr = error_addr;
        if(mprotect(head_frame->phy_addr, pagesize, PROT_READ) == -1)
            printf("mprotect fail\n");
        int evicted_page_spot = head_frame->page_spot;
        head_frame->page_spot = page_num;
        //executing the operator command in this frame, and reset the operator to 0
        int operator_command = head_frame->operator;
        head_frame->operator = 0;
        mm_logger(page_num, 0, evicted_page_spot, operator_command, head_frame->logic_addr + offset * sizeof(int));
        head_frame = head_frame->next;
    }
    else{
        //writing error
        Page *page = get_frame(page_num);
        if(page == NULL){

                if(mprotect(head_frame->phy_addr, pagesize, PROT_NONE) == -1)
                    printf("mprotect fail\n");
                head_frame->phy_addr = error_addr;
                if(mprotect(head_frame->phy_addr, pagesize, PROT_READ | PROT_WRITE) == -1)
                    printf("mprotect fail\n");
                int evicted_page_spot = head_frame->page_spot;
                head_frame->page_spot = page_num;
                int operator_command = head_frame->operator;
                head_frame->operator = 1;
                mm_logger(page_num, 1, evicted_page_spot, operator_command, head_frame->logic_addr+offset * sizeof(int));
                head_frame = head_frame->next;
            }
            else{
            if(mprotect(page->phy_addr, pagesize, PROT_READ|PROT_WRITE) == -1)
                printf("mprotect fail\n");
            if(page->operator == 0){
                page->operator = 1;
                mm_logger(page_num, 2, -1, 0, page->logic_addr + offset * sizeof(int));                
            }
        }
    }
}
void third_chance_replacement(void *error_addr, int page_num, int offset, int error_num){
    //error_num = 0 is read error, else is for write error
    if(error_num == 0){
        Page *page = get_frame(page_num);
        //if there is no specific memory page, set it to the head page
        if(page == NULL){
            Page *head = find_head();
            if (mprotect(head->phy_addr, pagesize, PROT_NONE) == -1)
                printf("mprotect fail\n");
            head->phy_addr = error_addr;
            if (mprotect(head->phy_addr, pagesize, PROT_READ) == -1)
                printf("mprotect fail\n");
            int evicted_page_spot = head->page_spot;
            int operator_command = head->operator;
            head->page_spot = page_num;
            head->operator = 0;
            head->ref_bit = 1;
            head->mod_bit = 0;
            head->chance = 0;

            //read the none present page
            mm_logger(page_num, 0, evicted_page_spot, operator_command, head->logic_addr + offset * sizeof(int));
            head_frame = head->next;    
        }
        else{
            if(mprotect(page->phy_addr, pagesize, PROT_READ) == -1)
                printf("mprotect fail\n");
            page->ref_bit = 1;
            page->chance = 0;
            // Track a "read" reference to the page that currently has Read and/or Write permissions .
            mm_logger(page_num, 3, -1, 0, page->logic_addr + offset * sizeof(int));
        }
    }
    else{
        Page *page = get_frame(page_num);
        if(page == NULL){
            Page *head = find_head();
            
            if (mprotect(head->phy_addr, pagesize, PROT_NONE) == -1)
                printf("mprotect fail\n");
            head->phy_addr = error_addr;
            if (mprotect(head->phy_addr, pagesize, PROT_READ | PROT_WRITE) == -1)
                printf("mprotect fail\n");
            int evicted_page_spot = head->page_spot;
            head->page_spot = page_num;
            int operator_command = head->operator;
            head->operator = 1;
            head->ref_bit = 1;
            head->mod_bit = 1;
            head->chance = 0;
            //write the non present page
            mm_logger(page_num, 1, evicted_page_spot, operator_command, head->logic_addr + offset * sizeof(int));
            head_frame = head->next;
        }
        else{
            if (mprotect(page->phy_addr, pagesize, PROT_READ| PROT_WRITE) == -1)
                printf("mprotect fail\n");
            if(page->operator == 0){
                page->operator = 1;
                mm_logger(page_num, 2, -1, 0, page->logic_addr + offset * sizeof(int));
            }
            else if(page->ref_bit == 0){
                mm_logger(page_num, 4, -1, 0, page->logic_addr + offset * sizeof(int));
            }
            page->ref_bit = 1; 
            page->mod_bit = 1;
            page->chance = 0; 
        }   
    }
}
void sighandler(int signalnumber, siginfo_t *siginfo, void *v_data)
{
    //set up the data
    ucontext_t *data = (ucontext_t *)(v_data);
    void *error_addr = siginfo->si_addr;
    int offset_addr = error_addr - start_of_vm;
    int page_num = offset_addr/pagesize;
    //Since the real offset bit shoud be 4(0100), so shift 2 bits to get number
    int offset = (offset_addr % pagesize) >> 2;
    void *start_of_error_addr = error_addr - (offset_addr % pagesize);

    //read_error:    error_num = 0
    //write_error:   error_num = 1
    int error_num = (data->uc_mcontext.gregs[REG_ERR] & 0X02) >> 1;
    if(pagepolicy == 1)
        FIFO_replacement(start_of_error_addr, page_num, offset, error_num);
    else
        third_chance_replacement(start_of_error_addr, page_num, offset, error_num);

}
