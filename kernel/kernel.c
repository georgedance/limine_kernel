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

static volatile struct limine_bootloader_info_request bootloader_info_request = {
    .id = LIMINE_BOOTLOADER_INFO_REQUEST,
    .revision = 0
};

static volatile struct limine_efi_system_table_request system_table_request = {
    .id = LIMINE_EFI_SYSTEM_TABLE_REQUEST,
    .revision = 0
};

static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
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
#define wrn_ "[WARN] "
#define wrn clr bld ylw wrn_ clr
#define inf_ "[INFO] "
#define inf clr bld blu inf_ clr

#define print_term(term, fs ...) \
    flanterm_write(gft_ctx, term, sizeof(term)); \
    printf_(fs);

#define print_error(fs ...) \
    print_term(err, fs)

#define print_warn(fs ...) \
    print_term(wrn, fs)

#define print_info(fs ...) \
    print_term(inf, fs);

struct flanterm_context *gft_ctx;

void putchar_(char c) {
    char str[] = {c, '\0'};
    flanterm_write(gft_ctx, str, sizeof(str));
}

char *memmap_type(uint64_t type) {
    switch(type) {
        case 0:
            return "Usable";
        case 1:
            return "Reserved";
        case 2:
            return "ACPI Reclaimable";
        case 3:
            return "ACPI NVS";
        case 4:
            return "BAD MEMORY";
        case 5:
            return "Bootloader Reclaimable";
        case 6:
            return "Kernel or Modules";
        case 7:
            return "Framebuffer";
        default:
            return "Unknown";
    }
}

#include "../limine-efi/inc/efi.h"

EFI_STATUS              status = EFI_SUCCESS;

EFI_SYSTEM_TABLE        *ST = {0};
#define gST              ST
EFI_RUNTIME_SERVICES    *RT = {0};
#define gRT              RT

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
    /*
    for (size_t i = 0; i < 100; i++) {
        volatile uint32_t *fb_ptr = framebuffer->address;
        fb_ptr[i * (framebuffer->pitch / 4) + i] = 0xffffff;
    }
    */

    gft_ctx = flanterm_fb_simple_init(
        framebuffer->address, framebuffer->width, framebuffer->height, framebuffer->pitch
    );

    /*
    for(int i = 0; i < 128; i++) {
        printf_("%c", i);
    }
    printf_("\n");
    */

    print_info("Initialised FlanTerm.\n");

    print_info("Framebuffer Count: %lu\n", framebuffer_request.response->framebuffer_count);

    print_term(bld grn, "Welcome to the kernel...\n");

    if(bootloader_info_request.response == NULL) {
        print_error("Couldn't get Bootloader Info!\n");
    }
    print_info("Bootloader Info: %s, v%s\n",
            bootloader_info_request.response->name,
            bootloader_info_request.response->version);

    if(system_table_request.response == NULL
    || system_table_request.response->address == NULL) {
        print_error("Couldn't get SystemTable Pointer!\n");
        hcf();
    }
    else {
        print_info("Acquired SystemTable Pointer.\n");
        gST = system_table_request.response->address;
        gRT = gST->RuntimeServices;
    }

    print_info("Firmware Vendor: %ls, Revision %d\n", gST->FirmwareVendor, gST->FirmwareRevision);

    EFI_TIME Time;
    gRT->GetTime(&Time, NULL);
    print_info("Time: %s%hhu:%hhu:%hhu\n", 
            (Time.TimeZone == 2047 ? "(UTC) " : "" ), 
            Time.Hour, Time.Minute, Time.Second);

    if(memmap_request.response == NULL) {
        print_error("Couldn't get memmap!\n");
    }
    else {
        print_info("Memmap entry count: %lu\n", memmap_request.response->entry_count);
        struct limine_memmap_response *memmap_response = memmap_request.response;
        struct limine_memmap_entry *memmap = *memmap_response->entries;
        for(size_t i = 0; i < memmap_response->entry_count; i++) {
            printf_("count: %-3lu base: 0x%-10lX length: 0x%-10lX type: %s\n", 
                        i, memmap->base, memmap->length, memmap_type(memmap->type));
            memmap++;
        }
    }


    print_error("Missing Kernel Implementation!!!\n");

    //print_warn("System Shutting down now");
    print_warn("System halting now");
    for(int i = 0; i < 5; i++) {
        EFI_TIME Timer;
        gRT->GetTime(&Timer, NULL);
        UINT8 secs = Timer.Second;
        UINT8 secs_old = secs;
        while(secs == secs_old) {
            gRT->GetTime(&Timer, NULL);
            secs = Timer.Second;
        }
        printf_(".");
    }

    //gRT->ResetSystem(EfiResetShutdown, status, 0, NULL);

    // We're done, just hang...
    hcf();
}
