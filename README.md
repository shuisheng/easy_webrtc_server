# easy_webrtc_server
简单的webrtc流媒体服务器
联系方式：qq864733526
# 依赖库
* sudo apt-get install -y \
    libssl-dev \
    libboost-all-dev \
		libavutil-dev \
		libavformat-dev \
		libavcodec-dev
* ~~muduo网络库：https://github.com/chenshuo/muduo~~ (muduo不支持windows，使用libhv)
* libhv: https://github.com/ithewei/libhv
* srtp https://github.com/cisco/libsrtp

# 目录说明
* webrtchtml webrtc视频播放的网页
* rtc webrtc协议栈，包括stun，dtls，srtp
* net 网络传输封装
* example rtp_src_example，转发rtp例子，打开h264文件，打包成rtp通过webrtc协议转发

# 使用说明
ubuntu20.04安装依赖库openssl1.1以上、srtp、ffmpeg、libhv(替换muduo，不支持windows) ~~muduo~~，srtp需要--enable-openssl
详细安装过程参考Dockerfile
```
mkdir build
cd build
cmake ..
make
```
## rtp_src_example
* 运行程序，第一个参数为IP地址，第二个参数为端口号：
* ./rtp_src_example 192.168.1.5 10000
* 打开webrtchtml/index.html 输入IP地址，播放视频

# 原理说明
* muduo不支持udp，本项目基于muduo的Channel类简单封装一个udp通信的类；
* 基于muduo_http建立一个http信令服务器，交换webrtc所需要的sdp信息。
* 网页上打开一个http连接，服务器建立一个WebRtcTransport，传输层是一个UdpSocket。
* WebRtcTransport生成sdp信息，通过http协议传到前端。
* sdp信息包括媒体信息如编码格式、ssrc等，stun协议需要的ice-ufrag、ice-pwd、candidate,dtls需要的fingerprint。
* 前端通过candidate获取ip地址和端口号，通过udp协议连接到服务器的。
* 服务器收到udp报文，先后通过类UdpSocket接收报文；StunPacket和IceServer解析stun协议，此处的Stun协议解析，只要收到stun request，验证账户密码成功，就认为连接成功。
* stun协议交互成功后，通过DtlsTransport进行dtls握手；交换密钥后就可以初始化SrtpChannel。此处没有通过签名验证客户端的证书，所以省略了前端返回sdp的步骤。
* 读取h264码流文件，通过ffmpeg生成rtp流，通过SrtpChannel加密，通过UdpSocket发送，前端就可以看到视频。
