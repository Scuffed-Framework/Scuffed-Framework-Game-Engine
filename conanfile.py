from conan import ConanFile
from conan.tools.cmake import cmake_layout
import platform


class SfEngineConan(ConanFile):
    name = "SF Engine"
    version = "1.0.0"

    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"

    generators = "CMakeDeps", "CMakeToolchain"

    # Package options
    options = {
        "shared": [True, False],
        "fPIC": [True, False]
    }

    default_options = {
        "shared": False,
        "fPIC": True
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def requirements(self):
        # Audio
        self.requires("openal-soft/1.24.3")
        self.requires("opusfile/0.12")

        # Video
        self.requires("dav1d/1.5.4")
        self.requires("libsvtav1/2.2.1")
        self.requires("libvpx/1.16.0")

        # 3D Model Loading
        self.requires("assimp/5.3.1")
        self.requires("openfbx/cci.20210426")
        self.requires("tinyobjloader/2.0.0-rc10")
        self.requires("fastgltf/0.9.1")

        # Formatting and Logging
        self.requires("fmt/10.2.1")
        self.requires("spdlog/1.13.0")

        # Font Rendering
        self.requires("freetype/2.13.2")

        # Window and Input
        self.requires("glfw/3.4")

        # Math Library
        self.requires("glm/cci.20230113")

        # XML Parsing
        self.requires("libxml2/2.12.5")
        self.requires("utfcpp/4.0.1", override=True)

        # Cryptography
        self.requires("libsodium/cci.20220430")

        # Vulkan
        self.requires("spirv-reflect/1.4.313.0")
        self.requires("spirv-cross/1.4.313.0")
        self.requires("vulkan-headers/1.4.313.0")
        self.requires("vulkan-loader/1.4.313.0")
        self.requires("volk/1.4.313.0")
        self.requires("vulkan-memory-allocator/3.0.1")

        # Image Loading
        self.requires("libpng/1.6.42")
        self.requires("libraw/0.21.1")
        self.requires("plutosvg/0.0.8")
        self.requires("tinyexr/1.0.7")
        self.requires("libwebp/1.6.0")

        # Compression
        self.requires("zlib/1.3.1")
        self.requires("miniz/3.0.2", override=True)

        # Platforms
        # self.requires("android-ndk/r29")

    def layout(self):
        cmake_layout(self)
