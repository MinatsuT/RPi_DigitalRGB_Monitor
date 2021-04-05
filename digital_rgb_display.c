//
// Digital RGB Display with fx2pipe
// 27-Mar-2021 by Minatsu (@tksm372)
//

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define DW 640
#define DH 200
#define MGL_IMPLEMENTATION
#include "MGL_dispmanx.h"

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <libusb.h>
#include <assert.h>

#define BIT_VSYNC 4
#define BIT_HSYNC 3
#define BIT_R 2
#define BIT_G 1
#define BIT_B 0

// Built-in firmware hex strings.
static char *firmware[] = {
#include "firmware/slave_sync_8.inc"
    NULL};

#define READ() usb_read()

//======================================================================
// USB
//======================================================================
#define VID 0x04b4
#define PID 0x8613
#define IN_EP (LIBUSB_ENDPOINT_IN | 6)
#define RX_SIZE (16 * 1024 * 4)
#define XFR_NUM 64
#define READ_SIZE (RX_SIZE * XFR_NUM)
static uint8_t buf[XFR_NUM][RX_SIZE];
static libusb_device_handle *usb_handle = NULL;
static struct libusb_transfer *xfr[XFR_NUM];
static volatile int usb_run_flag = 1;

//----------------------------------------------------------------------
// USB write RAM
//----------------------------------------------------------------------
#define USB_WRITE_RAM_MAX_SIZE 64
int usb_write_ram(int addr, uint8_t *dat, int size) {
    assert(usb_handle != NULL);

    for (int i = 0; i < size; i += USB_WRITE_RAM_MAX_SIZE) {
        int len = (size - i > USB_WRITE_RAM_MAX_SIZE) ? USB_WRITE_RAM_MAX_SIZE : size - i;
        int ret = libusb_control_transfer(usb_handle, LIBUSB_REQUEST_TYPE_VENDOR, 0xa0, addr + i, 0, dat + i, len, 1000);
        if (ret < 0) {
            fprintf(stderr, "USB: Write Ram at %04x (len %d) failed.\n", addr + i, len);
            return -1;
        }
    }
    return 0;
}

//----------------------------------------------------------------------
// USB load firmware
//----------------------------------------------------------------------
#define FIRMWARE_MAX_SIZE_PER_LINE 64
static uint8_t firmware_dat[FIRMWARE_MAX_SIZE_PER_LINE];
int usb_load_firmware(char *firmware[]) {
    int ret;

    // Take the CPU into RESET
    uint8_t dat = 1;
    ret = usb_write_ram(0xe600, &dat, sizeof(dat));
    if (ret < 0) {
        return -1;
    }

    // Load firmware
    int size, addr, record_type, tmp_dat;
    for (int i = 0; firmware[i] != NULL; i++) {
        char *p = firmware[i] + 1;

        // Extract size
        ret = sscanf(p, "%2x", &size);
        assert(ret != 0);
        assert(size <= FIRMWARE_MAX_SIZE_PER_LINE);
        p += 2;

        // Extract addr
        ret = sscanf(p, "%4x", &addr);
        assert(ret != 0);
        p += 4;

        // Extract record type
        ret = sscanf(p, "%2x", &record_type);
        assert(ret != 0);
        p += 2;

        // Write program to EZ-USB's RAM (record_type==0).
        if (record_type == 0) {
            for (int j = 0; j < size; j++) {
                ret = sscanf(p, "%2x", &tmp_dat);
                firmware_dat[j] = tmp_dat & 0xff;
                assert(ret != 0);
                p += 2;
            }

            ret = usb_write_ram(addr, firmware_dat, size);
            if (ret < 0) {
                return -1;
            }
        }
    }

    // Take the CPU out of RESET (run)
    dat = 0;
    ret = usb_write_ram(0xe600, &dat, sizeof(dat));
    if (ret < 0) {
        return -1;
    }

    return 0;
}

//----------------------------------------------------------------------
// Close USB
//----------------------------------------------------------------------
void usb_close() {
    for (int i = 0; i < XFR_NUM; i++) {
        if (xfr[i] != NULL) {
            libusb_cancel_transfer(xfr[i]);
        }
    }

    // Stop USB thread
    usb_run_flag = 0;

    if (usb_handle != NULL) {
        libusb_close(usb_handle);
        usb_handle = NULL;
    }

    for (int i = 0; i < XFR_NUM; i++) {
        if (xfr[i] != NULL) {
            libusb_free_transfer(xfr[i]);
        }
    }
}

//----------------------------------------------------------------------
// USB callback for bulk-in transfer
//----------------------------------------------------------------------
static pthread_cond_t usb_cond;
static pthread_mutex_t usb_mtx;
static volatile uint64_t usb_received_size = 0;
static pthread_mutex_t usb_received_size_mtx;
static volatile int usb_trans_pos = 0;
static int usb_closed_flag = 0;
void usb_callback(struct libusb_transfer *xfr) {
    switch (xfr->status) {
    case LIBUSB_TRANSFER_COMPLETED:
        pthread_mutex_lock(&usb_received_size_mtx);
        usb_received_size += xfr->actual_length;
        pthread_mutex_unlock(&usb_received_size_mtx);
        break;
    case LIBUSB_TRANSFER_ERROR:
        fprintf(stderr, "USB: transfer error.\n");
        break;
    case LIBUSB_TRANSFER_TIMED_OUT:
        fprintf(stderr, "USB: transfer timed out.\n");
        break;
    case LIBUSB_TRANSFER_OVERFLOW:
        fprintf(stderr, "USB: transfer overflow.\n");
        break;
    case LIBUSB_TRANSFER_CANCELLED:
    case LIBUSB_TRANSFER_NO_DEVICE:
    default:
        return;
    }
    usb_trans_pos = RX_SIZE * (int)xfr->user_data;
    pthread_cond_signal(&usb_cond);

    if (usb_run_flag) {
        if (libusb_submit_transfer(xfr) < 0) {
            fprintf(stderr, "USB: libusb_submit_transfer failed.\n");
            MGL_Quit();
        }
    }
}

//----------------------------------------------------------------------
// USB thread for bulk-in transfer
//----------------------------------------------------------------------
static pthread_t usb_th;
struct timeval tv = {0, 1};
void *usb_run(void *arg) {
    puts("USB: Start receiving VH-RGB signals.");

    // Submit USB transfers
    for (int i = 0; i < XFR_NUM; i++) {
        libusb_fill_bulk_transfer(xfr[i], usb_handle,
                                  IN_EP, // Endpoint ID
                                  buf[i], RX_SIZE, usb_callback, (void *)i, 0 /* no timeout */);
        if (libusb_submit_transfer(xfr[i]) < 0) {
            fprintf(stderr, "USB: libusb_submit_transfer failed.\n");
            MGL_Quit();
        }
    }

    // Waiting transfer completion repeatedly
    int64_t last = timemillis();
    int64_t cur;
    int64_t msec;
    float avg = 0;
    while (usb_run_flag) {
        libusb_handle_events_completed(NULL, &usb_closed_flag);
        cur = timemillis();
        msec = cur - last;
        if (msec > 1000) {
            pthread_mutex_lock(&usb_received_size_mtx);
            int size = usb_received_size;
            usb_received_size = 0;
            pthread_mutex_unlock(&usb_received_size_mtx);

            float mbps = size / ((cur - last) / 1000.0) / 1024.0 / 1024.0;
            avg = (!avg) ? mbps : avg * 0.95 + mbps * 0.05;
            printf("Receiving at %.3f MBps (Avg. %.3f Mbps)\r", mbps, avg);
            last = cur;
        }
    }

    puts("USB: Thread finished.");
}

//----------------------------------------------------------------------
// Read one "000VHRGB" signal byte via USB
//----------------------------------------------------------------------
int read_pos = 0;
inline static uint8_t usb_read() {
    while (read_pos == usb_trans_pos) {
        pthread_cond_wait(&usb_cond, &usb_mtx);
    }
    uint8_t dat = buf[0][read_pos++];
    read_pos %= READ_SIZE;
    return dat;
}

//======================================================================
// Main
//======================================================================
int main(int argc, char *argv[]) {
    setvbuf(stdout, (char *)NULL, _IONBF, 0);
    int ret;

    // Initialize USB
    ret = libusb_init(NULL);
    assert(ret == 0);
    usb_handle = libusb_open_device_with_vid_pid(NULL, VID, PID);
    assert(usb_handle != NULL);
    ret = libusb_set_auto_detach_kernel_driver(usb_handle, 1);
    assert(ret == 0);
    ret = libusb_claim_interface(usb_handle, 0);
    assert(ret == 0);
    ret = libusb_set_interface_alt_setting(usb_handle, 0, 1);
    assert(ret == 0);

    // load firmware
    printf("Main: Firmware download...");
    if (usb_load_firmware(firmware) >= 0) {
        puts("finished.");
    } else {
        puts("failed.");
        return -1;
    }

    // Allocating transfer request structures
    ZEROFILL(xfr);
    for (int i = 0; i < XFR_NUM; i++) {
        xfr[i] = libusb_alloc_transfer(0);
        if (xfr[i] == NULL) {
            usb_close();
            return -1;
        }
    }

    if (pthread_create(&usb_th, NULL, usb_run, NULL) != 0) {
        perror("Main: Failed to start USB thread");
        return -1;
    }

    // setvbuf(fp, buf, _IOFBF, 10240);
    MGL_Start();

    uint32_t d = 0;
    uint32_t vmask = 1 << BIT_VSYNC;
    uint32_t hmask = 1 << BIT_HSYNC;
    uint32_t vhmask = vmask | hmask;
    col_t col[8] = {0, WEB_RGB(0, 0, 5), WEB_RGB(5, 0, 0), WEB_RGB(5, 0, 5), WEB_RGB(0, 5, 0), WEB_RGB(0, 5, 5), WEB_RGB(5, 5, 0), WEB_RGB(5, 5, 5)};
    col_t *p;
    while (1) {
        // Wait V-Sync
        while (READ() & vmask)
            ; // wait untill low
        while (!(READ() & vmask))
            ; // wait untill hi

        // Skip V-Sync back porch
        for (int i = 0; i < 36; i++) {
            while (READ() & hmask)
                ; // wait untill low
            while (!(READ() & hmask))
                ; // wait untill hi
        }

        int x, y;
        for (y = 0; y < DH; y++) {
            // Wait H-Sync
            while (READ() & hmask)
                ; // wait untill low
            while (!(READ() & hmask))
                ; // wait untill hi

            // Skip H-Sync back porch
            for (x = 0; x < 132 - 1; x++) {
                READ();
            }

            p = &vram[y * GRP_W];
            for (x = 0; x < DW; x++) {
                d = READ();
                if ((~d) & vhmask) {
                    y = DH; // Sync is lost, skip this frame
                    break;
                }
                *p++ = col[d & 7];
            }
        }
    }
}

void finalize() {
    puts("\nMain: Finalizing...");
    usb_close();
    puts("Main: USB device closed.");

    usb_closed_flag = 1;

    if (pthread_join(usb_th, NULL) != 0) {
        perror("Main: Failed to join USB thread.");
    } else {
        puts("Main: USB thread joined.");
    }

    libusb_exit(NULL);

    MGL_Quit();
}
