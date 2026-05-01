FROM espressif/idf:latest

RUN apt-get update
RUN apt-get install -y lsb-release software-properties-common gnupg
RUN apt-get install -y bash-completion
RUN apt-get install -y protobuf-compiler
RUN apt-get install -y locales

RUN ${IDF_PATH}/tools/idf_tools.py install esp-clang

# ENV TARGET=esp32s2
# ENV CLANGD_FLAGS="--query-driver=/opt/esp/tools/xtensa-esp-elf/esp-15.1.0_20250607/xtensa-esp-elf/bin/xtensa-${TARGET}-elf-gcc,/opt/esp/tools/xtensa-esp-elf/esp-15.1.0_20250607/xtensa-esp-elf/bin/xtensa-${TARGET}-elf-g++"

ARG UID=1000
ARG GID=1000

# RUN addgroup -g ${GID} developer
# RUN adduser -D -u ${UID} -G developer -s /bin/bash developer

ENV SHELL=bash

# ENV HOME=/home/developer
# COPY --chown=${UID}:${GID} docker/shell_additions ${HOME}
# RUN echo "source ${HOME}/shell_additions" >> ${HOME}/.bashrc

# USER developer

WORKDIR /usr/app/src


ENTRYPOINT ["bash"]