/**
 * Copyright (C) Arm Limited 2010-2016. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef GATOR_ANNOTATE_H_
#define GATOR_ANNOTATE_H_

void gator_annotate_channel(int channel, const char *str);
void gator_annotate(const char *str);
void gator_annotate_channel_color(int channel, int color, const char *str);
void gator_annotate_color(int color, const char *str);
void gator_annotate_channel_end(int channel);
void gator_annotate_end(void);
void gator_annotate_name_channel(int channel, int group, const char *str);
void gator_annotate_name_group(int group, const char *str);
void gator_annotate_visual(const char *data, unsigned int length, const char *str);
void gator_annotate_marker(void);
void gator_annotate_marker_str(const char *str);
void gator_annotate_marker_color(int color);
void gator_annotate_marker_color_str(int color, const char *str);

#endif
