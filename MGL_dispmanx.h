//
// Minatsu Game Library with Dispmanx by Minatsu
//
#ifndef __MGL_DISPMANX_H_
#define __MGL_DISPMANX_H_

#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <stdint.h>

#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <sys/time.h>
#include "bcm_host.h"

// Parameters
#define GRP_W (640)
#define GRP_H (200)

// Utility macros
//--------------------------------------------------------------------------------
#define ZEROFILL(var) memset(&(var), 0, sizeof(var))

// VRAM
//--------------------------------------------------------------------------------
uint16_t *vram;

// Prototypes
//--------------------------------------------------------------------------------
int64_t timemillis(void);
void vsync(void);
int MGL_Init();
void draw_and_vsync(void);
int MGL_SDL_Init();
int MGL_Init(void);
void MGL_Quit(void);
int main_loop(void);
void finalize(void);
uint32_t rgb(int r, int g, int b);
uint32_t rgba(int r, int g, int b, int a);
void bgcolor(uint32_t c);

//================================================================================
/////////////////////////////   end header file   ////////////////////////////////
//================================================================================
#endif // __MGL_THREAD_H_
#ifdef MGL_IMPLEMENTATION

#ifndef ALIGN_UP
#define ALIGN_UP(x, y) ((x + (y)-1) & ~((y)-1))
#endif

//================================================================================
// Utilities
//================================================================================

// get current time in milliseconds
int64_t timemillis() {
    struct timeval now;
    gettimeofday(&now, NULL);
    return (now.tv_sec * 1000L) + (now.tv_usec / 1000L);
}

//================================================================================
// dispmanx
//================================================================================
typedef struct {
    DISPMANX_DISPLAY_HANDLE_T display;
    DISPMANX_MODEINFO_T info;
    void *image;
    DISPMANX_UPDATE_HANDLE_T update;
    DISPMANX_RESOURCE_HANDLE_T resource;
    DISPMANX_ELEMENT_HANDLE_T element;
    uint32_t vc_image_ptr;

} RECT_VARS_T;
RECT_VARS_T vars;
uint32_t screen = 0;
VC_RECT_T src_rect;
VC_RECT_T dst_rect;
VC_IMAGE_TYPE_T type = VC_IMAGE_RGB565;

int width = GRP_W, height = GRP_H;
int vram_pitch;
int aligned_height;

//================================================================================
// Renderer
//================================================================================
void dispmanx_vsync_callback(DISPMANX_UPDATE_HANDLE_T u, void *dat) {
    vars.update = vc_dispmanx_update_start(/* priority */ 10);
    assert(vars.update);

    vc_dispmanx_rect_set(&dst_rect, 0, 0, width, height);
    int ret = vc_dispmanx_resource_write_data(vars.resource, type, vram_pitch, vars.image, &dst_rect);
    assert(ret == 0);

    ret = vc_dispmanx_update_submit_sync(vars.update);
    assert(ret == 0);
}

//================================================================================
// Initializer
//================================================================================
int MGL_dispmanx_Init() {
    vram_pitch = ALIGN_UP(width * 2, 32); // bytes of a line
    aligned_height = ALIGN_UP(height, 16);

    int ret;

    // Make VRAM
    int vram_size_n = GRP_W * GRP_H;
    vram = calloc(sizeof(*vram), vram_size_n);
    if (!vram) {
        fprintf(stderr, "MGL: Cannot allocate vram (%dbytes)\n", sizeof(*vram) * vram_size_n);
        return -1;
    }

    // Init Dispmanx
    bcm_host_init();

    // Open
    printf("Dispmanx: Open display[%i]...\n", screen);
    vars.display = vc_dispmanx_display_open(screen);

    // Get info
    ret = vc_dispmanx_display_get_info(vars.display, &vars.info);
    assert(ret == 0);
    printf("Dispmanx: Display is %d x %d\n", vars.info.width, vars.info.height);

    // Create resource
    vars.image = vram;
    vars.resource = vc_dispmanx_resource_create(type, width, height, &vars.vc_image_ptr);
    assert(vars.resource);

    // Write image to the resource
    vc_dispmanx_rect_set(&dst_rect, 0, 0, width, height);
    ret = vc_dispmanx_resource_write_data(vars.resource, type, vram_pitch, vars.image, &dst_rect);
    assert(ret == 0);

    // Start update and get its handle
    vars.update = vc_dispmanx_update_start(/* priority */ 10);
    assert(vars.update);

    // Add element
    // Source Rectangle
    vc_dispmanx_rect_set(&src_rect, 0, 0, width << 16, height << 16);
    // Screen Rectangle
    float scale = (float)(vars.info.height - 0) / (height * 2);
    float dst_height = height * 2 * scale;
    float dst_width = width * scale;

    vc_dispmanx_rect_set(&dst_rect, (vars.info.width - dst_width) / 2, (vars.info.height - dst_height) / 1, dst_width, dst_height);

    printf("Dispmanx: screen=(%d,%d) element=(%d,%d)\n", vars.info.width, vars.info.height, width, height);
    printf("Dispmanx: src=(%d,%d)[%d,%d]\n", src_rect.x, src_rect.y, src_rect.width >> 16, src_rect.height >> 16);
    printf("Dispmanx: dst=(%d,%d)[%d,%d]\n", dst_rect.x, dst_rect.y, dst_rect.width, dst_rect.height);

    VC_DISPMANX_ALPHA_T alpha = {DISPMANX_FLAGS_ALPHA_FROM_SOURCE | DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS, /*alpha 0->255*/ 255, 0};

    vars.element = vc_dispmanx_element_add(vars.update, vars.display,
                                           2000, // layer
                                           &dst_rect, vars.resource, &src_rect, DISPMANX_PROTECTION_NONE, &alpha,
                                           NULL, // clamp
                                           VC_IMAGE_ROT0);
    // Update
    ret = vc_dispmanx_update_submit_sync(vars.update);
    assert(ret == 0);

    // Set callback
    ret = vc_dispmanx_vsync_callback(vars.display, dispmanx_vsync_callback, NULL);
    assert(ret == 0);

    return 0;
}

//================================================================================
// MGL
//================================================================================
int MGL_Init() {
    // dispmanx
    if (MGL_dispmanx_Init() < 0) {
        perror("MGL: Failed to init dispmanx.");
        return -1;
    }

    return 0;
}

//--------------------------------------------------------------------------------
// Signal handler
//--------------------------------------------------------------------------------
void sigintHandler(int sig) {
    finalize();
}

//--------------------------------------------------------------------------------
// Finalizer
//--------------------------------------------------------------------------------
void MGL_Quit() {
    int ret;
    ret = vc_dispmanx_vsync_callback(vars.display, NULL, NULL);
    assert(ret == 0);

    vars.update = vc_dispmanx_update_start(10);
    assert(vars.update);
    ret = vc_dispmanx_element_remove(vars.update, vars.element);
    assert(ret == 0);
    ret = vc_dispmanx_update_submit_sync(vars.update);
    assert(ret == 0);
    ret = vc_dispmanx_resource_delete(vars.resource);
    assert(ret == 0);
    ret = vc_dispmanx_display_close(vars.display);
    assert(ret == 0);

    // Release vram
    if (vram) {
        free(vram);
    }

    puts("MGL: Quit");
    exit(0);
}

//--------------------------------------------------------------------------------
// Startup
//--------------------------------------------------------------------------------
int MGL_Start() {
    // Adding signal handler
    puts("MGL: Adding signal handler.");
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigintHandler;
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        perror("MGL: Failed to add signal handler.");
        return 1;
    }

    if (MGL_Init() < 0) {
        perror("MGL: Failed to initialize.");
        exit(1);
    }

    puts("MGL: Initialized.\n");
    return 0;
}

#endif // __MGL_DISPMANX_H_
