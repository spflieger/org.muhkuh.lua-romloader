import os
import subprocess


def install_host_debs(astrDeb):
    astrInstall = []
    for strDeb in astrDeb:
        strDpkgStatus = subprocess.check_output("dpkg-query -W -f='${Status}' %s || echo 'unknown'" % strDeb, shell=True)
        print('Check for %s = %s' % (strDeb, strDpkgStatus))
        if strDpkgStatus != 'install ok installed':
            astrInstall.append(strDeb)
    if len(astrInstall) != 0:
        subprocess.check_call('sudo apt-get update --assume-yes', shell=True)
        subprocess.check_call('sudo apt-get install --assume-yes %s' % ' '.join(astrInstall), shell=True)


def install_foreign_debs(astrDeb, strCfg_workingFolder, strCfg_projectFolder):
    # Create the folders if they do not exist yet.
    astrFolders = [
        strCfg_workingFolder,
        os.path.join(strCfg_workingFolder, 'packages'),
    ]
    for strPath in astrFolders:
        if os.path.exists(strPath) is not True:
            os.makedirs(strPath)

    packagesPath = os.path.join(strCfg_workingFolder, 'packages')
    os.chdir(packagesPath)
    subProcessPath = os.path.join(strCfg_projectFolder, 'cmake', 'tools')
    subProcessCall = '%s/get_dependencies.sh %s' % (subProcessPath, ' '.join(astrDeb))
    subprocess.check_call(subProcessCall, shell=True)
    os.chdir(strCfg_workingFolder)
