# Zephyr RTOS development container for STM32G0 targets
FROM ubuntu:24.04

ARG ZEPHYR_VERSION=v4.4.2
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=1000

ENV DEBIAN_FRONTEND=noninteractive \
    LANG=C.UTF-8 \
    LC_ALL=C.UTF-8

# Zephyr host dependencies (per the Zephyr Getting Started guide) plus
# debug/flash tooling for ST-LINK probes.
RUN apt-get update && apt-get install -y --no-install-recommends \
        git cmake ninja-build gperf ccache dfu-util device-tree-compiler \
        wget curl ca-certificates xz-utils file make gcc g++ \
        gcc-multilib g++-multilib libsdl2-dev libmagic1 \
        python3 python3-dev python3-pip python3-venv python3-tk \
        openocd stlink-tools gdb-multiarch usbutils \
        sudo bash-completion less nano \
    && rm -rf /var/lib/apt/lists/*

# Replace the default 'ubuntu' user (UID 1000) with our dev user
RUN userdel -r ubuntu 2>/dev/null || true \
    && groupadd --gid ${USER_GID} ${USERNAME} \
    && useradd --uid ${USER_UID} --gid ${USER_GID} -m -s /bin/bash ${USERNAME} \
    && usermod -aG dialout,plugdev ${USERNAME} \
    && echo "${USERNAME} ALL=(root) NOPASSWD:ALL" > /etc/sudoers.d/${USERNAME} \
    && chmod 0440 /etc/sudoers.d/${USERNAME}

ENV ZEPHYR_WORKSPACE=/opt/zephyrproject \
    ZEPHYR_VENV=/opt/zephyr-venv
ENV ZEPHYR_BASE=${ZEPHYR_WORKSPACE}/zephyr \
    ZEPHYR_TOOLCHAIN_VARIANT=zephyr \
    PATH=${ZEPHYR_VENV}/bin:${PATH}

RUN mkdir -p ${ZEPHYR_WORKSPACE} ${ZEPHYR_VENV} \
    && chown ${USERNAME}:${USERNAME} ${ZEPHYR_WORKSPACE} ${ZEPHYR_VENV} /opt

USER ${USERNAME}

# Python virtual environment with west
RUN python3 -m venv ${ZEPHYR_VENV} \
    && pip install --no-cache-dir --upgrade pip wheel \
    && pip install --no-cache-dir west

# Zephyr source tree. Only the modules needed for STM32 (CMSIS + hal_stm32)
# are fetched to keep the image small; add more to the filter if needed.
RUN cd ${ZEPHYR_WORKSPACE} \
    && west init -m https://github.com/zephyrproject-rtos/zephyr --mr ${ZEPHYR_VERSION} \
    && west config manifest.project-filter -- '-.*,+cmsis,+cmsis_6,+hal_stm32' \
    && west update --narrow --fetch-opt=--depth=1 \
    && west zephyr-export \
    && pip install --no-cache-dir -r ${ZEPHYR_BASE}/scripts/requirements.txt

# Zephyr SDK (version taken from zephyr/SDK_VERSION): ARM toolchain + host tools
RUN cd ${ZEPHYR_WORKSPACE} \
    && west sdk install -b /opt -t arm-zephyr-eabi \
    && rm -rf /tmp/* ~/.cache

WORKDIR /workspaces
CMD ["/bin/bash"]
