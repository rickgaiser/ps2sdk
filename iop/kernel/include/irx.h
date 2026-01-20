/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright (c) 2003 Marcus R. Brown <mrbrown@0xd6.org>
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
*/

/**
 * @file
 * Convienence macros for IRX modules.
 */

#ifndef __IRX_H__
#define __IRX_H__

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IRX_VER(major, minor)	((((major) & 0xff) << 8) + ((minor) & 0xff))

struct irx_id {
	const char *n;
	u16 v;
};

#define IRX_ID(name, major, minor) \
struct irx_id _irx_id = { \
	name, IRX_VER(major, minor) \
};

/*
 * Module imports
 */
#define IMPORT_MAGIC	0x41e00000

struct irx_import_table
{
	u32	magic;
	struct irx_import_table *next;
	u16	version;
	u16	mode;
	char	name[8];
	void	*stubs[];
} __attribute ((packed));

struct irx_import_stub
{
    u32 jump;
    u16 fno;
    u16 ori_zero;
} __attribute ((packed));

/*
 * Ugly, yet functional.
 */
#ifdef __clang__
/*
 * LLVM/Clang version: Generate entire import table header using inline assembly.
 * This ensures the header is emitted immediately before the stubs, preserving
 * the required layout for IRX format (LLVM lacks -fno-toplevel-reorder).
 * Structure: magic(4) + next(4) + version(2) + mode(2) + name(8) = 20 bytes
 */
#define DECLARE_IMPORT_TABLE(modname, major, minor) \
    __asm__ ( \
        ".section .text\n\t" \
        ".local _imp_" #modname "\n\t" \
        "_imp_" #modname ":\n\t" \
        ".word 0x41e00000\n\t" \
        ".word 0\n\t" \
        ".hword ((" #major " << 8) | " #minor ")\n\t" \
        ".hword 0\n\t" \
        "99:\n\t" \
        ".ascii \"" #modname "\"\n\t" \
        ".space 8 - (. - 99b)\n\t" \
    );
#else
/* GCC version: uses section name hack for compatibility */
#define DECLARE_IMPORT_TABLE(modname, major, minor)	\
static struct irx_import_table _imp_##modname 		\
	__attribute__((section(".text\n\t#"), unused))= {	\
	magic: IMPORT_MAGIC, version: IRX_VER(major, minor),	\
	name: #modname, };
#endif

#define STR(val) #val
// .word 0x03e00008 == jr $ra (return immediately), this value will be patched later
#define DECLARE_IMPORT(ord, name) \
	__asm__ (".section\t.text\n\t"		\
		".globl\t"#name"\n\t"#name":\n\t"	\
		".word 0x03e00008\n\t"			\
		".word " STR(0x24000000|ord));

#define END_IMPORT_TABLE \
	__asm__ (".section\t.text\n\t.word\t0, 0");

/*
 * Module exports
 *
 * I'm not sure why GCC won't generate the fptrs initializers, so I fell back
 * to the above approach to generate the fptrs themselves.  Cleaner solutions
 * requested.
 */
#define EXPORT_MAGIC	0x41c00000

void __attribute__((unused)) _retonly();

struct irx_export_table {
	u32	magic;
	struct irx_export_table *next;
	u16	version;
	u16	mode;
	u8	name[8];
	void	*fptrs[];
};

#ifdef __clang__
/*
 * LLVM/Clang version: Generate entire export table header using inline assembly.
 * Export symbol must be global for RegisterLibraryEntries().
 * Structure: magic(4) + next(4) + version(2) + mode(2) + name(8) = 20 bytes
 */
#define DECLARE_EXPORT_TABLE(modname, major, minor) \
    __asm__ ( \
        ".section .text\n\t" \
        ".globl _exp_" #modname "\n\t" \
        "_exp_" #modname ":\n\t" \
        ".word 0x41c00000\n\t" \
        ".word 0\n\t" \
        ".hword ((" #major " << 8) | " #minor ")\n\t" \
        ".hword 0\n\t" \
        "99:\n\t" \
        ".ascii \"" #modname "\"\n\t" \
        ".space 8 - (. - 99b)\n\t" \
    );
#else
/* GCC version: uses section name hack for compatibility */
#define DECLARE_EXPORT_TABLE(modname, major, minor)	\
struct irx_export_table _exp_##modname			\
	__attribute__((section(".text\n\t#"), unused)) = {	\
	magic: EXPORT_MAGIC, version: IRX_VER(major, minor),	\
	name: #modname, };
#endif

#define DECLARE_EXPORT(fptr) \
	__asm__ (".section\t.text\n\t.word\t" STR(fptr));

#define END_EXPORT_TABLE  __asm__ (".section\t.text\n\t.word\t0");

#ifdef __cplusplus
}
#endif

#endif /* __IRX_H__ */
