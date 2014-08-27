/*
 * This file contains common external definitions
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#include "config_unix.h"
#include "config_win32.h"
#endif

#include "host.h"

#include "video_capture.h"
#include "video_compress.h"
#include <iostream>

using namespace std;

unsigned int cuda_device = 0;
unsigned int audio_capture_channels = 1;

unsigned int cuda_devices[MAX_CUDA_DEVICES] = { 0 };
unsigned int cuda_devices_count = 1;

int audio_init_state_ok;

uint32_t RTT = 0;               /*  this is computed by handle_rr in rtp_callback */

int uv_argc;
char **uv_argv;

char *export_dir = NULL;
volatile bool should_exit_receiver = false;

bool verbose = false;

bool ldgm_device_gpu = false;

const char *window_title = NULL;

int rxtx_mode; // MODE_SENDER, MODE_RECEIVER or both

void print_capabilities(void)
{
        cout << "Compressions:" << endl;
        auto const & compress_capabilities = get_compress_capabilities();
        for (auto const & it : compress_capabilities) {
                cout << "(" << get<0>(it) << ";" << get<1>(it) << ";" << get<2>(it) <<")\n";
        }
        cout << "Capturers:" << endl;
        print_available_capturers();
}
