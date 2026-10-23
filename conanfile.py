from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain, cmake_layout


class CarWearConan(ConanFile):
    name = "carwear"
    version = "0.1.0"
    settings = "os", "compiler", "build_type", "arch"

    options = {"with_tests": [True, False]}
    default_options = {"with_tests": True}

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        self.requires("tomlplusplus/3.4.0")
        if self.options.with_tests:
            self.requires("gtest/1.18.0")

    def generate(self):
        toolchain = CMakeToolchain(self)
        toolchain.variables["CARWEAR_BUILD_TESTS"] = bool(self.options.with_tests)
        toolchain.generate()
        CMakeDeps(self).generate()
