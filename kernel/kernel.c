#include <stdint.h>
#include <stddef.h>
#include "limine.h"

// The Limine requests can be placed anywhere, but it is important that
// the compiler does not optimise them away, so, usually, they should
// be made volatile or equivalent.

static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

// GCC and Clang reserve the right to generate calls to the following
// 4 functions even if they are not directly called.
// Implement them as the C specification mandates.
// DO NOT remove or rename these functions, or stuff will eventually break!
// They CAN be moved to a different .c file.

void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }

    return dest;
}

void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;

    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }

    return s;
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    if (src > dest) {
        for (size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if (src < dest) {
        for (size_t i = n; i > 0; i--) {
            pdest[i-1] = psrc[i-1];
        }
    }

    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }

    return 0;
}

// Halt and catch fire function.
static void hcf(void) {
    for (;;) {
#if defined (__x86_64__)
        asm ("hlt");
#elif defined (__aarch64__) || defined (__riscv)
        asm ("wfi");
#endif
    }
}

#include "flanterm/flanterm.h"
#include "flanterm/backends/fb.h"
#include "printf/printf.h"

#define blk "\e[30m"
#define red "\e[31m"
#define grn "\e[32m"
#define ylw "\e[33m"
#define blu "\e[34m"
#define pur "\e[35m"
#define cyn "\e[36m"
#define wht "\e[37m"
#define clr "\e[00m"
#define bld "\e[01m"
#define err_ "[ERROR] "
#define err clr bld red err_ clr
#define inf_ "[INFO] "
#define inf clr bld blu inf_ clr

#define print_term(term, fs ...) \
    flanterm_write(gft_ctx, term, sizeof(term)); \
    printf_(fs);

#define print_error(fs ...) \
    print_term(err, fs)

#define print_info(fs ...) \
    print_term(inf, fs);

struct flanterm_context *gft_ctx;

void putchar_(char c) {
    char str[] = {c, '\0'};
    flanterm_write(gft_ctx, str, sizeof(str));
}

// The following will be our kernel's entry point.
// If renaming _start() to something else, make sure to change the
// linker script accordingly.
void _start(void) {
    // Ensure we got a framebuffer.
    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }

    // Fetch the first framebuffer.
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];

    // Note: we assume the framebuffer model is RGB with 32-bit pixels.
    for (size_t i = 0; i < 100; i++) {
        volatile uint32_t *fb_ptr = framebuffer->address;
        fb_ptr[i * (framebuffer->pitch / 4) + i] = 0xffffff;
    }

    gft_ctx = flanterm_fb_simple_init(
        framebuffer->address, framebuffer->width, framebuffer->height, framebuffer->pitch
    );

    print_term(bld grn, "%s", "Success! Welcome to the kernel...\n");

    print_info("framebuffer count: ");
    printf_("%lu\n", framebuffer_request.response->framebuffer_count);

    print_error("%s", "Missing Kernel Implementation!!!");

    // We're done, just hang...
    hcf();
}
