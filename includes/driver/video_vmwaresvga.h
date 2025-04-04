/**
 * @file video_vmwaresvga.h
 * @brief vmware svga video driver
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef __VIDEO_VMWARESVGA_H
#define __VIDEO_VMWARESVGA_H 0

#include <types.h>
#include <pci.h>
#include <memory.h>

#ifdef __cplusplus
extern "C" {
#endif




#define VMWARE_SVGA2_MAGIC         0x900000UL
#define VMWARE_SVGA2_MAKE_ID(ver)  (VMWARE_SVGA2_MAGIC << 8 | (ver))

#define VMWARE_SVGA2_VERSION_2     2
#define VMWARE_SVGA2_ID_2          VMWARE_SVGA2_MAKE_ID(VMWARE_SVGA2_VERSION_2)

#define VMWARE_SVGA2_VERSION_1     1
#define VMWARE_SVGA2_ID_1          VMWARE_SVGA2_MAKE_ID(VMWARE_SVGA2_VERSION_1)

#define VMWARE_SVGA2_VERSION_0     0
#define VMWARE_SVGA2_ID_0          VMWARE_SVGA2_MAKE_ID(VMWARE_SVGA2_VERSION_0)

#define VMWARE_SVGA2_ID_INVALID    0xFFFFFFFF

#define VMWARE_SVGA2_INDEX_PORT         0x0
#define VMWARE_SVGA2_VALUE_PORT         0x1
#define VMWARE_SVGA2_BIOS_PORT          0x2
#define VMWARE_SVGA2_IRQSTATUS_PORT     0x8


#define VMWARE_SVGA2_MAX_PSEUDOCOLOR_DEPTH      8
#define VMWARE_SVGA2_MAX_PSEUDOCOLORS           (1 << VMWARE_SVGA2_MAX_PSEUDOCOLOR_DEPTH)
#define VMWARE_SVGA2_NUM_PALETTE_REGS           (3 * VMWARE_SVGA2_MAX_PSEUDOCOLORS)

typedef enum vmware_svga2_reg_t {
    VMWARE_SVGA2_REG_ID = 0,
    VMWARE_SVGA2_REG_ENABLE,
    VMWARE_SVGA2_REG_WIDTH,
    VMWARE_SVGA2_REG_HEIGHT,
    VMWARE_SVGA2_REG_MAX_WIDTH,
    VMWARE_SVGA2_REG_MAX_HEIGHT,
    VMWARE_SVGA2_REG_DEPTH,
    VMWARE_SVGA2_REG_BITS_PER_PIXEL,
    VMWARE_SVGA2_REG_PSEUDOCOLOR,
    VMWARE_SVGA2_REG_RED_MASK,
    VMWARE_SVGA2_REG_GREEN_MASK,
    VMWARE_SVGA2_REG_BLUE_MASK,
    VMWARE_SVGA2_REG_BYTES_PER_LINE,
    VMWARE_SVGA2_REG_FB_START,
    VMWARE_SVGA2_REG_FB_OFFSET,
    VMWARE_SVGA2_REG_VRAM_SIZE,
    VMWARE_SVGA2_REG_FB_SIZE,

    VMWARE_SVGA2_REG_CAPABILITIES = 17,
    VMWARE_SVGA2_REG_MEM_START,
    VMWARE_SVGA2_REG_MEM_SIZE,
    VMWARE_SVGA2_REG_CONFIG_DONE,
    VMWARE_SVGA2_REG_SYNC,
    VMWARE_SVGA2_REG_BUSY,
    VMWARE_SVGA2_REG_GUEST_ID,
    VMWARE_SVGA2_REG_CURSOR_ID,
    VMWARE_SVGA2_REG_CURSOR_X,
    VMWARE_SVGA2_REG_CURSOR_Y,
    VMWARE_SVGA2_REG_CURSOR_ON,
    VMWARE_SVGA2_REG_HOST_BITS_PER_PIXEL,
    VMWARE_SVGA2_REG_SCRATCH_SIZE,
    VMWARE_SVGA2_REG_MEM_REGS,
    VMWARE_SVGA2_REG_NUM_DISPLAYS,
    VMWARE_SVGA2_REG_PITCHLOCK,
    VMWARE_SVGA2_REG_IRQMASK,

    VMWARE_SVGA2_REG_GMR_ID = 41,
    VMWARE_SVGA2_REG_GMR_DESCRIPTOR,
    VMWARE_SVGA2_REG_GMR_MAX_IDS,
    VMWARE_SVGA2_REG_GMR_MAX_DESCRIPTOR_LENGTH,
    VMWARE_SVGA2_REG_TRACES,
    VMWARE_SVGA2_REG_GMRS_MAX_PAGES,
    VMWARE_SVGA2_REG_MEMORY_SIZE,
    VMWARE_SVGA2_REG_TOP,

    VMWARE_SVGA2_PALETTE_BASE = 1024,

    VMWARE_SVGA2_REG_SCRATCH_BASE = VMWARE_SVGA2_PALETTE_BASE + VMWARE_SVGA2_NUM_PALETTE_REGS,
} vmware_svga2_reg_t;

typedef enum vmware_svga2_capability_t {
    VMWARE_SVGA2_CAPABILITY_NONE               =0x00000000,
    VMWARE_SVGA2_CAPABILITY_RECT_FILL          =0x00000001,
    VMWARE_SVGA2_CAPABILITY_RECT_COPY          =0x00000002,
    VMWARE_SVGA2_CAPABILITY_CURSOR             =0x00000020,
    VMWARE_SVGA2_CAPABILITY_CURSOR_BYPASS      =0x00000040,
    VMWARE_SVGA2_CAPABILITY_CURSOR_BYPASS_2    =0x00000080,
    VMWARE_SVGA2_CAPABILITY_8BIT_EMULATION     =0x00000100,
    VMWARE_SVGA2_CAPABILITY_ALPHA_CURSOR       =0x00000200,
    VMWARE_SVGA2_CAPABILITY_3D                 =0x00004000,
    VMWARE_SVGA2_CAPABILITY_EXTENDED_FIFO      =0x00008000,
    VMWARE_SVGA2_CAPABILITY_MULTIMON           =0x00010000,
    VMWARE_SVGA2_CAPABILITY_PITCHLOCK          =0x00020000,
    VMWARE_SVGA2_CAPABILITY_IRQMASK            =0x00040000,
    VMWARE_SVGA2_CAPABILITY_DISPLAY_TOPOLOGY   =0x00080000,
    VMWARE_SVGA2_CAPABILITY_GMR                =0x00100000,
    VMWARE_SVGA2_CAPABILITY_TRACES             =0x00200000,
    VMWARE_SVGA2_CAPABILITY_GMR2               =0x00400000,
    VMWARE_SVGA2_CAPABILITY_SCREEN_OBJECT_2    =0x00800000,
} vmware_svga2_capability_t;

typedef enum vmware_svga2_fifo_reg_t {
    VMWARE_SVGA2_FIFO_REG_MIN = 0,
    VMWARE_SVGA2_FIFO_REG_MAX,
    VMWARE_SVGA2_FIFO_REG_NEXT_CMD,
    VMWARE_SVGA2_FIFO_REG_STOP,

    VMWARE_SVGA2_FIFO_REG_CAPABILITIES = 4,
    VMWARE_SVGA2_FIFO_REG_FLAGS,
    VMWARE_SVGA2_FIFO_REG_FENCE,
    VMWARE_SVGA2_FIFO_REG_3D_HWVERSION,
    VMWARE_SVGA2_FIFO_REG_PITCHLOCK,
    VMWARE_SVGA2_FIFO_REG_CURSOR_ON,
    VMWARE_SVGA2_FIFO_REG_CURSOR_X,
    VMWARE_SVGA2_FIFO_REG_CURSOR_Y,
    VMWARE_SVGA2_FIFO_REG_CURSOR_COUNT,
    VMWARE_SVGA2_FIFO_REG_CURSOR_LAST_UPDATED,

    VMWARE_SVGA2_FIFO_REG_RESERVED,

    VMWARE_SVGA2_FIFO_REG_SCREEN_ID,
    VMWARE_SVGA2_FIFO_REG_DEAD,
    VMWARE_SVGA2_FIFO_REG_3D_HWVERSION_REVISED,

    VMWARE_SVGA2_FIFO_REG_3D_CAPS = 32,
    VMWARE_SVGA2_FIFO_REG_3D_CAPS_LAST=32 + 255,

    VMWARE_SVGA2_FIFO_REG_GUEST_3D_HWVERSION,
    VMWARE_SVGA2_FIFO_REG_FENCE_GOAL,
    VMWARE_SVGA2_FIFO_REG_BUSY,

    VMWARE_SVGA2_FIFO_REG_NUM_REGS,
} vmware_svga2_fifo_reg_t;

typedef enum vmware_svga2_irqflag_t {
    VMWARE_SVGA2_IRQFLAG_ANY_FENCE = 0x00000001,
    VMWARE_SVGA2_IRQFLAG_FIFO_PROGRESS = 0x00000002,
    VMWARE_SVGA2_IRQFLAG_FENCE_GOAL = 0x00000004,
} vmware_svga2_irqflag_t;

typedef enum vmware_svga2_cmd_t {
    VMWARE_SVGA2_CMD_INVALID_CMD = 0,
    VMWARE_SVGA2_CMD_UPDATE = 1,
    VMWARE_SVGA2_CMD_RECT_FILL = 2,
    VMWARE_SVGA2_CMD_RECT_COPY = 3,
    VMWARE_SVGA2_CMD_DEFINE_BITMAP = 4,
    VMWARE_SVGA2_CMD_DEFINE_BITMAP_SCANLINE = 5,
    VMWARE_SVGA2_CMD_DEFINE_PIXMAP = 6,
    VMWARE_SVGA2_CMD_DEFINE_PIXMAP_SCANLINE = 7,
    VMWARE_SVGA2_CMD_RECT_BITMAP_FILL = 8,
    VMWARE_SVGA2_CMD_RECT_PIXMAP_FILL = 9,
    VMWARE_SVGA2_CMD_RECT_BITMAP_COPY = 10,
    VMWARE_SVGA2_CMD_RECT_PIXMAP_COPY = 11,
    VMWARE_SVGA2_CMD_FREE_OBJECT = 12,
    VMWARE_SVGA2_CMD_RECT_ROP_FILL = 13,
    VMWARE_SVGA2_CMD_RECT_ROP_COPY = 14,
    VMWARE_SVGA2_CMD_RECT_ROP_BITMAP_FILL = 15,
    VMWARE_SVGA2_CMD_RECT_ROP_PIXMAP_FILL = 16,
    VMWARE_SVGA2_CMD_RECT_ROP_BITMAP_COPY = 17,
    VMWARE_SVGA2_CMD_RECT_ROP_PIXMAP_COPY = 18,
    VMWARE_SVGA2_CMD_DEFINE_CURSOR = 19,
    VMWARE_SVGA2_CMD_DISPLAY_CURSOR = 20,
    VMWARE_SVGA2_CMD_MOVE_CURSOR = 21,
    VMWARE_SVGA2_CMD_DEFINE_ALPHA_CURSOR = 22,
    VMWARE_SVGA2_CMD_DRAW_GLYPH = 23,
    VMWARE_SVGA2_CMD_DRAW_GLYPH_CLIPPED = 24,
    VMWARE_SVGA2_CMD_UPDATE_VERBOSE = 25,
    VMWARE_SVGA2_CMD_SURFACE_FILL = 26,
    VMWARE_SVGA2_CMD_SURFACE_COPY = 27,
    VMWARE_SVGA2_CMD_SURFACE_ALPHA_BLEND = 28,
    VMWARE_SVGA2_CMD_FRONT_ROP_FILL = 29,
    VMWARE_SVGA2_CMD_FENCE = 30,
} vmare_svga2_cmd_t;

typedef struct vmware_svga2_t {
    uint16_t io_bar_addr;
    uint64_t fb_bar_addr_fa;
    uint64_t fb_bar_addr_va;
    uint64_t fb_bar_size;
    uint64_t fb_bar_frm_cnt;
    uint64_t fifo_bar_addr_fa;
    uint64_t fifo_bar_addr_va;
    uint64_t fifo_bar_size;
    uint64_t fifo_bar_frm_cnt;
    uint64_t version_id;
    uint32_t capabilities;
    uint32_t fifo_capabilities;
    uint32_t screen_width;
    uint32_t screen_height;
} vmware_svga2_t;


int8_t vmware_svga2_init(memory_heap_t* heap, const pci_dev_t * dev);

#ifdef __cplusplus
}
#endif


#endif
