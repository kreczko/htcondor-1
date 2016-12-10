import subprocess

def condor_version():
    '''
        Returns installed condor version
    '''
    cmd = 'condor_version'
    output = subprocess.Popen([cmd], stdout=subprocess.PIPE).communicate()[0]
    version = output.split(' ')[1]
    return version

def compile_python_bindings():
    commands = ['./configure_minimal', 'cd src/python-bindings', ['make']]


#from distutils.command.install import install as DistutilsInstall

#class MyInstall(DistutilsInstall):
#    def run(self):
#        do_pre_install_stuff()
#        DistutilsInstall.run(self)
#        do_post_install_stuff()
#
#...

#setup(..., cmdclass={'install': MyInstall}, ...)

# or
#from distutils.core import setup, Extension

#module1 = Extension('demo',
#                    sources = ['demo.c'])

#setup (name = 'PackageName',
#version = '1.0',
#description = 'This is a demo package',
#ext_modules = [module1])
