/*
 * 64-bit unsigned division for LLVM/Clang builds on MIPS-I
 */

typedef unsigned long long uint64_t;
typedef long long int64_t;

uint64_t __udivdi3(uint64_t num, uint64_t den)
{
    uint64_t quot = 0, qbit = 1;

    if (den == 0)
        return 0;

    while ((int64_t)den >= 0 && den < num) {
        den <<= 1;
        qbit <<= 1;
    }

    while (qbit) {
        if (num >= den) {
            num -= den;
            quot += qbit;
        }
        den >>= 1;
        qbit >>= 1;
    }

    return quot;
}
