from conan import ConanFile
from conan.tools.files import get, copy
import os


class Utf8CppConan(ConanFile):
    name = "utf8cpp"
    version = "4.0.5"

    package_type = "header-library"

    license = "BSL-1.0"
    url = "https://github.com/nemtrif/utfcpp"

    settings = "os", "arch", "compiler", "build_type"

    def source(self):
        get(
            self,
            "https://github.com/nemtrif/utfcpp/archive/refs/tags/v4.0.5.tar.gz",
            strip_root=True
        )

    def package(self):
        copy(
            self,
            "*.h",
            src=os.path.join(self.source_folder, "source"),
            dst=os.path.join(self.package_folder, "include")
        )

    def package_info(self):
        self.cpp_info.includedirs = ["include"]