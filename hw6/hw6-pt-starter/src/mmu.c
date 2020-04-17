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
  return (paddr_ptr) (pfn << 12);
}

//Returns the base pointer to the page directory table
paddr_ptr pd_addr(vaddr_ptr vaddr, paddr_ptr pdpt) {
  vaddr_ptr mask = 0xC0000000;
  paddr_ptr index = (paddr_ptr) ((vaddr & mask) >> 30);
  paddr_ptr entry = pdpt + (((paddr_ptr) sizeof(pgd_t)) * index);
  void * buffer = malloc(sizeof(pgd_t));
  ram_fetch(entry, buffer, sizeof(pgd_t));
  
//  if (!((pgd_t *) buffer)->present)
//	  return NULL;

  return pfn_to_addr(((pgd_t *) buffer)->pfn);
}

//Returns the base pointer to the page table
paddr_ptr pt_addr(vaddr_ptr vaddr, paddr_ptr pdt) {
  vaddr_ptr mask = 0x3FE00000;
  paddr_ptr index = (paddr_ptr) ((vaddr & mask) >> 21);
  paddr_ptr entry = pdt + (((paddr_ptr) sizeof(pmd_t)) * index);
  void * buffer = malloc(sizeof(pmd_t));
  ram_fetch(entry, buffer, sizeof(pmd_t));

//  if (!((pmd_t *) buffer)->present)
//	  return NULL;

  return pfn_to_addr(((pmd_t *) buffer)->pfn);
}

//Returns the base pointer to the page
paddr_ptr pg_addr(vaddr_ptr vaddr, paddr_ptr pt) {
  vaddr_ptr mask = 0x001FF000;
  paddr_ptr index = (paddr_ptr) ((vaddr & mask) >> 12);
  paddr_ptr entry = pt + (((paddr_ptr) sizeof(pte_t)) * index);
  void * buffer = malloc(sizeof(pte_t));
  ram_fetch(entry, buffer, sizeof(pte_t));
  
//  if (!((pte_t*) buffer)->present)
//	  return NULL;

  return pfn_to_addr(((pte_t *) buffer)->pfn);
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
  paddr_ptr pd = pd_addr(vaddr, cr3); //page directory table
//  if (pd == NULL)
//	  return -1;

  paddr_ptr pt =  pt_addr(vaddr, pd); //page table
//  if (pt == NULL)
//	  return -1;

  paddr_ptr pg = pg_addr(vaddr, pt); //page
//  if (pg == NULL)
//	  return -1;

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
