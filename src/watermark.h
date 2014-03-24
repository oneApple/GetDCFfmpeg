/*
 * watermark.h
 *
 *  Created on: 2013-9-21
 *      Author: zhangnh
 */

#ifndef WATERMARK_H_
#define WATERMARK_H_

extern int IncreFrameNum(void);
extern int watermark_init(char * filename);
extern int embed_watermark(uint8_t * y,int linesize,int width,int height);
extern int extract_watermark(uint8_t * y,int linesize,int width,int height);
extern int write_watermark_file(char * filename);
//int embed;

//char watermark_file[250];   //存放水印文件名
//static int encode_count = 0;
//static int hdtv = 0;  //高清
//static int sdtv = 0;   //标请
//static int border = 32;  //边界大小
//static int embed_count = 0;  //标记是否为第一次进入块，进入编码中改变值


#endif /* WATERMARK_H_ */
