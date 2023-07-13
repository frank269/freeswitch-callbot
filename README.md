# mod_call_bot

conversation_id
String
Mã hội thoại trên hệ thống Callbot
call_at
int
Thời gian thực hiện gọi ra, đơn vị Timestamp millisecond
VD: 1677472589602
pickup_at
int
Thời gian KH nhấc máy, đơn vị Timestamp millisecond
VD: 1677472601371
Nếu KH không nghe máy, để pickup_at=0
hangup_at
int
Thời gian kết thúc cuộc gọi, đơn vị Timestamp millisecond
VD: 1677472616108
status
int
Trạng thái cuộc gọi, là 1 trong các giá trị sau:
100 - KH nhấc máy, và Callbot chủ động kết thúc cuộc gọi
101 - KH nhấc máy, nhưng KH cúp máy giữa chừng
102 - KH nhấc máy, sau đó hệ thống chuyển tiếp cuộc gọi đi
103 - Không nghe máy, máy bận
104 - Không liên lạc được, thuê báo
105 - Hệ thống lỗi (Bất kỳ exception nào phát sinh trong lúc đàm thoại)
sip_code
int
Mã sip_code trả về từ tổng đài





apt-get install -y libprotobuf-dev protobuf-compiler python3-pip && pip3 install SQLAlchemy && \
cd /usr/src && git clone https://github.com/grpc/grpc.git && cd grpc && git checkout c66d2cc \
&& git submodule update --init --recursive \
&& mkdir -p cmake/build && cd cmake/build \
&& cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr -DBUILD_SHARED_LIBS=ON -DgRPC_SSL_PROVIDER=package -DgRPC_INSTALL=ON -DCMAKE_BUILD_TYPE=Release -DgRPC_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=RelWithDebInfo ../.. \
&& make -j 4 && make install 

// append two line above to ~/.bashrc
PATH="$PATH:/usr/local/freeswitch/bin"
CPLUS_INCLUDE_PATH="/usr/include/grpc:$CPLUS_INCLUDE_PATH"
source ~/.bashrc



// build from source
cd /usr/src \
&& wget https://github.com/signalwire/freeswitch/archive/refs/tags/v1.10.8.zip \
&& unzip v1.10.8.zip && mv freeswitch-1.10.8 freeswitch && cd freeswitch \
&& cd /usr/src/freeswitch/src/mod/applications/ \
&& git clone https://github.com/frank269/freeswitch-callbot.git \
&& mv freeswitch-callbot mod_call_bot && cd mod_call_bot && git checkout release


cd /usr/src/freeswitch && \
./bootstrap.sh -j \
&& cp /usr/src/freeswitch/src/mod/applications/mod_call_bot/modules.conf /usr/src/freeswitch/ \
&& ./configure -C \
  --prefix=/usr \
  --localstatedir=/var \
  --sysconfdir=/etc \
  --with-openssl \
  --enable-portable-binary \
  --enable-core-pgsql-support \
  --disable-dependency-tracking \
&& make && make install


-DCMAKE_TOOLCHAIN_FILE=/usr/src/vcpkg/scripts/buildsystems/vcpkg.cmake

copy 2 file: bot_init.lua and bot_event.lua to /opt/freeswitch/scripts
add line to: /etc/freeswitch/autoload_configs/lua.conf.xml
<!-- <hook event="CUSTOM" subclass="mod_call_bot::bot_hangup" script="/opt/freeswitch/scripts/bot_event.lua" /> -->
<hook event="CUSTOM" subclass="mod_call_bot::bot_transfer" script="/opt/freeswitch/scripts/bot_transfer.lua" />
<hook event="CHANNEL_HANGUP_COMPLETE" script="/opt/freeswitch/scripts/bot_hangup.lua" />