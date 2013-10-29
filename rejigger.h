#ifndef __REJIGGER_H__
#define __REJIGGER_H__

#include <libavutil/frame.h>

void rejigger_small_frame(AVFrame *in_frame, AVFrame *out_frame);

void unjigger_small_frame(AVFrame *in_frame, AVFrame *out_frame);

#endif//__REJIGGER_H__
