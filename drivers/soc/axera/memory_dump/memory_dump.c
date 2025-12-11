#include <asm/cacheflush.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/mm.h>
#include <linux/kcore.h>
#include <linux/user.h>
#include <linux/platform_device.h>
#include <linux/elfcore.h>
#include <linux/export.h>
#include <linux/printk.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/elf.h>
#include <linux/elfcore.h>
#include <linux/uaccess.h>
#include <linux/kallsyms.h>
#include <asm/stacktrace.h>
#include <asm-generic/kdebug.h>
#include <linux/kdebug.h>
#include <linux/of_fdt.h>
#include <linux/elfcore.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <asm/fixmap.h>
#include <asm/memory.h>
#include <asm/pgtable.h>
#include <linux/list.h>
#include <linux/crash_core.h>
#include <asm/page.h>
#include <asm/sections.h>
#include <linux/reboot.h>
#include <linux/timer.h>
#include <linux/timex.h>
#include <linux/rtc.h>
#include <linux/libfdt.h>
#include <asm/neon.h>


#define TIME_ADDR 1088
#define MMU_ADDR  2048
#define MAGIC_ADDR  1024
#define SYS_MEM_SIZE 1048
#define DUMP_OFFSET  0x3000

void *axera_dump_vaddr;
struct arm_v8_mmu_regs {
	u64 sctlr_el1;
	u64 ttbr0_el1;
	u64 ttbr1_el1;
	u64 tcr_el1;
	u64 mair_el1;
	u64 amair_el1;
	u64 contextidr_el1;
	u64 reg_null;
};

struct axera_memory_info {
	unsigned long info_paddr;
	unsigned long info_size;
	unsigned long info_vaddr;
	unsigned long paddr;
	unsigned long vaddr;
	unsigned long size;
};

static u32 all_mem_size;
struct arm_v8_mmu_regs *ax650_mmu_regs;
struct axera_memory_info dump_info;

static int __init mem_setup(char *str)
{
	get_option(&str, &all_mem_size);
	printk("mem_setup = %s all_mem_size = %d\n", str, all_mem_size);
	return 1;
}

__setup("mem=", mem_setup);

static void axera_save_mmu_regs(struct arm_v8_mmu_regs *mmu_regs)
{

#ifdef CONFIG_ARM64
	u64 tmp = 0;
	asm volatile ("mrs %1, sctlr_el1\n\t"
		      "str %1, [%0]\n\t"
		      "mrs %1, ttbr0_el1\n\t"
		      "str %1, [%0, #8]\n\t"
		      "mrs %1, ttbr1_el1\n\t"
		      "str %1, [%0, #0x10]\n\t"
		      "mrs %1, tcr_el1\n\t"
		      "str %1, [%0, #0x18]\n\t"
		      "mrs %1, mair_el1\n\t"
		      "str %1, [%0, #0x20]\n\t"
		      "mrs %1, amair_el1\n\t"
		      "str %1, [%0, #0x28]\n\t"
		      "mrs %1, contextidr_el1\n\t" "str %1, [%0, #0x30]\n\t"::"r" (mmu_regs),
		      "r"(tmp):"%0", "%1");
#else
	asm volatile ("mrc    p15, 0, r1, c1, c0, 0\n\t"  /* SCTLR */
		"str r1, [%0]\n\t" "mrc    p15, 0, r1, c2, c0, 0\n\t"       /* TTBR0 */
		"str r1, [%0,#4]\n\t" "mrc    p15, 0, r1, c2, c0,1\n\t"     /* TTBR1 */
		"str r1, [%0,#8]\n\t" "mrc    p15, 0, r1, c2, c0,2\n\t"     /* TTBCR */
		"str r1, [%0,#12]\n\t" "mrc    p15, 0, r1, c3, c0,0\n\t"    /* DACR */
		"str r1, [%0,#16]\n\t" "mrc    p15, 0, r1, c5, c0,0\n\t"    /* DFSR */
		"str r1, [%0,#20]\n\t" "mrc    p15, 0, r1, c6, c0,0\n\t"    /* DFAR */
		"str r1, [%0,#24]\n\t" "mrc    p15, 0, r1, c5, c0,1\n\t"    /* IFSR */
		"str r1, [%0,#28]\n\t" "mrc    p15, 0, r1, c6, c0,2\n\t"    /* IFAR */
		"str r1, [%0,#32]\n\t":
		:           "r"(mmu_regs)
		:           "%r1", "memory");
#endif
}

void axera_final_note(Elf_Word *buf)
{
	memset(buf, 0xff, sizeof(struct elf_note));
}


Elf_Word *axera_append_elf_note(Elf_Word *buf, char *name, unsigned int type,void *data, size_t data_len)
{
	struct elf_note *note = (struct elf_note *)buf;

	note->n_namesz = strlen(name) + 1;
	note->n_descsz = data_len;
	note->n_type   = type;
	buf += DIV_ROUND_UP(sizeof(*note), sizeof(Elf_Word));
	memcpy(buf, name, note->n_namesz);
	buf += DIV_ROUND_UP(note->n_namesz, sizeof(Elf_Word));
	memcpy(buf, data, data_len);
	buf += DIV_ROUND_UP(data_len, sizeof(Elf_Word));
	return buf;
}

inline void axera_crash_save_cpu(struct pt_regs *regs, int cpu,void *addr)
{
	struct elf_prstatus prstatus;
	u32 *buf;

	if ((cpu < 0) || (cpu >= nr_cpu_ids))
		return;
	if(addr == NULL)
		return ;

	buf = (u32 *)(addr + cpu * (sizeof(note_buf_t) - sizeof(struct elf_note)));
	if (!buf)
		return;
	memset(&prstatus, 0, sizeof(prstatus));
#ifdef CONFIG_ARM64
	//prstatus.common.pr_pid = current->pid;
#endif
	if(regs != NULL)
		elf_core_copy_kernel_regs(&prstatus.pr_reg, regs);
	axera_append_elf_note(buf, "CORE", NT_PRSTATUS,&prstatus, sizeof(prstatus));
	if (ax650_mmu_regs != NULL)
		axera_save_mmu_regs(ax650_mmu_regs + cpu);
}

EXPORT_SYMBOL(axera_crash_save_cpu);

void axera_prepare_elf32_headers(void *elfdr)
{
	Elf32_Phdr *phdr;
	Elf32_Ehdr *ehdr;

	ehdr = (Elf32_Ehdr *)elfdr;
	ehdr->e_ident[EI_MAG0] = ELFMAG0,
	ehdr->e_ident[EI_MAG1] = ELFMAG1,
	ehdr->e_ident[EI_MAG2] = ELFMAG2,
	ehdr->e_ident[EI_MAG3] = ELFMAG3,
	ehdr->e_ident[EI_CLASS] = ELF_CLASS,
	ehdr->e_ident[EI_DATA] = ELF_DATA,
	ehdr->e_ident[EI_VERSION] = EV_CURRENT,
	ehdr->e_ident[EI_OSABI] = ELF_OSABI,
	ehdr->e_ident[EI_CLASS] = ELFCLASS32;
	ehdr->e_type = ET_CORE;
	ehdr->e_machine = ELF_ARCH;
	ehdr->e_version = EV_CURRENT;
	ehdr->e_entry = 0;
	ehdr->e_phoff = sizeof(Elf32_Ehdr);
	ehdr->e_shoff = 0;
	ehdr->e_flags = 0;
	ehdr->e_ehsize = sizeof(Elf32_Ehdr);
	ehdr->e_phentsize = sizeof(Elf32_Phdr);
	ehdr->e_phnum = 2;
	ehdr->e_shentsize = 0;
	ehdr->e_shnum = 0;
	ehdr->e_shstrndx = 0;
	phdr = elfdr + sizeof(Elf32_Ehdr);
	phdr->p_type = PT_NOTE;
	phdr->p_offset = 4096;
	phdr->p_vaddr = 0;
	phdr->p_paddr = 0;
	phdr->p_filesz = sizeof(note_buf_t)*NR_CPUS + VMCOREINFO_NOTE_SIZE;
	phdr->p_memsz = sizeof(note_buf_t)*NR_CPUS + VMCOREINFO_NOTE_SIZE;
	phdr->p_flags = 0;
	phdr->p_align = 0;
	phdr ++;
	phdr->p_type = PT_LOAD;
	phdr->p_flags = PF_R | PF_W | PF_X;
	phdr->p_offset = DUMP_OFFSET;
	phdr->p_vaddr = dump_info.vaddr;
	phdr->p_paddr = dump_info.paddr;
	phdr->p_filesz = dump_info.size;
	phdr->p_memsz = dump_info.size;
	phdr->p_align = 0;
}

void axera_prepare_elf64_headers(void *elfdr)
{
	Elf64_Phdr *phdr;
	Elf64_Ehdr *ehdr;

	ehdr = (Elf64_Ehdr *)elfdr;
	ehdr->e_ident[EI_MAG0] = ELFMAG0,
	ehdr->e_ident[EI_MAG1] = ELFMAG1,
	ehdr->e_ident[EI_MAG2] = ELFMAG2,
	ehdr->e_ident[EI_MAG3] = ELFMAG3,
	ehdr->e_ident[EI_CLASS] = ELF_CLASS,
	ehdr->e_ident[EI_DATA] = ELF_DATA,
	ehdr->e_ident[EI_VERSION] = EV_CURRENT,
	ehdr->e_ident[EI_OSABI] = ELF_OSABI,
	ehdr->e_ident[EI_CLASS] = ELFCLASS64;
	ehdr->e_type = ET_CORE;
	ehdr->e_machine = ELF_ARCH;
	ehdr->e_version = EV_CURRENT;
	ehdr->e_entry = 0;
	ehdr->e_phoff = sizeof(Elf64_Ehdr);
	ehdr->e_shoff = 0;
	ehdr->e_flags = 0;
	ehdr->e_ehsize = sizeof(Elf64_Ehdr);
	ehdr->e_phentsize = sizeof(Elf64_Phdr);
	ehdr->e_phnum = 2;
	ehdr->e_shentsize = 0;
	ehdr->e_shnum = 0;
	ehdr->e_shstrndx = 0;
	phdr = elfdr + sizeof(Elf64_Ehdr);
	phdr->p_type = PT_NOTE;
	phdr->p_offset = 4096;
	phdr->p_vaddr = 0;
	phdr->p_paddr = 0;
	phdr->p_filesz = sizeof(note_buf_t)*NR_CPUS + VMCOREINFO_NOTE_SIZE;
	phdr->p_memsz = sizeof(note_buf_t)*NR_CPUS + VMCOREINFO_NOTE_SIZE;
	phdr->p_flags = 0;
	phdr->p_align = 0;
	phdr ++;
	phdr->p_type = PT_LOAD;
	phdr->p_flags = PF_R | PF_W | PF_X;
	phdr->p_offset = DUMP_OFFSET;
	phdr->p_vaddr = dump_info.vaddr;
	phdr->p_paddr = dump_info.paddr;
	phdr->p_filesz = dump_info.size;
	phdr->p_memsz = dump_info.size;
	phdr->p_align = 0;
}

static inline void axera_crash_setup_regs(struct pt_regs *newregs,
                                    struct pt_regs *oldregs)
{
        if (oldregs) {
                memcpy(newregs, oldregs, sizeof(*newregs));
        } else {
#ifdef CONFIG_ARM64
                u64 tmp1, tmp2;

                __asm__ __volatile__ (
                        "stp     x0,   x1, [%2, #16 *  0]\n"
                        "stp     x2,   x3, [%2, #16 *  1]\n"
                        "stp     x4,   x5, [%2, #16 *  2]\n"
                        "stp     x6,   x7, [%2, #16 *  3]\n"
                        "stp     x8,   x9, [%2, #16 *  4]\n"
                        "stp    x10,  x11, [%2, #16 *  5]\n"
                        "stp    x12,  x13, [%2, #16 *  6]\n"
                        "stp    x14,  x15, [%2, #16 *  7]\n"
                        "stp    x16,  x17, [%2, #16 *  8]\n"
                        "stp    x18,  x19, [%2, #16 *  9]\n"
                        "stp    x20,  x21, [%2, #16 * 10]\n"
                        "stp    x22,  x23, [%2, #16 * 11]\n"
                        "stp    x24,  x25, [%2, #16 * 12]\n"
                        "stp    x26,  x27, [%2, #16 * 13]\n"
                        "stp    x28,  x29, [%2, #16 * 14]\n"
                        "mov     %0,  sp\n"
                        "stp    x30,  %0,  [%2, #16 * 15]\n"

                        "/* faked current PSTATE */\n"
                        "mrs     %0, CurrentEL\n"
                        "mrs     %1, SPSEL\n"
                        "orr     %0, %0, %1\n"
                        "mrs     %1, DAIF\n"
                        "orr     %0, %0, %1\n"
                        "mrs     %1, NZCV\n"
                        "orr     %0, %0, %1\n"
                        /* pc */
                        "adr     %1, 1f\n"
                "1:\n"
                        "stp     %1, %0,   [%2, #16 * 16]\n"
                        : "=&r" (tmp1), "=&r" (tmp2)
                        : "r" (newregs)
                        : "memory"
                );
#else
		__asm__ __volatile__("stmia  %[regs_base], {r0-r12}\n\t"
			"mov    %[_ARM_sp], sp\n\t"
			"str    lr, %[_ARM_lr]\n\t"
			"adr    %[_ARM_pc], 1f\n\t"
			"mrs    %[_ARM_cpsr], cpsr\n\t"
			"1:":[_ARM_pc] "=r"(newregs->ARM_pc),
			     [_ARM_cpsr] "=r"(newregs->ARM_cpsr),
			     [_ARM_sp] "=r"(newregs->ARM_sp),[_ARM_lr] "=o"(newregs->ARM_lr)
			    :[regs_base] "r"(&newregs->ARM_r0)
			    :"memory");
#endif
        }
}

void axera_save_memory_dump(void)
{
	struct timespec64 txc;
	struct rtc_time tm;
	u32 reason_mask = 0xf98e7c6d;
	u32 *buf;
	int cpu;
	struct pt_regs newregs;

	axera_crash_setup_regs(&newregs,NULL);

#ifdef CONFIG_ARM64
	axera_prepare_elf64_headers((void *)dump_info.info_vaddr);
#else
	axera_prepare_elf32_headers((void *)dump_info.info_vaddr);
#endif
	bust_spinlocks(1);
	ktime_get_real_ts64(&txc);
	txc.tv_sec -= sys_tz.tz_minuteswest * 60;
	rtc_time64_to_tm(txc.tv_sec, &tm);
	tm.tm_mon += 1;
	printk("kernel panic dump : %d-%d-%d %d:%d:%d\n", tm.tm_year + 1900,
	       tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	memcpy((void *)(dump_info.info_vaddr + TIME_ADDR), &tm, sizeof(tm));
	memcpy((void *)(dump_info.info_vaddr + MAGIC_ADDR), &reason_mask, sizeof(reason_mask));
	memcpy((void *)(dump_info.info_vaddr + SYS_MEM_SIZE), &all_mem_size, sizeof(all_mem_size));
	ax650_mmu_regs = (struct arm_v8_mmu_regs *)(dump_info.info_vaddr + MMU_ADDR);
	buf = (u32 *)((dump_info.info_vaddr +0x1000) + 8 * (sizeof(note_buf_t) - sizeof(struct elf_note)));
	if (!buf)
		return;
	memcpy(buf,vmcoreinfo_note,VMCOREINFO_NOTE_SIZE);
	for_each_present_cpu(cpu) {
		axera_crash_save_cpu(NULL, cpu,axera_dump_vaddr);
	}
	axera_crash_save_cpu(&newregs,smp_processor_id(),axera_dump_vaddr);

	mdelay(100);
#ifdef CONFIG_ARM64
	crash_smp_send_stop();
#else
	system_state = SYSTEM_RESTART;
	smp_send_stop();
#endif
	mdelay(2000);
#ifdef CONFIG_ARM64
	axera_armv8_flush_cache_all();
#else
	flush_cache_all();
#endif
	mdelay(1000);
	bust_spinlocks(0);
}

static int __init axera_memory_dump_init(void)
{
	int offset;
	int len;
	unsigned long addr, size, memory_addr;
	const u32 *val;
	unsigned long dump_info_vaddr;

	void *fdt = initial_boot_params;

	struct device_node *of_node = of_find_compatible_node(NULL, NULL, "axera_memory_dump");
	if (!of_node) {
		pr_err("%s:can't find memory_dump tree node\n", __func__);
		return -ENODEV;
	}

	if (fdt_check_header(fdt)) {
		pr_err("Invalid device tree header \n");
		return -ENODEV;
	}
	offset = fdt_path_offset(fdt, "/reserved-memory/axera_memory_dump@0");
	if (offset < 0) {
		pr_err("dtb get reserved_mem info error \n");
		return -ENODEV;
	}

	val = fdt_getprop(fdt, offset, "reg", &len);

#ifdef CONFIG_ARM64
	addr = fdt32_to_cpu(val[0]);
	addr = addr << 32;
	addr |= fdt32_to_cpu(val[1]);
	size = fdt32_to_cpu(val[2]);
	size = size << 32;
	size |= fdt32_to_cpu(val[3]);
#else
	addr = fdt32_to_cpu(val[1]);
	size = fdt32_to_cpu(val[3]);
#endif
	dump_info_vaddr = (unsigned long)ioremap_wc(addr,size);
	if (dump_info_vaddr == 0x0)
		return -EIO;
	memset((void *)dump_info_vaddr, 0, size);
	dump_info.info_paddr = addr;
	dump_info.info_size = size;
	dump_info.info_vaddr = (unsigned long)dump_info_vaddr;
	axera_dump_vaddr = (void *)(dump_info_vaddr + 0x1000);
	offset = fdt_path_offset(fdt, "/memory@40000000");
	if (offset < 0)
		pr_err("memory node error \n");
	val = fdt_getprop(fdt, offset, "reg", &len);

#ifdef CONFIG_ARM64
	memory_addr = fdt32_to_cpu(val[0]);
	memory_addr = memory_addr << 32;
	memory_addr |= fdt32_to_cpu(val[1]);
#else
	memory_addr = fdt32_to_cpu(val[1]);
#endif
	dump_info.paddr = memory_addr;
	dump_info.size = all_mem_size * SZ_1M;
	dump_info.vaddr = (unsigned long)phys_to_virt(memory_addr);
	return 0;
}

module_init(axera_memory_dump_init);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("xuxiyang@axera-tech.com");
MODULE_VERSION("1.0");
