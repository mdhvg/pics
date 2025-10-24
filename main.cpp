#include "Debug.h"
#include "Application.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include <spdlog/spdlog.h>

int main(int, char **) {
	spdlog::set_default_logger(spdlog::stdout_color_mt("console"));

	Application &app = Application::get_instance();
	app.start();
	return 0;
}
