import os
import subprocess
import platform

from SetupPython import PythonConfiguration as PythonRequirements

# Make sure everything we need for the setup is installed
PythonRequirements.Validate()

from SetupPremake import PremakeConfiguration as PremakeRequirements
from SetupVulkan import VulkanConfiguration as VulkanRequirements
os.chdir('./../') # Change from devtools/scripts directory to root

premakeInstalled = PremakeRequirements.Validate()
vulkanInstalled = VulkanRequirements.Validate()

print("\nUpdating submodules...")
subprocess.call(["git", "submodule", "update", "--init", "--recursive"])

if not premakeInstalled:
    print("Hydro requires Premake to generate project files.")
elif not vulkanInstalled:
    print("\nInstall the Vulkan SDK (see above), then run the setup again.")
else:
    system = platform.system()
    print("\nRunning premake...")
    if system == "Windows":
        subprocess.call([os.path.abspath("./scripts/Win-GenProjects.bat"), "nopause"])
    elif system == "Darwin":
        subprocess.call(["bash", os.path.abspath("./scripts/Mac-GenProjects.sh")])
    else:
        subprocess.call(["bash", os.path.abspath("./scripts/Linux-GenProjects.sh")])

    print("\nSetup completed!")
