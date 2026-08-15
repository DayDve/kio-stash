FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies and KDE Neon apt repository for KF6 dev headers
RUN apt-get update && apt-get install --no-install-recommends -y \
    ca-certificates \
    curl \
    gnupg \
    git \
    build-essential \
    cmake \
    extra-cmake-modules \
    dpkg-dev \
    gettext \
    pkg-config \
    && curl -fsSL https://archive.neon.kde.org/public.key | gpg --dearmor -o /etc/apt/keyrings/kde-neon.gpg \
    && echo "deb [signed-by=/etc/apt/keyrings/kde-neon.gpg] http://archive.neon.kde.org/user noble main" > /etc/apt/sources.list.d/kde-neon.list \
    && apt-get update \
    && apt-get install --no-install-recommends -y \
       kf6-extra-cmake-modules \
       qt6-base-dev \
       kf6-kio-dev \
       kf6-kconfig-dev \
       kf6-kdbusaddons-dev \
       kf6-ki18n-dev \
       kf6-kcoreaddons-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . /src

ARG PKG_VERSION=1.0.0+daydve1

RUN rm -rf /src/build && \
    mkdir -p /src/build && \
    cd /src/build && \
    cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DKDE_INSTALL_USE_QT_SYS_PATHS=ON && \
    make -j$(nproc)

RUN mkdir -p /pkg/DEBIAN \
    /pkg/usr/lib/x86_64-linux-gnu/qt6/plugins/kf6/kio \
    /pkg/usr/lib/x86_64-linux-gnu/qt6/plugins/kf6/kded \
    /pkg/usr/share/kservices6 \
    /pkg/usr/share/metainfo

RUN DESTDIR=/pkg make -C /src/build install

RUN cat <<EOF > /pkg/DEBIAN/control
Package: kio-stash
Version: ${PKG_VERSION}
Architecture: amd64
Maintainer: daydve <daydve@smbit.pro>
Section: kde
Priority: optional
Depends: kf6-kio, libc6 (>= 2.38), libqt6core6t64, libqt6dbus6t64, libqt6gui6t64, libstdc++6 (>= 13)
Recommends: dolphin
Enhances: dolphin
Description: Virtual folder protocol (stash:/) for Dolphin on KDE Plasma 6
 Custom KF6 port built for KDE Neon / Noble.
EOF

RUN dpkg-deb --build /pkg /kio-stash_${PKG_VERSION}_amd64.deb

FROM scratch AS output
COPY --from=builder /kio-stash_*.deb /
