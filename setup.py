from distutils.command.install import install as DistutilsInstall
from distutils.core import setup
import subprocess
import shutil
import os
import platform

HTCONDOR_GIT_URL = 'https://github.com/htcondor/htcondor.git'
CONFIGURE_SCRIPT = 'configure_minimal'
HTCONDOR_TMP = 'tmp'
HTCONDOR_PACKAGES = ['htcondor', 'classad_module']
PYTHON_LIBRARIES = [os.path.join(HTCONDOR_TMP, 'src', 'python-bindings', l)
                    for l in ['classad.so', 'htcondor.so']]
LIBRARIES = [
    os.path.join(HTCONDOR_TMP, 'src', 'python-bindings',
                 'libpyclassad2.7_*_*_*.so'),
    os.path.join(HTCONDOR_TMP, 'src', 'condor_utils',
                 'libcondor_utils_*_*_*.so'),
]


def condor_version():
    '''
        Returns installed condor version
    '''
    cmd = 'condor_version'
    p = subprocess.Popen([cmd], stdout=subprocess.PIPE)
    output = p.communicate()[0]
    if not p.returncode == 0:
        raise RuntimeWarning(
            'Something went wrong during call of `condor_version`\n Is htcondor installed?')
    version = output.split(' ')[1]
    return version

__version__ = condor_version()


def checkout_version(condor_version):
    # git clone https://github.com/htcondor/htcondor.git tmp
    # cd tmp
    # git checkout <correct versiontag> # e.g. tag format = V8_1_4
    # but condor version output is 8.4.10
    git_tag = 'V{0}_{1}_{2}'.format(*condor_version.split('.'))
    commands = [

        'git clone {HTCONDOR_GIT_URL} {HTCONDOR_TMP}',
        'cp -p {CONFIGURE_SCRIPT} {HTCONDOR_TMP}/.',
        'cd {HTCONDOR_TMP}',
        'git checkout {git_tag}',

    ]
    all_in_one = ' && '.join(commands)
    all_in_one = all_in_one.format(
        HTCONDOR_GIT_URL=HTCONDOR_GIT_URL,
        HTCONDOR_TMP=HTCONDOR_TMP,
        CONFIGURE_SCRIPT=CONFIGURE_SCRIPT,
        git_tag=git_tag,
    )

    subprocess.call(all_in_one, shell=True)


def compile_python_bindings():
    commands = [
        'cd {HTCONDOR_TMP}',
        './{CONFIGURE_SCRIPT}',
        'make {PACKAGES}',
    ]
    if platform.dist()[0].lower() == 'ubuntu':
        commands.insert(
            1, 'curl http://t2.unl.edu/store/sources/condor-prefix.tar.gz | tar zxv')
    all_in_one = ' && '.join(commands)
    all_in_one = all_in_one.format(
        HTCONDOR_TMP=HTCONDOR_TMP,
        CONFIGURE_SCRIPT=CONFIGURE_SCRIPT,
        PACKAGES=' '.join(HTCONDOR_PACKAGES),
    )
    subprocess.call(all_in_one, shell=True)


class HTCondorInstall(DistutilsInstall):

    def run(self):
        self._pre_install()
        self._install()
        self._post_install()

    def _pre_install(self):
        print('Found installed condor version "{0}"'.format(__version__))
        checkout_version(__version__)

    def _install(self):
        '''
        need to copy
        ./src/python-bindings/classad.so
        ./src/python-bindings/htcondor.so
        to site-packages
        and
        ./src/python-bindings/libpyclassad2.7_X_Y_X.so
        ./src/condor_utils/libcondor_utils_X_Y_Z.so

        to /usr/lib
        (./bld_external/classads-X.Y.Z/install/lib/libclassad.so should already be there)
        '''
        compile_python_bindings()
        target_dir = os.path.join(self.install_lib)
        for f in PYTHON_LIBRARIES:
            dst = os.path.join(target_dir, os.path.basename(f))
            shutil.copyfile(f, dst)
        if platform.dist()[0].lower() == 'ubuntu':
            target_dir = '/usr/lib'
        else:
            target_dir = '/usr/lib64'
        for f in LIBRARIES:
            dst = os.path.join(target_dir, os.path.basename(f))
            shutil.copyfile(f, dst)

    def _post_install(self):
        pass
        # shutil.rmtree(HTCONDOR_TMP)

setup(
    name='htcondor',
    version=__version__,

    cmdclass={
        'install': HTCondorInstall,
    }
)
