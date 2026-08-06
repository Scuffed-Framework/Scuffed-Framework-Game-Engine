from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout
from conan.tools.files import get


class LibEbmlConan(ConanFile):
    name = "libebml"
    version = "1.4.7"

    license = "LGPL-2.1-or-later"
    url = "https://github.com/Matroska-Org/libebml"
    description = "Extensible Binary Meta Language library"

    package_type = "library"

    settings = "os", "compiler", "build_type", "arch"

    options = {
        "shared": [True, False]
    }

    default_options = {
        "shared": False
    }

    def layout(self):
        cmake_layout(self)

    def source(self):
        get(
            self,
            "https://github.com/Matroska-Org/libebml/archive/refs/tags/release-1.4.7.tar.gz",
            strip_root=True
        )

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_SHARED_LIBS"] = self.options.shared
        tc.variables["CMAKE_POLICY_VERSION_MINIMUM"] = "3.5"
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure(
            build_script_folder=".",
            variables={
                "CMAKE_POLICY_VERSION_MINIMUM": "3.5"
            }
        )
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["ebml"]

        self.cpp_info.set_property(
            "cmake_file_name",
            "EBML"
        )

        self.cpp_info.set_property(
            "cmake_target_name",
            "EBML::ebml"
        )