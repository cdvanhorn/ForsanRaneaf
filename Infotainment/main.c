#include "utilities/ini_config.h"
#include "utilities/logger.h"
#include "frinfo/frinfo.h"

static void config_frinfo(struct frinfo_config *cfg) {
#ifdef RPI
	inicfg_getstring("serial", "pipath", &(cfg->serial_path));
	inicfg_getint("ui", "pi_window_width", &(cfg->window_width));
	inicfg_getint("ui", "pi_window_height", &(cfg->window_height));
	inicfg_getint("ui", "pi_rotation", &(cfg->window_rotation));
#else
	inicfg_getstring("serial", "path", &(cfg->serial_path));
	inicfg_getint("ui", "window_width", &(cfg->window_width));
	inicfg_getint("ui", "window_height", &(cfg->window_height));
	inicfg_getint("ui", "rotation", &(cfg->window_rotation));
#endif
	inicfg_getint("serial", "baud", &(cfg->serial_baud_rate));
	inicfg_getint("serial", "buffer_size", &(cfg->serial_buffer_size));
}

int main(int argc, char *argv[])
{
	if (log_open(ZT_LOG_DEBUG) > 0) {
		return 1;
	}
	log_write(LOG_TAG_INFO, "logger initialized");

	inicfg_open();

	struct frinfo_config frinfocfg;
	config_frinfo(&frinfocfg);
	frinfo_start(&frinfocfg);

	inicfg_close();

	log_close();
	log_write(LOG_TAG_INFO, "logger destroyed");
	return 0;
}
