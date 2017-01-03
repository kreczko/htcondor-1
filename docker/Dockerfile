FROM ubuntu:16.04

RUN apt -qq update -y

# install requirements
COPY files/requirements.txt /tmp/requirements.txt
COPY files/condor /etc/condor
COPY files/keyboard /etc/default/keyboard
COPY files/console-setup /etc/default/console-setup

RUN apt -qq install $(grep -vE "^\s*#" /tmp/requirements.txt  | tr "\n" " ") -y
RUN echo "force-confold" >> /etc/dpkg/dpkg.cfg &&\
    echo "force-confdef" >> /etc/dpkg/dpkg.cfg
RUN DEBIAN_FRONTEND=noninteractive apt install -y htcondor
RUN apt clean

RUN git clone https://github.com/kreczko/htcondor-1.git /opt/htcondor && \
  cd /opt/htcondor && \
  git checkout worlds-weirdest-setup.py

WORKDIR /opt/htcondor

RUN curl http://t2.unl.edu/store/sources/condor-prefix.tar.gz | tar zxv

RUN CMAKE_INCLUDE_PATH=$PWD/condor-prefix/usr/include CMAKE_LIBRARY_PATH=$PWD/condor-prefix/usr/lib64 ./configure_minimal && \
    make htcondor classad_module
RUN python setup.py install
