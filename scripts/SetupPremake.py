import os
import stat
import platform
from pathlib import Path

import Utils

class PremakeConfiguration:
    premakeVersion = "5.0.0-beta7"
    premakeLicenseUrl = "https://raw.githubusercontent.com/premake/premake-core/master/LICENSE.txt"
    premakeDirectory = "./vendor/premake/bin"

    # Release archive and executable name per host OS.
    @classmethod
    def __Archive(cls):
        system = platform.system()
        if system == "Windows":
            return f"premake-{cls.premakeVersion}-windows.zip", "premake5.exe"
        if system == "Darwin":
            return f"premake-{cls.premakeVersion}-macosx.tar.gz", "premake5"
        return f"premake-{cls.premakeVersion}-linux.tar.gz", "premake5"

    @classmethod
    def Validate(cls):
        if (not cls.CheckIfPremakeInstalled()):
            print("Premake is not installed.")
            return False

        print(f"Correct Premake located at {os.path.abspath(cls.premakeDirectory)}")
        return True

    @classmethod
    def CheckIfPremakeInstalled(cls):
        _, executable = cls.__Archive()
        if (not Path(f"{cls.premakeDirectory}/{executable}").exists()):
            return cls.InstallPremake()

        return True

    @classmethod
    def InstallPremake(cls):
        permissionGranted = False
        while not permissionGranted:
            reply = str(input("Premake not found. Would you like to download Premake {0:s}? [Y/N]: ".format(cls.premakeVersion))).lower().strip()[:1]
            if reply == 'n':
                return False
            permissionGranted = (reply == 'y')

        archive, executable = cls.__Archive()
        url = f"https://github.com/premake/premake-core/releases/download/v{cls.premakeVersion}/{archive}"
        archivePath = f"{cls.premakeDirectory}/{archive}"
        print("Downloading {0:s} to {1:s}".format(url, archivePath))
        Utils.DownloadFile(url, archivePath)
        print("Extracting", archivePath)
        if archive.endswith(".zip"):
            Utils.UnzipFile(archivePath, deleteZipFile=True)
        else:
            Utils.UntarFile(archivePath, deleteTarFile=True)
            executablePath = Path(f"{cls.premakeDirectory}/{executable}")
            executablePath.chmod(executablePath.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
        print(f"Premake {cls.premakeVersion} has been downloaded to '{cls.premakeDirectory}'")

        # The license is nice to have; premake works without it.
        premakeLicensePath = f"{cls.premakeDirectory}/LICENSE.txt"
        try:
            print("Downloading {0:s} to {1:s}".format(cls.premakeLicenseUrl, premakeLicensePath))
            Utils.DownloadFile(cls.premakeLicenseUrl, premakeLicensePath)
            print(f"Premake License file has been downloaded to '{cls.premakeDirectory}'")
        except Exception as error:
            print(f"Could not download the premake license ({error}); continuing.")

        return True
