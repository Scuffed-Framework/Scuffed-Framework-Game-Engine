#pragma once

#include <cstdint>

// ============================================================================
// Platform Detection
// ============================================================================
#if defined(_MSC_VER)
    #define SF_COMPILER_MSVC 1
    #include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
    #define SF_COMPILER_GNU 1
#endif

#if defined(__x86_64__) || defined(_M_X64)
    #define SF_ARCH_X64 1
#elif defined(__i386__) || defined(_M_IX86)
    #define SF_ARCH_X86 1
#endif

#if defined(__SSE__)
    #include <xmmintrin.h>
#endif
#if defined(__SSE2__)
    #include <emmintrin.h>
#endif
#if defined(__SSE3__)
    #include <pmmintrin.h>
#endif

// ============================================================================
// Assembly Block Wrapper
// ============================================================================

#if defined(SF_COMPILER_GNU)
    // GCC/Clang: Inline assembly with AT&T syntax
    #define SF_ASM_VOLATILE_BEGIN   asm volatile(
    #define SF_ASM_VOLATILE_END     );
    #define SF_ASM_LINE(code)       code "\n\t"
    
    // Operand constraints
    #define SF_ASM_OUT(constraints) : constraints
    #define SF_ASM_IN(constraints)  : constraints
    #define SF_ASM_CLOBBER(list)    : list
    
#elif defined(SF_COMPILER_MSVC) && defined(SF_ARCH_X86)
    // MSVC 32-bit: Intel syntax inline assembly
    #define SF_ASM_VOLATILE_BEGIN   __asm {
    #define SF_ASM_VOLATILE_END     }
    #define SF_ASM_LINE(code)       code
    #define SF_ASM_OUT(constraints)
    #define SF_ASM_IN(constraints)
    #define SF_ASM_CLOBBER(list)
    
#elif defined(SF_COMPILER_MSVC) && defined(SF_ARCH_X64)
    // MSVC 64-bit: No inline assembly, must use intrinsics
    #define SF_ASM_UNSUPPORTED 1
    #define SF_ASM_VOLATILE_BEGIN   {
    #define SF_ASM_VOLATILE_END     }
    #define SF_ASM_LINE(code)
    #define SF_ASM_OUT(constraints)
    #define SF_ASM_IN(constraints)
    #define SF_ASM_CLOBBER(list)
    
#else
    #define SF_ASM_UNSUPPORTED 1
    #define SF_ASM_VOLATILE_BEGIN   {
    #define SF_ASM_VOLATILE_END     }
    #define SF_ASM_LINE(code)
    #define SF_ASM_OUT(constraints)
    #define SF_ASM_IN(constraints)
    #define SF_ASM_CLOBBER(list)
#endif

// ============================================================================
// Register Names - Platform Independent
// ============================================================================

#if defined(SF_COMPILER_GNU)
    // AT&T syntax: registers prefixed with %
    
    // 64-bit registers
    #define RAX     %rax
    #define RBX     %rbx
    #define RCX     %rcx
    #define RDX     %rdx
    #define RSI     %rsi
    #define RDI     %rdi
    #define RBP     %rbp
    #define RSP     %rsp
    #define R8      %r8
    #define R9      %r9
    #define R10     %r10
    #define R11     %r11
    #define R12     %r12
    #define R13     %r13
    #define R14     %r14
    #define R15     %r15
    
    // 32-bit registers
    #define EAX     %eax
    #define EBX     %ebx
    #define ECX     %ecx
    #define EDX     %edx
    #define ESI     %esi
    #define EDI     %edi
    #define EBP     %ebp
    #define ESP     %esp
    
    // 16-bit registers
    #define AX      %ax
    #define BX      %bx
    #define CX      %cx
    #define DX      %dx
    
    // 8-bit registers
    #define AL      %al
    #define AH      %ah
    #define BL      %bl
    #define BH      %bh
    #define CL      %cl
    #define CH      %ch
    #define DL      %dl
    #define DH      %dh
    
    // XMM registers
    #define XMM0    %xmm0
    #define XMM1    %xmm1
    #define XMM2    %xmm2
    #define XMM3    %xmm3
    #define XMM4    %xmm4
    #define XMM5    %xmm5
    #define XMM6    %xmm6
    #define XMM7    %xmm7
    #define XMM8    %xmm8
    #define XMM9    %xmm9
    #define XMM10   %xmm10
    #define XMM11   %xmm11
    #define XMM12   %xmm12
    #define XMM13   %xmm13
    #define XMM14   %xmm14
    #define XMM15   %xmm15
    
    // Immediates and memory
    #define IMM(x)          $##x
    #define MEM(addr)       addr
    #define MEM_OFF(o, b)   o(b)
    
#else
    // Intel syntax: no prefix
    
    // 64-bit registers
    #define RAX     rax
    #define RBX     rbx
    #define RCX     rcx
    #define RDX     rdx
    #define RSI     rsi
    #define RDI     rdi
    #define RBP     rbp
    #define RSP     rsp
    #define R8      r8
    #define R9      r9
    #define R10     r10
    #define R11     r11
    #define R12     r12
    #define R13     r13
    #define R14     r14
    #define R15     r15
    
    // 32-bit registers
    #define EAX     eax
    #define EBX     ebx
    #define ECX     ecx
    #define EDX     edx
    #define ESI     esi
    #define EDI     edi
    #define EBP     ebp
    #define ESP     esp
    
    // 16-bit registers
    #define AX      ax
    #define BX      bx
    #define CX      cx
    #define DX      dx
    
    // 8-bit registers
    #define AL      al
    #define AH      ah
    #define BL      bl
    #define BH      bh
    #define CL      cl
    #define CH      ch
    #define DL      dl
    #define DH      dh
    
    // XMM registers
    #define XMM0    xmm0
    #define XMM1    xmm1
    #define XMM2    xmm2
    #define XMM3    xmm3
    #define XMM4    xmm4
    #define XMM5    xmm5
    #define XMM6    xmm6
    #define XMM7    xmm7
    #define XMM8    xmm8
    #define XMM9    xmm9
    #define XMM10   xmm10
    #define XMM11   xmm11
    #define XMM12   xmm12
    #define XMM13   xmm13
    #define XMM14   xmm14
    #define XMM15   xmm15
    
    // Immediates and memory
    #define IMM(x)          x
    #define MEM(addr)       [addr]
    #define MEM_OFF(o, b)   [b + o]
#endif

// ============================================================================
// Instruction Macros - Platform Independent Interface
// ============================================================================

#if defined(SF_COMPILER_GNU)
    // AT&T syntax: src, dst order
    
    // Move
    #define SF_MOV(dst, src)        "mov " #src ", " #dst
    #define SF_MOVL(dst, src)       "movl " #src ", " #dst
    #define SF_MOVQ(dst, src)       "movq " #src ", " #dst
    
    // Arithmetic
    #define SF_ADD(dst, src)        "add " #src ", " #dst
    #define SF_SUB(dst, src)        "sub " #src ", " #dst
    #define SF_MUL(src)             "mul " #src
    #define SF_IMUL(dst, src)       "imul " #src ", " #dst
    #define SF_DIV(src)             "div " #src
    #define SF_INC(dst)             "inc " #dst
    #define SF_DEC(dst)             "dec " #dst
    #define SF_NEG(dst)             "neg " #dst
    
    // Bitwise
    #define SF_AND(dst, src)        "and " #src ", " #dst
    #define SF_OR(dst, src)         "or " #src ", " #dst
    #define SF_XOR(dst, src)        "xor " #src ", " #dst
    #define SF_NOT(dst)             "not " #dst
    #define SF_SHL(dst, cnt)        "shl " #cnt ", " #dst
    #define SF_SHR(dst, cnt)        "shr " #cnt ", " #dst
    #define SF_SAL(dst, cnt)        "sal " #cnt ", " #dst
    #define SF_SAR(dst, cnt)        "sar " #cnt ", " #dst
    #define SF_ROL(dst, cnt)        "rol " #cnt ", " #dst
    #define SF_ROR(dst, cnt)        "ror " #cnt ", " #dst
    
    // Comparison
    #define SF_CMP(op1, op2)        "cmp " #op2 ", " #op1
    #define SF_TEST(op1, op2)       "test " #op2 ", " #op1
    
    // Stack
    #define SF_PUSH(src)            "push " #src
    #define SF_POP(dst)             "pop " #dst
    
    // x87 FPU
    #define SF_FLD(src)             "fld " #src
    #define SF_FLDS(src)            "flds " #src
    #define SF_FLDL(src)            "fldl " #src
    #define SF_FST(dst)             "fst " #dst
    #define SF_FSTP(dst)            "fstp " #dst
    #define SF_FSTS(dst)            "fsts " #dst
    #define SF_FSTPS(dst)           "fstps " #dst
    #define SF_FSIN                 "fsin"
    #define SF_FCOS                 "fcos"
    #define SF_FSQRT                "fsqrt"
    #define SF_FADD                 "fadd"
    #define SF_FSUB                 "fsub"
    #define SF_FMUL                 "fmul"
    #define SF_FDIV                 "fdiv"
    
    // SSE - Scalar
    #define SF_MOVSS(dst, src)      "movss " #src ", " #dst
    #define SF_ADDSS(dst, src)      "addss " #src ", " #dst
    #define SF_SUBSS(dst, src)      "subss " #src ", " #dst
    #define SF_MULSS(dst, src)      "mulss " #src ", " #dst
    #define SF_DIVSS(dst, src)      "divss " #src ", " #dst
    #define SF_SQRTSS(dst, src)     "sqrtss " #src ", " #dst
    #define SF_RSQRTSS(dst, src)    "rsqrtss " #src ", " #dst
    #define SF_MINSS(dst, src)      "minss " #src ", " #dst
    #define SF_MAXSS(dst, src)      "maxss " #src ", " #dst
    
    // SSE - Packed
    #define SF_MOVAPS(dst, src)     "movaps " #src ", " #dst
    #define SF_MOVUPS(dst, src)     "movups " #src ", " #dst
    #define SF_ADDPS(dst, src)      "addps " #src ", " #dst
    #define SF_SUBPS(dst, src)      "subps " #src ", " #dst
    #define SF_MULPS(dst, src)      "mulps " #src ", " #dst
    #define SF_DIVPS(dst, src)      "divps " #src ", " #dst
    #define SF_SQRTPS(dst, src)     "sqrtps " #src ", " #dst
    #define SF_RSQRTPS(dst, src)    "rsqrtps " #src ", " #dst
    #define SF_MINPS(dst, src)      "minps " #src ", " #dst
    #define SF_MAXPS(dst, src)      "maxps " #src ", " #dst
    #define SF_ANDPS(dst, src)      "andps " #src ", " #dst
    #define SF_ORPS(dst, src)       "orps " #src ", " #dst
    #define SF_XORPS(dst, src)      "xorps " #src ", " #dst
    
    // SSE3
    #define SF_HADDPS(dst, src)     "haddps " #src ", " #dst
    #define SF_HSUBPS(dst, src)     "hsubps " #src ", " #dst
    
    // BMI/ABM
    #define SF_LZCNT(dst, src)      "lzcnt " #src ", " #dst
    #define SF_TZCNT(dst, src)      "tzcnt " #src ", " #dst
    #define SF_POPCNT(dst, src)     "popcnt " #src ", " #dst
    
    // Misc
    #define SF_NOP                  "nop"
    #define SF_BSWAP(reg)           "bswap " #reg
    
#else
    // Intel syntax: dst, src order
    
    // Move
    #define SF_MOV(dst, src)        mov dst, src
    #define SF_MOVL(dst, src)       mov dst, src
    #define SF_MOVQ(dst, src)       mov dst, src
    
    // Arithmetic
    #define SF_ADD(dst, src)        add dst, src
    #define SF_SUB(dst, src)        sub dst, src
    #define SF_MUL(src)             mul src
    #define SF_IMUL(dst, src)       imul dst, src
    #define SF_DIV(src)             div src
    #define SF_INC(dst)             inc dst
    #define SF_DEC(dst)             dec dst
    #define SF_NEG(dst)             neg dst
    
    // Bitwise
    #define SF_AND(dst, src)        and dst, src
    #define SF_OR(dst, src)         or dst, src
    #define SF_XOR(dst, src)        xor dst, src
    #define SF_NOT(dst)             not dst
    #define SF_SHL(dst, cnt)        shl dst, cnt
    #define SF_SHR(dst, cnt)        shr dst, cnt
    #define SF_SAL(dst, cnt)        sal dst, cnt
    #define SF_SAR(dst, cnt)        sar dst, cnt
    #define SF_ROL(dst, cnt)        rol dst, cnt
    #define SF_ROR(dst, cnt)        ror dst, cnt
    
    // Comparison
    #define SF_CMP(op1, op2)        cmp op1, op2
    #define SF_TEST(op1, op2)       test op1, op2
    
    // Stack
    #define SF_PUSH(src)            push src
    #define SF_POP(dst)             pop dst
    
    // x87 FPU
    #define SF_FLD(src)             fld src
    #define SF_FLDS(src)            fld DWORD PTR src
    #define SF_FLDL(src)            fld QWORD PTR src
    #define SF_FST(dst)             fst dst
    #define SF_FSTP(dst)            fstp dst
    #define SF_FSTS(dst)            fst DWORD PTR dst
    #define SF_FSTPS(dst)           fstp DWORD PTR dst
    #define SF_FSIN                 fsin
    #define SF_FCOS                 fcos
    #define SF_FSQRT                fsqrt
    #define SF_FADD                 fadd
    #define SF_FSUB                 fsub
    #define SF_FMUL                 fmul
    #define SF_FDIV                 fdiv
    
    // SSE - Scalar
    #define SF_MOVSS(dst, src)      movss dst, src
    #define SF_ADDSS(dst, src)      addss dst, src
    #define SF_SUBSS(dst, src)      subss dst, src
    #define SF_MULSS(dst, src)      mulss dst, src
    #define SF_DIVSS(dst, src)      divss dst, src
    #define SF_SQRTSS(dst, src)     sqrtss dst, src
    #define SF_RSQRTSS(dst, src)    rsqrtss dst, src
    #define SF_MINSS(dst, src)      minss dst, src
    #define SF_MAXSS(dst, src)      maxss dst, src
    
    // SSE - Packed
    #define SF_MOVAPS(dst, src)     movaps dst, src
    #define SF_MOVUPS(dst, src)     movups dst, src
    #define SF_ADDPS(dst, src)      addps dst, src
    #define SF_SUBPS(dst, src)      subps dst, src
    #define SF_MULPS(dst, src)      mulps dst, src
    #define SF_DIVPS(dst, src)      divps dst, src
    #define SF_SQRTPS(dst, src)     sqrtps dst, src
    #define SF_RSQRTPS(dst, src)    rsqrtps dst, src
    #define SF_MINPS(dst, src)      minps dst, src
    #define SF_MAXPS(dst, src)      maxps dst, src
    #define SF_ANDPS(dst, src)      andps dst, src
    #define SF_ORPS(dst, src)       orps dst, src
    #define SF_XORPS(dst, src)      xorps dst, src
    
    // SSE3
    #define SF_HADDPS(dst, src)     haddps dst, src
    #define SF_HSUBPS(dst, src)     hsubps dst, src
    
    // BMI/ABM
    #define SF_LZCNT(dst, src)      lzcnt dst, src
    #define SF_TZCNT(dst, src)      tzcnt dst, src
    #define SF_POPCNT(dst, src)     popcnt dst, src
    
    // Misc
    #define SF_NOP                  nop
    #define SF_BSWAP(reg)           bswap reg
#endif

// ============================================================================
// Constraint helpers for GCC/Clang
// ============================================================================
#if defined(SF_COMPILER_GNU)
    #define SF_OUT_REG(var)         "=r"(var)
    #define SF_OUT_MEM(var)         "=m"(var)
    #define SF_OUT_XMM(var)         "=x"(var)
    #define SF_IN_REG(var)          "r"(var)
    #define SF_IN_MEM(var)          "m"(var)
    #define SF_IN_XMM(var)          "x"(var)
    #define SF_INOUT_REG(var)       "+r"(var)
    #define SF_INOUT_XMM(var)       "+x"(var)
#else
    #define SF_OUT_REG(var)
    #define SF_OUT_MEM(var)
    #define SF_OUT_XMM(var)
    #define SF_IN_REG(var)
    #define SF_IN_MEM(var)
    #define SF_IN_XMM(var)
    #define SF_INOUT_REG(var)
    #define SF_INOUT_XMM(var)
#endif