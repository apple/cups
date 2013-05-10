/* Copyright (C) 1993, 1996 Aladdin Enterprises.  All rights reserved.
  
  This file is part of GNU Ghostscript.
  
  GNU Ghostscript is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility to
  anyone for the consequences of using it or for whether it serves any
  particular purpose or works at all, unless he says so in writing.  Refer to
  the GNU General Public License for full details.
  
  Everyone is granted permission to copy, modify and redistribute GNU
  Ghostscript, but only under the conditions described in the GNU General
  Public License.  A copy of this license is supposed to have been given to
  you along with GNU Ghostscript so you can know your rights and
  responsibilities.  It should be in a file named COPYING.  Among other
  things, the copyright notice and this notice must be preserved on all
  copies.
  
  Aladdin Enterprises is not affiliated with the Free Software Foundation or
  the GNU Project.  GNU Ghostscript, as distributed by Aladdin Enterprises,
  does not depend on any other GNU software.
*/

/* gsstruct.h */
/* Definitions for Ghostscript modules that define allocatable structures */
/* Requires gstypes.h */

#ifndef gsstruct_INCLUDED
#  define gsstruct_INCLUDED

/*
 * Ghostscript structures are defined with names of the form (gs_)xxx_s,
 * with a corresponding typedef of the form (gs_)xxx or (gs_)xxx_t.
 * By extension, the structure descriptor is named st_[gs_]xxx.
 * (Note that the descriptor name may omit the gs_ even if the type has it.)
 * Structure descriptors are always allocated statically and are
 * always const; they may be either public or private.
 *
 * In order to ensure that there is a descriptor for each structure type,
 * we require, by convention, that the following always appear together
 * if the structure is defined in a .h file:
 *	- The definition of the structure xxx_s;
 *	- If the descriptor is public, an extern_st(st_xxx);
 *	- The definition of a macro public_st_xxx() or private_st_xxx()
 *	that creates the actual descriptor.
 * This convention makes the descriptor visible (if public) to any module
 * that can see the structure definition.  This is more liberal than
 * we would like, but it is a reasonable compromise between restricting
 * visibility and keeping all the definitional elements of a structure
 * together.  We require that there be no other externs for (public)
 * structure descriptors; if the definer of a structure wants to make
 * available the ability to create an instance but does not want to
 * expose the structure definition, it must export a creator procedure.
 *
 * Because of bugs in some compilers' bookkeeping for undefined structure
 * types, any file that uses extern_st must include gsstruct.h.
 * (If it weren't for these bugs, the definition of extern_st could
 * go in gsmemory.h.)
 */
#define extern_st(st) extern const gs_memory_struct_type_t st
/*
 * If the structure is defined in a .c file, we require that the following
 * appear together:
 *	- The definition of the structure xxx_s;
 *	- The gs_private_st_xxx macro that creates the descriptor.
 * Note that we only allow this if the structure is completely private
 * to a single file.  Again, the file must export a creator procedure
 * if it wants external clients to be able to create instances.
 *
 * Some structures are embedded inside others.  In order to be able to
 * construct the composite pointer enumeration procedures, for such
 * structures we must define not only the st_xxx descriptor, but also
 * a st_xxx_max_ptrs constant that gives the maximum number of pointers
 * the enumeration procedure will return.  This is an unfortunate consequence
 * of the method we have chosen for implementing pointer enumeration.
 *
 * Some structures may exist as elements of homogenous arrays.
 * In order to be able to enumerate and relocate such arrays, we adopt
 * the convention that the structure representing an element must be
 * distinguished from the structure per se, and the name of the element
 * structure always ends with "_element".  Element structures cannot be
 * embedded in other structures.
 *
 * Note that the definition of the xxx_s structure may be separate from
 * the typedef for the type xxx(_t).  This still allows us to have full
 * structure type abstraction.
 *
 * Descriptor definitions are not required for structures to which
 * no traceable pointers from garbage-collectable space will ever exist.
 * For example, the struct that defines structure types themselves does not
 * require a descriptor.
 */

/* An opaque type for an object header. */
#ifndef obj_header_DEFINED
#  define obj_header_DEFINED
typedef struct obj_header_s obj_header_t;
#endif

/*
 * A descriptor for an object (structure) type.
 */
typedef struct struct_shared_procs_s struct_shared_procs_t;
struct gs_memory_struct_type_s {
	uint ssize;
	struct_name_t sname;

	/* ------ Procedures shared among many structure types. ------ */
	/* Note that this pointer is usually 0. */

	const struct_shared_procs_t _ds *shared;

	/* ------ Procedures specific to this structure type. ------ */
	/* Note that these procedures may be 0. */

		/* Clear the marks of a structure. */

#define struct_proc_clear_marks(proc)\
  void proc(P2(void /*obj_header_t*/ *pre, uint size))
	struct_proc_clear_marks((*clear_marks));

		/* Enumerate the pointers in a structure. */

#define struct_proc_enum_ptrs(proc)\
  gs_ptr_type_t proc(P4(void /*obj_header_t*/ *ptr, uint size, uint index, void **pep))
	struct_proc_enum_ptrs((*enum_ptrs));

		/* Relocate all the pointers in this structure. */

#define struct_proc_reloc_ptrs(proc)\
  void proc(P3(void /*obj_header_t*/ *ptr, uint size, gc_state_t *gcst))
	struct_proc_reloc_ptrs((*reloc_ptrs));

		/*
		 * Finalize this structure just before freeing it.
		 * Finalization procedures must not allocate or resize
		 * any objects in any space managed by the allocator,
		 * and must not assume that any objects in such spaces
		 * referenced by this structure still exist.  However,
		 * finalization procedures may free such objects, and
		 * may allocate, free, and reference objects allocated
		 * in other ways, such as objects allocated with malloc
		 * by libraries.
		 */

#define struct_proc_finalize(proc)\
  void proc(P1(void /*obj_header_t*/ *ptr))
	struct_proc_finalize((*finalize));

};
#define struct_type_name_string(pstype) ((const char *)((pstype)->sname))
/* Default pointer processing */
struct_proc_enum_ptrs(gs_no_struct_enum_ptrs);
struct_proc_reloc_ptrs(gs_no_struct_reloc_ptrs);
/* Standard relocation procedures */
ptr_proc_reloc(gs_reloc_struct_ptr, void /*obj_header_t*/);
void gs_reloc_string(P2(gs_string *, gc_state_t *));
void gs_reloc_const_string(P2(gs_const_string *, gc_state_t *));

/* Define a 'type' descriptor for free blocks. */
extern_st(st_free);

/* Define a type descriptor for byte objects. */
extern_st(st_bytes);

/* ================ Macros for defining structure types ================ */

#define public_st public const gs_memory_struct_type_t
#define private_st private const gs_memory_struct_type_t

/* -------------- Simple structures (no internal pointers). -------------- */

#define gs__st_simple(scope_st, stname, stype, sname)\
  scope_st stname = { sizeof(stype), sname, 0, 0, 0, 0, 0 }
#define gs_public_st_simple(stname, stype, sname)\
  gs__st_simple(public_st, stname, stype, sname)
#define gs_private_st_simple(stname, stype, sname)\
  gs__st_simple(private_st, stname, stype, sname)

/* ---------------- Structures with explicit procedures. ---------------- */

	/* Complex structures with their own clear_marks, */
	/* enum, reloc, and finalize procedures. */

#define gs__st_complex_only(scope_st, stname, stype, sname, pclear, penum, preloc, pfinal)\
  scope_st stname = { sizeof(stype), sname, 0, pclear, penum, preloc, pfinal }
#define gs_public_st_complex_only(stname, stype, sname, pclear, penum, preloc, pfinal)\
  gs__st_complex_only(public_st, stname, stype, sname, pclear, penum, preloc, pfinal)
#define gs_private_st_complex_only(stname, stype, sname, pclear, penum, preloc, pfinal)\
  gs__st_complex_only(private_st, stname, stype, sname, pclear, penum, preloc, pfinal)

#define gs__st_complex(scope_st, stname, stype, sname, pclear, penum, preloc, pfinal)\
  private struct_proc_clear_marks(pclear);\
  private struct_proc_enum_ptrs(penum);\
  private struct_proc_reloc_ptrs(preloc);\
  private struct_proc_finalize(pfinal);\
  gs__st_complex_only(scope_st, stname, stype, sname, pclear, penum, preloc, pfinal)
#define gs_public_st_complex(stname, stype, sname, pclear, penum, preloc, pfinal)\
  gs__st_complex(public_st, stname, stype, sname, pclear, penum, preloc, pfinal)
#define gs_private_st_complex(stname, stype, sname, pclear, penum, preloc, pfinal)\
  gs__st_complex(private_st, stname, stype, sname, pclear, penum, preloc, pfinal)

	/* Composite structures with their own enum and reloc procedures. */

#define gs__st_composite(scope_st, stname, stype, sname, penum, preloc)\
  private struct_proc_enum_ptrs(penum);\
  private struct_proc_reloc_ptrs(preloc);\
  gs__st_complex_only(scope_st, stname, stype, sname, 0, penum, preloc, 0)
#define gs_public_st_composite(stname, stype, sname, penum, preloc)\
  gs__st_composite(public_st, stname, stype, sname, penum, preloc)
#define gs_private_st_composite(stname, stype, sname, penum, preloc)\
  gs__st_composite(private_st, stname, stype, sname, penum, preloc)

	/* Composite structures with finalization. */

#define gs__st_composite_final(scope_st, stname, stype, sname, penum, preloc, pfinal)\
  private struct_proc_enum_ptrs(penum);\
  private struct_proc_reloc_ptrs(preloc);\
  private struct_proc_finalize(pfinal);\
  gs__st_complex_only(scope_st, stname, stype, sname, 0, penum, preloc, pfinal)
#define gs_public_st_composite_final(stname, stype, sname, penum, preloc, pfinal)\
  gs__st_composite_final(public_st, stname, stype, sname, penum, preloc, pfinal)
#define gs_private_st_composite_final(stname, stype, sname, penum, preloc, pfinal)\
  gs__st_composite_final(private_st, stname, stype, sname, penum, preloc, pfinal)

	/* Composite structures with enum and reloc procedures */
	/* already declared. */

#define gs__st_composite_only(scope_st, stname, stype, sname, penum, preloc)\
  gs__st_complex_only(scope_st, stname, stype, sname, 0, penum, preloc, 0)
#define gs_public_st_composite_only(stname, stype, sname, penum, preloc)\
  gs__st_composite_only(public_st, stname, stype, sname, penum, preloc)
#define gs_private_st_composite_only(stname, stype, sname, penum, preloc)\
  gs__st_composite_only(private_st, stname, stype, sname, penum, preloc)

/* ---------------- Special kinds of structures ---------------- */

	/* Element structures, for use in arrays of structures. */
	/* Note that these require that the underlying structure's */
	/* enum_ptrs procedure always return the same number of pointers. */

#define gs__st_element(scope_st, stname, stype, sname, penum, preloc, basest)\
  private ENUM_PTRS_BEGIN_PROC(penum) {\
    uint count = size / (uint)sizeof(stype);\
    if ( count == 0 ) return 0;\
    return (*basest.enum_ptrs)((char *)vptr + (index % count) * sizeof(stype),\
      sizeof(stype), index / count, pep);\
  } ENUM_PTRS_END_PROC\
  private RELOC_PTRS_BEGIN(preloc) {\
    uint count = size / (uint)sizeof(stype);\
    for ( ; count; count--, vptr = (char *)vptr + sizeof(stype) )\
      (*basest.reloc_ptrs)(vptr, sizeof(stype), gcst);\
  } RELOC_PTRS_END\
  gs__st_composite_only(scope_st, stname, stype, sname, penum, preloc)
#define gs_public_st_element(stname, stype, sname, penum, preloc, basest)\
  gs__st_element(public_st, stname, stype, sname, penum, preloc, basest)
#define gs_private_st_element(stname, stype, sname, penum, preloc, basest)\
  gs__st_element(private_st, stname, stype, sname, penum, preloc, basest)

	/* A "structure" just consisting of a pointer. */
	/* Note that in this case only, stype is a pointer type. */

#define gs__st_ptr(scope_st, stname, stype, sname, penum, preloc)\
  private ENUM_PTRS_BEGIN(penum) return 0;\
    case 0: *pep = (void *)*(stype *)vptr; break;\
  ENUM_PTRS_END\
  private RELOC_PTRS_BEGIN(preloc) ;\
    *(stype *)vptr = gs_reloc_struct_ptr((const void *)*(stype *)vptr, gcst);\
  RELOC_PTRS_END\
  gs__st_composite_only(scope_st, stname, stype, sname, penum, preloc)
#define gs_public_st_ptr(stname, stype, sname, penum, preloc)\
  gs__st_ptr(public_st, stname, stype, sname, penum, preloc)
#define gs_private_st_ptr(stname, stype, sname, penum, preloc)\
  gs__st_ptr(private_st, stname, stype, sname, penum, preloc)

/* ---------------- Ordinary structures ---------------- */

/*
 * The simplest kind of composite structure is one with a fixed set of
 * pointers, each of which points to a struct.  We provide macros for
 * defining this kind of structure conveniently, either all at once in
 * the structure definition macro, or using the following template:

ENUM_PTRS_BEGIN(xxx_enum_ptrs) return 0;
	... ENUM_PTR(i, xxx, elt); ...
ENUM_PTRS_END
RELOC_PTRS_BEGIN(xxx_reloc_ptrs) ;
	... RELOC_PTR(xxx, elt) ...
RELOC_PTRS_END

 */
/*
 * We have to pull the 'private' outside the ENUM_PTRS_BEGIN and
 * RELOC_PTRS_BEGIN macros because of a bug in the Borland C++ preprocessor.
 * We also have to make sure there is more on the line after these
 * macros, so as not to confuse ansi2knr.
 */
#ifdef __PROTOTYPES__
#  define ENUM_PTRS_BEGIN_PROC(proc)\
    gs_ptr_type_t proc(void *vptr, uint size, uint index, void **pep)
#else
#  define ENUM_PTRS_BEGIN_PROC(proc)\
    gs_ptr_type_t proc(vptr, size, index, pep) void *vptr; uint size; uint index; void **pep;
#endif
#define ENUM_PTRS_BEGIN(proc)\
  ENUM_PTRS_BEGIN_PROC(proc) { switch ( index ) { default:
#define ENUM_PTR(i, typ, elt)\
  case i: ENUM_RETURN_PTR(typ, elt)
#define ENUM_RETURN_PTR(typ, elt)\
  ENUM_RETURN(((typ *)vptr)->elt)
#define ENUM_RETURN(ptr)\
  *pep = (void *)(ptr); break		/* discard const */
#define ENUM_STRING_PTR(i, typ, elt)\
  case i: ENUM_RETURN_STRING_PTR(typ, elt)
#define ENUM_RETURN_STRING_PTR(typ, elt)\
  *pep = (void *)&((typ *)vptr)->elt; return ptr_string_type
#define ENUM_CONST_STRING_PTR(i, typ, elt)\
  case i: ENUM_RETURN_CONST_STRING_PTR(typ, elt)
#define ENUM_RETURN_CONST_STRING_PTR(typ, elt)\
  *pep = (void *)&((typ *)vptr)->elt; return ptr_const_string_type
#define ENUM_PTRS_END\
  } return ptr_struct_type; ENUM_PTRS_END_PROC }
#define ENUM_PTRS_END_PROC /* */
#ifdef __PROTOTYPES__
#  define RELOC_PTRS_BEGIN(proc)\
    void proc(void *vptr, uint size, gc_state_t *gcst) {
#else
#  define RELOC_PTRS_BEGIN(proc)\
    void proc(vptr, size, gcst) void *vptr; uint size; gc_state_t *gcst; {
#endif
#define RELOC_PTR(typ, elt)\
  ((typ *)vptr)->elt =\
    gs_reloc_struct_ptr((const void *)((const typ *)vptr)->elt, gcst)
/* Relocate a pointer that points to a known offset within an object. */
/* OFFSET is for byte offsets, TYPED_OFFSET is for element offsets. */
#define RELOC_OFFSET_PTR(typ, elt, offset)\
  ((typ *)vptr)->elt = (void *)\
    ((char *)gs_reloc_struct_ptr((char *)((typ *)vptr)->elt - (offset), gcst) +\
     (offset))
#define RELOC_TYPED_OFFSET_PTR(typ, elt, offset)\
  (((typ *)vptr)->elt = (void *)\
    gs_reloc_struct_ptr(((typ *)vptr)->elt - (offset), gcst),\
   ((typ *)vptr)->elt += (offset))
#define RELOC_STRING_PTR(typ, elt)\
  gs_reloc_string(&((typ *)vptr)->elt, gcst)
#define RELOC_CONST_STRING_PTR(typ, elt)\
  gs_reloc_const_string(&((typ *)vptr)->elt, gcst)
#define RELOC_PTRS_END\
  }

/*
 * Boilerplate for clear_marks procedures.
 */
#ifdef __PROTOTYPES__
#  define CLEAR_MARKS_PROC(proc)\
    void proc(void *vptr, uint size)
#else
#  define CLEAR_MARKS_PROC(proc)\
    void proc(vptr, size) void *vptr; uint size;
#endif

/* ---------------- Structures with a fixed set of pointers ---------------- */

	/* Structures with 1 pointer. */

#define gs__st_ptrs1(scope_st, stname, stype, sname, penum, preloc, e1)\
  private ENUM_PTRS_BEGIN(penum) return 0;\
    ENUM_PTR(0,stype,e1);\
  ENUM_PTRS_END\
  private RELOC_PTRS_BEGIN(preloc) ;\
    RELOC_PTR(stype,e1);\
  RELOC_PTRS_END\
  gs__st_composite_only(scope_st, stname, stype, sname, penum, preloc)
#define gs_public_st_ptrs1(stname, stype, sname, penum, preloc, e1)\
  gs__st_ptrs1(public_st, stname, stype, sname, penum, preloc, e1)
#define gs_private_st_ptrs1(stname, stype, sname, penum, preloc, e1)\
  gs__st_ptrs1(private_st, stname, stype, sname, penum, preloc, e1)

	/* Structures with 2 pointers. */

#define gs__st_ptrs2(scope_st, stname, stype, sname, penum, preloc, e1, e2)\
  private ENUM_PTRS_BEGIN(penum) return 0;\
    ENUM_PTR(0,stype,e1); ENUM_PTR(1,stype,e2);\
  ENUM_PTRS_END\
  private RELOC_PTRS_BEGIN(preloc) ;\
    RELOC_PTR(stype,e1); RELOC_PTR(stype,e2);\
  RELOC_PTRS_END\
  gs__st_composite_only(scope_st, stname, stype, sname, penum, preloc)
#define gs_public_st_ptrs2(stname, stype, sname, penum, preloc, e1, e2)\
  gs__st_ptrs2(public_st, stname, stype, sname, penum, preloc, e1, e2)
#define gs_private_st_ptrs2(stname, stype, sname, penum, preloc, e1, e2)\
  gs__st_ptrs2(private_st, stname, stype, sname, penum, preloc, e1, e2)

	/* Structures with 3 pointers. */

#define gs__st_ptrs3(scope_st, stname, stype, sname, penum, preloc, e1, e2, e3)\
  private ENUM_PTRS_BEGIN(penum) return 0;\
    ENUM_PTR(0,stype,e1); ENUM_PTR(1,stype,e2); ENUM_PTR(2,stype,e3);\
  ENUM_PTRS_END\
  private RELOC_PTRS_BEGIN(preloc) ;\
    RELOC_PTR(stype,e1); RELOC_PTR(stype,e2); RELOC_PTR(stype,e3);\
  RELOC_PTRS_END\
  gs__st_composite_only(scope_st, stname, stype, sname, penum, preloc)
#define gs_public_st_ptrs3(stname, stype, sname, penum, preloc, e1, e2, e3)\
  gs__st_ptrs3(public_st, stname, stype, sname, penum, preloc, e1, e2, e3)
#define gs_private_st_ptrs3(stname, stype, sname, penum, preloc, e1, e2, e3)\
  gs__st_ptrs3(private_st, stname, stype, sname, penum, preloc, e1, e2, e3)

	/* Structures with 4 pointers. */

#define gs__st_ptrs4(scope_st, stname, stype, sname, penum, preloc, e1, e2, e3, e4)\
  private ENUM_PTRS_BEGIN(penum) return 0;\
    ENUM_PTR(0,stype,e1); ENUM_PTR(1,stype,e2); ENUM_PTR(2,stype,e3);\
    ENUM_PTR(3,stype,e4);\
  ENUM_PTRS_END\
  private RELOC_PTRS_BEGIN(preloc) ;\
    RELOC_PTR(stype,e1); RELOC_PTR(stype,e2); RELOC_PTR(stype,e3);\
    RELOC_PTR(stype,e4);\
  RELOC_PTRS_END\
  gs__st_composite_only(scope_st, stname, stype, sname, penum, preloc)
#define gs_public_st_ptrs4(stname, stype, sname, penum, preloc, e1, e2, e3, e4)\
  gs__st_ptrs4(public_st, stname, stype, sname, penum, preloc, e1, e2, e3, e4)
#define gs_private_st_ptrs4(stname, stype, sname, penum, preloc, e1, e2, e3, e4)\
  gs__st_ptrs4(private_st, stname, stype, sname, penum, preloc, e1, e2, e3, e4)

	/* Structures with 5 pointers. */

#define gs__st_ptrs5(scope_st, stname, stype, sname, penum, preloc, e1, e2, e3, e4, e5)\
  private ENUM_PTRS_BEGIN(penum) return 0;\
    ENUM_PTR(0,stype,e1); ENUM_PTR(1,stype,e2); ENUM_PTR(2,stype,e3);\
    ENUM_PTR(3,stype,e4); ENUM_PTR(4,stype,e5);\
  ENUM_PTRS_END\
  private RELOC_PTRS_BEGIN(preloc) ;\
    RELOC_PTR(stype,e1); RELOC_PTR(stype,e2); RELOC_PTR(stype,e3);\
    RELOC_PTR(stype,e4); RELOC_PTR(stype,e5);\
  RELOC_PTRS_END\
  gs__st_composite_only(scope_st, stname, stype, sname, penum, preloc)
#define gs_public_st_ptrs5(stname, stype, sname, penum, preloc, e1, e2, e3, e4, e5)\
  gs__st_ptrs5(public_st, stname, stype, sname, penum, preloc, e1, e2, e3, e4, e5)
#define gs_private_st_ptrs5(stname, stype, sname, penum, preloc, e1, e2, e3, e4, e5)\
  gs__st_ptrs5(private_st, stname, stype, sname, penum, preloc, e1, e2, e3, e4, e5)

/* ---------------- Suffix subclasses ---------------- */

/*
 * Boilerplate for suffix subclasses.  Special subclasses constructed
 * 'by hand' may use this also.
 */
#define ENUM_PREFIX(supst, n)\
  return (supst.enum_ptrs ? (*supst.enum_ptrs)(vptr,size,index-(n),pep) : 0)
#define RELOC_PREFIX(supst)\
  if ( supst.reloc_ptrs ) (*supst.reloc_ptrs)(vptr,size,gcst)

	/* Suffix subclasses with no additional pointers. */

#define gs__st_suffix_add0(scope_st, stname, stype, sname, penum, preloc, supstname)\
  private ENUM_PTRS_BEGIN_PROC(penum) {\
    return (*supstname.enum_ptrs)(vptr, size, index, pep);\
  } ENUM_PTRS_END_PROC\
  private RELOC_PTRS_BEGIN(preloc) {\
    (*supstname.reloc_ptrs)(vptr, size, gcst);\
  } RELOC_PTRS_END\
  gs__st_composite_only(scope_st, stname, stype, sname, penum, preloc)
#define gs_public_st_suffix_add0(stname, stype, sname, penum, preloc, supstname)\
  gs__st_suffix_add0(public_st, stname, stype, sname, penum, preloc, supstname)
#define gs_private_st_suffix_add0(stname, stype, sname, penum, preloc, supstname)\
  gs__st_suffix_add0(private_st, stname, stype, sname, penum, preloc, supstname)

	/* Suffix subclasses with no additional pointers and finalization. */
	/* This is a hack -- subclasses should inherit finalization, */
	/* but that would require a subclass pointer in the descriptor, */
	/* which would perturb things too much right now. */

#define gs__st_suffix_add0_final(scope_st, stname, stype, sname, penum, preloc, pfinal, supstname)\
  private ENUM_PTRS_BEGIN_PROC(penum) {\
    return (*supstname.enum_ptrs)(vptr, size, index, pep);\
  } ENUM_PTRS_END_PROC\
  private RELOC_PTRS_BEGIN(preloc) {\
    (*supstname.reloc_ptrs)(vptr, size, gcst);\
  } RELOC_PTRS_END\
  gs__st_complex_only(scope_st, stname, stype, sname, 0, penum, preloc, pfinal)
#define gs_public_st_suffix_add0_final(stname, stype, sname, penum, preloc, pfinal, supstname)\
  gs__st_suffix_add0_final(public_st, stname, stype, sname, penum, preloc, pfinal, supstname)
#define gs_private_st_suffix_add0_final(stname, stype, sname, penum, preloc, pfinal, supstname)\
  gs__st_suffix_add0_final(private_st, stname, stype, sname, penum, preloc, pfinal, supstname)

	/* Suffix subclasses with 1 additional pointer. */

#define gs__st_suffix_add1(scope_st, stname, stype, sname, penum, preloc, supstname, e1)\
  private ENUM_PTRS_BEGIN(penum) ENUM_PREFIX(supstname,1);\
    ENUM_PTR(0,stype,e1);\
  ENUM_PTRS_END\
  private RELOC_PTRS_BEGIN(preloc) RELOC_PREFIX(supstname);\
    RELOC_PTR(stype,e1);\
  RELOC_PTRS_END\
  gs__st_composite_only(scope_st, stname, stype, sname, penum, preloc)
#define gs_public_st_suffix_add1(stname, stype, sname, penum, preloc, supstname, e1)\
  gs__st_suffix_add1(public_st, stname, stype, sname, penum, preloc, supstname, e1)
#define gs_private_st_suffix_add1(stname, stype, sname, penum, preloc, supstname, e1)\
  gs__st_suffix_add1(private_st, stname, stype, sname, penum, preloc, supstname, e1)

	/* Suffix subclasses with 1 additional pointer and finalization. */
	/* This is a hack -- see above. */

#define gs__st_suffix_add1_final(scope_st, stname, stype, sname, penum, preloc, pfinal, supstname, e1)\
  private ENUM_PTRS_BEGIN(penum) ENUM_PREFIX(supstname,1);\
    ENUM_PTR(0,stype,e1);\
  ENUM_PTRS_END\
  private RELOC_PTRS_BEGIN(preloc) RELOC_PREFIX(supstname);\
    RELOC_PTR(stype,e1);\
  RELOC_PTRS_END\
  gs__st_complex_only(scope_st, stname, stype, sname, 0, penum, preloc, pfinal)
#define gs_public_st_suffix_add1_final(stname, stype, sname, penum, preloc, pfinal, supstname, e1)\
  gs__st_suffix_add1_final(public_st, stname, stype, sname, penum, preloc, pfinal, supstname, e1)
#define gs_private_st_suffix_add1_final(stname, stype, sname, penum, preloc, pfinal, supstname, e1)\
  gs__st_suffix_add1_final(private_st, stname, stype, sname, penum, preloc, pfinal, supstname, e1)

	/* Suffix subclasses with 2 additional pointers. */

#define gs__st_suffix_add2(scope_st, stname, stype, sname, penum, preloc, supstname, e1, e2)\
  private ENUM_PTRS_BEGIN(penum) ENUM_PREFIX(supstname,2);\
    ENUM_PTR(0,stype,e1); ENUM_PTR(1,stype,e2);\
  ENUM_PTRS_END\
  private RELOC_PTRS_BEGIN(preloc) RELOC_PREFIX(supstname);\
    RELOC_PTR(stype,e1); RELOC_PTR(stype,e2);\
  RELOC_PTRS_END\
  gs__st_composite_only(scope_st, stname, stype, sname, penum, preloc)
#define gs_public_st_suffix_add2(stname, stype, sname, penum, preloc, supstname, e1, e2)\
  gs__st_suffix_add2(public_st, stname, stype, sname, penum, preloc, supstname, e1, e2)
#define gs_private_st_suffix_add2(stname, stype, sname, penum, preloc, supstname, e1, e2)\
  gs__st_suffix_add2(private_st, stname, stype, sname, penum, preloc, supstname, e1, e2)

	/* Suffix subclasses with 3 additional pointers. */

#define gs__st_suffix_add3(scope_st, stname, stype, sname, penum, preloc, supstname, e1, e2, e3)\
  private ENUM_PTRS_BEGIN(penum) ENUM_PREFIX(supstname,3);\
    ENUM_PTR(0,stype,e1); ENUM_PTR(1,stype,e2); ENUM_PTR(2,stype,e3);\
  ENUM_PTRS_END\
  private RELOC_PTRS_BEGIN(preloc) RELOC_PREFIX(supstname);\
    RELOC_PTR(stype,e1); RELOC_PTR(stype,e2); RELOC_PTR(stype,e3);\
  RELOC_PTRS_END\
  gs__st_composite_only(scope_st, stname, stype, sname, penum, preloc)
#define gs_public_st_suffix_add3(stname, stype, sname, penum, preloc, supstname, e1, e2, e3)\
  gs__st_suffix_add3(public_st, stname, stype, sname, penum, preloc, supstname, e1, e2, e3)
#define gs_private_st_suffix_add3(stname, stype, sname, penum, preloc, supstname, e1, e2, e3)\
  gs__st_suffix_add3(private_st, stname, stype, sname, penum, preloc, supstname, e1, e2, e3)

/* ---------------- General subclasses ---------------- */

/*
 * Boilerplate for general subclasses.
 */
#define ENUM_SUPER(stype, supst, member, n)\
  return (*supst.enum_ptrs)(&((stype *)vptr)->member, sizeof(((stype *)vptr)->member),\
    index-(n), pep)
#define RELOC_SUPER(stype, supst, member)\
  (*supst.reloc_ptrs)(&((stype *)vptr)->member, sizeof(((stype *)vptr)->member), gcst)

	/* General subclasses with no additional pointers. */

#define gs__st_ptrs_add0(scope_st, stname, stype, sname, penum, preloc, supstname, member)\
  private ENUM_PTRS_BEGIN(penum) ENUM_SUPER(stype,supstname,member,0);\
  ENUM_PTRS_END\
  private RELOC_PTRS_BEGIN(preloc) RELOC_SUPER(stype,supstname,member);\
  RELOC_PTRS_END\
  gs__st_composite_only(scope_st, stname, stype, sname, penum, preloc)
#define gs_public_st_ptrs_add0(stname, stype, sname, penum, preloc, supstname, member)\
  gs__st_ptrs_add0(public_st, stname, stype, sname, penum, preloc, supstname, member)
#define gs_private_st_ptrs_add0(stname, stype, sname, penum, preloc, supstname, member)\
  gs__st_ptrs_add0(private_st, stname, stype, sname, penum, preloc, supstname, member)

	/* General subclasses with 1 additional pointer. */

#define gs__st_ptrs_add1(scope_st, stname, stype, sname, penum, preloc, supstname, member, e1)\
  private ENUM_PTRS_BEGIN(penum) ENUM_SUPER(stype,supstname,member,1);\
    ENUM_PTR(0,stype,e1);\
  ENUM_PTRS_END\
  private RELOC_PTRS_BEGIN(preloc) RELOC_SUPER(stype,supstname,member);\
    RELOC_PTR(stype,e1);\
  RELOC_PTRS_END\
  gs__st_composite_only(scope_st, stname, stype, sname, penum, preloc)
#define gs_public_st_ptrs_add1(stname, stype, sname, penum, preloc, supstname, member, e1)\
  gs__st_ptrs_add1(public_st, stname, stype, sname, penum, preloc, supstname, member, e1)
#define gs_private_st_ptrs_add1(stname, stype, sname, penum, preloc, supstname, member, e1)\
  gs__st_ptrs_add1(private_st, stname, stype, sname, penum, preloc, supstname, member, e1)

	/* General subclasses with 2 additional pointers. */

#define gs__st_ptrs_add2(scope_st, stname, stype, sname, penum, preloc, supstname, member, e1, e2)\
  private ENUM_PTRS_BEGIN(penum) ENUM_SUPER(stype,supstname,member,2);\
    ENUM_PTR(0,stype,e1); ENUM_PTR(1,stype,e2);\
  ENUM_PTRS_END\
  private RELOC_PTRS_BEGIN(preloc) RELOC_SUPER(stype,supstname,member);\
    RELOC_PTR(stype,e1); RELOC_PTR(stype,e2);\
  RELOC_PTRS_END\
  gs__st_composite_only(scope_st, stname, stype, sname, penum, preloc)
#define gs_public_st_ptrs_add2(stname, stype, sname, penum, preloc, supstname, member, e1, e2)\
  gs__st_ptrs_add2(public_st, stname, stype, sname, penum, preloc, supstname, member, e1, e2)
#define gs_private_st_ptrs_add2(stname, stype, sname, penum, preloc, supstname, member, e1, e2)\
  gs__st_ptrs_add2(private_st, stname, stype, sname, penum, preloc, supstname, member, e1, e2)

#endif					/* gsstruct_INCLUDED */
