extern "C" {
#include <libavcodec/bsf.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/timestamp.h>
}

#include <atomic>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <thread>

#include "common/utils.h"
// #include "muduo/base/Logging.h"
// #include "muduo/net/EventLoop.h"
// #include "muduo/net/http/HttpRequest.h"
// #include "muduo/net/http/HttpResponse.h"
// #include "muduo/net/http/HttpServer.h"
#include "HttpServer.h"
#include "net/udp_connection.h"
#include "net/udp_server.h"
#include "rtc/dtls_transport.h"
#include "rtc/srtp_session.h"
#include "rtc/stun_packet.h"
#include "rtc/transport_interface.h"
#include "rtc/webrtc_transport.h"
#include "session/webrtc_session.h"

using namespace hv;

// using namespace muduo;
// using namespace muduo::net;

static int WriteRtpCallback(void* opaque, uint8_t* buf, int buf_size) {
  WebRTCSessionFactory* webrtc_session_factory = (WebRTCSessionFactory*)opaque;
  std::vector<std::shared_ptr<WebRTCSession>> all_sessions;
  std::shared_ptr<uint8_t> shared_buf(new uint8_t[buf_size]);
  memcpy(shared_buf.get(), buf, buf_size);
  webrtc_session_factory->GetAllReadyWebRTCSession(&all_sessions);
  for (const auto& session : all_sessions) {
    auto connection = session->GetNetworkTransport()->connection();
    session->loop()->runInLoop([session, shared_buf, buf_size]() {
      session->webrtc_transport()->EncryptAndSendRtpPacket(shared_buf.get(),
                                                           buf_size);
    });
  }
  return buf_size;
}

static int H2642Rtp(const char* in_filename, void* opaque) {
  const AVOutputFormat* ofmt = NULL;
  AVIOContext* avio_ctx = NULL;
  AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
  AVPacket* pkt = NULL;
  int ret, i;
  int in_stream_index = 0, out_stream_index = 0;
  int stream_mapping_size = 0;
  int64_t pts = 0;
  uint8_t *buffer = NULL, *avio_ctx_buffer = NULL;
  size_t buffer_size, avio_ctx_buffer_size = 4096;

  const AVBitStreamFilter* abs_filter = NULL;
  AVBSFContext* abs_ctx = NULL;
  abs_filter = av_bsf_get_by_name("h264_mp4toannexb");
  av_bsf_alloc(abs_filter, &abs_ctx);

  avio_ctx_buffer = (uint8_t*)av_malloc(avio_ctx_buffer_size);
  if (!avio_ctx_buffer) {
    ret = AVERROR(ENOMEM);
    goto end;
  }
  avio_ctx = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size, 1,
                                opaque, NULL, WriteRtpCallback, NULL);
  if (!avio_ctx) {
    ret = AVERROR(ENOMEM);
    goto end;
  }
  avio_ctx->max_packet_size = 1400;

  pkt = av_packet_alloc();
  if (!pkt) {
    fprintf(stderr, "Could not allocate AVPacket\n");
    return 1;
  }

  if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
    fprintf(stderr, "Could not open input file '%s'", in_filename);
    goto end;
  }

  if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
    fprintf(stderr, "Failed to retrieve input stream information");
    goto end;
  }

  av_dump_format(ifmt_ctx, 0, in_filename, 0);

  avformat_alloc_output_context2(&ofmt_ctx, NULL, "rtp", NULL);
  if (!ofmt_ctx) {
    fprintf(stderr, "Could not create output context\n");
    ret = AVERROR_UNKNOWN;
    goto end;
  }

  ofmt = ofmt_ctx->oformat;
  av_opt_set_int(ofmt_ctx->priv_data, "ssrc", 12345678, 0);

  for (i = 0; i < ifmt_ctx->nb_streams; i++) {
    AVStream* out_stream;
    AVStream* in_stream = ifmt_ctx->streams[i];
    AVCodecParameters* in_codecpar = in_stream->codecpar;

    if (in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO) {
      continue;
    }
    out_stream = avformat_new_stream(ofmt_ctx, NULL);
    if (!out_stream) {
      fprintf(stderr, "Failed allocating output stream\n");
      ret = AVERROR_UNKNOWN;
      goto end;
    }
    ret = avcodec_parameters_copy(out_stream->codecpar, in_codecpar);
    if (ret < 0) {
      fprintf(stderr, "Failed to copy codec parameters\n");
      goto end;
    }

    avcodec_parameters_copy(abs_ctx->par_in, in_codecpar);
    av_bsf_init(abs_ctx);
    avcodec_parameters_copy(out_stream->codecpar, abs_ctx->par_out);

    in_stream_index = i;
    out_stream_index = out_stream->index;
    break;
  }

  av_dump_format(ofmt_ctx, 0, NULL, 1);

  ofmt_ctx->pb = avio_ctx;

  ret = avformat_write_header(ofmt_ctx, NULL);
  if (ret < 0) {
    fprintf(stderr, "Error occurred when opening output file\n");
    goto end;
  }

  while (1) {
    AVStream *in_stream, *out_stream;

    ret = av_read_frame(ifmt_ctx, pkt);
    if (ret < 0) {
      for (size_t i = 0; i < ifmt_ctx->nb_streams; i++) {
        av_seek_frame(ifmt_ctx, i, 0, AVSEEK_FLAG_BYTE);
      }
      continue;
    }
    if (pkt->stream_index != in_stream_index) {
      continue;
    }

    in_stream = ifmt_ctx->streams[in_stream_index];
    out_stream = ofmt_ctx->streams[0];
    // log_packet(ifmt_ctx, pkt, "in");
    pts += 40;
    pkt->pts = pts;
    pkt->dts = pts;

    av_bsf_send_packet(abs_ctx, pkt);
    av_bsf_receive_packet(abs_ctx, pkt);
    /* copy packet */
    av_packet_rescale_ts(pkt, in_stream->time_base, out_stream->time_base);
    pkt->pos = -1;
    // log_packet(ofmt_ctx, pkt, "out");
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    ret = av_interleaved_write_frame(ofmt_ctx, pkt);
    /* pkt is now blank (av_interleaved_write_frame() takes ownership of
     * its contents and resets pkt), so that no unreferencing is necessary.
     * This would be different if one used av_write_frame(). */
    if (ret < 0) {
      fprintf(stderr, "Error muxing packet\n");
      break;
    }
  }

  av_write_trailer(ofmt_ctx);
end:
  av_packet_free(&pkt);

  avformat_close_input(&ifmt_ctx);

  /* close output */
  if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
    avio_closep(&ofmt_ctx->pb);
  avformat_free_context(ofmt_ctx);

  av_bsf_free(&abs_ctx);

  if (ret < 0 && ret != AVERROR_EOF) {
    fprintf(stderr, "Error occurred: %d\n", ret);
    return 1;
  }

  return 0;
}

int main(int argc, char* argv[]) {
  // ffmpeg -re -f lavfi -i testsrc2=size=640*480:rate=25 -vcodec libx264
  // -profile:v baseline -keyint_min 60 -g 60 -sc_threshold 0 -f rtp
  // rtp://127.0.0.1:56000 ffmpeg -re -stream_loop -1 -i  test.mp4 -vcodec copy
  // -bsf:v h264_mp4toannexb -ssrc 12345678 -f rtp rtp://127.0.0.1:56000

  std::string ip("127.0.0.1");
  uint16_t port = 10000;
  if (argc == 3) {
    ip = argv[1];
    port = atoi(argv[2]);
  }

  Utils::Crypto::ClassInit();
  RTC::DtlsTransport::ClassInit();
  RTC::DepLibSRTP::ClassInit();
  RTC::SrtpSession::ClassInit();
  // EventLoop loop;
  WebRTCSessionFactory webrtc_session_factory;

  std::thread flv_2_rtp_thread([&webrtc_session_factory]() {
    H2642Rtp(
        "D:\\development\\opensource\\simple_webrtc_server\\out\\Debug\\test."
        "h264",
        &webrtc_session_factory);
  });

  // UdpServer rtc_server(&loop, muduo::net::InetAddress("0.0.0.0", port),
  // "rtc_server", 2);
  UdpServerOuter rtc_server(port);
  // HttpServer http_server(&loop, muduo::net::InetAddress("0.0.0.0", 8000),
  // "http_server",
  //                        TcpServer::kReusePort);

  rtc_server.SetPacketCallback(
      [&webrtc_session_factory](UdpServerOuter* server, const uint8_t* buf,
                                size_t len, const string& peer_addr,
                                struct timeval& timestamp) {
        WebRTCSessionFactory::HandlePacket(&webrtc_session_factory, server, buf,
                                           len, peer_addr, timestamp);
      });

  rtc_server.udpSrv_->onMessage = [&rtc_server](const SocketChannelPtr& channel,
                                                Buffer* buf) {
    ServerPacketCallback cb = rtc_server.GetPacketCallback();
    if (cb) {
      struct sockaddr_in sockaddr = {0};
      string remoteAddr = channel->peeraddr();
      string localAddr = channel->localaddr();
      struct timeval receive_time;
      Utils::Time::gettimeofday(&receive_time, NULL);
      cb(&rtc_server, (const uint8_t*)buf->data(), buf->size(), remoteAddr,
         receive_time);
    }
  };

  rtc_server.Start();

  hv::HttpService router;
  router.GET("/webrtc", [&](HttpRequest* req, HttpResponse* resp) {
    resp->json["origin"] = req->client_addr.ip;
    resp->json["url"] = req->url;

    // resp->json["headers"] = req->headers;
    // resp->json["headers"]["Access-Control-Allow-Origin"] = "*";
    // resp->setContentType("text/plain");
    // resp->addHeader("Access-Control-Allow-Origin", "*");
    auto rtc_session = webrtc_session_factory.CreateWebRTCSession(ip, port);
    resp->SetBody(rtc_session->webrtc_transport()->GetLocalSdp());
    std::cout << rtc_session->webrtc_transport()->GetLocalSdp() << std::endl;
    return 200;
  });

  http_server_t server;
  server.service = &router;
  server.port = 8000;
  http_server_run(&server, 0);
  // http_server.setHttpCallback(
  //     [&loop, &webrtc_session_factory, port, ip](const HttpRequest& req,
  //     HttpResponse* resp) {
  //       if (req.path() == "/webrtc") {
  //         resp->setStatusCode(HttpResponse::k200Ok);
  //         resp->setStatusMessage("OK");
  //         resp->setContentType("text/plain");
  //         resp->addHeader("Access-Control-Allow-Origin", "*");
  //         auto rtc_session = webrtc_session_factory.CreateWebRTCSession(ip,
  //         port);
  //         resp->setBody(rtc_session->webrtc_transport()->GetLocalSdp());
  //         std::cout << rtc_session->webrtc_transport()->GetLocalSdp() <<
  //         std::endl;
  //       }
  //     });
  // loop.runInLoop([&]() {
  //   rtc_server.Start();
  //   http_server.start();
  // });
  // loop.loop();

  while (getchar() != '\n');
  http_server_stop(&server);
}
