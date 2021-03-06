#ifdef HAVE_CONFIG_H
#include "config.h"
#include "config_unix.h"
#include "config_win32.h"
#endif // HAVE_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

struct module;
struct video_frame;

void *recompress_init(struct module *parent, const char *host, const char *compress,
                unsigned short rx_port, unsigned short tx_port, int mtu, char *fec,
                long long bitrate);
void recompress_assign_ssrc(void *state, uint32_t ssrc);
void recompress_done(void *state);
uint32_t recompress_get_ssrc(void *state);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <memory>
#include <string>
void recompress_process_async(void *state, std::shared_ptr<video_frame> frame);
#endif
