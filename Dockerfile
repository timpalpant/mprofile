FROM ubuntu:16.04

RUN apt-get update && apt-get install -y curl git build-essential \
  libssl-dev zlib1g-dev libncurses5-dev libncursesw5-dev libreadline-dev libsqlite3-dev \
  libgdbm-dev libdb5.3-dev libbz2-dev libexpat1-dev liblzma-dev libffi-dev libssl-dev uuid-dev \
  python python3 python-dev python3-dev

RUN curl -L https://github.com/pyenv/pyenv-installer/raw/master/bin/pyenv-installer | bash
ENV HOME=/root
ENV PYENV_ROOT $HOME/.pyenv
ENV PATH $PYENV_ROOT/shims:$PYENV_ROOT/bin:$PATH

RUN pyenv install 3.4.10
RUN pyenv install 3.5.10
RUN pyenv install 3.6.14
RUN pyenv install 3.7.13
RUN pyenv install 3.8.13
RUN pyenv install 3.9.13
RUN pyenv install 3.10.4
