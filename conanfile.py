import os

from conan import ConanFile
from conan.tools.build import can_run, check_min_cppstd
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout


class BootpdConan(ConanFile):
    name = "bootpd"
    version = "1.1.0"
    description = "DHCP/BOOTP and TFTP server for network booting (PXE)"
    license = "MIT"
    url = "https://github.com/2bitsin/bootpd"
    package_type = "application"

    settings = "os", "compiler", "build_type", "arch"
    options = {"with_tests": [True, False], "warnings_as_errors": [True, False]}
    default_options = {"with_tests": True, "warnings_as_errors": False}

    exports_sources = "CMakeLists.txt", "LICENSE", "common/*", "bootpd/*", "tests/*", "example/*"

    def validate(self):
        check_min_cppstd(self, 20)

    def build_requirements(self):
        if self.options.with_tests:
            self.test_requires("gtest/1.15.0")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        toolchain = CMakeToolchain(self)
        toolchain.cache_variables["BOOTPD_BUILD_TESTS"] = bool(self.options.with_tests)
        toolchain.cache_variables["BOOTPD_WARNINGS_AS_ERRORS"] = bool(self.options.warnings_as_errors)
        toolchain.generate()
        CMakeDeps(self).generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        if self.options.with_tests and can_run(self):
            os.environ["CTEST_OUTPUT_ON_FAILURE"] = "1"
            cmake.test()

    def package(self):
        CMake(self).install()

    def package_info(self):
        self.cpp_info.bindirs = ["bin"]
        self.cpp_info.libdirs = []
        self.cpp_info.includedirs = []
