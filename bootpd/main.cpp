#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

#include <common/arguments.hpp>
#include <common/async.hpp>
#include <common/config_ini.hpp>
#include <common/control_c.hpp>
#include <common/logger.hpp>

#include "dhcp_server_v4.hpp"
#include "tftp_server_v4.hpp"

#ifndef BOOTPD_VERSION
	#define BOOTPD_VERSION "dev"
#endif

namespace
{
	constexpr std::string_view usage_text = R"(bootpd - DHCP/BOOTP and TFTP server for network booting

Usage:
  bootpd [options]

Options:
  -C <file>         Configuration file (default: config.ini)
  -O "<key=value>"  Override a global configuration value, may be repeated
  -v, --verbose     Debug log output (-vv for trace output)
  -q, --quiet       Only log warnings and errors
  -h, --help        Show this help and exit
  -V, --version     Show the version and exit

Press Ctrl+C to stop the server.
See README.md for the configuration file reference.
)";

	auto load_config(std::filesystem::path const& config_path) -> config_ini
	{
		using namespace std::filesystem;
		if (!exists(config_path))
			throw std::runtime_error("Unable to find configuration file : " + config_path.string());
		if (!is_regular_file(config_path))
			throw std::runtime_error("Not a regular file : " + config_path.string());
		std::ifstream stream_v{ config_path };
		if (!stream_v)
			throw std::runtime_error("Unable to open configuration file : " + config_path.string());
		return config_ini(stream_v);
	}
}

int main(int argc, char** argv)
{
	using namespace std::string_view_literals;
	using level = basic_logger::filter_level;

	try
	{
		const arguments args(argc, argv);

		if (args.has("-h"sv, "--help"sv, "/?"sv)) {
			std::cout << usage_text;
			return 0;
		}
		if (args.has("-V"sv, "--version"sv)) {
			std::cout << "bootpd " << BOOTPD_VERSION << "\n";
			return 0;
		}

		if (args.has("-q"sv, "--quiet"sv))
			Glog.level(level::warning);
		if (args.has("-v"sv, "--verbose"sv))
			Glog.level(level::debug);
		if (args.has("-vv"sv))
			Glog.level(level::trace);

		const std::filesystem::path config_path(args.value_or("-C"sv, "config.ini"sv));
		auto config_v = load_config(config_path);

		for (auto line : args.values("-O"sv)) {
			Glog.debug("Overriding configuration : '{}'", line);
			config_v.insert_line(line);
		}

		const auto enabled = [&config_v](std::string_view key) {
			return config_v.value_or(key, true);
		};

		std::optional<dhcp_server_v4> dhcp_server_v;
		std::optional<tftp_server_v4> tftp_server_v;
		if (enabled("dhcp_enabled"sv))
			dhcp_server_v.emplace(config_v).bind();
		if (enabled("tftp_enabled"sv))
			tftp_server_v.emplace(config_v).bind();
		if (!dhcp_server_v && !tftp_server_v)
			throw std::runtime_error("Both the DHCP and the TFTP server are disabled, nothing to do.");

		async::io_context context_v;
		if (dhcp_server_v)
			context_v.spawn(dhcp_server_v->run(context_v));
		if (tftp_server_v)
			context_v.spawn(tftp_server_v->run(context_v));

		control_c::install();
		context_v.run([] { return control_c::stop_requested(); });

		Glog.info("Stopped.");
		return 0;
	}
	catch (std::exception const& ex)
	{
		Glog.fatal("{}", ex.what());
	}
	catch (...)
	{
		Glog.fatal("Unknown unhandled exception");
	}
	return 1;
}
