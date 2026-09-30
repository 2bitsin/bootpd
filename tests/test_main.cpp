#include <cstdlib>
#include <string_view>

#include <gtest/gtest.h>

#include <common/logger.hpp>

int main(int argc, char** argv)
{
	::testing::InitGoogleTest(&argc, argv);

	// Many tests provoke errors on purpose; keep the output readable unless
	// BOOTPD_TEST_LOG=1 asks for the full server log.
	const char* verbose = std::getenv("BOOTPD_TEST_LOG");
	Glog.level(verbose && std::string_view(verbose) == "1"
		? basic_logger::filter_level::trace
		: basic_logger::filter_level::fatal);

	return RUN_ALL_TESTS();
}
