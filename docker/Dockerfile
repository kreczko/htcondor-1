FROM ubuntu:16.04

RUN apt update -y

# install requirements
COPY requirements.txt /tmp/requirements.txt
RUN apt install $(grep -vE "^\s*#" /tmp/requirements.txt  | tr "\n" " ") -y

RUN git clone https://github.com/kreczko/htcondor-1.git /opt/htcondor && \
  cd /opt/htcondor && \
  git checkout worlds-weirdest-setup.py


RUN cd /opt/htcondor && \
  curl http://t2.unl.edu/store/sources/condor-prefix.tar.gz | tar zxv

RUN cd /opt/htcondor && \
  CMAKE_INCLUDE_PATH=$PWD/condor-prefix/usr/include CMAKE_LIBRARY_PATH=$PWD/condor-prefix/usr/lib64 ./configure_minimal && \
  make htcondor classad_module


WORKDIR /opt/htcondor
