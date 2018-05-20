
/*
 * -- SuperLU MT routine (version 2.1) --
 * Lawrence Berkeley National Lab, Univ. of California Berkeley,
 * and Xerox Palo Alto Research Center.
 * September 10, 2007
 *
 * Sparse matrix types and function prototypes.
 *
 */

#ifndef __SUPERLU_zSP_DEFS /* allow multiple inclusions */
#define __SUPERLU_zSP_DEFS

/*
 * File name:       pzsp_defs.h
 * Purpose:         Sparse matrix types and function prototypes
 * Modified:        03/20/2013    
 */

/* Define my integer type int_t */
typedef int int_t; /* default */

#include "slu_mt_machines.h"
#include "slu_mt_Cnames.h"
#include "supermatrix.h"
#include "slu_mt_util.h"
#include "pxgstrf_synch.h"

/****************************
  Include thread header file
  ***************************/
#if ( MACH==PTHREAD )
    #include <pthread.h>
#elif ( MACH==OPENMP )
    #include <omp.h>
#elif ( MACH==SUN )
    #include <thread.h>
    #include <sched.h>
#elif ( MACH==DEC )
    #include <pthread.h>
    #include <unistd.h>
    #include <sys/mman.h>
#elif ( MACH==CRAY_PVP )
    #include <fortran.h>
    #include <string.h>
#endif


#include "slu_dcomplex.h"

/*
 * *************************************************
 *  Global data structures used in LU factorization
 * *************************************************
 * 
 *   nsuper: number of supernodes = nsuper+1, numbered between 0 and nsuper.
 *
 *   (supno, xsup, xsup_end):
 *      supno[i] is the supernode number to which column i belongs;
 *	xsup[s] points to the first column of supernode s;
 *      xsup_end[s] points to one past the last column of supernode s.
 *	Example: supno  0 1 2 2 3 3 3 4 4 4 4 4   (n=12)
 *	          xsup  0 1 2 4 7
 *            xsup_end  1 2 4 7 12
 *	Note: dfs will be performed on supernode rep. relative to the new 
 *	      row pivoting ordering
 *
 *   (lsub, xlsub, xlsub_end):
 *      lsub[*] contains the compressed subscripts of the supernodes;
 *      xlsub[j] points to the starting location of the j-th column in
 *               lsub[*]; 
 *      xlsub_end[j] points to one past the ending location of the j-th
 *               column in lsub[*].
 *	Storage: original row subscripts in A.
 *
 *      During the course of sparse LU factorization, we also use
 *	(lsub, xlsub, xlsub_end, xprune) to represent symmetrically 
 *      pruned graph. Contention will occur when one processor is
 *      performing DFS on supernode S, while another processor is pruning
 *      supernode S. We use the following data structure to deal with
 *      this problem. Suppose each supernode contains columns {s,s+1,...,t},
 *      with first column s and last column t.
 *
 *      (1) if t > s, only the subscript sets for column s and column t
 *          are stored. Column t represents pruned adjacency structure.
 *
 *                  --------------------------------------------
 *          lsub[*]    ... |   col s    |   col t   | ...
 *                  --------------------------------------------
 *                          ^            ^           ^
 *                       xlsub[s]    xlsub_end[s]  xlsub_end[s+1]
 *                                   xlsub[s+1]      :
 *                                       :           :
 *                                       :         xlsub_end[t]
 *                                   xlsub[t]      xprune[t] 
 *                                   xprune[s]    
 *
 *      (2) if t == s, i.e., a singleton supernode, the subscript set
 *          is stored twice:
 *
 *                  --------------------------------------
 *          lsub[*]    ... |      s     |     s     | ...
 *                  --------------------------------------
 *                          ^            ^           ^
 *                       xlsub[s]   xlsub_end[s]  xprune[s]
 *
 *      There are two subscript sets for each supernode, the last column
 *      structures (for pruning) will be removed after the numerical LU
 *      factorization phase:
 *        o lsub[j], j = xlsub[s], ..., xlsub_end[s]-1
 *          is the structure of column s (i.e. structure of this supernode).
 *          It is used for the storage of numerical values.
 *	  o lsub[j], j = xlsub[t], ..., xlsub_end[t]-1
 *	    is the structure of the last column t of this supernode.
 *	    It is for the purpose of symmetric pruning. Therefore, the
 *	    structural subscripts can be rearranged without making physical
 *	    interchanges among the numerical values.
 *
 *       DFS will traverse the first subscript set if the supernode
 *       has not been pruned; otherwise it will traverse the second
 *       subscript list, i.e., the part of the pruned graph.
 *
 *   (lusup, xlusup, xlusup_end):
 *      lusup[*] contains the numerical values of the supernodes;
 *      xlusup[j] points to the starting location of the j-th column in
 *                storage vector lusup[*]; 
 *      xlusup_end[j] points to one past the ending location of the j-th 
 *                column in lusup[*].
 *	Each supernode is stored in column-major, consistent with Fortran
 *      two-dimensional array storage.
 *
 *   (ucol, usub, xusub, xusub_end):
 *      ucol[*] stores the numerical values of the U-columns above the
 *              supernodes. 
 *      usub[k] stores the row subscripts of nonzeros ucol[k];
 *      xusub[j] points to the starting location of column j in ucol/usub[]; 
 *      xusub_end[j] points to one past the ending location column j in
 *                   ucol/usub[].
 *	Storage: new row subscripts; that is indexed intp PA.
 *
 */
typedef struct {
    int     *xsup;    /* supernode and column mapping */
    int     *xsup_end;
    int     *supno;   
    int     *lsub;    /* compressed L subscripts */
    int	    *xlsub;
    int     *xlsub_end;
    doublecomplex  *lusup;   /* L supernodes */
    int     *xlusup;
    int     *xlusup_end;
    doublecomplex  *ucol;    /* U columns */
    int     *usub;
    int	    *xusub;
    int     *xusub_end;
    int     nsuper;   /* current supernode number */
    int     nextl;    /* next position in lsub[] */
    int     nextu;    /* next position in usub[]/ucol[] */
    int     nextlu;   /* next position in lusup[] */
    int     nzlmax;   /* current max size of lsub[] */
    int     nzumax;   /*    "    "    "      ucol[] */
    int     nzlumax;  /*    "    "    "     lusup[] */
    /* ---------------------------------------------------------------
     *  Memory managemant for L supernodes 
     */
    int  *map_in_sup;  /* size n+1 - the address offset of each column
                        * in lusup[*], which is divided into regions 
			* by the supernodes of Householder matrix H.
			* If column k starts a supernode in H,
			* map_in_sup[k] is the next open position in
			* lusup[*]; otherwise map_in_sup[k] gives the
			* offset (negative) to the leading column
			* of the supernode in H.
			*/
    int  dynamic_snode_bound;
    /* --------------------------------------------------------------- */
} GlobalLU_t;


/* 
 * *********************************************************************
 * The pxgstrf_shared_t structure contains the shared task queue and
 * the synchronization variables to facilitate parallel factorization. 
 * It also contains the shared L and U data structures.
 * *********************************************************************
 */
typedef struct {
    /* ----------------------------------------------------------------
     * Global variables introduced in parallel code for synchronization.
     */
    volatile int tasks_remain; /* number of untaken panels */
    int          num_splits;   /* number of panels split at the top */
    queue_t      taskq;        /* size ncol - shared work queue */
    mutex_t      *lu_locks;    /* 5 named mutual exclusive locks */
    volatile int *spin_locks;  /* size ncol - mark every busy column */
    pan_status_t *pan_status;  /* size ncol - panel status */
    int          *fb_cols;     /* size ncol - mark farthest busy column */
    /* ---------------------------------------------------------------- */
    int        *inv_perm_c;
    int        *inv_perm_r;
    int        *xprune;
    int        *ispruned;
    SuperMatrix *A;
    GlobalLU_t *Glu;
    Gstat_t    *Gstat;
    int        *info;
} pxgstrf_shared_t;

/* Arguments passed to each thread. */
typedef struct {
    int  pnum; /* process number */
    int  info; /* error code returned from each thread */       
    superlumt_options_t *superlumt_options;
    pxgstrf_shared_t  *pxgstrf_shared; /* shared for LU factorization */
} pzgstrf_threadarg_t;


/* *********************
   Function prototypes
   *********************/

#ifdef __cplusplus
extern "C" {
#endif


/* ----------------
   Driver routines 
   ---------------*/
extern void
pzgssv(int, SuperMatrix *, int *, int *, SuperMatrix *, SuperMatrix *, 
       SuperMatrix *, int *);
extern void
pzgssvx(int, superlumt_options_t *, SuperMatrix *, int *, int *,  
	equed_t *, double *, double *, SuperMatrix *, SuperMatrix *,
	SuperMatrix *, SuperMatrix *, 
	double *, double *, double *, double *, superlu_memusage_t *, 
	int *);

/* ---------------
   Driver related 
   ---------------*/
extern void zgsequ (SuperMatrix *, double *, double *, double *,
                    double *, double *, int *);
extern void zlaqgs (SuperMatrix *, double *, double *, double, 
		    double, double, equed_t *);
extern void zgscon (char *, SuperMatrix *, SuperMatrix *,
		    double, double *, int *);
extern double zPivotGrowth(int, SuperMatrix *, int *,
			   SuperMatrix *, SuperMatrix *);
extern void zgsrfs (trans_t, SuperMatrix *, SuperMatrix *, SuperMatrix *,
		    int *, int *, equed_t, double *, double *, SuperMatrix *,
		    SuperMatrix *, double *, double *, Gstat_t *, int *);
extern int  sp_ztrsv (char *, char *, char *, SuperMatrix *, SuperMatrix *,
		      doublecomplex *, int *);
extern int  sp_zgemv (char *, doublecomplex, SuperMatrix *, doublecomplex *,
		      int, doublecomplex, doublecomplex *, int);
extern int  sp_zgemm (char *, int, int, int, doublecomplex, SuperMatrix *, 
		      doublecomplex *, int, doublecomplex, doublecomplex *, int);

/* ----------------------
   Factorization related
   ----------------------*/
extern void pxgstrf_scheduler (const int, const int, const int *,
			       int *, int *, pxgstrf_shared_t *);
extern int  zParallelInit (int, pxgstrf_relax_t *, superlumt_options_t *,
			  pxgstrf_shared_t *);
extern int  ParallelFinalize ();
extern int  queue_init (queue_t *, int);
extern int  queue_destroy (queue_t *);
extern int  EnqueueRelaxSnode (queue_t *, int, pxgstrf_relax_t *,
			      pxgstrf_shared_t *);
extern int  EnqueueDomains(queue_t *, struct Branch *, pxgstrf_shared_t *);
extern int  Enqueue (queue_t *, qitem_t);
extern int  Dequeue (queue_t *, qitem_t *);
extern int  NewNsuper (const int, pxgstrf_shared_t *, int *);
extern int  lockon(int *);
extern void PartDomains(const int, const float, SuperMatrix *, int *, int *);

extern void
zCreate_CompCol_Matrix(SuperMatrix *, int, int, int, doublecomplex *,
		      int *, int *, Stype_t, Dtype_t, Mtype_t);
void
zCreate_CompCol_Permuted(SuperMatrix *, int, int, int, doublecomplex *, int *,
			 int *, int *, Stype_t, Dtype_t, Mtype_t);
extern void
zCopy_CompCol_Matrix(SuperMatrix *, SuperMatrix *);
extern void
zCreate_Dense_Matrix(SuperMatrix *, int, int, doublecomplex *, int,
		     Stype_t, Dtype_t, Mtype_t);
extern void
zCreate_SuperNode_Matrix(SuperMatrix *, int, int, int, doublecomplex *, int *, int *,
			int *, int *, int *, Stype_t, Dtype_t, Mtype_t);
extern void
zCreate_SuperNode_Permuted(SuperMatrix *, int, int, int, doublecomplex *, 
			   int *, int *, int *, int *, int *, int *, 
			   int *, int *, Stype_t, Dtype_t, Mtype_t);
extern void
zCopy_Dense_Matrix(int, int, doublecomplex *, int, doublecomplex *, int);

extern void Destroy_SuperMatrix_Store(SuperMatrix *);
extern void Destroy_CompCol_Matrix(SuperMatrix *);
extern void Destroy_CompCol_Permuted(SuperMatrix *);
extern void Destroy_CompCol_NCP(SuperMatrix *);
extern void Destroy_SuperNode_Matrix(SuperMatrix *);
extern void Destroy_SuperNode_SCP(SuperMatrix *);

extern void zallocateA (int, int, doublecomplex **, int **, int **);
extern void StatAlloc (const int, const int, const int, const int, Gstat_t*);
extern void StatInit  (const int, const int, Gstat_t*);
extern void StatFree  (Gstat_t*);
extern void get_perm_c(int, SuperMatrix *, int *);
extern void zsp_colorder (SuperMatrix *, int *, superlumt_options_t *,
			 SuperMatrix *);
extern int  sp_coletree (int *, int *, int *, int, int, int *);
extern int  zPresetMap (const int, SuperMatrix *, pxgstrf_relax_t *, 
		       superlumt_options_t *, GlobalLU_t *);
extern int  qrnzcnt (int, int, int *, int *, int *, int *, int *, int *,
		     int *, int *, int *, int *);
extern int  DynamicSetMap(const int, const int, const int, pxgstrf_shared_t*);
extern void pzgstrf (superlumt_options_t *, SuperMatrix *, int *, 
		     SuperMatrix *, SuperMatrix *, Gstat_t *, int *);
extern void pzgstrf_init (int, fact_t, trans_t, yes_no_t, int, int, double, yes_no_t, double,
			  int *, int *, void *, int, SuperMatrix *,
			  SuperMatrix *, superlumt_options_t *, Gstat_t *);
extern pzgstrf_threadarg_t*
pzgstrf_thread_init (SuperMatrix *, SuperMatrix *, SuperMatrix *,
		     superlumt_options_t*, pxgstrf_shared_t*, Gstat_t*, int*);
extern void
pzgstrf_thread_finalize (pzgstrf_threadarg_t *, pxgstrf_shared_t *,
			 SuperMatrix *, int *, SuperMatrix *, SuperMatrix *);
extern void pzgstrf_finalize(superlumt_options_t *, SuperMatrix *);
extern void pxgstrf_finalize(superlumt_options_t *, SuperMatrix *);
extern void pzgstrf_relax_snode (const int, superlumt_options_t *,
				 pxgstrf_relax_t *);
extern int
pzgstrf_factor_snode (const int, const int, SuperMatrix *, const double,
		      yes_no_t *, int *, int *, int*, int*, int*, int*,
		      doublecomplex *, doublecomplex *, pxgstrf_shared_t *, int *);
extern void
pxgstrf_mark_busy_descends (int, int, int *, pxgstrf_shared_t *, int *, int *);
extern int  pzgstrf_snode_dfs (const int, const int, const int, const int *,
			       const int *, const int *, int*, int *, int *,
			       pxgstrf_shared_t *);
extern int  pzgstrf_snode_bmod (const int, const int, const int, const int,
				doublecomplex *, doublecomplex *, GlobalLU_t*, Gstat_t*);
extern void pzgstrf_panel_dfs (const int, const int, const int, const int,
			       SuperMatrix *, int*, int*, int*, int*, int*, 
			       int*, int*, int*, int*, int*, int*, int*, int*,
			       doublecomplex*, GlobalLU_t *);
extern void pzgstrf_panel_bmod (const int, const int, const int, const int,
				const int, int*, int*, int*, int*, int*, int*,
				int*, int*, doublecomplex*, doublecomplex*, 
				pxgstrf_shared_t *);
extern void pzgstrf_bmod1D (const int, const int, const int, const int, 
			    const int, const int, const int, int, int,
			    int *, int *, int *, int *, doublecomplex *, doublecomplex *, 
			    GlobalLU_t *, Gstat_t *);
extern void pzgstrf_bmod2D (const int, const int, const int, const int,
			    const int, const int, const int, int, int,
			    int *, int *, int *, int *, doublecomplex *, doublecomplex *,
			    GlobalLU_t *, Gstat_t *);
extern void pzgstrf_bmod1D_mv2 (const int, const int, const int, const int, 
				const int, const int, const int, int, int,
				int *, int *, int *, int *, doublecomplex *, 
				doublecomplex *, GlobalLU_t *, Gstat_t *);
extern void pzgstrf_bmod2D_mv2 (const int, const int, const int, const int,
				const int, const int, const int, int, int,
				int *, int *, int *, int *, doublecomplex *, doublecomplex *,
				GlobalLU_t *, Gstat_t *);
extern void pxgstrf_super_bnd_dfs (const int, const int, const int, 
				   const int, const int, SuperMatrix*,
				   int*, int*, int*, int *, int *, int *,
				   int *, pxgstrf_shared_t *);
extern int  pzgstrf_column_dfs(const int, const int, const int, const int,
			       int*, int*, int*, int, int*, int*, int*, int*,
			       int *, int *, int *, int *, pxgstrf_shared_t *);
extern int  pzgstrf_column_bmod(const int, const int, const int, const int, 
				int*, int*, doublecomplex*, doublecomplex*,
				pxgstrf_shared_t *, Gstat_t *);
extern int  pzgstrf_pivotL (const int, const int, const double, yes_no_t*,
			    int*, int*, int*, int*, GlobalLU_t*, Gstat_t*);
extern int  pzgstrf_copy_to_ucol (const int, const int, const int, const int *,
				  const int *, const int *, doublecomplex*,
				  pxgstrf_shared_t*);
extern void pxgstrf_pruneL (const int, const int *, const int, const int,
			    const int *, const int *, int*, int *,
			    GlobalLU_t *);
extern void pxgstrf_resetrep_col (const int, const int *, int *);
extern void countnz (const int, int*, int *, int *, GlobalLU_t *);
extern void fixupL (const int, const int *, GlobalLU_t *);
extern void compressSUP (const int, GlobalLU_t *);
extern int  spcoletree (int *, int *, int *, int, int, int *);
extern int  *TreePostorder (int, int *);
extern void zreadmt (int *, int *, int *, doublecomplex **, int **, int **);
extern void zreadhb (int *, int *, int *, doublecomplex **, int **, int **);
extern void zGenXtrue (int, int, doublecomplex *, int);
extern void zFillRHS (trans_t, int, doublecomplex *, int, 
		      SuperMatrix *, SuperMatrix *);
extern void zgstrs (trans_t, SuperMatrix *, SuperMatrix*, 
		    int*, int*, SuperMatrix*, Gstat_t *, int *);
extern void zlsolve (int, int, doublecomplex *, doublecomplex *);
extern void zusolve (int, int, doublecomplex *, doublecomplex *);
extern void zmatvec (int, int, int, doublecomplex *, doublecomplex *, doublecomplex *);


/* ---------------
   BLAS 
   ---------------*/
extern int zgemm_(char*, char*, int*, int*, int*, doublecomplex*,
                  doublecomplex*, int*, doublecomplex*, int*, doublecomplex*,
                  doublecomplex*, int*);
extern int ztrsm_(char*, char*, char*, char*, int*, int*, doublecomplex*,
                  doublecomplex*, int*, doublecomplex*, int*);
extern int ztrsv_(char*, char*, char*, int*, doublecomplex*, int*,
                  doublecomplex*, int*);
extern int zgemv_(char*, int*, int*, doublecomplex*, doublecomplex*, 
		   int*, doublecomplex*, int*, doublecomplex*, doublecomplex*, int*);

/* ---------------
   Memory related 
   ---------------*/
extern float pzgstrf_MemInit (int, int, superlumt_options_t *,
			SuperMatrix *, SuperMatrix *, GlobalLU_t *);
extern float pzgstrf_memory_use(const int, const int, const int);
extern int  pzgstrf_WorkInit (int, int, int **, doublecomplex **);
extern void pxgstrf_SetIWork (int, int, int *, int **, int **, int **,
		      int **, int **, int **, int **);
extern void pzgstrf_SetRWork (int, int, doublecomplex *, doublecomplex **, doublecomplex **);
extern void pzgstrf_WorkFree (int *, doublecomplex *, GlobalLU_t *);
extern int  pzgstrf_MemXpand (int, int, MemType, int *, GlobalLU_t *);

extern int  *intMalloc (int);
extern int  *intCalloc (int);
extern doublecomplex *doublecomplexMalloc(int);
extern doublecomplex *doublecomplexCalloc(int);
extern int  memory_usage ();
extern int  superlu_zQuerySpace (int, SuperMatrix *, SuperMatrix *, int, 
				 superlu_memusage_t *);
extern int  Glu_alloc (const int, const int, const int, const MemType,
		       int *, pxgstrf_shared_t *);

/* -------------------
   Auxiliary routines
   -------------------*/
extern double  SuperLU_timer_();
extern int     sp_ienv(int);
extern double  dlamch_();
extern int     lsame_(char *, char *);
extern int     xerbla_(char *, int *);
extern void    superlu_abort_and_exit(char *);
extern void    ifill(int *, int, int);
extern void    zfill(doublecomplex *, int, doublecomplex);
extern void    zinf_norm_error(int, SuperMatrix *, doublecomplex *);
extern void    dstat_allocate(int);
extern void    snode_profile(int, int *);
extern void    super_stats(int, int *, int *);
extern void    panel_stats(int, int, int *, Gstat_t *);
extern void    PrintSumm(char *, int, int, int);
extern void    zPrintPerf(SuperMatrix *, SuperMatrix *, superlu_memusage_t *,
			 double, double, double *, double *, char *,
			 Gstat_t *);
extern void    zCompRow_to_CompCol(int m, int n, int nnz, 
                           doublecomplex *a, int *colind, int *rowptr,
                           doublecomplex **at, int **rowind, int **colptr);


/* -----------------------
   Routines for debugging
   -----------------------*/
extern void    print_lu_col(int, char *, int, int, int, int *, GlobalLU_t *);
extern void    print_panel_seg(int, int, int, int, int *, int *);
extern void    zcheck_zero_vec(int, char *, int, doublecomplex *);
extern void    check_repfnz(int, int, int, int *);

#ifdef __cplusplus
	   }
#endif


#endif /* __SUPERLU_ZSP_DEFS */
