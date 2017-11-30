#include "idt.h"
#include "assert.h"
#include "isr.h"
#include "irq.h"
#include "gdt.h"
#include "cpuid.h"
#include "conio.h"
#include "printk.h"
#include "cpu/halt.h"
#include "time.h"
#include "interrupts.h"
#include "control_regs.h"
#include "string.h"
#include "except.h"
#include "mm.h"
#include "asm_constants.h"

#include "apic.h"

// Enforce that we use the correct value in syscall.S
C_ASSERT(SYSCALL_RFLAGS == (EFLAGS_IF | 2));

// Enforce that we don't try to set any flags bits that
// will be cleared anyway on sysret
C_ASSERT((SYSCALL_RFLAGS & ~uintptr_t(0x3C7FD7)) == 0);

static idt_entry_64_t idt[256];

static irq_dispatcher_handler_t irq_dispatcher_vec;
static idt_unhandled_exception_handler_t unhandled_exception_handler_vec;

uint32_t xsave_supported_states;
uint32_t xsave_enabled_states;

static format_flag_info_t const cpu_eflags_info[] = {
    { "ID",   EFLAGS_ID_BIT,   1, 0 },
    { "VIP",  EFLAGS_VIP_BIT,  1, 0 },
    { "VIF",  EFLAGS_VIF_BIT,  1, 0 },
    { "AC",   EFLAGS_AC_BIT,   1, 0 },
    { "VM",   EFLAGS_VM_BIT,   1, 0 },
    { "RF",   EFLAGS_RF_BIT,   1, 0 },
    { "NT",   EFLAGS_NT_BIT,   1, 0 },
    { "IOPL", EFLAGS_IOPL_BIT, EFLAGS_IOPL_MASK, 0 },
    { "OF",   EFLAGS_OF_BIT,   1, 0 },
    { "DF",   EFLAGS_DF_BIT,   1, 0 },
    { "IF",   EFLAGS_IF_BIT,   1, 0 },
    { "TF",   EFLAGS_TF_BIT,   1, 0 },
    { "SF",   EFLAGS_SF_BIT,   1, 0 },
    { "ZF",   EFLAGS_ZF_BIT,   1, 0 },
    { "AF",   EFLAGS_AF_BIT,   1, 0 },
    { "PF",   EFLAGS_PF_BIT,   1, 0 },
    { "CF",   EFLAGS_CF_BIT,   1, 0 },
    { 0,      -1,              0, 0 }
};

static char const *cpu_mxcsr_rc[] = {
    "Nearest",
    "Down",
    "Up",
    "Truncate"
};

static format_flag_info_t const cpu_mxcsr_info[] = {
    { "IE",     MXCSR_IE_BIT, 1, 0 },
    { "DE",     MXCSR_DE_BIT, 1, 0 },
    { "ZE",     MXCSR_ZE_BIT, 1, 0 },
    { "OE",     MXCSR_OE_BIT, 1, 0 },
    { "UE",     MXCSR_UE_BIT, 1, 0 },
    { "PE",     MXCSR_PE_BIT, 1, 0 },
    { "DAZ",    MXCSR_DAZ_BIT, 1, 0 },
    { "IM",     MXCSR_IM_BIT, 1, 0 },
    { "DM",     MXCSR_DM_BIT, 1, 0 },
    { "ZM",     MXCSR_ZM_BIT, 1, 0 },
    { "OM",     MXCSR_OM_BIT, 1, 0 },
    { "UM",     MXCSR_UM_BIT, 1, 0 },
    { "PM",     MXCSR_PM_BIT, 1, 0 },
    { "RC",     MXCSR_RC_BIT, MXCSR_RC_BITS, cpu_mxcsr_rc },
    { "FZ",     MXCSR_FZ_BIT, 1, 0 },
    { 0,        -1,           0, 0 }
};

static char const *cpu_fpucw_pc[] = {
    "24-bit",
    "??",
    "53-bit",
    "64-bit"
};

static format_flag_info_t const cpu_fpucw_info[] = {
    { "IM",     FPUCW_IM_BIT, 1, 0 },
    { "DM",     FPUCW_DM_BIT, 1, 0 },
    { "ZM",     FPUCW_ZM_BIT, 1, 0 },
    { "OM",     FPUCW_OM_BIT, 1, 0 },
    { "UM",     FPUCW_UM_BIT, 1, 0 },
    { "PM",     FPUCW_PM_BIT, 1, 0 },
    { "PC",     FPUCW_PC_BIT, FPUCW_PC_BITS, cpu_fpucw_pc },
    { "RC",     FPUCW_RC_BIT, FPUCW_RC_BITS, cpu_mxcsr_rc },
    { 0,        -1,           0, 0 }
};

static format_flag_info_t const cpu_fpusw_info[] = {
    { "IE",     FPUSW_IE_BIT, 1, 0 },
    { "DE",     FPUSW_DE_BIT, 1, 0 },
    { "ZE",     FPUSW_ZE_BIT, 1, 0 },
    { "OE",     FPUSW_OE_BIT, 1, 0 },
    { "UE",     FPUSW_UE_BIT, 1, 0 },
    { "PE",     FPUSW_PE_BIT, 1, 0 },
    { "SF",     FPUSW_SF_BIT, 1, 0 },
    { "ES",     FPUSW_ES_BIT, 1, 0 },
    { "C0(c)",  FPUSW_C0_BIT, 1, 0 },
    { "C1",     FPUSW_C1_BIT, 1, 0 },
    { "C2(p)",  FPUSW_C2_BIT, 1, 0 },
    { "TOP",    FPUSW_TOP_BIT, FPUSW_TOP_BITS, 0 },
    { "C3(z)",  FPUSW_C3_BIT, 1, 0 },
    { "B",      FPUSW_B_BIT,  1, 0 },
    { 0,        -1,           0, 0 }
};

static char const reserved_exception[] = "Reserved";

static char const * const exception_names[] = {
    "#DE Divide Error",
    "#DB Debug",
    "#NM NMI",
    "#BP Breakpoint,",
    "#OF Overflow",
    "#BR BOUND Range Exceeded",
    "#UD Invalid Opcode",
    "#NM Device Not Available",
    "#DF Double Fault",
    reserved_exception,
    "#TS Invalid TSS",
    "#NP Segment Not Present",
    "#SS Stack Fault",
    "#GP General Protection",
    "#PF Page Fault",
    reserved_exception,
    "#MF Floating-Point Error",
    "#AC Alignment Check",
    "#MC Machine Check",
    "#XM SIMD",
    "#VE Virtualization",
    reserved_exception,
    reserved_exception,
    reserved_exception,
    reserved_exception,
    reserved_exception,
    reserved_exception,
    reserved_exception,
    reserved_exception,
    reserved_exception,
    reserved_exception,
    reserved_exception
};

typedef void (*isr_entry_t)(void);

isr_entry_t const isr_entry_points[256] = {
    isr_entry_0,   isr_entry_1,   isr_entry_2,   isr_entry_3,
    isr_entry_4,   isr_entry_5,   isr_entry_6,   isr_entry_7,
    isr_entry_8,   isr_entry_9,   isr_entry_10,  isr_entry_11,
    isr_entry_12,  isr_entry_13,  isr_entry_14,  isr_entry_15,
    isr_entry_16,  isr_entry_17,  isr_entry_18,  isr_entry_19,
    isr_entry_20,  isr_entry_21,  isr_entry_22,  isr_entry_23,
    isr_entry_24,  isr_entry_25,  isr_entry_26,  isr_entry_27,
    isr_entry_28,  isr_entry_29,  isr_entry_30,  isr_entry_31,

    isr_entry_32,  isr_entry_33,  isr_entry_34,  isr_entry_35,
    isr_entry_36,  isr_entry_37,  isr_entry_38,  isr_entry_39,
    isr_entry_40,  isr_entry_41,  isr_entry_42,  isr_entry_43,
    isr_entry_44,  isr_entry_45,  isr_entry_46,  isr_entry_47,

    isr_entry_48,  isr_entry_49,  isr_entry_50,  isr_entry_51,
    isr_entry_52,  isr_entry_53,  isr_entry_54,  isr_entry_55,
    isr_entry_56,  isr_entry_57,  isr_entry_58,  isr_entry_59,
    isr_entry_60,  isr_entry_61,  isr_entry_62,  isr_entry_63,
    isr_entry_64,  isr_entry_65,  isr_entry_66,  isr_entry_67,
    isr_entry_68,  isr_entry_69,  isr_entry_70,  isr_entry_71,

    isr_entry_72,  isr_entry_73,  isr_entry_74,  isr_entry_75,
    isr_entry_76,  isr_entry_77,  isr_entry_78,  isr_entry_79,
    isr_entry_80,  isr_entry_81,  isr_entry_82,  isr_entry_83,
    isr_entry_84,  isr_entry_85,  isr_entry_86,  isr_entry_87,
    isr_entry_88,  isr_entry_89,  isr_entry_90,  isr_entry_91,
    isr_entry_92,  isr_entry_93,  isr_entry_94,  isr_entry_95,
    isr_entry_96,  isr_entry_97,  isr_entry_98,  isr_entry_99,
    isr_entry_100, isr_entry_101, isr_entry_102, isr_entry_103,
    isr_entry_104, isr_entry_105, isr_entry_106, isr_entry_107,
    isr_entry_108, isr_entry_109, isr_entry_110, isr_entry_111,
    isr_entry_112, isr_entry_113, isr_entry_114, isr_entry_115,
    isr_entry_116, isr_entry_117, isr_entry_118, isr_entry_119,
    isr_entry_120, isr_entry_121, isr_entry_122, isr_entry_123,
    isr_entry_124, isr_entry_125, isr_entry_126, isr_entry_127,
    isr_entry_128, isr_entry_129, isr_entry_130, isr_entry_131,
    isr_entry_132, isr_entry_133, isr_entry_134, isr_entry_135,
    isr_entry_136, isr_entry_137, isr_entry_138, isr_entry_139,
    isr_entry_140, isr_entry_141, isr_entry_142, isr_entry_143,
    isr_entry_144, isr_entry_145, isr_entry_146, isr_entry_147,
    isr_entry_148, isr_entry_149, isr_entry_150, isr_entry_151,
    isr_entry_152, isr_entry_153, isr_entry_154, isr_entry_155,
    isr_entry_156, isr_entry_157, isr_entry_158, isr_entry_159,
    isr_entry_160, isr_entry_161, isr_entry_162, isr_entry_163,
    isr_entry_164, isr_entry_165, isr_entry_166, isr_entry_167,
    isr_entry_168, isr_entry_169, isr_entry_170, isr_entry_171,
    isr_entry_172, isr_entry_173, isr_entry_174, isr_entry_175,
    isr_entry_176, isr_entry_177, isr_entry_178, isr_entry_179,
    isr_entry_180, isr_entry_181, isr_entry_182, isr_entry_183,
    isr_entry_184, isr_entry_185, isr_entry_186, isr_entry_187,
    isr_entry_188, isr_entry_189, isr_entry_190, isr_entry_191,
    isr_entry_192, isr_entry_193, isr_entry_194, isr_entry_195,
    isr_entry_196, isr_entry_197, isr_entry_198, isr_entry_199,
    isr_entry_200, isr_entry_201, isr_entry_202, isr_entry_203,
    isr_entry_204, isr_entry_205, isr_entry_206, isr_entry_207,
    isr_entry_208, isr_entry_209, isr_entry_210, isr_entry_211,
    isr_entry_212, isr_entry_213, isr_entry_214, isr_entry_215,
    isr_entry_216, isr_entry_217, isr_entry_218, isr_entry_219,
    isr_entry_220, isr_entry_221, isr_entry_222, isr_entry_223,
    isr_entry_224, isr_entry_225, isr_entry_226, isr_entry_227,
    isr_entry_228, isr_entry_229, isr_entry_230, isr_entry_231,
    isr_entry_232, isr_entry_233, isr_entry_234, isr_entry_235,
    isr_entry_236, isr_entry_237, isr_entry_238, isr_entry_239,
    isr_entry_240, isr_entry_241, isr_entry_242, isr_entry_243,
    isr_entry_244, isr_entry_245, isr_entry_246, isr_entry_247,
    isr_entry_248, isr_entry_249, isr_entry_250, isr_entry_251,
    isr_entry_252, isr_entry_253, isr_entry_254, isr_entry_255
};

void irq_dispatcher_set_handler(irq_dispatcher_handler_t handler)
{
    irq_dispatcher_vec = handler;
}

isr_context_t *irq_dispatcher(int intr, isr_context_t *ctx)
{
    return irq_dispatcher_vec(intr, ctx);
}

static void idtr_load(table_register_64_t *table_reg)
{
    __asm__ __volatile__ (
        "lidtq (%[table_reg])\n\t"
        :
        : [table_reg] "r" (&table_reg->limit)
        : "memory"
    );
}

void idt_xsave_detect(int ap)
{
    (void)ap;

    cpu_scoped_irq_disable intr_was_enabled;

    cpu_has_fsgsbase = cpuid_has_fsgsbase();

    while (cpuid_has_xsave()) {
        cpuid_t info;

//        if (!cpuid(&info, CPUID_INFO_XSAVE, 0))
//            break;

//        xsave_supported_states = ((uint64_t)info.edx << 32) | info.eax;

//        uint32_t wanted_states = XCR0_X87 |
//                XCR0_SSE | XCR0_AVX |
//                XCR0_AVX512_OPMASK |
//                XCR0_AVX512_UPPER |
//                XCR0_AVX512_XREGS;

//        xsave_enabled_states = wanted_states & xsave_supported_states;

//        // Change enabled XSAVE features
//        cpu_xcr_change_bits(0, xsave_supported_states, xsave_enabled_states);

//        if (ap)
//            break;

        uintptr_t bv = cpu_xcr_change_bits(0, 0, 0);

        printk("orig xcr0=%016zx\n", bv);

        printk("Detecting xsave support\n");

        // Get size of save area
        if (!cpuid(&info, CPUID_INFO_XSAVE, 0))
            break;

        sse_context_save = isr_save_xsave;
        sse_context_restore = isr_restore_xrstor;

        // Store size of save area
        assert(info.ebx < UINT16_MAX);
        sse_context_size = (info.ebx + 15) & -16;

        // Use compact format if available
        if (cpuid(&info, CPUID_INFO_XSAVE, 1)) {
            sse_context_size = (sse_context_size + 63) & -64;

            if (info.eax & 1)
                sse_context_save = isr_save_xsaveopt;
            else if (info.eax & 2)
                sse_context_save = isr_save_xsavec;
        }

        // Save offsets/sizes of extended contexts

        if (cpuid(&info, CPUID_INFO_XSAVE, XCR0_AVX_BIT)) {
            assert(info.ebx < UINT16_MAX);
            assert(info.eax < UINT16_MAX);
            assert(info.ebx + info.eax <= UINT16_MAX);

            sse_avx_offset = (uint16_t)info.ebx;
            sse_avx_size = (uint16_t)info.eax;
        }

        if (cpuid(&info, CPUID_INFO_XSAVE, XCR0_AVX512_OPMASK_BIT)) {
            assert(info.ebx < UINT16_MAX);
            assert(info.eax < UINT16_MAX);
            assert(info.ebx + info.eax <= UINT16_MAX);

            sse_avx512_opmask_offset = (uint16_t)info.ebx;
            sse_avx512_opmask_size = (uint16_t)info.eax;
        }

        if (cpuid(&info, CPUID_INFO_XSAVE, XCR0_AVX512_UPPER_BIT)) {
            assert(info.ebx < UINT16_MAX);
            assert(info.eax < UINT16_MAX);
            assert(info.ebx + info.eax <= UINT16_MAX);

            sse_avx512_upper_offset = (uint16_t)info.ebx;
            sse_avx512_upper_size = (uint16_t)info.eax;
        }

        if (cpuid(&info, CPUID_INFO_XSAVE, XCR0_AVX512_XREGS_BIT)) {
            assert(info.ebx < UINT16_MAX);
            assert(info.eax < UINT16_MAX);
            assert(info.ebx + info.eax <= UINT16_MAX);

            sse_avx512_xregs_offset = (uint16_t)info.ebx;
            sse_avx512_xregs_size = (uint16_t)info.eax;
        }

        // Sanity check offsets
        assert(sse_avx_offset +
               sse_avx_size <= sse_context_size);

        assert(sse_avx512_opmask_offset +
               sse_avx512_opmask_size <= sse_context_size);

        assert(sse_avx512_upper_offset +
               sse_avx512_upper_size <= sse_context_size);

        assert(sse_avx512_xregs_offset +
               sse_avx512_xregs_size <= sse_context_size);

        return;
    }

    if (!ap) {
        assert(sse_context_size == 0);
        assert(sse_context_save == 0);
        assert(sse_context_restore == 0);

        sse_context_size = 512;
        sse_context_save = isr_save_fxsave;
        sse_context_restore = isr_restore_fxrstor;
    }
}

int idt_init(int ap)
{
    uintptr_t addr;

    if (!ap) {
        for (size_t i = 0; i < countof(isr_entry_points); ++i) {
            addr = (uintptr_t)isr_entry_points[i];
            idt[i].offset_lo = (uint16_t)(addr & 0xFFFF);
            idt[i].offset_hi = (uint16_t)((addr >> 16) & 0xFFFF);
            idt[i].offset_64_31 = (uint32_t)
                    ((addr >> 32) & 0xFFFFFFFF);

            idt[i].type_attr = IDT_PRESENT | IDT_INTR;

            idt[i].selector = IDT_SEL;
        }

        // Assign IST entries to interrupts
        idt[INTR_EX_STACK].ist = 1;
        idt[INTR_EX_DBLFAULT].ist = 2;
        //idt[INTR_EX_TSS].ist = 3;
        //idt[INTR_EX_GPF].ist = 4;
        //idt[INTR_EX_PAGE].ist = 5;
    }

    table_register_64_t idtr;

    addr = (uintptr_t)idt;
    idtr.base = addr;
    idtr.limit = sizeof(idt) - 1;

    idtr_load(&idtr);

    return 0;
}

size_t cpu_describe_eflags(char *buf, size_t buf_size, uintptr_t rflags)
{
    return format_flags_register(buf, buf_size, rflags,
                                     cpu_eflags_info);
}

size_t cpu_describe_mxcsr(char *buf, size_t buf_size, uintptr_t mxcsr)
{
    return format_flags_register(buf, buf_size, mxcsr,
                                     cpu_mxcsr_info);

}

size_t cpu_describe_fpucw(char *buf, size_t buf_size, uint16_t fpucw)
{
    return format_flags_register(buf, buf_size, fpucw,
                                     cpu_fpucw_info);

}

size_t cpu_describe_fpusw(char *buf, size_t buf_size, uint16_t fpusw)
{
    return format_flags_register(buf, buf_size, fpusw,
                                     cpu_fpusw_info);

}


struct xsave_hdr_t {
    uint64_t xstate_bv;
    uint64_t xcomp_bv;
};

// NOTE: need separate get/set to handle modifying compact format in place
static uint64_t const *cpu_get_fpr_reg(isr_context_t *ctx, uint8_t reg)
{
    xsave_hdr_t const *hdr = (xsave_hdr_t*)((char*)ctx->fpr + 512);
    uint64_t const *area;
    bool compact = (hdr->xcomp_bv & (uint64_t(1) << 63)) != 0;
    int index = 0;
    bool bit;

    static uint64_t constexpr zero = 0;

    if (reg < 16) {
        // reg  0-15 xmm0-xmm15 (128 bits each)
        return ctx->fpr->xmm[reg].qword;
    } else if (sse_avx_offset && reg < 32) {
        // reg 16-31 ymm0h-ymm15h (128 bits each)
        reg -= 16;

        area = (uint64_t*)((char*)ctx->fpr + sse_avx_offset);

        if (compact) {
            // Compact format

            for (uint8_t i = 0; i < 16; ++i) {
                bit = hdr->xcomp_bv & (1 << (i + 2));
                index += bit;
            }

            if (!bit)
                return &zero;

            return area + (index * 2);
        } else {
            // Standard format

            return area + (reg * 2);
        }
    } else if (sse_avx512_upper_offset && reg < 48) {
        // reg 32-47 zmm0h-zmm15h (256 bits each)
        reg -= 32;

    } else if (sse_avx512_xregs_offset && reg < 64) {
        // reg 48-63 zmm16-zmm31  (512 bits each)
        reg -= 48;

    } else if (sse_avx512_opmask_offset && reg < 72) {
        // reg 64-72 are opmask (64 bits each)
    }

    return nullptr;
}

static void stack_trace(isr_context_t *ctx,
                        void (*cb)(uintptr_t rbp, uintptr_t rip))
{
    uintptr_t *frame_ptr = (uintptr_t*)ISR_CTX_REG_RBP(ctx);

    uintptr_t frame_rbp;
    uintptr_t frame_rip;

    do {
        if (!mpresent(uintptr_t(frame_ptr), sizeof(uintptr_t) * 2))
            return;

        frame_rbp = frame_ptr[0];
        frame_rip = frame_ptr[1];

        cb(frame_rbp, frame_rip);

        frame_ptr = (uintptr_t*)frame_rbp;
    } while (frame_rbp);
}

static void dump_frame(uintptr_t rbp, uintptr_t rip)
{
    printk("at rbp=%016zx rip=%016zx\n", rbp, rip);
}

void dump_context(isr_context_t *ctx, int to_screen)
{
    char fmt_buf[64];
    int color = 0x0F;
    int width;
    static char const *reg_names[] = {
        "rdi",
        "rsi",
        "rdx",
        "rcx",
        " r8",
        " r9",
        "rax",
        "rbx",
        "rbp",
        "r10",
        "r11",
        "r12",
        "r13",
        "r14",
        "r15"
    };

    static char const *seg_names[] = {
        "ds", "es", "fs", "gs"
    };

    //
    // Dump context to debug console

    printdbg("- Exception -------------------------------\n");

    // General registers (except rsp)
    for (int i = 0; i < 15; ++i) {
        printdbg("%s=%016lx\n",
                 reg_names[i],
                 ctx->gpr->r[i]);
    }

    if (sse_avx512_xregs_offset && sse_avx512_xregs_size) {
        // 32 register AVX-512

    } else if (sse_avx512_upper_offset && sse_avx512_upper_size) {
        // AVX-512

    } else if (sse_avx_offset && sse_avx_size) {
        // AVX
        for (int i = 0; i < 16; ++i) {
            uint64_t const *lo = cpu_get_fpr_reg(ctx, i);
            uint64_t const *hi = cpu_get_fpr_reg(ctx, i + 16);
            printdbg("%symm%d=%016lx:%016lx:%016lx:%016lx\n",
                     i > 9 ? "" : " ", i,
                     hi[1], hi[0], lo[1], lo[0]);
        }
    } else {
        // xmm registers
        for (int i = 0; i < 16; ++i) {
            printdbg("%sxmm%d=%016lx%016lx\n",
                     i > 9 ? "" : " ", i,
                     ctx->fpr->xmm[i].qword[1],
                    ctx->fpr->xmm[i].qword[0]);
        }
    }

    // Segment registers
    for (int i = 0; i < 4; ++i) {
        printdbg("%s=%04x\n",
                 seg_names[i],
                 ctx->gpr->s[i]);
    }

    // ss:rsp
    printdbg("ss:rsp=%04lx:%012lx\n",
             ctx->gpr->iret.ss,
             ctx->gpr->iret.rsp);
    // cs:rip
    printdbg("cs:rip=%04lx:%012lx\n",
             ctx->gpr->iret.cs,
             (uintptr_t)ctx->gpr->iret.rip);

    // Exception
    if (ctx->gpr->info.interrupt < 32) {
        printdbg("Exception 0x%02lx %s\n",
                 ctx->gpr->info.interrupt,
                 exception_names[ctx->gpr->info.interrupt]);
    } else {
        printdbg("Interrupt 0x%02lx\n",
                 ctx->gpr->info.interrupt);
    }

    // mxcsr and description
    cpu_describe_mxcsr(fmt_buf, sizeof(fmt_buf),
                       ctx->fpr->mxcsr);
    printdbg("mxcsr=%04x %s\n",
             ctx->fpr->mxcsr, fmt_buf);

    // fpucw and description
    cpu_describe_fpucw(fmt_buf, sizeof(fmt_buf),
                       ctx->fpr->fcw);
    printdbg("fpucw=%04x %s\n",
             ctx->fpr->fcw, fmt_buf);

    // fpusw and description
    cpu_describe_fpusw(fmt_buf, sizeof(fmt_buf),
                       ctx->fpr->fsw);
    printdbg("fpusw=%04x %s\n",
             ctx->fpr->fsw, fmt_buf);

    // fault address
    printdbg("cr2=%012lx\n",
             cpu_get_fault_address());

    // error code
    printdbg("Error code 0x%012lx\n",
             ctx->gpr->info.error_code);

    // rflags (it's actually only 22 bits) and description
    cpu_describe_eflags(fmt_buf, sizeof(fmt_buf),
                       ctx->gpr->iret.rflags);
    printdbg("rflags=%06lx %s\n",
             ctx->gpr->iret.rflags,
             fmt_buf);

    // fsbase
    printdbg("fsbase=%16zx\n",
             (uintptr_t)ctx->gpr->fsbase);

    // gsbase
    printdbg("gsbase=%16lx\n",
             cpu_msr_get(MSR_GSBASE));

    printdbg("-------------------------------------------\n");

    //
    // Dump context to screen

    if (!to_screen)
        return;

    for (int i = 0; i < 16; ++i) {
        if (i < 15) {
            // General register name
            con_draw_xy(0, i, reg_names[i], color);
            // General register value
            snprintf(fmt_buf, sizeof(fmt_buf), "=%016lx ",
                     ctx->gpr->r[i]);
            con_draw_xy(3, i, fmt_buf, color);
        }

        // XMM register name
        snprintf(fmt_buf, sizeof(fmt_buf), " %sxmm%d",
                 i < 10 ? " " : "",
                 i);
        con_draw_xy(29, i, fmt_buf, color);

        // XMM register value
        snprintf(fmt_buf, sizeof(fmt_buf), "=%016lx%016lx ",
                ctx->fpr->xmm[i].qword[0],
                ctx->fpr->xmm[i].qword[1]);
        con_draw_xy(35, i, fmt_buf, color);
    }

    for (int i = 0; i < 4; ++i) {
        // Segment register name
        con_draw_xy(37+i*8, 18, seg_names[i], color);
        // Segment register value
        snprintf(fmt_buf, sizeof(fmt_buf), "=%04x ",
                 ctx->gpr->s[i]);
        con_draw_xy(39+i*8, 18, fmt_buf, color);
    }

    // ss:rsp
    con_draw_xy(0, 15, "ss:rsp", color);
    snprintf(fmt_buf, sizeof(fmt_buf), "=%04lx:%012lx ",
             ctx->gpr->iret.ss, ctx->gpr->iret.rsp);
    con_draw_xy(6, 15, fmt_buf, color);

    // cs:rip
    con_draw_xy(0, 16, "cs:rip", color);
    snprintf(fmt_buf, sizeof(fmt_buf), "=%04lx:%012zx",
             ctx->gpr->iret.cs, (uintptr_t)ctx->gpr->iret.rip);
    con_draw_xy(6, 16, fmt_buf, color);

    if (ctx->gpr->info.interrupt < 32) {
        // exception
        con_draw_xy(0, 17, "Exception", color);
        snprintf(fmt_buf, sizeof(fmt_buf), " 0x%02lx %s",
                 ctx->gpr->info.interrupt,
                 exception_names[ctx->gpr->info.interrupt]);
        con_draw_xy(9, 17, fmt_buf, color);
    } else {
        con_draw_xy(0, 17, "Interrupt", color);
        snprintf(fmt_buf, sizeof(fmt_buf), " 0x%02lx",
                 ctx->gpr->info.interrupt);
        con_draw_xy(9, 17, fmt_buf, color);
    }

    // MXCSR
    width = snprintf(fmt_buf, sizeof(fmt_buf), "=%04x",
                     ctx->fpr->mxcsr);
    con_draw_xy(63-width, 16, "mxcsr", color);
    con_draw_xy(68-width, 16, fmt_buf, color);

    // MXCSR description
    width = cpu_describe_mxcsr(fmt_buf, sizeof(fmt_buf),
                       ctx->fpr->mxcsr);
    con_draw_xy(68-width, 17, fmt_buf, color);

    // fault address
    con_draw_xy(48, 19, "cr2", color);
    snprintf(fmt_buf, sizeof(fmt_buf), "=%012lx",
             cpu_get_fault_address());
    con_draw_xy(51, 19, fmt_buf, color);

    // error code
    con_draw_xy(0, 18, "Error code", color);
    snprintf(fmt_buf, sizeof(fmt_buf), " 0x%012lx",
             ctx->gpr->info.error_code);
    con_draw_xy(10, 18, fmt_buf, color);

    // rflags (it's actually only 22 bits)
    con_draw_xy(0, 19, "rflags", color);
    width = snprintf(fmt_buf, sizeof(fmt_buf), "=%06lx ",
             ctx->gpr->iret.rflags);
    con_draw_xy(6, 19, fmt_buf, color);

    // rflags description
    cpu_describe_eflags(fmt_buf, sizeof(fmt_buf),
                       ctx->gpr->iret.rflags);
    con_draw_xy(6+width, 19, fmt_buf, color);

    // fsbase
    con_draw_xy(0, 20, "fsbase", color);
    snprintf(fmt_buf, sizeof(fmt_buf), "=%12zx ",
             (uintptr_t)ctx->gpr->fsbase);
    con_draw_xy(6, 20, fmt_buf, color);

    // gsbase
    con_draw_xy(0, 21, "gsbase", color);
    snprintf(fmt_buf, sizeof(fmt_buf), "=%12lx ",
                     cpu_msr_get(MSR_GSBASE));
    con_draw_xy(6, 21, fmt_buf, color);

    // last branch
    //con_draw_xy(0, 22, "lastbr", color);
    //snprintf(fmt_buf, sizeof(fmt_buf), "=%12lx ",
    //                 msr_get(0x1DD));
    //con_draw_xy(6, 22, fmt_buf, color);

    if (ctx->gpr->info.interrupt == INTR_EX_GPF)
        apic_dump_regs(0);

    stack_trace(ctx, dump_frame);
}

isr_context_t *unhandled_exception_handler(isr_context_t *ctx)
{
    if (unhandled_exception_handler_vec) {
        isr_context_t *handled_ctx = unhandled_exception_handler_vec(
                    ctx->gpr->info.interrupt, ctx);

        if (handled_ctx)
            return handled_ctx;
    }

    char const *name = ctx->gpr->info.interrupt < countof(exception_names)
            ? exception_names[ctx->gpr->info.interrupt]
            : nullptr;

    printk("\nUnhandled exception 0x%zx (%s) at RIP=%p\n",
           ctx->gpr->info.interrupt,
           name ? name : "??",
           (void*)ctx->gpr->iret.rip);

    dump_context(ctx, 1);
    cpu_debug_break();
    halt_forever();
    return ctx;
}

//isr_context_t *exception_isr_handler(int intr, isr_context_t *ctx)
//{
//    if (!intr_has_handler(intr) || !intr_invoke(intr, ctx)) {
//        if (__exception_handler_invoke(intr))
//            return unhandled_exception_handler(ctx);
//        cpu_debug_break();
//    }
//
//    return ctx;
//}

void idt_set_unhandled_exception_handler(
        idt_unhandled_exception_handler_t handler)
{
    assert(!unhandled_exception_handler_vec);
    unhandled_exception_handler_vec = handler;
}

void idt_override_vector(int intr, irq_dispatcher_handler_t handler)
{
    idt[intr].offset_lo = uint16_t(uintptr_t(handler) >> 0);
    idt[intr].offset_hi = uint16_t(uintptr_t(handler) >> 16);
    idt[intr].offset_64_31 = uintptr_t(handler) >> 32;
}

void idt_clone_debug_exception_dispatcher(void)
{
    // From linker script
    extern char ___isr_st[];
    extern char ___isr_en[];
    char const * bp_entry = (char const * const)isr_entry_3;
    char const * debug_entry = (char const * const)isr_entry_1;

    size_t isr_size = ___isr_en - ___isr_st;
    size_t bp_entry_ofs = bp_entry - ___isr_st;
    size_t debug_entry_ofs = debug_entry - ___isr_st;

    char *clone = (char*)mmap(0, isr_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                       MAP_POPULATE, -1, 0);

    memcpy(clone, ___isr_st, isr_size);

    auto clone_breakpoint = irq_dispatcher_handler_t(clone + bp_entry_ofs);
    auto clone_debug = irq_dispatcher_handler_t(clone + debug_entry_ofs);

    idt_override_vector(INTR_EX_BREAKPOINT, clone_breakpoint);
    idt_override_vector(INTR_EX_DEBUG, clone_debug);
}
