#include "utilities/ini_config.h"
#include "utilities/logger.h"

#include <stddef.h>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

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
	// if (log_open(LOG_DEBUG) > 0) {
	// 	return 1;
	// }
	// log_write(LOG_TAG_INFO, "logger initialized");
	//
	// inicfg_open();
	//
	// inicfg_close();
	//
	// log_close();
	// log_write(LOG_TAG_INFO, "logger destroyed");
	// return 0;

	int fd;
	char buffer[256];
	struct termios tty;

	// Open the serial port
	fd = open("/dev/ttyAMA0", O_RDWR | O_NOCTTY);
	if (fd < 0) {
		perror("Error opening serial port");
		return 1;
	}

	// Get the current serial port settings
	if (tcgetattr(fd, &tty) != 0) {
		perror("Error getting serial port settings");
		close(fd);
		return 1;
	}

	// Configure the serial port
	tty.c_cflag = B9600 | CS8 | CLOCAL | CREAD; // Set baud rate, data bits, and enable receiver
	tty.c_iflag = IGNPAR; // Ignore parity errors
	tty.c_oflag = 0; // Raw output
	tty.c_lflag = 0; // Non-canonical mode

	// Apply the new settings
	if (tcsetattr(fd, TCSANOW, &tty) != 0) {
		perror("Error setting serial port settings");
		close(fd);
		return 1;
	}

	// Read data from the serial port
	ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
	if (bytes_read < 0) {
		perror("Error reading from serial port");
		close(fd);
		return 1;
	}

	// Print the received data
	printf("Received data: %.*s\n", (int)bytes_read, buffer);

	// Close the serial port
	close(fd);
	return 0;
}
