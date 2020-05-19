FROM ubuntu:14.04

RUN apt-get update && apt-get install -y curl git build-essential \
  libssl-dev zlib1g-dev libncurses5-dev libncursesw5-dev libreadline-dev libsqlite3-dev \
  libgdbm-dev libdb5.3-dev libbz2-dev libexpat1-dev liblzma-dev libffi-dev uuid-dev \
  python python3 python-dev python3-dev

RUN curl -L https://github.com/pyenv/pyenv-installer/raw/master/bin/pyenv-installer | bash
ENV HOME=/root
ENV PYENV_ROOT $HOME/.pyenv
ENV PATH $PYENV_ROOT/shims:$PYENV_ROOT/bin:$PATH

RUN pyenv install 3.4.10
RUN pyenv install 3.5.7
RUN pyenv install 3.6.9

# Python 3.7 requires ssl >= 1.1 but Ubuntu 14.04 only provides 1.0.1.
# Build openssl 1.1 and install in homedir.
RUN curl -LO https://www.openssl.org/source/openssl-1.1.1b.tar.gz
RUN tar zxvf openssl-1.1.1b.tar.gz
WORKDIR openssl-1.1.1b
RUN ./config --prefix=$HOME/openssl --openssldir=$HOME/openssl
RUN make install
ENV CFLAGS=-I$HOME/openssl/include
ENV LDFLAGS=-L$HOME/openssl/lib
ENV LD_LIBRARY_PATH=$HOME/openssl/lib:$LD_LIBRARY_PATH
ENV SSH=$HOME/openssl
WORKDIR /

RUN pyenv install 3.7.4
RUN pyenv install 3.8.2
