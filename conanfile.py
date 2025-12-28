# This file is part of the UCB project
# SPDX-FileCopyrightText: 2025 Åke Svedin <ake@svedin.org>
# SPDX-License-Identifier: MIT

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout
from conan.tools.build import can_run
from conan.tools.scm import Git


class UcbConan(ConanFile):
    name = "ucb"
    version = "0.1.0"
    languages = "C"

    description = "A cross-platform C library for essentials"
    license = "MIT"
    author = "Åke Svedin - <ake@svedin.org>"
    homepage = "https://github.com/herring-swe/ucb"
    url = "https://github.com/herring-swe/ucb"
    topics = (
        "c",
        "cross-platform",
        "essentials",
        "beaver",
        "library",
    )  # optional topic

    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_beaver": [True, False],  # Yes, we want beaver mode
        "with_utf8": [True, False],
        "with_iconv": [None, "iconv"],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_beaver": False,  # But maybe not by default :'(
        "with_utf8": True,
        "with_iconv": None,
    }
    option_description = {
        "with_beaver": "Beaver mode or serious mode - you decide",
        "with_utf8": "With UTF-8 library",
        "with_iconv": "Use iconv for better encoding support",
    }

    exports_sources = (
        "CMakeLists.txt",
        "cmake/*",
        "src/*",
        "include/*",
        "tests/*",
        "LICENSE",
        "README.md",
    )

    # def configure(self):
    #     # We don't need C++, verify if lanuage setting do this for us
    #     self.settings.rm_safe("compiler.libcxx")
    #     self.settings.rm_safe("compiler.cppstd")

    def config_options(self):
        if self.settings.os == "Windows":
            self.options.rm_safe("fPIC")
        if not self.options["utf8"]:
            self.options.rm_safe("with_iconv")

    def requirements(self):
        if self.options.get_safe("with_iconv"):
            self.requires("libiconv/[^1.18]")

    def build_requirements(self):
        self.tool_requires("cmake/[^3.28]")
        self.tool_requires("ninja/[^1.13]")
        self.test_requires("doctest/[^2.4.12]")

    def source(self):
        git = Git(self)
        git.clone(self.url, target=".")
        # git.checkout(f"v{self.version}")

    def layout(self):
        # Should be default
        self.folders.build_folder_vars = ["settings.os", "settings.compiler", "settings.compiler.version", "settings.arch", "settings.build_type"] 
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self, generator="Ninja")
        # tc.variables["UCB_CLANG_TIDY"] = False
        tc.variables["UCB_BEAVER_MODE"] = self.options.with_beaver
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        if can_run(self):
            cmake.test()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        # FIXME
        pass
