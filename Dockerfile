FROM ubuntu:20.04 as core_deps
MAINTAINER metahash
EXPOSE 9999
RUN mkdir /opt/metahash
WORKDIR /opt/metahash
ENV TZ=Europe/Moscow
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone
RUN apt update && apt install -y libgoogle-perftools4 libgmp10 liburiparser1 libcurl4 gcc g++ liburiparser-dev libssl-dev libevent-dev git automake libtool make cmake libcurl4-openssl-dev libcrypto++-dev libgnutls28-dev libgcrypt20-dev libgoogle-perftools-dev git curl wget libboost-dev libboost-dev && rm -rf /var/cache/apt
RUN git clone https://github.com/metahashorg/Node-Core src
RUN mkdir src/build && cd src/build
WORKDIR /opt/metahash/src/build
RUN pwd
RUN cmake ..
RUN make
FROM ubuntu:20.04
WORKDIR /opt/metahash
COPY --from=core_deps /opt/metahash/src/build/bin/core_service  /opt/metahash/
RUN apt update && apt install -y libgoogle-perftools4 libssl1.1 &&  rm -rf /var/cache/apt