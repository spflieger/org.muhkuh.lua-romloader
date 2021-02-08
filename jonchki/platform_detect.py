import os
import platform
import re
import string
import subprocess


class PlatformDetect:
    def __init__(self):
        self.strHostCpuArchitecture = None
        self.strHostDistributionId = None
        self.strHostDistributionVersion = None
        self.strHostDistributionLike = None
        self.strStandardArchiveFormat = None

    def __windows_get_cpu_architecture_env(self):
        strCpuArchitecture = None
        strEnvProcessorArchitecture = None
        strEnvProcessorArchiteW6432 = None
        if 'PROCESSOR_ARCHITECTURE' in os.environ:
            strEnvProcessorArchitecture = string.lower(
                os.environ['PROCESSOR_ARCHITECTURE']
            )
        if 'PROCESSOR_ARCHITEW6432' in os.environ:
            strEnvProcessorArchiteW6432 = string.lower(
                os.environ['PROCESSOR_ARCHITEW6432']
            )
        # See here for details: https://blogs.msdn.microsoft.com/david.wang/
        # 2006/03/27/howto-detect-process-bitness/
        if((strEnvProcessorArchitecture == 'amd64') or
           (strEnvProcessorArchiteW6432 == 'amd64')):
            strCpuArchitecture = 'x86_64'
        elif((strEnvProcessorArchitecture == 'x86') and
             (strEnvProcessorArchiteW6432 is None)):
            strCpuArchitecture = 'x86'
        else:
            print('Failed to detect the CPU architecture on Windows with the '
                  'ENV variables.')
            print('PROCESSOR_ARCHITECTURE = %s' %
                  (str(strEnvProcessorArchitecture)))
            print('PROCESSOR_ARCHITEW6432 = %s' %
                  (str(strEnvProcessorArchiteW6432)))

        return strCpuArchitecture

    def __linux_get_os_architecture_getconf(self):
        strCpuArchitecture = None

        # Try to parse the output of the 'getconf LONG_BIT' command.
        strOutput = subprocess.check_output(['getconf', 'LONG_BIT'])
        strOutputStrip = strOutput.strip()
        if strOutputStrip == '32':
            strCpuArchitecture = 'x86'
        elif strOutputStrip == '64':
            strCpuArchitecture = 'x86_64'

        return strCpuArchitecture

    def __linux_get_cpu_architecture_lscpu(self):
        strCpuArchitecture = None
        astrReplacements = {
            'i386': 'x86',
            'i486': 'x86',
            'i586': 'x86',
            'i686': 'x86'
        }

        # Try to parse the output of the 'lscpu' command.
        strOutput = subprocess.check_output(['lscpu']).decode(
            "utf-8",
            "replace"
        )
        tMatch = re.search(r'Architecture: *(\S+)', strOutput)
        if tMatch is None:
            raise Exception('Failed to get the CPU architecture with "lscpu".')

        strCpuArchitecture = tMatch.group(1)
        # Replace the CPU architectures found in the list.
        if strCpuArchitecture in astrReplacements:
            strCpuArchitecture = astrReplacements[strCpuArchitecture]

        return strCpuArchitecture

    def __linux_detect_distribution_etc_os_release(self):
        strDistributionId = None
        strDistributionVersion = None
        strDistributionLike = None

        # Try to open /etc/os-release.
        try:
            tFile = open('/etc/os-release', 'rt')
        except:
            raise

        for strLine in tFile:
            tMatch = re.match('ID=(.+)', strLine)
            if tMatch is not None:
                strDistributionId = tMatch.group(1).lower()
            tMatch = re.match('VERSION_ID=(.+)', strLine)
            if tMatch is not None:
                strDistributionVersion = tMatch.group(1).strip('"')
            tMatch = re.match('ID_LIKE=(.+)', strLine)
            if tMatch is not None:
                strDistributionLike = tMatch.group(1).strip('"')
        tFile.close()

        # Return both components or none.
        if (strDistributionId is None) or (strDistributionVersion is None):
            strDistributionId = None
            strDistributionVersion = None

        return strDistributionId, strDistributionVersion, strDistributionLike

    def __linux_detect_distribution_etc_lsb_release(self):
        strDistributionId = None
        strDistributionVersion = None

        # Try to open /etc/lsb-release.
        tFile = open('/etc/lsb-release', 'rt')
        if tFile is None:
            raise Exception('Failed to detect the Linux distribution with '
                            '/etc/lsb-release.')
        for strLine in tFile:
            tMatch = re.match('DISTRIB_ID=(.+)', strLine)
            if tMatch is not None:
                strDistributionId = tMatch.group(1).lower()
            tMatch = re.match('DISTRIB_RELEASE=(.+)', strLine)
            if tMatch is not None:
                strDistributionVersion = tMatch.group(1)
        tFile.close()

        # Return both components or none.
        if (strDistributionId is None) or (strDistributionVersion is None):
            strDistributionId = None
            strDistributionVersion = None

        return strDistributionId, strDistributionVersion

    def detect(self):
        strSystem = platform.system()
        if strSystem == 'Windows':
            # This is windows.

            # Detect the CPU architecture.
            self.strHostCpuArchitecture =\
                self.__windows_get_cpu_architecture_env()

            # Set the distribution version and ID.
            self.strHostDistributionId = 'windows'
            self.strHostDistributionVersion = None

            # Windows uses ZIP as standard archive format.
            self.strStandardArchiveFormat = 'zip'
        elif strSystem == 'Linux':
            # This is a Linux.

            # Detect the CPU architecture.
            # Prefer the OS architecture over the CPU architecture to honour a
            # 32bit OS on a 64bit CPU. This happens with a 32bit Docker
            # container on a 64bit host.
            strCpuArch = self.__linux_get_os_architecture_getconf()
            if strCpuArch is None:
                strCpuArch = self.__linux_get_cpu_architecture_lscpu()
            self.strHostCpuArchitecture = strCpuArch

            # Detect the distribution.
            try:
                self.strHostDistributionId, self.strHostDistributionVersion, self.strHostDistributionLike = self.__linux_detect_distribution_etc_os_release()
            except:
                self.strHostDistributionId, self.strHostDistributionVersion =\
                    self.__linux_detect_distribution_etc_lsb_release()

            if self.strHostDistributionLike.find('ubuntu') <= 0:
                print(self.strHostDistributionLike)
                self.strHostDistributionId = 'ubuntu'

            # Linux uses TAR GZIP as standard archive format.
            self.strStandardArchiveFormat = 'tar.gz'
        else:
            raise Exception('Unknown platform: "%s"' % (strSystem))
