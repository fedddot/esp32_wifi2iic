FROM espressif/idf:latest

RUN apt-get update
RUN apt-get install -y lsb-release software-properties-common gnupg
RUN apt-get install -y bash-completion
RUN apt-get install -y protobuf-compiler
RUN apt-get install -y locales

RUN ${IDF_PATH}/tools/idf_tools.py install esp-clang

ARG UID=1000
ARG GID=1000

RUN userdel -r ubuntu 2>/dev/null; groupdel ubuntu 2>/dev/null; true
RUN groupadd -g ${GID} developer
RUN useradd -m -u ${UID} -g developer -s /bin/bash developer

RUN chmod -R a+rwX ${IDF_TOOLS_PATH}

ENV SHELL=bash

ENV HOME=/home/developer
COPY --chown=${UID}:${GID} docker/shell_additions ${HOME}
RUN echo "source ${HOME}/shell_additions" >> ${HOME}/.bashrc

RUN sed -i -e 's/# en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen && \
    dpkg-reconfigure --frontend=noninteractive locales && \
    update-locale LANG=en_US.UTF-8
ENV LANG="en_US.UTF-8" 

USER developer

ENV TARGET=esp32s2

WORKDIR /usr/app/src

RUN $IDF_TOOLS_PATH/entrypoint.sh
# RUN pip install --upgrade --break-system-packages protobuf grpcio-tools
# RUN bash --init-file $IDF_PATH/export.sh -c "$IDF_PYTHON_ENV_PATH/bin/pip install protobuf grpcio-tools"

ENTRYPOINT ["bash"]