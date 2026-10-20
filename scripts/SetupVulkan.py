import os
import re
import platform
import subprocess
from pathlib import Path

import Utils

class VulkanConfiguration:
    # Oldest SDK that works: 1.3 on Windows; on macOS a 1.4 SDK (MoltenVK + KosmicKrisp, portability extensions).
    minimumVersion = (1, 3)
    windowsInstallVersion = "1.3.296.0"
    vulkanDirectory = "./Hydro/vendor/VulkanSDK"

    @classmethod
    def Validate(cls):
        system = platform.system()
        if system == "Windows":
            return cls.__ValidateWindows()
        if system == "Darwin":
            return cls.__ValidateMac()
        return cls.__ValidateLinux()

    @classmethod
    def __SdkVersion(cls, vulkanSDK):
        match = re.search(r"(\d+)\.(\d+)\.(\d+)", vulkanSDK)
        if match is None:
            return None
        return tuple(int(part) for part in match.groups())

    @classmethod
    def __CheckVersion(cls, vulkanSDK):
        version = cls.__SdkVersion(vulkanSDK)
        if version is None:
            print(f"Could not read the SDK version from {vulkanSDK}; assuming it is recent enough.")
            return True
        if version[:2] < cls.minimumVersion:
            print("You don't have the correct Vulkan SDK version! (Hydro needs {0}.{1} or newer)".format(*cls.minimumVersion))
            return False
        print(f"Vulkan SDK {'.'.join(str(part) for part in version)} located at {vulkanSDK}")
        return True

    # ---- Windows ------------------------------------------------------------------------------
    @classmethod
    def __ValidateWindows(cls):
        vulkanSDK = os.environ.get("VULKAN_SDK")
        if vulkanSDK is None:
            print("\nYou don't have the Vulkan SDK installed!")
            cls.__InstallWindows()
            return False
        if not cls.__CheckVersion(vulkanSDK):
            cls.__InstallWindows()
            return False
        return True

    @classmethod
    def __InstallWindows(cls):
        permissionGranted = False
        while not permissionGranted:
            reply = str(input("Would you like to install VulkanSDK {0:s}? [Y/N]: ".format(cls.windowsInstallVersion))).lower().strip()[:1]
            if reply == 'n':
                return
            permissionGranted = (reply == 'y')

        vulkanInstallURL = f"https://sdk.lunarg.com/sdk/download/{cls.windowsInstallVersion}/windows/VulkanSDK-{cls.windowsInstallVersion}-Installer.exe"
        vulkanPath = f"{cls.vulkanDirectory}/VulkanSDK-{cls.windowsInstallVersion}-Installer.exe"
        print("Downloading {0:s} to {1:s}".format(vulkanInstallURL, vulkanPath))
        Utils.DownloadFile(vulkanInstallURL, vulkanPath)
        print("Running Vulkan SDK installer...")
        os.startfile(os.path.abspath(vulkanPath))
        print("Re-run this script after installation!")
        quit()

    # ---- macOS --------------------------------------------------------------------------------
    @classmethod
    def __ValidateMac(cls):
        ok = True
        if subprocess.call(["xcrun", "--find", "clang++"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) != 0:
            print("\nThe Xcode command line tools are missing. Install them with:  xcode-select --install")
            ok = False

        vulkanSDK = os.environ.get("VULKAN_SDK")
        if vulkanSDK is None:
            print("\nVULKAN_SDK is not set. To install the Vulkan SDK on macOS:")
            print("  1. Download the macOS SDK from https://vulkan.lunarg.com/sdk/home#mac")
            print("  2. Run the installer (the default folder is ~/VulkanSDK/<version>)")
            print("  3. Add this line to ~/.zshrc and open a new terminal:")
            print("       source ~/VulkanSDK/<version>/setup-env.sh")
            print("  4. Run scripts/Setup.sh again")
            return False

        if not cls.__CheckVersion(vulkanSDK):
            return False

        for required in ["include/vulkan/vulkan.h", "include/shaderc/shaderc.hpp", "lib/libvulkan.dylib", "bin/glslc"]:
            if not Path(f"{vulkanSDK}/{required}").exists():
                print(f"Missing {required} in {vulkanSDK}. VULKAN_SDK must point at the SDK's macOS folder (setup-env.sh sets it).")
                ok = False

        return ok

    # ---- Linux (build check only) ---------------------------------------------------------------
    @classmethod
    def __ValidateLinux(cls):
        vulkanSDK = os.environ.get("VULKAN_SDK")
        if vulkanSDK is None:
            if Path("/usr/include/vulkan/vulkan.h").exists():
                print("\nUsing the system Vulkan headers in /usr (VULKAN_SDK is not set).")
                return True
            print("\nInstall the Vulkan SDK (https://vulkan.lunarg.com/sdk/home#linux) and run its setup-env.sh,")
            print("or your distribution's Vulkan, shaderc and glslc development packages.")
            return False
        return cls.__CheckVersion(vulkanSDK)

if __name__ == "__main__":
    VulkanConfiguration.Validate()
