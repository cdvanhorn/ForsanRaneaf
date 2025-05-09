#include "utilities/ini_config.h"
#include "utilities/logger.h"

#include <stddef.h>

/*static void config_game(struct game_cfg *cfg)
{
	inicfg_getstring("game", "core", &(cfg->core));
	inicfg_getstring("input", "core", &(cfg->input_core));
	inicfg_getuint8_t("input", "cps", &(cfg->input_cps));
	inicfg_getstring("render", "core", &(cfg->render_core));
	inicfg_getuint8_t("render", "fps", &(cfg->render_fps));
	inicfg_getstring("render", "renderer", &(cfg->render_renderer));
	inicfg_getstring("audio", "core", &(cfg->audio_core));
	inicfg_getuint8_t("audio", "cps", &(cfg->audio_cps));
	inicfg_getuint8_t("simulation", "cps", &(cfg->sim_cps));
}*/

int main(int argc, char *argv[])
{
	if (log_open(LOG_DEBUG) > 0) {
		return 1;
	}
	log_write(LOG_TAG_INFO, "logger initialized");

	inicfg_open();

	inicfg_close();

	log_close();
	log_write(LOG_TAG_INFO, "logger destroyed");
	return 0;
}
