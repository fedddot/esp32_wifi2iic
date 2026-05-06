FROM espressif/idf:latest AS builder

RUN apt-get update
RUN apt-get install -y lsb-release software-properties-common gnupg
RUN apt-get install -y bash-completion
RUN apt-get install -y protobuf-compiler
RUN apt-get install -y locales

RUN sed -i -e 's/# en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen
RUN locale-gen
ENV LANG=en_US.UTF-8  
ENV LANGUAGE=en_US:en  
ENV LC_ALL=en_US.UTF-8

WORKDIR ${IDF_PATH}
RUN . ./export.sh && ${IDF_PYTHON_ENV_PATH}/bin/pip install protobuf grpcio-tools

COPY docker/dev-env-setup.sh /etc/profile.d
RUN chmod a+x /etc/profile.d/dev-env-setup.sh

ENV TARGET=esp32s2

FROM builder AS developer

WORKDIR ${IDF_PATH}
RUN ./tools/idf_tools.py install esp-clang
RUN . ./export.sh && ln -sf "$(which clangd)" /usr/local/bin/clangd

ARG UID=1000
ARG GID=1000
ARG USERNAME=developer

RUN apt-get install -y gh

ENV COPILOT_HOME=/usr/local/copilot
RUN curl -fsSL https://gh.io/copilot-install | PREFIX=${COPILOT_HOME} bash
ENV PATH="${COPILOT_HOME}/bin:${PATH}"
RUN chmod -R a+rwX ${COPILOT_HOME}

RUN userdel -r ubuntu 2>/dev/null; groupdel ubuntu 2>/dev/null; true
RUN groupadd -g ${GID} ${USERNAME}
RUN useradd -m -u ${UID} -g ${USERNAME} -s /bin/bash -d /home/${USERNAME} ${USERNAME}
ENV HOME=/home/${USERNAME}
ENV SHELL=bash

USER ${USERNAME}

WORKDIR /workspace

ENTRYPOINT ["bash"]