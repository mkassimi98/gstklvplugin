# Author: Mouhsine Kassimi Farhaoui
# Mail: mouhsine98@gmail.com

include(GstKlvExample)

gstklv_add_example(gstklv_srt_sender
  SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/srt-pipelines/cpp/srt_sender_95tags.cpp"
  LIBRARIES gstklv::klv
)

gstklv_add_example(gstklv_udp_sender
  SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/udp-pipelines/cpp/udp_sender_95tags.cpp"
  LIBRARIES gstklv::klv
)

gstklv_add_example(gstklv_srt_receiver
  SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/srt-pipelines/cpp/srt_receiver_95tags.cpp"
  LIBRARIES gstklv::klv
)

gstklv_add_example(gstklv_udp_receiver
  SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/udp-pipelines/cpp/udp_receiver_95tags.cpp"
  LIBRARIES gstklv::klv
)

gstklv_add_example(gstklv_ts_recorder
  SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/ts/cpp/ts_recorder_95tags.cpp"
  USE_GST_APP
  LIBRARIES gstklv::klv
)

gstklv_add_example(gstklv_ts_reader
  SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/ts/cpp/ts_video_reader_95tags.cpp"
  USE_GST_APP
  LIBRARIES
    gstklv::klv
    gstklv::ts
)
