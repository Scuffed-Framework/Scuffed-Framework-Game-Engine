from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout
from conan.tools.files import get


class LibMatroskaConan(ConanFile):
    name = "libmatroska"
    version = "1.7.1"

    license = "LGPL-2.1-or-later"
    url = "https://github.com/Matroska-Org/libmatroska"
    description = "C++ library to parse and create Matroska files"

    package_type = "library"

    settings = "os", "compiler", "build_type", "arch"

    options = {
        "shared": [True, False]
    }

    default_options = {
        "shared": True
    }

    requires = (
        "libebml/1.4.7",
        "utf8cpp/4.0.5",
    )

    def layout(self):
        cmake_layout(self)

    def source(self):
        get(
            self,
            "https://github.com/Matroska-Org/libmatroska/archive/refs/tags/release-1.7.1.tar.gz",
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
        self.cpp_info.libs = ["matroska"]

        self.cpp_info.set_property(
            "cmake_file_name",
            "Matroska"
        )

        self.cpp_info.set_property(
            "cmake_target_name",
            "Matroska::Matroska"
        )