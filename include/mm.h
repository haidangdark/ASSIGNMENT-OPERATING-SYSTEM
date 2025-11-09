#ifndef MM_H

#include "common.h"
#include "bitops.h"

/* CPU Bus definition */
#define PAGING_CPU_BUS_WIDTH 22 /* 22bit bus - MAX SPACE 4MB */
#define PAGING_PAGESZ  256      /* 256B or 8-bits PAGE NUMBER */
#define PAGING_MEMRAMSZ BIT(21)
#define PAGING_PAGE_ALIGNSZ(sz) (DIV_ROUND_UP(sz,PAGING_PAGESZ)*PAGING_PAGESZ)

#define PAGING_MEMSWPSZ BIT(29)
#define PAGING_SWPFPN_OFFSET 5  
#define PAGING_MAX_PGN  (DIV_ROUND_UP(BIT(PAGING_CPU_BUS_WIDTH),PAGING_PAGESZ))

#define PAGING_SBRK_INIT_SZ PAGING_PAGESZ
/* PTE BIT */
#define PAGING_PTE_PRESENT_MASK BIT(31) 
#define PAGING_PTE_SWAPPED_MASK BIT(30)
#define PAGING_PTE_RESERVE_MASK BIT(29)
#define PAGING_PTE_DIRTY_MASK BIT(28)
#define PAGING_PTE_EMPTY01_MASK BIT(14)
#define PAGING_PTE_EMPTY02_MASK BIT(13)

/* PTE BIT PRESENT */
#define PAGING_PTE_SET_PRESENT(pte) (pte=pte|PAGING_PTE_PRESENT_MASK)
#define PAGING_PAGE_PRESENT(pte) (pte&PAGING_PTE_PRESENT_MASK)

/* USRNUM */
#define PAGING_PTE_USRNUM_LOBIT 15
#define PAGING_PTE_USRNUM_HIBIT 27
/* FPN */
#define PAGING_PTE_FPN_LOBIT 0
#define PAGING_PTE_FPN_HIBIT 12
/* SWPTYP */
#define PAGING_PTE_SWPTYP_LOBIT 0
#define PAGING_PTE_SWPTYP_HIBIT 4
/* SWPOFF */
#define PAGING_PTE_SWPOFF_LOBIT 5
#define PAGING_PTE_SWPOFF_HIBIT 25

/* PTE */
#define PAGING_PTE_USRNUM_MASK GENMASK(PAGING_PTE_USRNUM_HIBIT,PAGING_PTE_USRNUM_LOBIT)
#define PAGING_PTE_FPN_MASK    GENMASK(PAGING_PTE_FPN_HIBIT,PAGING_PTE_FPN_LOBIT)
#define PAGING_PTE_SWPTYP_MASK GENMASK(PAGING_PTE_SWPTYP_HIBIT,PAGING_PTE_SWPTYP_LOBIT)
#define PAGING_PTE_SWPOFF_MASK GENMASK(PAGING_PTE_SWPOFF_HIBIT,PAGING_PTE_SWPOFF_LOBIT)

/* Extract PTE */
#define PAGING_PTE_OFFST(pte) GETVAL(pte,PAGING_OFFST_MASK,PAGING_ADDR_OFFST_LOBIT)
#define PAGING_PTE_PGN(pte)   GETVAL(pte,PAGING_PGN_MASK,PAGING_ADDR_PGN_LOBIT)
#define PAGING_PTE_FPN(pte)   GETVAL(pte,PAGING_PTE_FPN_MASK,PAGING_PTE_FPN_LOBIT)
#define PAGING_PTE_SWP(pte)   GETVAL(pte,PAGING_PTE_SWPOFF_MASK,PAGING_SWPFPN_OFFSET)

/* OFFSET */
#define PAGING_ADDR_OFFST_LOBIT 0
#define PAGING_ADDR_OFFST_HIBIT (NBITS(PAGING_PAGESZ) - 1)

/* PAGE Num */
#define PAGING_ADDR_PGN_LOBIT NBITS(PAGING_PAGESZ)
#define PAGING_ADDR_PGN_HIBIT (PAGING_CPU_BUS_WIDTH - 1)

/* Frame PHY Num */
#define PAGING_ADDR_FPN_LOBIT NBITS(PAGING_PAGESZ)
#define PAGING_ADDR_FPN_HIBIT (NBITS(PAGING_MEMRAMSZ) - 1)

/* SWAPFPN */
#define PAGING_SWP_LOBIT NBITS(PAGING_PAGESZ)
#define PAGING_SWP_HIBIT (NBITS(PAGING_MEMSWPSZ) - 1)
#define PAGING_SWP(pte) ((pte&PAGING_PTE_SWPOFF_MASK) >> PAGING_SWPFPN_OFFSET)

/* Value operators */
#define SETBIT(v,mask) (v=v|mask)
#define CLRBIT(v,mask) (v=v&~mask)

#define SETVAL(v,value,mask,offst) (v=(v&~mask)|((value<<offst)&mask))
#define GETVAL(v,mask,offst) ((v&mask)>>offst)

/* Masks */
#define PAGING_OFFST_MASK  GENMASK(PAGING_ADDR_OFFST_HIBIT,PAGING_ADDR_OFFST_LOBIT)
#define PAGING_PGN_MASK  GENMASK(PAGING_ADDR_PGN_HIBIT,PAGING_ADDR_PGN_LOBIT)
#define PAGING_FPN_MASK  GENMASK(PAGING_ADDR_FPN_HIBIT,PAGING_ADDR_FPN_LOBIT)
#define PAGING_SWP_MASK  GENMASK(PAGING_SWP_HIBIT,PAGING_SWP_LOBIT)

/* Extract OFFSET */
//#define PAGING_OFFST(x)  ((x&PAGING_OFFST_MASK) >> PAGING_ADDR_OFFST_LOBIT)
#define PAGING_OFFST(x)  GETVAL(x,PAGING_OFFST_MASK,PAGING_ADDR_OFFST_LOBIT)
/* Extract Page Number*/
#define PAGING_PGN(x)  GETVAL(x,PAGING_PGN_MASK,PAGING_ADDR_PGN_LOBIT)
/* Extract FramePHY Number*/
#define PAGING_FPN(x)  GETVAL(x,PAGING_PTE_FPN_MASK,PAGING_PTE_FPN_LOBIT)
/* Extract SWAPFPN */
#define PAGING_PGN(x)  GETVAL(x,PAGING_PGN_MASK,PAGING_ADDR_PGN_LOBIT)
/* Extract SWAPTYPE */
#define PAGING_FPN(x)  GETVAL(x,PAGING_PTE_FPN_MASK,PAGING_PTE_FPN_LOBIT)

/* Memory range operator */
/* TODO implement the INCLUDE and OVERLAP checking mechanism */
#define INCLUDE(x1,x2,y1,y2) (0)
#define OVERLAP(x1,x2,y1,y2) (0)

/* VM region prototypes */
struct vm_rg_struct * init_vm_rg(addr_t rg_start, addr_t rg_end);
int enlist_vm_rg_node(struct vm_rg_struct **rglist, struct vm_rg_struct* rgnode);
int enlist_pgn_node(struct pgn_t **pgnlist, addr_t pgn);
int vmap_pgd_memset(struct pcb_t *caller, addr_t addr, int pgnum);
addr_t vmap_page_range(struct pcb_t *caller, addr_t addr, int pgnum, 
                    struct framephy_struct *frames, struct vm_rg_struct *ret_rg);
addr_t vm_map_ram(struct pcb_t *caller, addr_t astart, addr_t aend, addr_t mapstart, int incpgnum, struct vm_rg_struct *ret_rg);
addr_t alloc_pages_range(struct pcb_t *caller, int incpgnum, struct framephy_struct **frm_lst);
int __swap_cp_page(struct memphy_struct *mpsrc, addr_t srcfpn,
                struct memphy_struct *mpdst, addr_t dstfpn) ;
int get_pd_from_address(addr_t addr, addr_t* pgd, addr_t* p4d, addr_t* pud, addr_t* pmd, addr_t* pt);
int get_pd_from_pagenum(addr_t pgn, addr_t* pgd, addr_t* p4d, addr_t* pud, addr_t* pmd, addr_t* pt);
int pte_set_fpn(struct pcb_t *caller, addr_t pgn, addr_t fpn);
int pte_set_swap(struct pcb_t *caller, addr_t pgn, int swptyp, addr_t swpoff);
uint32_t pte_get_entry(struct pcb_t *caller, addr_t pgn);
int pte_set_entry(struct pcb_t *caller, addr_t pgn, uint32_t pte_val);
int init_pte(addr_t *pte,
             int pre,    // present
             addr_t fpn,    // FPN
             int drt,    // dirty
             int swp,    // swap
             int swptyp, // swap type
             addr_t swpoff); //swap offset
int __alloc(struct pcb_t *caller, int vmaid, int rgid, addr_t size, addr_t *alloc_addr);
int __free(struct pcb_t *caller, int vmaid, int rgid);
int __read(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE *data);
int __write(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE value);
int init_mm(struct mm_struct *mm, struct pcb_t *caller);

/* VM prototypes */
int pgalloc(struct pcb_t *proc, uint32_t size, uint32_t reg_index);
int pgfree_data(struct pcb_t *proc, uint32_t reg_index);
int pgread(
		struct pcb_t * proc, // Process executing the instruction
		uint32_t source, // Index of source register
		addr_t offset, // Source address = [source] + [offset]
		uint32_t destination);
int pgwrite(
		struct pcb_t * proc, // Process executing the instruction
		BYTE data, // Data to be wrttien into memory
		uint32_t destination, // Index of destination register
		addr_t offset);
/* Local VM prototypes */
struct vm_rg_struct * get_symrg_byid(struct mm_struct* mm, int rgid);
int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, addr_t vmastart, addr_t vmaend);
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg);
int inc_vma_limit(struct pcb_t *caller, int vmaid, addr_t inc_sz);
int find_victim_page(struct mm_struct* mm, addr_t *pgn);
struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid);

/* MEM/PHY protypes */
int MEMPHY_get_freefp(struct memphy_struct *mp, addr_t *fpn);
int MEMPHY_put_freefp(struct memphy_struct *mp, addr_t fpn);
int MEMPHY_read(struct memphy_struct * mp, addr_t addr, BYTE *value);
int MEMPHY_write(struct memphy_struct * mp, addr_t addr, BYTE data);
int MEMPHY_dump(struct memphy_struct * mp);
int init_memphy(struct memphy_struct *mp, addr_t max_size, int randomflg);

/* print list */
int print_list_fp(struct framephy_struct *fp);
int print_list_rg(struct vm_rg_struct *rg);
int print_list_vma(struct vm_area_struct *rg);


int print_list_pgn(struct pgn_t *ip);
int print_pgtbl(struct pcb_t *ip, addr_t start, addr_t end);
#endif
