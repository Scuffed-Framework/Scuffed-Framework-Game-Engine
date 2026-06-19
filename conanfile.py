from conan import ConanFile
from conan.tools.cmake import cmake_layout

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
        self.requires("openal-soft/1.23.1")
        
        # 3D Model Loading
        self.requires("assimp/5.3.1")
        
        # Formatting and Logging
        self.requires("fmt/10.2.1")
        self.requires("spdlog/1.13.0")
        
        # OpenCL
        self.requires("opencl-headers/2023.12.14")
        self.requires("opencl-icd-loader/2023.12.14")
        
        # Font Rendering
        self.requires("freetype/2.13.2")
        
        # Window and Input
        self.requires("glfw/3.4")
        
        # Math Library
        self.requires("glm/cci.20230113")
        
        # ImGui Extensions
        self.requires("implot/0.16")
        self.requires("imguizmo/1.83")
        
        # XML Parsing
        self.requires("libxml2/2.12.5")
        self.requires("utfcpp/4.0.1", override=True)
        
        # Cryptography
        self.requires("libsodium/cci.20220430")

        # Vulkan
        self.requires("spirv-reflect/1.4.313.0")
        self.requires("spirv-cross/1.4.313.0")
        self.requires("glslang/1.4.313.0")
        self.requires("vulkan-headers/1.4.313.0")
        self.requires("vulkan-loader/1.4.313.0")
        self.requires("volk/1.4.313.0")
        self.requires("vulkan-memory-allocator/3.0.1")  # this one is independent, leave as-is
        
        # Image Loading
        self.requires("libpng/1.6.42")
        self.requires("libraw/0.21.1")
        
        # Compression
        self.requires("zlib/1.3.1")

        # Scripting
        self.requires("sol2/3.3.0")

        # Physics
        self.requires("bullet3/3.25")
    
    def layout(self):
        cmake_layout(self)