#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#include "utility/utility.h"
#include "media/recorder.h"


void Recorder::Stop() {
  recording_status_ = false;
}

void Recorder::UpdateSinkName() {

  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  memset(sink_name_, 0, sizeof(sink_name_));
  sprintf(sink_name_, "/mnt/%d%02d%02d-%02d%02d%02d.mp4", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec); 
}

int Recorder::OpenInput() {

  const char *source_name = "/tmp/record.264";
  if(access(source_name, F_OK) != 0) {
    PLOGI("Camera is not ready");
    return 0;
  }

  if(avformat_open_input(&input_format_context_, source_name, NULL, NULL) < 0) {
    PLOGE("Could not open input file");
    avformat_close_input(&input_format_context_);
    avformat_free_context(input_format_context_);
    return 0;
  }

  if(avformat_find_stream_info(input_format_context_, 0) < 0) {
    PLOGE("Failed to retrieve input stream information");
    return 0;
  }

  input_time_base_ = input_format_context_->streams[0]->time_base;
  calc_duration_ = (double)AV_TIME_BASE/av_q2d(input_format_context_->streams[0]->r_frame_rate);
  duration_ = (double)calc_duration_/(double)(av_q2d(input_time_base_)*AV_TIME_BASE);
  av_dict_set(&opts_, "movflags", "frag_keyframe+empty_moov+default_base_moof", 0);

  return 1;
};


void Recorder::CloseInput() {
  avformat_close_input(&input_format_context_);
  avformat_free_context(input_format_context_);
}

int Recorder::Start() {
  OpenInput();
  recording_status_ = true;
  while(recording_status_) {
    Mux();
  }
  CloseInput();
  return 0;
}

int Recorder::Mux() {

  UpdateSinkName();

  AVPacket packet;
  AVStream *video_stream = NULL;
  AVFormatContext *output_format_context = NULL;
  AVRational output_time_base;
  int ret;
  int frame_index=0;

  do {

    avformat_alloc_output_context2(&output_format_context, NULL, NULL, sink_name_);

    video_stream = avformat_new_stream(output_format_context, NULL);
    if(video_stream == NULL) {
      PLOGE("Failed to create video stream\n");
      break;
    }

    avcodec_parameters_copy(video_stream->codecpar, input_format_context_->streams[0]->codecpar);
    av_dump_format(output_format_context, 0, sink_name_, 1);
    avio_open(&output_format_context->pb, sink_name_, AVIO_FLAG_WRITE);

    ret = avformat_write_header(output_format_context, &opts_);

    if(ret < 0) {
      PLOGE("Error occurred when opening output file\n");
      break;
    }
    output_time_base = output_format_context->streams[0]->time_base;

    while(recording_status_) {

      if(frame_index > max_frame_num_) {
        PLOGI("Exceed maximum of frames");
        break;
      }

      ret = av_read_frame(input_format_context_, &packet);
      if(ret < 0)
        break;

      packet.stream_index = 0;
      packet.pts = (double)(frame_index*calc_duration_)/(double)(av_q2d(input_time_base_)*AV_TIME_BASE);
      packet.dts = packet.pts;
      packet.duration = duration_;
      frame_index++;

      packet.pts = av_rescale_q_rnd(packet.pts, input_time_base_, output_time_base, static_cast<AVRounding>(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
      packet.dts = av_rescale_q_rnd(packet.dts, input_time_base_, output_time_base, static_cast<AVRounding>(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
      packet.duration = av_rescale_q(packet.duration, input_time_base_, output_time_base);
      packet.pos = -1;

      ret = av_interleaved_write_frame(output_format_context, &packet);

      if(ret < 0) {
        PLOGE("Error muxing packet\n");
        break;
      }
      av_packet_unref(&packet);
      usleep(30000);
    }
    av_write_trailer(output_format_context);

  } while(0);


  if(output_format_context)
    avio_close(output_format_context->pb);
  avformat_free_context(output_format_context);

  return 0;
}
