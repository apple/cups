/* Copyright (C) 1993, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
  
  This file is part of GNU Ghostscript.
  
  GNU Ghostscript is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility
  to anyone for the consequences of using it or for whether it serves any
  particular purpose or works at all, unless he says so in writing.  Refer
  to the GNU General Public License for full details.
  
  Everyone is granted permission to copy, modify and redistribute GNU
  Ghostscript, but only under the conditions described in the GNU General
  Public License.  A copy of this license is supposed to have been given
  to you along with GNU Ghostscript so you can know your rights and
  responsibilities.  It should be in a file named COPYING.  Among other
  things, the copyright notice and this notice must be preserved on all
  copies.
  
  Aladdin Enterprises supports the work of the GNU Project, but is not
  affiliated with the Free Software Foundation or the GNU Project.  GNU
  Ghostscript, as distributed by Aladdin Enterprises, does not require any
  GNU software to build or run it.
*/

/*$Id: gsstruct.h,v 1.2 2000/03/08 23:14:47 mike Exp $ */
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
 *      - The definition of the structure xxx_s;
 *      - If the descriptor is public, an extern_st(st_xxx);
 *      - The definition of a macro public_st_xxx() or private_st_xxx()
 *      that creates the actual descriptor.
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
 *      - The definition of the structure xxx_s;
 *      - The gs_private_st_xxx macro that creates the descriptor.
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

/* Define an opaque type for the garbage collector state. */
typedef struct gc_state_s gc_state_t;

/*
 * Define pointer types, which define how to mark the referent of the
 * pointer.
 */
/*typedef struct gs_ptr_procs_s gs_ptr_procs_t;*/  /* in gsmemory.h */
struct gs_ptr_procs_s {

    /* Unmark the referent of a pointer. */

#define ptr_proc_unmark(proc)\
  void proc(P2(void *, gc_state_t *))
    ptr_proc_unmark((*unmark));

    /* Mark the referent of a pointer. */
    /* Return true iff it was unmarked before. */

#define ptr_proc_mark(proc)\
  bool proc(P2(void *, gc_state_t *))
    ptr_proc_mark((*mark));

    /* Relocate a pointer. */
    /* Note that the argument is const, but the */
    /* return value is not: this shifts the compiler */
    /* 'discarding const' warning from the call sites */
    /* (the reloc_ptr routines) to the implementations. */

#define ptr_proc_reloc(proc, typ)\
  typ *proc(P2(const typ *, gc_state_t *))
    ptr_proc_reloc((*reloc), void);

};
/*typedef const gs_ptr_procs_t *gs_ptr_type_t;*/  /* in gsmemory.h */

/* Define the pointer type for ordinary structure pointers. */
extern const gs_ptr_procs_t ptr_struct_procs;
#define ptr_struct_type (&ptr_struct_procs)

/* Define the pointer types for a pointer to a gs_[const_]string. */
extern const gs_ptr_procs_t ptr_string_procs;
#define ptr_string_type (&ptr_string_procs)
extern const gs_ptr_procs_t ptr_const_string_procs;
#define ptr_const_string_type (&ptr_const_string_procs)

/*
 * Define the type for a GC root.
 */
/*typedef struct gs_gc_root_s gs_gc_root_t;*/  /* in gsmemory.h */
struct gs_gc_root_s {
    gs_gc_root_t *next;
    gs_ptr_type_t ptype;
    void **p;
    bool free_on_unregister;
};

#define public_st_gc_root_t()	/* in gsmemory.c */\
  gs_public_st_ptrs1(st_gc_root_t, gs_gc_root_t, "gs_gc_root_t",\
    gc_root_enum_ptrs, gc_root_reloc_ptrs, next)

/* Print a root debugging message. */
#define if_debug_root(c, msg, rp)\
  if_debug4(c, "%s 0x%lx: 0x%lx -> 0x%lx\n",\
	    msg, (ulong)(rp), (ulong)(rp)->p, (ulong)*(rp)->p)

/*
 * We don't want to tie the allocator to using a single garbage collector,
 * so we pass all the relevant GC procedures in to the structure pointer
 * enumeration and relocation procedures.  The GC state must begin with
 * a pointer to the following procedure vector.
 *
 * By default, this is all the procedures we know about, but there are
 * additional procedures defined in the interpreter for dealing with
 * 'ref' objects.
 */
#define string_proc_reloc(proc)\
  void proc(P2(gs_string *, gc_state_t *))
#define const_string_proc_reloc(proc)\
  void proc(P2(gs_const_string *, gc_state_t *))
#define gc_procs_common\
	/* Relocate a pointer to an object. */\
  ptr_proc_reloc((*reloc_struct_ptr), void /*obj_header_t*/);\
	/* Relocate a pointer to a string. */\
  string_proc_reloc((*reloc_string));\
	/* Relocate a pointer to a const string. */\
  const_string_proc_reloc((*reloc_const_string))
typedef struct gc_procs_common_s {
    gc_procs_common;
} gc_procs_common_t;

#define gc_proc(gcst, proc) ((*(const gc_procs_common_t **)(gcst))->proc)

/*
 * The first argument of enum_ptrs procedures is logically const *.
 * Unfortunately, actually declaring it as such would produce many compiler
 * warnings from places that cast this argument to a non-const non-void
 * pointer type.  For the moment, we define EV_CONST as empty, with the
 * intention of changing it to const at some future time.
 */
/*#define EV_CONST const */
#define EV_CONST		/* */

/* Define the procedures for structure types. */

		/* Clear the marks of a structure. */

#define struct_proc_clear_marks(proc)\
  void proc(P3(void /*obj_header_t*/ *pre, uint size,\
    const gs_memory_struct_type_t *pstype))

		/* Enumerate the pointers in a structure. */

#define struct_proc_enum_ptrs(proc)\
  gs_ptr_type_t proc(P6(EV_CONST void /*obj_header_t*/ *ptr, uint size,\
    int index, const void **pep, const gs_memory_struct_type_t *pstype,\
    gc_state_t *gcst))

		/* Relocate all the pointers in this structure. */

#define struct_proc_reloc_ptrs(proc)\
  void proc(P4(void /*obj_header_t*/ *ptr, uint size,\
    const gs_memory_struct_type_t *pstype, gc_state_t *gcst))

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

/*
 * A descriptor for an object (structure) type.
 */
typedef struct struct_shared_procs_s struct_shared_procs_t;

struct gs_memory_struct_type_s {
	uint ssize;
	struct_name_t sname;

	/* ------ Procedures shared among many structure types. ------ */
	/* Note that this pointer is usually 0. */

	const struct_shared_procs_t *shared;

	/* ------ Procedures specific to this structure type. ------ */

	struct_proc_clear_marks((*clear_marks));
	struct_proc_enum_ptrs((*enum_ptrs));
	struct_proc_reloc_ptrs((*reloc_ptrs));
	struct_proc_finalize((*finalize));

	/* A pointer to additional data for the above procedures. */

	const void *proc_data;

};

#define struct_type_name_string(pstype) ((const char *)((pstype)->sname))
/* Default pointer processing */
struct_proc_enum_ptrs(gs_no_struct_enum_ptrs);
struct_proc_reloc_ptrs(gs_no_struct_reloc_ptrs);

/* Define 'type' descriptors for some standard objects. */

    /* Free blocks */

extern_st(st_free);

    /* Byte objects */

extern_st(st_bytes);

    /* GC roots */

extern_st(st_gc_root_t);

    /* Elements and arrays of const strings. */

#define private_st_const_string()\
  private const gc_ptr_element_t const_string_elts[] = {\
    { GC_ELT_CONST_STRING, 0 }\
  };\
  gs__st_basic_with_final(private_st, st_const_string, gs_const_string,\
    "gs_const_string", 1, const_string_elts, const_string_sdata, 0, 0, 0)

extern_st(st_const_string_element);
#define public_st_const_string_element()\
  gs_public_st_element(st_const_string_element, gs_const_string,\
    "gs_const_string[]", const_string_elt_enum_ptrs,\
    const_string_elt_reloc_ptrs, st_const_string)

/* ================ Macros for defining structure types ================ */

#define public_st public const gs_memory_struct_type_t
#define private_st private const gs_memory_struct_type_t

/*
 * As an alternative to defining different enum_ptrs and reloc_ptrs
 * procedures for basic structure types that only have a fixed number of
 * pointers and possibly a single supertype, we can define the type's GC
 * information using stock procedures and a table.  Each entry in the table
 * defines one element of the structure.
 */

/* Define the pointer types of individual elements. */

typedef enum {
    GC_ELT_OBJ,			/* obj * or const obj * */
    GC_ELT_STRING,		/* gs_string */
    GC_ELT_CONST_STRING,	/* gs_const_string */
    GC_ELT_REF
} gc_ptr_type_index_t;

typedef struct gc_ptr_element_s {
    ushort /*gc_ptr_type_index_t */ type;
    ushort offset;
} gc_ptr_element_t;

#define GC_OBJ_ELT(typ, elt)\
  { GC_ELT_OBJ, offset_of(typ, elt) }
#define GC_OBJ_ELT2(typ, e1, e2)\
  GC_OBJ_ELT(typ, e1), GC_OBJ_ELT(typ, e2)
#define GC_OBJ_ELT3(typ, e1, e2, e3)\
  GC_OBJ_ELT(typ, e1), GC_OBJ_ELT(typ, e2), GC_OBJ_ELT(typ, e3)
#define GC_STRING_ELT(typ, elt)\
  { GC_ELT_STRING, offset_of(typ, elt) }
#define GC_CONST_STRING_ELT(typ, elt)\
  { GC_ELT_CONST_STRING, offset_of(typ, elt) }
#define GC_REF_ELT(typ, elt)\
  { GC_ELT_REF, offset_of(typ, elt) }

/* Define the complete table of descriptor data. */

typedef struct gc_struct_data_s {
    ushort num_ptrs;
    ushort super_offset;
    const gs_memory_struct_type_t *super_type; /* 0 if none */
    const gc_ptr_element_t *ptrs;
} gc_struct_data_t;

/*
 * Define the enum_ptrs and reloc_ptrs procedures, and the declaration
 * macros, for table-specified structures.  For such structures, the
 * proc_data points to a gc_struct_data_t.  The standard defining form
 * is:

 private const gc_ptr_element_t XXX[] = {
    ... elements ...
 };
 gs_(private|public)_st_basic(stname, stype, sname, XXX, YYY, supst, supoff);

 */
struct_proc_enum_ptrs(basic_enum_ptrs);
struct_proc_reloc_ptrs(basic_reloc_ptrs);

#define gs__st_basic_with_final(scope_st, stname, stype, sname, nelts, elts, sdata, supst, supoff, pfinal)\
  private const gc_struct_data_t sdata = {\
    nelts, supoff, supst, elts\
  };\
  scope_st stname = {\
    sizeof(stype), sname, 0, 0, basic_enum_ptrs, basic_reloc_ptrs,\
    pfinal, &sdata\
  }
#define gs__st_basic_final(scope_st, stname, stype, sname, elts, sdata, supst, supoff, pfinal)\
  gs__st_basic_with_final(scope_st, stname, stype, sname, countof(elts), elts, sdata, supst, supoff, pfinal)
#define gs_public_st_basic_final(stname, stype, sname, elts, sdata, supst, supoff, pfinal)\
  gs__st_basic_final(public_st, stname, stype, sname, elts, sdata, supst, supoff, pfinal)
#define gs_private_st_basic_final(stname, stype, sname, elts, sdata, supst, supoff, pfinal)\
  gs__st_basic_final(private_st, stname, stype, sname, elts, sdata, supst, supoff, pfinal)
#define gs__st_basic(scope_st, stname, stype, sname, elts, sdata, supst, supoff)\
  gs__st_basic_with_final(scope_st, stname, stype, sname, countof(elts), elts, sdata, supst, supoff, 0)
#define gs_public_st_basic(stname, stype, sname, elts, sdata, supst, supoff)\
  gs__st_basic(public_st, stname, stype, sname, elts, sdata, supst, supoff)
#define gs_private_st_basic(stname, stype, sname, elts, sdata, supst, supoff)\
  gs__st_basic(private_st, stname, stype, sname, elts, sdata, supst, supoff)

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

     /* Begin enumeration */

#ifdef __PROTOTYPES__
#  define ENUM_PTRS_BEGIN_PROC(proc)\
    gs_ptr_type_t proc(EV_CONST void *vptr, uint size, int index, const void **pep, const gs_memory_struct_type_t *pstype, gc_state_t *gcst)
#else
#  define ENUM_PTRS_BEGIN_PROC(proc)\
    gs_ptr_type_t proc(vptr, size, index, pep, pstype, gcst) EV_CONST void *vptr; uint size; int index; const void **pep; const gs_memory_struct_type_t *pstype; gc_state_t *gcst;
#endif
#define ENUM_PTRS_BEGIN(proc)\
  ENUM_PTRS_BEGIN_PROC(proc) { switch ( index ) { default:

    /* Enumerate elements */

#define ENUM_OBJ(ptr)		/* pointer to object */\
  (*pep = (const void *)(ptr), ptr_struct_type)
#define ENUM_STRING(ptr)	/* pointer to gs_string */\
  (*pep = (const void *)(ptr), ptr_string_type)
#define ENUM_CONST_STRING(ptr)	/* pointer to gs_const_string */\
  (*pep = (const void *)(ptr), ptr_const_string_type)

#define ENUM_OBJ_ELT(typ, elt)\
  ENUM_OBJ(((const typ *)vptr)->elt)
#define ENUM_STRING_ELT(typ, elt)\
  ENUM_STRING(&((const typ *)vptr)->elt)
#define ENUM_CONST_STRING_ELT(typ, elt)\
  ENUM_CONST_STRING(&((const typ *)vptr)->elt)

#define ENUM_PTR(i, typ, elt)\
  case i: return ENUM_OBJ_ELT(typ, elt)
#define ENUM_PTR3(i, typ, e1, e2, e3) /* just an abbreviation */\
  ENUM_PTR(i, typ, e1); ENUM_PTR((i)+1, typ, e2); ENUM_PTR((i)+2, typ, e3)
#define ENUM_STRING_PTR(i, typ, elt)\
  case i: return ENUM_STRING_ELT(typ, elt)
#define ENUM_CONST_STRING_PTR(i, typ, elt)\
  case i: return ENUM_CONST_STRING_ELT(typ, elt)

    /* End enumeration */

#define ENUM_PTRS_END\
  } /* mustn't fall through! */ ENUM_PTRS_END_PROC }
#define ENUM_PTRS_END_PROC	/* */

    /* Begin relocation */

#ifdef __PROTOTYPES__
#  define RELOC_PTRS_BEGIN(proc)\
    void proc(void *vptr, uint size, const gs_memory_struct_type_t *pstype, gc_state_t *gcst) {
#else
#  define RELOC_PTRS_BEGIN(proc)\
    void proc(vptr, size, pstype, gcst) void *vptr; uint size; const gs_memory_struct_type_t *pstype; gc_state_t *gcst; {
#endif

    /* Relocate elements */

#define RELOC_OBJ(ptr)\
  (gc_proc(gcst, reloc_struct_ptr)((const void *)(ptr), gcst))
#define RELOC_OBJ_VAR(ptrvar)\
  (ptrvar = RELOC_OBJ(ptrvar))
#define RELOC_STRING_VAR(ptrvar)\
  (gc_proc(gcst, reloc_string)(&(ptrvar), gcst))
#define RELOC_CONST_STRING_VAR(ptrvar)\
  (gc_proc(gcst, reloc_const_string)(&(ptrvar), gcst))

#define RELOC_OBJ_ELT(typ, elt)\
  RELOC_VAR(((typ *)vptr)->elt)
#define RELOC_STRING_ELT(typ, elt)\
  RELOC_STRING_VAR(((typ *)vptr)->elt)
#define RELOC_CONST_STRING_ELT(typ, elt)\
  RELOC_CONST_STRING_VAR(((typ *)vptr)->elt)

/* Relocate a pointer that points to a known offset within an object. */
/* OFFSET is for byte offsets, TYPED_OFFSET is for element offsets. */
#define RELOC_OFFSET_ELT(typ, elt, offset)\
  ((typ *)vptr)->elt = (void *)\
    ((char *)RELOC_OBJ((char *)((typ *)vptr)->elt - (offset)) +\
     (offset))
#define RELOC_TYPED_OFFSET_ELT(typ, elt, offset)\
  (((typ *)vptr)->elt = (void *)RELOC_OBJ(((typ *)vptr)->elt - (offset)),\
   ((typ *)vptr)->elt += (offset))

    /* Backward compatibility */

#define RELOC_VAR(ptrvar)\
  RELOC_OBJ_VAR(ptrvar)
#define RELOC_PTR(typ, elt)\
  RELOC_OBJ_ELT(typ, elt)
#define RELOC_PTR3(typ, e1, e2, e3) /* just an abbreviation */\
  RELOC_PTR(typ,e1); RELOC_PTR(typ,e2); RELOC_PTR(typ,e3)
#define RELOC_OFFSET_PTR(typ, elt, offset)\
  RELOC_OFFSET_ELT(typ, elt, offset)
#define RELOC_TYPED_OFFSET_PTR(typ, elt, offset)\
  RELOC_TYPED_OFFSET_ELT(typ, elt, offset)
#define RELOC_STRING_PTR(typ, elt)\
  RELOC_STRING_ELT(typ, elt)
#define RELOC_CONST_STRING_PTR(typ, elt)\
  RELOC_CONST_STRING_ELT(typ, elt)

    /* End relocation */

#define RELOC_PTRS_END\
  }

    /* Subclass support */

#define ENUM_USING(supst, ptr, size, index)\
  (*(supst).enum_ptrs)(ptr, size, index, pep, &(supst), gcst)

#define RELOC_USING(supst, ptr, size)\
  (*(supst).reloc_ptrs)(ptr, size, &(supst), gcst)

    /*
     * Support for suffix subclasses.  Special subclasses constructed
     * 'by hand' may use this also.
     */

#define ENUM_PREFIX(supst, n)\
  return ENUM_USING(supst, vptr, size, index-(n))

#define RELOC_PREFIX(supst)\
  RELOC_USING(supst, vptr, size)

    /*
     * Support for general subclasses.
     */

#define ENUM_SUPER_ELT(stype, supst, member, n)\
  ENUM_USING(supst, &((EV_CONST stype *)vptr)->member, sizeof(((EV_CONST stype *)vptr)->member), index-(n))
#define ENUM_SUPER(stype, supst, member, n)\
  return ENUM_SUPER_ELT(stype, supst, member, n)

#define RELOC_SUPER_ELT(stype, supst, member)\
  RELOC_USING(supst, &((stype *)vptr)->member, sizeof(((stype *)vptr)->member))
#define RELOC_SUPER(stype, supst, member)\
  RELOC_SUPER_ELT(stype, supst, member)

    /* Backward compatibility. */

#define ENUM_RETURN(ptr) return ENUM_OBJ(ptr)
#define ENUM_RETURN_PTR(typ, elt) return ENUM_OBJ_ELT(typ, elt)
#define ENUM_RETURN_STRING_PTR(typ, elt) return ENUM_STRING_ELT(typ, elt)
#define ENUM_RETURN_CONST_STRING(ptr) return ENUM_CONST_STRING(ptr)
#define ENUM_RETURN_CONST_STRING_PTR(typ, elt) return ENUM_CONST_STRING_ELT(typ, elt)

/* -------------- Simple structures (no internal pointers). -------------- */

#define gs__st_simple(scope_st, stname, stype, sname)\
  scope_st stname = { sizeof(stype), sname, 0, 0, gs_no_struct_enum_ptrs, gs_no_struct_reloc_ptrs, 0 }
#define gs_public_st_simple(stname, stype, sname)\
  gs__st_simple(public_st, stname, stype, sname)
#define gs_private_st_simple(stname, stype, sname)\
  gs__st_simple(private_st, stname, stype, sname)

/* ---------------- Structures with explicit procedures. ---------------- */

/*
 * Boilerplate for clear_marks procedures.
 */
#ifdef __PROTOTYPES__
#  define CLEAR_MARKS_PROC(proc)\
    void proc(void *vptr, uint size, const gs_memory_struct_type_t *pstype)
#else
#  define CLEAR_MARKS_PROC(proc)\
    void proc(vptr, size, pstype) void *vptr; uint size; const gs_memory_struct_type_t *pstype;
#endif

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
    return ENUM_USING(basest, (EV_CONST char *)vptr + (index % count) * sizeof(stype),\
      sizeof(stype), index / count);\
  } ENUM_PTRS_END_PROC\
  private RELOC_PTRS_BEGIN(preloc) {\
    uint count = size / (uint)sizeof(stype);\
    for ( ; count; count--, vptr = (char *)vptr + sizeof(stype) )\
      RELOC_USING(basest, vptr, sizeof(stype));\
  } RELOC_PTRS_END\
  gs__st_composite_only(scope_st, stname, stype, sname, penum, preloc)
#define gs_public_st_element(stname, stype, sname, penum, preloc, basest)\
  gs__st_element(public_st, stname, stype, sname, penum, preloc, basest)
#define gs_private_st_element(stname, stype, sname, penum, preloc, basest)\
  gs__st_element(private_st, stname, stype, sname, penum, preloc, basest)

	/* A "structure" just consisting of a pointer. */
	/* Note that in this case only, stype is a pointer type. */
	/* Fortunately, C's bizarre 'const' syntax does what we want here. */

#define gs__st_ptr(scope_st, stname, stype, sname, penum, preloc)\
  private ENUM_PTRS_BEGIN(penum) return 0;\
    case 0: return ENUM_OBJ(*(stype const *)vptr);\
  ENUM_PTRS_END\
  private RELOC_PTRS_BEGIN(preloc) ;\
    RELOC_VAR(*(stype *)vptr);\
  RELOC_PTRS_END\
  gs__st_composite_only(scope_st, stname, stype, sname, penum, preloc)
#define gs_public_st_ptr(stname, stype, sname, penum, preloc)\
  gs__st_ptr(public_st, stname, stype, sname, penum, preloc)
#define gs_private_st_ptr(stname, stype, sname, penum, preloc)\
  gs__st_ptr(private_st, stname, stype, sname, penum, preloc)

/* ---------- Ordinary structures with a fixed set of pointers ----------- */
/* Note that we "cannibalize" the penum and preloc names for elts and sdata. */

	/* Structures with 1 pointer. */

#define gs__st_ptrs1(scope_st, stname, stype, sname, penum, preloc, e1)\
  private const gc_ptr_element_t penum[] = {\
    GC_OBJ_ELT(stype, e1)\
  };\
  gs__st_basic(scope_st, stname, stype, sname, penum, preloc, 0, 0)
#define gs_public_st_ptrs1(stname, stype, sname, penum, preloc, e1)\
  gs__st_ptrs1(public_st, stname, stype, sname, penum, preloc, e1)
#define gs_private_st_ptrs1(stname, stype, sname, penum, preloc, e1)\
  gs__st_ptrs1(private_st, stname, stype, sname, penum, preloc, e1)

	/* Structures with 1 string. */

#define gs__st_strings1(scope_st, stname, stype, sname, penum, preloc, e1)\
  private const gc_ptr_element_t penum[] = {\
    GC_STRING_ELT(stype, e1)\
  };\
  gs__st_basic(scope_st, stname, stype, sname, penum, preloc, 0, 0)
#define gs_public_st_strings1(stname, stype, sname, penum, preloc, e1)\
  gs__st_strings1(public_st, stname, stype, sname, penum, preloc, e1)
#define gs_private_st_strings1(stname, stype, sname, penum, preloc, e1)\
  gs__st_strings1(private_st, stname, stype, sname, penum, preloc, e1)

	/* Structures with 1 const string. */

#define gs__st_const_strings1(scope_st, stname, stype, sname, penum, preloc, e1)\
  private const gc_ptr_element_t penum[] = {\
    GC_CONST_STRING_ELT(stype, e1)\
  };\
  gs__st_basic(scope_st, stname, stype, sname, penum, preloc, 0, 0)
#define gs_public_st_const_strings1(stname, stype, sname, penum, preloc, e1)\
  gs__st_const_strings1(public_st, stname, stype, sname, penum, preloc, e1)
#define gs_private_st_const_strings1(stname, stype, sname, penum, preloc, e1)\
  gs__st_const_strings1(private_st, stname, stype, sname, penum, preloc, e1)

	/* Structures with 1 pointer and 1 string. */

#define gs__st_ptrs1_strings1(scope_st, stname, stype, sname, penum, preloc, e1, e2)\
  private const gc_ptr_element_t penum[] = {\
    GC_OBJ_ELT(stype, e1), GC_STRING_ELT(stype, e2)\
  };\
  gs__st_basic(scope_st, stname, stype, sname, penum, preloc, 0, 0)
#define gs_public_st_ptrs1_strings1(stname, stype, sname, penum, preloc, e1, e2)\
  gs__st_ptrs1_strings1(public_st, stname, stype, sname, penum, preloc, e1, e2)
#define gs_private_st_ptrs1_strings1(stname, stype, sname, penum, preloc, e1, e2)\
  gs__st_ptrs1_strings1(private_st, stname, stype, sname, penum, preloc, e1, e2)

	/* Structures with 1 pointer and 2 strings. */

#define gs__st_ptrs1_strings2(scope_st, stname, stype, sname, penum, preloc, e1, e2, e3)\
  private const gc_ptr_element_t penum[] = {\
    GC_OBJ_ELT(stype, e1), GC_STRING_ELT(stype, e2), GC_STRING_ELT(stype, e3)\
  };\
  gs__st_basic(scope_st, stname, stype, sname, penum, preloc, 0, 0)
#define gs_public_st_ptrs1_strings2(stname, stype, sname, penum, preloc, e1, e2, e3)\
  gs__st_ptrs1_strings2(public_st, stname, stype, sname, penum, preloc, e1, e2, e3)
#define gs_private_st_ptrs1_strings2(stname, stype, sname, penum, preloc, e1, e2, e3)\
  gs__st_ptrs1_strings2(private_st, stname, stype, sname, penum, preloc, e1, e2, e3)

	/* Structures with 2 const strings. */

#define gs__st_const_strings2(scope_st, stname, stype, sname, penum, preloc, e1, e2)\
  private const gc_ptr_element_t penum[] = {\
    GC_CONST_STRING_ELT(stype, e1), GC_CONST_STRING_ELT(stype, e2)\
  };\
  gs__st_basic(scope_st, stname, stype, sname, penum, preloc, 0, 0)
#define gs_public_st_const_strings2(stname, stype, sname, penum, preloc, e1, e2)\
  gs__st_const_strings2(public_st, stname, stype, sname, penum, preloc, e1, e2)
#define gs_private_st_const_strings2(stname, stype, sname, penum, preloc, e1, e2)\
  gs__st_const_strings2(private_st, stname, stype, sname, penum, preloc, e1, e2)

	/* Structures with 2 pointers. */

#define gs__st_ptrs2(scope_st, stname, stype, sname, penum, preloc, e1, e2)\
  private const gc_ptr_element_t penum[] = {\
    GC_OBJ_ELT2(stype, e1, e2)\
  };\
  gs__st_basic(scope_st, stname, stype, sname, penum, preloc, 0, 0)
#define gs_public_st_ptrs2(stname, stype, sname, penum, preloc, e1, e2)\
  gs__st_ptrs2(public_st, stname, stype, sname, penum, preloc, e1, e2)
#define gs_private_st_ptrs2(stname, stype, sname, penum, preloc, e1, e2)\
  gs__st_ptrs2(private_st, stname, stype, sname, penum, preloc, e1, e2)

	/* Structures with 3 pointers. */

#define gs__st_ptrs3(scope_st, stname, stype, sname, penum, preloc, e1, e2, e3)\
  private const gc_ptr_element_t penum[] = {\
    GC_OBJ_ELT3(stype, e1, e2, e3)\
  };\
  gs__st_basic(scope_st, stname, stype, sname, penum, preloc, 0, 0)
#define gs_public_st_ptrs3(stname, stype, sname, penum, preloc, e1, e2, e3)\
  gs__st_ptrs3(public_st, stname, stype, sname, penum, preloc, e1, e2, e3)
#define gs_private_st_ptrs3(stname, stype, sname, penum, preloc, e1, e2, e3)\
  gs__st_ptrs3(private_st, stname, stype, sname, penum, preloc, e1, e2, e3)

	/* Structures with 4 pointers. */

#define gs__st_ptrs4(scope_st, stname, stype, sname, penum, preloc, e1, e2, e3, e4)\
  private const gc_ptr_element_t penum[] = {\
    GC_OBJ_ELT3(stype, e1, e2, e3), GC_OBJ_ELT(stype, e4)\
  };\
  gs__st_basic(scope_st, stname, stype, sname, penum, preloc, 0, 0)
#define gs_public_st_ptrs4(stname, stype, sname, penum, preloc, e1, e2, e3, e4)\
  gs__st_ptrs4(public_st, stname, stype, sname, penum, preloc, e1, e2, e3, e4)
#define gs_private_st_ptrs4(stname, stype, sname, penum, preloc, e1, e2, e3, e4)\
  gs__st_ptrs4(private_st, stname, stype, sname, penum, preloc, e1, e2, e3, e4)

	/* Structures with 5 pointers. */

#define gs__st_ptrs5(scope_st, stname, stype, sname, penum, preloc, e1, e2, e3, e4, e5)\
  private const gc_ptr_element_t penum[] = {\
    GC_OBJ_ELT3(stype, e1, e2, e3), GC_OBJ_ELT2(stype, e4, e5)\
  };\
  gs__st_basic(scope_st, stname, stype, sname, penum, preloc, 0, 0)
#define gs_public_st_ptrs5(stname, stype, sname, penum, preloc, e1, e2, e3, e4, e5)\
  gs__st_ptrs5(public_st, stname, stype, sname, penum, preloc, e1, e2, e3, e4, e5)
#define gs_private_st_ptrs5(stname, stype, sname, penum, preloc, e1, e2, e3, e4, e5)\
  gs__st_ptrs5(private_st, stname, stype, sname, penum, preloc, e1, e2, e3, e4, e5)

	/* Structures with 6 pointers. */

#define gs__st_ptrs6(scope_st, stname, stype, sname, penum, preloc, e1, e2, e3, e4, e5, e6)\
  private const gc_ptr_element_t penum[] = {\
    GC_OBJ_ELT3(stype, e1, e2, e3), GC_OBJ_ELT3(stype, e4, e5, e6)\
  };\
  gs__st_basic(scope_st, stname, stype, sname, penum, preloc, 0, 0)
#define gs_public_st_ptrs6(stname, stype, sname, penum, preloc, e1, e2, e3, e4, e5, e6)\
  gs__st_ptrs6(public_st, stname, stype, sname, penum, preloc, e1, e2, e3, e4, e5, e6)
#define gs_private_st_ptrs6(stname, stype, sname, penum, preloc, e1, e2, e3, e4, e5, e6)\
  gs__st_ptrs6(private_st, stname, stype, sname, penum, preloc, e1, e2, e3, e4, e5, e6)

/* ---------------- Suffix subclasses ---------------- */

	/* Suffix subclasses with no additional pointers. */

#define gs__st_suffix_add0(scope_st, stname, stype, sname, penum, preloc, supstname)\
  gs__st_basic_with_final(scope_st, stname, stype, sname, 0, 0, preloc, &supstname, 0, 0)
#define gs_public_st_suffix_add0(stname, stype, sname, penum, preloc, supstname)\
  gs__st_suffix_add0(public_st, stname, stype, sname, penum, preloc, supstname)
#define gs_private_st_suffix_add0(stname, stype, sname, penum, preloc, supstname)\
  gs__st_suffix_add0(private_st, stname, stype, sname, penum, preloc, supstname)

	/* Suffix subclasses with no additional pointers, */
	/* and with the superclass defined earlier in the same file */
	/* as a 'basic' type. */
	/* In this case, we don't even need new procedures. */

#define gs__st_suffix_add0_local(scope_st, stname, stype, sname, supenum, supreloc, supstname)\
  scope_st stname = {\
    sizeof(stype), sname, 0, 0, basic_enum_ptrs, basic_reloc_ptrs,\
    0, &supreloc\
  }
#define gs_public_st_suffix_add0_local(stname, stype, sname, supenum, supreloc, supstname)\
  gs__st_suffix_add0_local(public_st, stname, stype, sname, supenum, supreloc, supstname)
#define gs_private_st_suffix_add0_local(stname, stype, sname, supenum, supreloc, supstname)\
  gs__st_suffix_add0_local(private_st, stname, stype, sname, supenum, supreloc, supstname)

	/* Suffix subclasses with no additional pointers and finalization. */
	/* This is a hack -- subclasses should inherit finalization, */
	/* but that would require a superclass pointer in the descriptor, */
	/* which would perturb things too much right now. */

#define gs__st_suffix_add0_final(scope_st, stname, stype, sname, penum, preloc, pfinal, supstname)\
  private ENUM_PTRS_BEGIN_PROC(penum) {\
    ENUM_PREFIX(supstname, 0);\
  } ENUM_PTRS_END_PROC\
  private RELOC_PTRS_BEGIN(preloc) {\
    RELOC_PREFIX(supstname);\
  } RELOC_PTRS_END\
  gs__st_complex_only(scope_st, stname, stype, sname, 0, penum, preloc, pfinal)
#define gs_public_st_suffix_add0_final(stname, stype, sname, penum, preloc, pfinal, supstname)\
  gs__st_suffix_add0_final(public_st, stname, stype, sname, penum, preloc, pfinal, supstname)
#define gs_private_st_suffix_add0_final(stname, stype, sname, penum, preloc, pfinal, supstname)\
  gs__st_suffix_add0_final(private_st, stname, stype, sname, penum, preloc, pfinal, supstname)

	/* Suffix subclasses with 1 additional pointer. */

#define gs__st_suffix_add1(scope_st, stname, stype, sname, penum, preloc, supstname, e1)\
  private const gc_ptr_element_t penum[] = {\
    GC_OBJ_ELT(stype, e1)\
  };\
  gs__st_basic(scope_st, stname, stype, sname, penum, preloc, &supstname, 0)
#define gs_public_st_suffix_add1(stname, stype, sname, penum, preloc, supstname, e1)\
  gs__st_suffix_add1(public_st, stname, stype, sname, penum, preloc, supstname, e1)
#define gs_private_st_suffix_add1(stname, stype, sname, penum, preloc, supstname, e1)\
  gs__st_suffix_add1(private_st, stname, stype, sname, penum, preloc, supstname, e1)

	/* Suffix subclasses with 1 additional pointer and finalization. */
	/* See above regarding finalization and subclasses. */

#define gs__st_suffix_add1_final(scope_st, stname, stype, sname, penum, preloc, pfinal, supstname, e1)\
  private const gc_ptr_element_t penum[] = {\
    GC_OBJ_ELT(stype, e1)\
  };\
  gs__st_basic_final(scope_st, stname, stype, sname, penum, preloc, &supstname, 0, pfinal)
#define gs_public_st_suffix_add1_final(stname, stype, sname, penum, preloc, pfinal, supstname, e1)\
  gs__st_suffix_add1_final(public_st, stname, stype, sname, penum, preloc, pfinal, supstname, e1)
#define gs_private_st_suffix_add1_final(stname, stype, sname, penum, preloc, pfinal, supstname, e1)\
  gs__st_suffix_add1_final(private_st, stname, stype, sname, penum, preloc, pfinal, supstname, e1)

	/* Suffix subclasses with 2 additional pointers. */

#define gs__st_suffix_add2(scope_st, stname, stype, sname, penum, preloc, supstname, e1, e2)\
  private const gc_ptr_element_t penum[] = {\
    GC_OBJ_ELT2(stype, e1, e2)\
  };\
  gs__st_basic(scope_st, stname, stype, sname, penum, preloc, &supstname, 0)
#define gs_public_st_suffix_add2(stname, stype, sname, penum, preloc, supstname, e1, e2)\
  gs__st_suffix_add2(public_st, stname, stype, sname, penum, preloc, supstname, e1, e2)
#define gs_private_st_suffix_add2(stname, stype, sname, penum, preloc, supstname, e1, e2)\
  gs__st_suffix_add2(private_st, stname, stype, sname, penum, preloc, supstname, e1, e2)

	/* Suffix subclasses with 2 additional pointers and finalization. */
	/* See above regarding finalization and subclasses. */

#define gs__st_suffix_add2_final(scope_st, stname, stype, sname, penum, preloc, pfinal, supstname, e1, e2)\
  private const gc_ptr_element_t penum[] = {\
    GC_OBJ_ELT2(stype, e1, e2)\
  };\
  gs__st_basic_final(scope_st, stname, stype, sname, penum, preloc, &supstname, 0, pfinal)
#define gs_public_st_suffix_add2_final(stname, stype, sname, penum, preloc, pfinal, supstname, e1, e2)\
  gs__st_suffix_add2_final(public_st, stname, stype, sname, penum, preloc, pfinal, supstname, e1, e2)
#define gs_private_st_suffix_add2_final(stname, stype, sname, penum, preloc, pfinal, supstname, e1, e2)\
  gs__st_suffix_add2_final(private_st, stname, stype, sname, penum, preloc, pfinal, supstname, e1, e2)

	/* Suffix subclasses with 3 additional pointers. */

#define gs__st_suffix_add3(scope_st, stname, stype, sname, penum, preloc, supstname, e1, e2, e3)\
  private const gc_ptr_element_t penum[] = {\
    GC_OBJ_ELT3(stype, e1, e2, e3)\
  };\
  gs__st_basic(scope_st, stname, stype, sname, penum, preloc, &supstname, 0)
#define gs_public_st_suffix_add3(stname, stype, sname, penum, preloc, supstname, e1, e2, e3)\
  gs__st_suffix_add3(public_st, stname, stype, sname, penum, preloc, supstname, e1, e2, e3)
#define gs_private_st_suffix_add3(stname, stype, sname, penum, preloc, supstname, e1, e2, e3)\
  gs__st_suffix_add3(private_st, stname, stype, sname, penum, preloc, supstname, e1, e2, e3)

	/* Suffix subclasses with 3 additional pointers and finalization. */
	/* See above regarding finalization and subclasses. */

#define gs__st_suffix_add3_final(scope_st, stname, stype, sname, penum, preloc, pfinal, supstname, e1, e2, e3)\
  private const gc_ptr_element_t penum[] = {\
    GC_OBJ_ELT3(stype, e1, e2, e3)\
  };\
  gs__st_basic_final(scope_st, stname, stype, sname, penum, preloc, &supstname, 0, pfinal)
#define gs_public_st_suffix_add3_final(stname, stype, sname, penum, preloc, pfinal, supstname, e1, e2, e3)\
  gs__st_suffix_add3_final(public_st, stname, stype, sname, penum, preloc, pfinal, supstname, e1, e2, e3)
#define gs_private_st_suffix_add3_final(stname, stype, sname, penum, preloc, pfinal, supstname, e1, e2, e3)\
  gs__st_suffix_add3_final(private_st, stname, stype, sname, penum, preloc, pfinal, supstname, e1, e2, e3)

	/* Suffix subclasses with 4 additional pointers. */

#define gs__st_suffix_add4(scope_st, stname, stype, sname, penum, preloc, supstname, e1, e2, e3, e4)\
  private const gc_ptr_element_t penum[] = {\
    GC_OBJ_ELT3(stype, e1, e2, e3), GC_OBJ_ELT(stype, e4)\
  };\
  gs__st_basic(scope_st, stname, stype, sname, penum, preloc, &supstname, 0)
#define gs_public_st_suffix_add4(stname, stype, sname, penum, preloc, supstname, e1, e2, e3, e4)\
  gs__st_suffix_add4(public_st, stname, stype, sname, penum, preloc, supstname, e1, e2, e3, e4)
#define gs_private_st_suffix_add4(stname, stype, sname, penum, preloc, supstname, e1, e2, e3, e4)\
  gs__st_suffix_add4(private_st, stname, stype, sname, penum, preloc, supstname, e1, e2, e3, e4)

/* ---------------- General subclasses ---------------- */

	/* General subclasses with no additional pointers. */

#define gs__st_ptrs_add0(scope_st, stname, stype, sname, penum, preloc, supstname, member)\
  gs__st_basic_with_final(scope_st, stname, stype, sname, 0, 0, preloc, &supstname, offset_of(stype, member), 0)
#define gs_public_st_ptrs_add0(stname, stype, sname, penum, preloc, supstname, member)\
  gs__st_ptrs_add0(public_st, stname, stype, sname, penum, preloc, supstname, member)
#define gs_private_st_ptrs_add0(stname, stype, sname, penum, preloc, supstname, member)\
  gs__st_ptrs_add0(private_st, stname, stype, sname, penum, preloc, supstname, member)

	/* General subclasses with 1 additional pointer. */

#define gs__st_ptrs_add1(scope_st, stname, stype, sname, penum, preloc, supstname, member, e1)\
  private const gc_ptr_element_t penum[] = {\
    GC_OBJ_ELT(stype, e1)\
  };\
  gs__st_basic(scope_st, stname, stype, sname, penum, preloc, &supstname, offset_of(stype, member))
#define gs_public_st_ptrs_add1(stname, stype, sname, penum, preloc, supstname, member, e1)\
  gs__st_ptrs_add1(public_st, stname, stype, sname, penum, preloc, supstname, member, e1)
#define gs_private_st_ptrs_add1(stname, stype, sname, penum, preloc, supstname, member, e1)\
  gs__st_ptrs_add1(private_st, stname, stype, sname, penum, preloc, supstname, member, e1)

	/* General subclasses with 2 additional pointers. */

#define gs__st_ptrs_add2(scope_st, stname, stype, sname, penum, preloc, supstname, member, e1, e2)\
  private const gc_ptr_element_t penum[] = {\
    GC_OBJ_ELT2(stype, e1, e2)\
  };\
  gs__st_basic(scope_st, stname, stype, sname, penum, preloc, &supstname, offset_of(stype, member))
#define gs_public_st_ptrs_add2(stname, stype, sname, penum, preloc, supstname, member, e1, e2)\
  gs__st_ptrs_add2(public_st, stname, stype, sname, penum, preloc, supstname, member, e1, e2)
#define gs_private_st_ptrs_add2(stname, stype, sname, penum, preloc, supstname, member, e1, e2)\
  gs__st_ptrs_add2(private_st, stname, stype, sname, penum, preloc, supstname, member, e1, e2)

#endif /* gsstruct_INCLUDED */
