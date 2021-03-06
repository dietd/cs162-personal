#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constants.h"
#include "page.h"
#include "ram.h"


/* These macros may or may not be useful.
 * */


//#define PMD_PFN_MASK 
//#define PTE_PFN_MASK
//#define PAGE_OFFSET_MASK
//
//#define vaddr_pgd(vaddr)
//#define vaddr_pmd(vaddr)
//#define vaddr_pte(vaddr)
//#define vaddr_off(vaddr)
//
//#define pfn_to_addr(pfn) (pfn << PAGE_SHIFT)

paddr_ptr pfn_to_addr(paddr_ptr pfn) {
  return (paddr_ptr) (pfn << PAGE_SHIFT);
}

paddr_ptr get_addr(void * buffer) {
  int64_t mask = 0xFFFFFFFF << 12;
  int64_t entry = * (int64_t *) buffer;
  return entry & mask;
}

bool is_present(void * buffer) {
  int64_t mask = 0x1;
  int64_t entry = * (int64_t *) buffer;
  if ((entry & mask) == 1)
	  return true;
  return false;
}


//Returns the base pointer to the page directory table
paddr_ptr pd_addr(vaddr_ptr vaddr, paddr_ptr pdpt, bool * succ) {
  vaddr_ptr mask = 0xC0000000;
  paddr_ptr index = (paddr_ptr) ((vaddr & mask) >> PGDIR_SHIFT);
  paddr_ptr entry = pdpt + (((paddr_ptr) sizeof(pgd_t)) * index);
  void * buffer = malloc(8);
  ram_fetch(entry, buffer, 8);
  
  if (!is_present(buffer))
	  succ = false;

  return get_addr(buffer);
}

//Returns the base pointer to the page table
paddr_ptr pt_addr(vaddr_ptr vaddr, paddr_ptr pdt, bool * succ) {
  vaddr_ptr mask = 0x3FE00000;
  paddr_ptr index = (paddr_ptr) ((vaddr & mask) >> PMD_SHIFT);
  paddr_ptr entry = pdt + (((paddr_ptr) sizeof(pmd_t)) * index);
  void * buffer = malloc(8);
  ram_fetch(entry, buffer, 8);

  if (!is_present(buffer))
	  succ = false;

  return get_addr(buffer);
}

//Returns the base pointer to the page
paddr_ptr pg_addr(vaddr_ptr vaddr, paddr_ptr pt, bool * succ) {
  vaddr_ptr mask = 0x001FF000;
  paddr_ptr index = (paddr_ptr) ((vaddr & mask) >> PAGE_SHIFT);
  paddr_ptr entry = pt + (((paddr_ptr) sizeof(pte_t)) * index);
  void * buffer = malloc(8);
  ram_fetch(entry, buffer, 8);
  
  if (!is_present(buffer))
	  *succ = false;

  return get_addr(buffer);
}

//Returns the physical address pointer based on the page
paddr_ptr phys_addr(vaddr_ptr vaddr, paddr_ptr pg) {
  int32_t mask = 0x00000FFF;
  paddr_ptr index = (paddr_ptr) (vaddr & mask);
  return index + pg;
}

/* Translates the virtual address vaddr and stores the physical address in paddr.
 * If a page fault occurs, return a non-zero value, otherwise return 0 on a successful translation.
 * */

int virt_to_phys(vaddr_ptr vaddr, paddr_ptr cr3, paddr_ptr *paddr) {
  bool succ = true;
  bool * succ_ptr = &succ; 
  paddr_ptr pd = pd_addr(vaddr, cr3, succ_ptr); //page directory table
  if (!succ)
	  return -1;

  paddr_ptr pt =  pt_addr(vaddr, pd, succ_ptr); //page table
  if (!succ)
	  return -1;

  paddr_ptr pg = pg_addr(vaddr, pt, succ_ptr); //page
  if (!succ)
	  return -1;

  *paddr = phys_addr(vaddr, pg); //physical address

  return 0;
}

char *str_from_virt(vaddr_ptr vaddr, paddr_ptr cr3) {
  size_t buf_len = 1;
  char *buf = malloc(buf_len);
  char c = ' ';
  paddr_ptr paddr;

  for (int i=0; c; i++) {
    if(virt_to_phys(vaddr + i, cr3, &paddr)){
      printf("Page fault occured at address %p\n", (void *) vaddr + i);
      return (void *) 0;
    }

    ram_fetch(paddr, &c, 1);
    buf[i] = c;
    if (i + 1 >= buf_len) {
      buf_len <<= 1;
      buf = realloc(buf, buf_len);
    }
    buf[i + 1] = '\0';
  }
  return buf;
}

int main(int argc, char **argv) {

  if (argc != 4) {
    printf("Usage: ./mmu <mem_file> <cr3> <vaddr>\n");
    return 1;
  }

  paddr_ptr translated;

  ram_init();
  ram_load(argv[1]);

  paddr_ptr cr3 = strtol(argv[2], NULL, 0);
  vaddr_ptr vaddr = strtol(argv[3], NULL, 0);

  if(virt_to_phys(vaddr, cr3, &translated)){
    printf("Page fault occured at address %p\n", vaddr);
    exit(1);
  }

  char *str = str_from_virt(vaddr, cr3);
  printf("Virtual address %p translated to physical address %p\n", vaddr, translated);
  printf("String representation of data at virtual address %p: %s\n", vaddr, str);

  return 0;
}
