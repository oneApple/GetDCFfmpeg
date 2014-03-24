/*
 * watermark.c
 *
 *  Created on: 2013-9-21
 *      Author: zhangnh
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include "watermark.h"
#include "libavutil/mem.h"

#define WATERMARK_HEAD_INFO_FREQ 20
#define LENGTH_FREQ 32
#define FREQ_FREQ 16

static int mark_end = 0;
static int extract_malloc = 1;  //用于在提取水印中，给变量分配空间
char * scramb_data;   //存放置乱后的水印数据
FILE * o1;
FILE * o2;
FILE * w1;
FILE * w2;
FILE * w3;
FILE * w4;

typedef struct WaterMarkInfo
{
	int flag_mark;   //标记64个1是否嵌入完成
	int flag_length;    //标记32位的长度信息是否嵌入完成
	int flag_end;     //标记一帧是否将水印嵌入完
	int flag_freq;
	char length_data[LENGTH_FREQ];
	char freq_data[FREQ_FREQ];
	int length;      //标记水印的长度，是扩频之后的长度
	int freq;      //标记扩频的大小  5
	int length_tmp;    //水印长度，中间变量
	char * mark;    //水印信息
	char * tmp;     //中间变量，指向水印
}WaterMarkInfo;

WaterMarkInfo * wmark;

typedef struct ExtractWaterMarkInfo
{
	int extract_finish;
	int flag;
	int flag_mark;
	int flag_length;
	int flag_freq;
	int flag_end;
	int length;
	int length1;
	int freq;
	char * mark;
	char * tmp;
	char mark_data[64];
	char length_data[LENGTH_FREQ];
	char freq_data[FREQ_FREQ];
	char end_data[16];
}ExtractWaterMarkInfo;

ExtractWaterMarkInfo * extractwmark;
#define W1 2841					/* 2048*sqrt(2)*cos(1*pi/16) */
#define W2 2676					/* 2048*sqrt(2)*cos(2*pi/16) */
#define W3 2408					/* 2048*sqrt(2)*cos(3*pi/16) */
#define W5 1609					/* 2048*sqrt(2)*cos(5*pi/16) */
#define W6 1108					/* 2048*sqrt(2)*cos(6*pi/16) */
#define W7 565					/* 2048*sqrt(2)*cos(7*pi/16) */

#define USE_ACCURATE_ROUNDING

#define RIGHT_SHIFT(x, shft)  ((x) >> (shft))

#ifdef USE_ACCURATE_ROUNDING
#define ONE ((int) 1)
#define DESCALE(x, n)  RIGHT_SHIFT((x) + (ONE << ((n) - 1)), n)
#else
#define DESCALE(x, n)  RIGHT_SHIFT(x, n)
#endif

#define CONST_BITS  13
#define PASS1_BITS  2

#define FIX_0_298631336  ((int)  2446)	/* FIX(0.298631336) */
#define FIX_0_390180644  ((int)  3196)	/* FIX(0.390180644) */
#define FIX_0_541196100  ((int)  4433)	/* FIX(0.541196100) */
#define FIX_0_765366865  ((int)  6270)	/* FIX(0.765366865) */
#define FIX_0_899976223  ((int)  7373)	/* FIX(0.899976223) */
#define FIX_1_175875602  ((int)  9633)	/* FIX(1.175875602) */
#define FIX_1_501321110  ((int) 12299)	/* FIX(1.501321110) */
#define FIX_1_847759065  ((int) 15137)	/* FIX(1.847759065) */
#define FIX_1_961570560  ((int) 16069)	/* FIX(1.961570560) */
#define FIX_2_053119869  ((int) 16819)	/* FIX(2.053119869) */
#define FIX_2_562915447  ((int) 20995)	/* FIX(2.562915447) */
#define FIX_3_072711026  ((int) 25172)	/* FIX(3.072711026) */

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <sys/types.h>
#include <sys/stat.h>

static char *file = NULL;
static char *dir = NULL;
static FILE *fp = NULL;
char *gfilename;
static int iframe = 0;
static int idcnum = 0;
static int lastframe = 0;

static char *getfilename() {
	char *first = strrchr(gfilename, '/') + 1;
	char *last = strchr(gfilename, '.');
	int len = last - first;
	char *filename = malloc(len + 1);
	memset(filename, 0, len + 1);
	strncpy(filename, first, len);
	printf("%s,%s,%s,%s\n", gfilename, first, last, filename);
	return filename;
}

//
static int CreateFrameFile(int frame_type) {
	//if (frame_type == 1) //如果是i帧
	//{
	if (file == NULL) {
		char *filename = getfilename();
		mkdir(filename, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
		int filenamelen = strlen(filename);
		dir = malloc(filenamelen + 12 + 8);
		strncpy(dir, filename, filenamelen);
		file = dir + filenamelen;
		free(filename);
	}
	sprintf(file, "/%d", iframe);
	strcat(file, ".iydc");

	if ((fp = fopen(dir, "w")) == NULL) /*打开只写的文本文件*/
	{
		return 1;
	}
	//}
	//iframe++;
	return 0;
}
//读取水印文件的函数
int watermark_init(char * filename)
{
    int i,j,k;
    char ctemp;
    FILE *fd;   //打开水印文件
    int file_length;    //水印文件的字符大小
    char * freq_tmp;    //存放转换为二进制并扩频后的字符
    char *file_tmp;    //存放水印文件中的字符
    int freq_length_tmp = 15;
    wmark = malloc(sizeof(WaterMarkInfo));
    wmark->flag_end = 0;
    wmark->flag_length = 0;
    wmark->flag_mark = 0;
    wmark->flag_freq = 0;
    wmark->freq = freq_length_tmp;


    //从水印文件中读取水印信息
    fd = fopen(filename,"rb");
    if( !fd )
    {
    	printf("open watermark file error");
    	return 0;
    }
    fseek(fd, 0, SEEK_END);		// skip to end of file, to determine its size
    file_length = ftell(fd);
    fseek(fd, 0, SEEK_SET);		// and skip back!

    file_tmp = (char*)malloc(sizeof(char)*file_length+1);
    if(!file_tmp)
    {
    	printf("Cannot load file into memory!");
    	return 0;
    }
    if(fread(file_tmp,sizeof(char),file_length,fd)!=file_length)
    {
    	printf("Filelength and readable length do not concur...\n\tFile may be corrupted or was badly read...");
    }
    file_tmp[file_length] = '\0';
    fclose(fd);

    //转换为二进制并扩频
    //freq_tmp = malloc( sizeof(char) * file_length * freq_length_tmp*8 +1);
    wmark->mark = malloc(sizeof(char) * file_length * freq_length_tmp*8 +1);
    freq_tmp = wmark->mark;
    wmark->tmp = wmark->mark;

    for(i=0;i<file_length;i++)
    {
    	for(j=0;j<8;j++)
    	{
    		ctemp = *file_tmp;
    		ctemp = (ctemp>>(7-j))&0x01;

    		for(k=0;k<freq_length_tmp;k++)
    		{
    			*freq_tmp = ctemp+48;  //'0'=48 ,'1'= 49
    			freq_tmp++;
    		}
    	}
    	file_tmp++;
    }

    wmark->length = freq_length_tmp *file_length *8;
    wmark->length_tmp = wmark->length;

    for(i = 0;i< LENGTH_FREQ ;i++)//表示长度的数据放入all_length_data中
    {
    	wmark->length_data[i] = (wmark->length>>i)&0x00000001;
    }
    for(i = 0;i< FREQ_FREQ ;i++)//表示长度的数据放入all_length_data中
    {
        wmark->freq_data[i] = (wmark->freq>>i)&0x00000001;
    }
    file_tmp = NULL;
    return 1;
}

static char * inverse_scrambling(char * s,int length,int freq)
{
	int i,j;
	int scramb_len;
	char * scramb;
	char * scramb1;
	scramb = (char *)malloc(length*sizeof(char));
	if(scramb == NULL)
		return NULL;

	scramb1 = scramb;
	scramb_len = length/freq;

	for(j=0;j<scramb_len;j++)
		for(i=0;i<freq;i++)
		{
			*(scramb1++) = *(s+i*scramb_len+j);
		}
	//memcpy(*scramb, s, length);

	return scramb;
}

int write_watermark_file(char * filename)
{
	int efreq;
	int elength;
	FILE * efd1 = NULL;
	char * write_data = NULL;
	int len_freq;
	int i,j;
	char * scramb_tmp;
	int count = 0;
	char tmp = '0';
	int k=0;

	if(extractwmark->extract_finish == 1)
	{
		printf("enter the write file\n");
		elength = extractwmark->length;
		efreq = extractwmark->freq;
		extractwmark->tmp = extractwmark->mark;
		printf("elength:%d,efreq:%d\n",elength,efreq);
		if(elength<=0 || efreq<=0 || efreq>elength || efreq >20)
		{
			printf("length/freq is not rational\n");
			return 1;
		}
		len_freq = elength/efreq;

		//反置乱水印数据
		scramb_data = inverse_scrambling(extractwmark->mark,elength,extractwmark->freq);
		if(scramb_data == NULL)
		{
			printf("malloc scramb is error\n");
			return 1;
		}
		scramb_tmp = scramb_data;

		write_data = (char *)av_malloc(sizeof(char)*len_freq+1);
		if(write_data == NULL)
		{
			printf("write_data is NULL\n");
			return 0;
		}

		//打开输出水印的文件
    	efd1 = fopen(filename,"a+");
    	if(!efd1)
    	{
    	   printf("write watermark file error %s\n", filename);
    	}

    	//将读取的水印信息转换为去掉扩频的bit
    	for(i=0;i<len_freq;i++)
    	{
    		for(j=0;j<efreq;j++)
    		{
    			if(*scramb_data == '1')//(*(extractwmark->mark) == '1')
    				count++;
    			if(j == extractwmark->freq-1)
    			{
    				if(2*count >= efreq)
    					write_data[k++] = '1';
    				else
    					write_data[k++] = '0';
    				count = 0;
    			}
    			scramb_data++;
    		}
    	}

    	//将数据转换为字符，写入文件
    	for(i=0;i<len_freq/8;i++)
    	{
    		for(j=0;j<8;j++)
    		{
    			if(write_data[i*8+j] == '1')
    				tmp = tmp | 0x01;
    			else
    				tmp = tmp & 0xfe;
    			if(j != 7)
    				tmp = tmp<<1;
    		}
    		fputc(tmp,efd1);
    	}

    	free(scramb_tmp);
    	scramb_tmp = NULL;
    	scramb_data = NULL;
    	fclose(efd1);
    	efd1 = NULL;
    	free(write_data);
    	write_data = NULL;
    	free(extractwmark->tmp);
    	extractwmark->tmp = NULL;
    	extractwmark->mark = NULL;
	}
	return 1;
}


//******************************************************************************************************************
static void fdct_int32(short * block)
{
	int tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
	int tmp10, tmp11, tmp12, tmp13;
	int z1, z2, z3, z4, z5;
	short *blkptr;
	int *dataptr;
	int data[64];
	int i;

	/* Pass 1: process rows. */
	/* Note results are scaled up by sqrt(8) compared to a true DCT; */
	/* furthermore, we scale the results by 2**PASS1_BITS. */

	dataptr = data;
	blkptr = block;
	for (i = 0; i < 8; i++) {
		tmp0 = blkptr[0] + blkptr[7];
		tmp7 = blkptr[0] - blkptr[7];
		tmp1 = blkptr[1] + blkptr[6];
		tmp6 = blkptr[1] - blkptr[6];
		tmp2 = blkptr[2] + blkptr[5];
		tmp5 = blkptr[2] - blkptr[5];
		tmp3 = blkptr[3] + blkptr[4];
		tmp4 = blkptr[3] - blkptr[4];

		/* Even part per LL&M figure 1 --- note that published figure is faulty;
		 * rotator "sqrt(2)*c1" should be "sqrt(2)*c6".
		 */

		tmp10 = tmp0 + tmp3;
		tmp13 = tmp0 - tmp3;
		tmp11 = tmp1 + tmp2;
		tmp12 = tmp1 - tmp2;

		dataptr[0] = (tmp10 + tmp11) << PASS1_BITS;
		dataptr[4] = (tmp10 - tmp11) << PASS1_BITS;

		z1 = (tmp12 + tmp13) * FIX_0_541196100;
		dataptr[2] =
			DESCALE(z1 + tmp13 * FIX_0_765366865, CONST_BITS - PASS1_BITS);
		dataptr[6] =
			DESCALE(z1 + tmp12 * (-FIX_1_847759065), CONST_BITS - PASS1_BITS);

		/* Odd part per figure 8 --- note paper omits factor of sqrt(2).
		 * cK represents cos(K*pi/16).
		 * i0..i3 in the paper are tmp4..tmp7 here.
		 */

		z1 = tmp4 + tmp7;
		z2 = tmp5 + tmp6;
		z3 = tmp4 + tmp6;
		z4 = tmp5 + tmp7;
		z5 = (z3 + z4) * FIX_1_175875602;	/* sqrt(2) * c3 */

		tmp4 *= FIX_0_298631336;	/* sqrt(2) * (-c1+c3+c5-c7) */
		tmp5 *= FIX_2_053119869;	/* sqrt(2) * ( c1+c3-c5+c7) */
		tmp6 *= FIX_3_072711026;	/* sqrt(2) * ( c1+c3+c5-c7) */
		tmp7 *= FIX_1_501321110;	/* sqrt(2) * ( c1+c3-c5-c7) */
		z1 *= -FIX_0_899976223;	/* sqrt(2) * (c7-c3) */
		z2 *= -FIX_2_562915447;	/* sqrt(2) * (-c1-c3) */
		z3 *= -FIX_1_961570560;	/* sqrt(2) * (-c3-c5) */
		z4 *= -FIX_0_390180644;	/* sqrt(2) * (c5-c3) */

		z3 += z5;
		z4 += z5;

		dataptr[7] = DESCALE(tmp4 + z1 + z3, CONST_BITS - PASS1_BITS);
		dataptr[5] = DESCALE(tmp5 + z2 + z4, CONST_BITS - PASS1_BITS);
		dataptr[3] = DESCALE(tmp6 + z2 + z3, CONST_BITS - PASS1_BITS);
		dataptr[1] = DESCALE(tmp7 + z1 + z4, CONST_BITS - PASS1_BITS);

		dataptr += 8;			/* advance pointer to next row */
		blkptr += 8;
	}

	/* Pass 2: process columns.
	 * We remove the PASS1_BITS scaling, but leave the results scaled up
	 * by an overall factor of 8.
	 */

	dataptr = data;
	for (i = 0; i < 8; i++) {
		tmp0 = dataptr[0] + dataptr[56];
		tmp7 = dataptr[0] - dataptr[56];
		tmp1 = dataptr[8] + dataptr[48];
		tmp6 = dataptr[8] - dataptr[48];
		tmp2 = dataptr[16] + dataptr[40];
		tmp5 = dataptr[16] - dataptr[40];
		tmp3 = dataptr[24] + dataptr[32];
		tmp4 = dataptr[24] - dataptr[32];

		/* Even part per LL&M figure 1 --- note that published figure is faulty;
		 * rotator "sqrt(2)*c1" should be "sqrt(2)*c6".
		 */

		tmp10 = tmp0 + tmp3;
		tmp13 = tmp0 - tmp3;
		tmp11 = tmp1 + tmp2;
		tmp12 = tmp1 - tmp2;

		dataptr[0] = DESCALE(tmp10 + tmp11, PASS1_BITS);
		dataptr[32] = DESCALE(tmp10 - tmp11, PASS1_BITS);

		z1 = (tmp12 + tmp13) * FIX_0_541196100;
		dataptr[16] =
			DESCALE(z1 + tmp13 * FIX_0_765366865, CONST_BITS + PASS1_BITS);
		dataptr[48] =
			DESCALE(z1 + tmp12 * (-FIX_1_847759065), CONST_BITS + PASS1_BITS);

		/* Odd part per figure 8 --- note paper omits factor of sqrt(2).
		 * cK represents cos(K*pi/16).
		 * i0..i3 in the paper are tmp4..tmp7 here.
		 */

		z1 = tmp4 + tmp7;
		z2 = tmp5 + tmp6;
		z3 = tmp4 + tmp6;
		z4 = tmp5 + tmp7;
		z5 = (z3 + z4) * FIX_1_175875602;	/* sqrt(2) * c3 */

		tmp4 *= FIX_0_298631336;	/* sqrt(2) * (-c1+c3+c5-c7) */
		tmp5 *= FIX_2_053119869;	/* sqrt(2) * ( c1+c3-c5+c7) */
		tmp6 *= FIX_3_072711026;	/* sqrt(2) * ( c1+c3+c5-c7) */
		tmp7 *= FIX_1_501321110;	/* sqrt(2) * ( c1+c3-c5-c7) */
		z1 *= -FIX_0_899976223;	/* sqrt(2) * (c7-c3) */
		z2 *= -FIX_2_562915447;	/* sqrt(2) * (-c1-c3) */
		z3 *= -FIX_1_961570560;	/* sqrt(2) * (-c3-c5) */
		z4 *= -FIX_0_390180644;	/* sqrt(2) * (c5-c3) */

		z3 += z5;
		z4 += z5;

		dataptr[56] = DESCALE(tmp4 + z1 + z3, CONST_BITS + PASS1_BITS);
		dataptr[40] = DESCALE(tmp5 + z2 + z4, CONST_BITS + PASS1_BITS);
		dataptr[24] = DESCALE(tmp6 + z2 + z3, CONST_BITS + PASS1_BITS);
		dataptr[8] = DESCALE(tmp7 + z1 + z4, CONST_BITS + PASS1_BITS);

		dataptr++;				/* advance pointer to next column */
	}
	/* descale */
	for (i = 0; i < 64; i++)
		block[i] = (short int) DESCALE(data[i], 3);
}

static void idct_int32(short * block)
{
	int j;
	static short iclip[1024];		/* clipping table */
	static short *iclp;
    static short *blk;
	static long i;
	static long X0, X1, X2, X3, X4, X5, X6, X7, X8;

	iclp = iclip + 512;
	for (j = -512; j < 512; j++)
		iclp[j] = (j < -256) ? -256 : ((j > 255) ? 255 : j);

	for (i = 0; i < 8; i++)		/* idct rows */
	{
		blk = block + (i << 3);
		if (!
			((X1 = blk[4] << 11) | (X2 = blk[6]) | (X3 = blk[2]) | (X4 =
																	blk[1]) |
			 (X5 = blk[7]) | (X6 = blk[5]) | (X7 = blk[3]))) {
			blk[0] = blk[1] = blk[2] = blk[3] = blk[4] = blk[5] = blk[6] =
				blk[7] = blk[0] << 3;
			continue;
		}

		X0 = (blk[0] << 11) + 128;	/* for proper rounding in the fourth stage  */

		/* first stage  */
		X8 = W7 * (X4 + X5);
		X4 = X8 + (W1 - W7) * X4;
		X5 = X8 - (W1 + W7) * X5;
		X8 = W3 * (X6 + X7);
		X6 = X8 - (W3 - W5) * X6;
		X7 = X8 - (W3 + W5) * X7;

		/* second stage  */
		X8 = X0 + X1;
		X0 -= X1;
		X1 = W6 * (X3 + X2);
		X2 = X1 - (W2 + W6) * X2;
		X3 = X1 + (W2 - W6) * X3;
		X1 = X4 + X6;
		X4 -= X6;
		X6 = X5 + X7;
		X5 -= X7;

		/* third stage  */
		X7 = X8 + X3;
		X8 -= X3;
		X3 = X0 + X2;
		X0 -= X2;
		X2 = (181 * (X4 + X5) + 128) >> 8;
		X4 = (181 * (X4 - X5) + 128) >> 8;

		/* fourth stage  */

		blk[0] = (short) ((X7 + X1) >> 8);
		blk[1] = (short) ((X3 + X2) >> 8);
		blk[2] = (short) ((X0 + X4) >> 8);
		blk[3] = (short) ((X8 + X6) >> 8);
		blk[4] = (short) ((X8 - X6) >> 8);
		blk[5] = (short) ((X0 - X4) >> 8);
		blk[6] = (short) ((X3 - X2) >> 8);
		blk[7] = (short) ((X7 - X1) >> 8);

	}							/* end for ( i = 0; i < 8; ++i ) IDCT-rows */



	for (i = 0; i < 8; i++)		/* idct columns */
	{
		blk = block + i;
		/* shortcut  */
		if (!
			((X1 = (blk[8 * 4] << 8)) | (X2 = blk[8 * 6]) | (X3 =
															 blk[8 *
																 2]) | (X4 =
																		blk[8 *
																			1])
			 | (X5 = blk[8 * 7]) | (X6 = blk[8 * 5]) | (X7 = blk[8 * 3]))) {
			blk[8 * 0] = blk[8 * 1] = blk[8 * 2] = blk[8 * 3] = blk[8 * 4] =
				blk[8 * 5] = blk[8 * 6] = blk[8 * 7] =
				iclp[(blk[8 * 0] + 32) >> 6];
			continue;
		}

		X0 = (blk[8 * 0] << 8) + 8192;

		/* first stage  */
		X8 = W7 * (X4 + X5) + 4;
		X4 = (X8 + (W1 - W7) * X4) >> 3;
		X5 = (X8 - (W1 + W7) * X5) >> 3;
		X8 = W3 * (X6 + X7) + 4;
		X6 = (X8 - (W3 - W5) * X6) >> 3;
		X7 = (X8 - (W3 + W5) * X7) >> 3;

		/* second stage  */
		X8 = X0 + X1;
		X0 -= X1;
		X1 = W6 * (X3 + X2) + 4;
		X2 = (X1 - (W2 + W6) * X2) >> 3;
		X3 = (X1 + (W2 - W6) * X3) >> 3;
		X1 = X4 + X6;
		X4 -= X6;
		X6 = X5 + X7;
		X5 -= X7;

		/* third stage  */
		X7 = X8 + X3;
		X8 -= X3;
		X3 = X0 + X2;
		X0 -= X2;
		X2 = (181 * (X4 + X5) + 128) >> 8;
		X4 = (181 * (X4 - X5) + 128) >> 8;

		/* fourth stage  */
		blk[8 * 0] = iclp[(X7 + X1) >> 14];
		blk[8 * 1] = iclp[(X3 + X2) >> 14];
		blk[8 * 2] = iclp[(X0 + X4) >> 14];
		blk[8 * 3] = iclp[(X8 + X6) >> 14];
		blk[8 * 4] = iclp[(X8 - X6) >> 14];
		blk[8 * 5] = iclp[(X0 - X4) >> 14];
		blk[8 * 6] = iclp[(X3 - X2) >> 14];
		blk[8 * 7] = iclp[(X7 - X1) >> 14];
	}
}
static void embed_mark(short *block)  //直接嵌入1
{
	short tmp;
	if(block[1] < block[8])
	{
		tmp = block[1];
		block[1] = block[8];
		block[8] = tmp;
		if(block[1] - block[8] < 50)
			block[8] -= 0;
	}
	else if(block[1] == block[8])
		block[1] += 15;
}

static void embed_length(short *block)  //嵌入水印长度信息
{
	short tmp;

	if(wmark->length_data[wmark->flag_length] == 1)
	{
		if(block[1] < block[8])
		{
			tmp = block[1];
			block[1] = block[8];
			block[8] = tmp;
			if(block[1] - block[8] < 50)
				block[8] -= 0;
		}
		else if(block[1] == block[8])
			block[1] += 15;
	}
	else if(wmark->length_data[wmark->flag_length] == 0)
	{
		if(block[1] > block[8])
	    {
			tmp = block[1];
		    block[1] = block[8];
		    block[8] = tmp;
			if(block[1] - block[8] < 50)
				block[1] -= 0;
	    }
	   else if(block[1] == block[8])
		    block[1] -= 15;
	}
}
static void embed_freq(short *block)  //嵌入水印长度信息
{
	short tmp;

	if(wmark->freq_data[wmark->flag_freq] == 1)
	{
		if(block[1] < block[8])
		{
			tmp = block[1];
			block[1] = block[8];
			block[8] = tmp;
			if(block[1] - block[8] < 50)
				block[8] -= 0;
		}
		else if(block[1] == block[8])
			block[1] += 15;
	}
	else if(wmark->freq_data[wmark->flag_freq] == 0)
	{
		if(block[1] > block[8])
	    {
			tmp = block[1];
		    block[1] = block[8];
		    block[8] = tmp;
			if(block[8] - block[1] < 50)
				block[1] -= 0;
	    }
	   else if(block[1] == block[8])
		    block[1] -= 15;
	}
}
static void embed_end(short *block)  //直接嵌入1
{
	short tmp;
	if(mark_end)
	{
		if(block[1] < block[8])
		{
			tmp = block[1];
			block[1] = block[8];
			block[8] = tmp;
			if(block[1] - block[8] < 50)
				block[8] -= 0;
		}
		else if(block[1] == block[8])
			block[1] += 15;
	}
	else
	{
		if(block[1] > block[8])
		{
			tmp = block[1];
			block[1] = block[8];
			block[8] = tmp;
			if(block[8] - block[1] < 50)
				block[1] -= 0;
		}
		else if(block[1] == block[8])
			block[1] -= 15;
	}
}
static void embed_water_mark(short *block)
{
	short tmp;
	if(*scramb_data == '1')  //嵌入1 block[0][1] > block[1][0]
	{
		if(block[1] < block[8])
		{
			tmp = block[1];
			block[1] = block[8];
			block[8] = tmp;
			if(block[1] - block[8] < 50)
				block[8] -= 0;
		}
		else if(block[1] == block[8])
			block[1] += 15;
	}
	else  //嵌入0
	{
		if(block[1] > block[8])
		{
			tmp = block[1];
			block[1] = block[8];
			block[8] = tmp;
			if(block[8] - block[1] < 50)
				block[1] -= 0;
		}
		else if(block[1] == block[8])
			block[1] -= 15;
	}
	//wmark->tmp++;
}
static int sjisuan(short* block,int ti,int tj)
{
	int dc,mean,u;
	int i,j;

	mean = 0;
	u = 0;
	dc = block[0];

	for(i=0;i<8;i++)
		for(j=0;j<8;j++)
			mean += block[i*8+j];
	mean = mean/64;

	for(i=0;i<8;i++)
		for(j=0;j<8;j++)
			u += (block[i*8+j] - mean)*(block[i*8+j] - mean);
	u = u/64;

	//fprintf(o1,"(%d,%d) dc=%d,mean=%d,u=%d;",ti,tj,dc,mean,u);
	return mean;
}
static int cjisuan(short* block,int ti,int tj)
{
	int dc,mean,u;
	int i,j;

	mean = 0;
	u = 0;
	dc = block[0];

	for(i=0;i<8;i++)
		for(j=0;j<8;j++)
			mean += block[i*8+j];
	mean = mean/64;

	for(i=0;i<8;i++)
		for(j=0;j<8;j++)
			u += (block[i*8+j] - mean)*(block[i*8+j] - mean);
	u = u/64;

	fprintf(o2,"(%d,%d) dc=%d,mean=%d,u=%d;",ti,tj,dc,mean,u);
	return 1;
}
static int write_block_data(short* block,int ti,int tj,FILE *f)
{
	int i,j;
	fprintf(f,"(i=%d,j=%d):",ti,tj);
	for(i=0;i<8;i++)
	{
		for(j=0;j<8;j++)
		{
			fprintf(f,"%d-",block[i*8+j]);
		}
	}
	fputc('\r',f);
	fputc('\n',f);
	return 1;
}


static char* scrambling(char * s,int length,int freq)
{
	int i,j;
	int scramb_len;
	char * scramb;
	char * scramb1;
	scramb = (char *)malloc(length*sizeof(char));
	if(scramb == NULL)
		return NULL;

	scramb1 = scramb;
	scramb_len = length/freq;

	for(j=0;j<freq;j++)
		for(i=0;i<scramb_len;i++)
		{
			*(scramb1++) = *(s+i*freq+j);
		}
	//memcpy(*scramb, s, length);

	return scramb;
}

int IncreFrameNum(void)
{
	iframe ++ ;
	return 0;
}

int embed_watermark(uint8_t * y,int linesize,int width,int height)
{
	int i,j,t,s;
	int w,h;
	int useful_length;
	unsigned char block[8*8];
	short block_tmp[8*8];                           //用于DCT的块数据
	uint8_t *ybuffer[1080];
                   //头信息的扩频大小

	w = width;                               //1920
	h = height;                              //1080

	//keym
	CreateFrameFile(1);
    //keym
    for(i=0;i<h;i++)
    {
    	ybuffer[i] = y+i*linesize;
    }

	//按8*8的块进行DCT变换
	for(i=0;i<h/8;i++)
	{	for(j=0;j<w/8;j++)
		{
			for(t=0;t<8;t++)//获得块的数据
				for(s=0;s<8;s++)
				{
					block[t*8+s] = *(ybuffer[i*8+t]+j*8+s);
				}

			for(t=0;t<64;t++)
			{
				block_tmp[t] = block[t];

			}
			fdct_int32(block_tmp);//将块的数据进行DCT变换
			fprintf(fp, "%d,", block_tmp[0]);
		}
		fputs("\n", fp);
	} //for

	if (fp != NULL)
	{
		fprintf(fp, "%d,%d\n", w/8, h/8);
		fclose(fp); /*关文件*/
		fp = NULL;
	}

    for(i=0;i<h;i++)
    {
    	ybuffer[i] = NULL;//(unsigned char *)malloc(w*sizeof(unsigned char *));
    }

	return 1;
}

//**************************************************************************************//
static int temp20 = WATERMARK_HEAD_INFO_FREQ;
char temp[WATERMARK_HEAD_INFO_FREQ];
static void extract_mark(short * block)
{
	if(block[1] > block[8])
		extractwmark->mark_data[extractwmark->flag_mark] = 1;
	else if(block[1] < block[8])
		extractwmark->mark_data[extractwmark->flag_mark] = 0;
	else
		extractwmark->mark_data[extractwmark->flag_mark] = '*';
}
static void extract_length(short * block)
{
	if(block[1] > block[8])
		temp[temp20] = 1;
		//extractwmark->length_data[extractwmark->flag_length] = 1;
	else
		temp[temp20] = 0;
		//extractwmark->length_data[extractwmark->flag_length] = 0;
}
static void extract_freq(short * block)
{
	if(block[1] > block[8])
		temp[temp20] = 1;
		//extractwmark->freq_data[extractwmark->flag_freq] = 1;
	else
		temp[temp20] = 0;
		//extractwmark->freq_data[extractwmark->flag_freq] = 0;
}
static void extract_end(short * block)
{
	if(block[1] > block[8])
		extractwmark->end_data[extractwmark->flag_end] = 1;
	else
		extractwmark->end_data[extractwmark->flag_end] = 0;
}
static void extract_water_mark(short * block)
{
	if(block[1] > block[8])
		*(extractwmark->tmp) = '1';
	else
		*(extractwmark->tmp) = '0';

	extractwmark->tmp++;
}

int extract_watermark(uint8_t * y,int linesize,int width,int height)
{
	int i,j,t,s;
	int w,h;
	int tmp1,tmp2;
	int c;
	int count1;
	unsigned char block[8*8];
	short block_tmp[8*8];
    uint8_t *ybuffer[1080];
    int dc = 0;
    int low10 = 0;
	tmp1 = 0;
	tmp2 = 0;
	temp20 = 20;
	w = width;//1920
	h = height;//1080


    for(i=0;i<h;i++)
    {
    	ybuffer[i] = y+i*linesize;//(unsigned char *)malloc(w*sizeof(unsigned char *));
    	//memset(ybuffer[i],'\0',w*sizeof(unsigned char *));
    }

    if(extract_malloc)
    {
    	extractwmark = malloc(sizeof(ExtractWaterMarkInfo));
    	extractwmark->extract_finish = 0;
    	extract_malloc = 0;
    }
    //标记位的初始化
    extractwmark->flag = 1;
    extractwmark->flag_end = 16;
    extractwmark->flag_freq = FREQ_FREQ;
    extractwmark->flag_length = LENGTH_FREQ;
    extractwmark->flag_mark = 64;
    extractwmark->mark = NULL;
    extractwmark->tmp = NULL;


    extractwmark->freq = 0;
    extractwmark->length = 0;
    extractwmark->length1 = 0;
    extractwmark->extract_finish = 0;
	//将data[0]中的数据放到二维数组中
//	for(i=0;i<h;i++)
//	{
//		for(j=0;j<w;j++)
//		{
//			*(ybuffer[i]+j) = (unsigned char)*(y+i*(w+32)+j);//ybuffer[i][j] = picture->data[0][i*w+j];
//		}
//	}

	//按8*8的块进行DCT变换
	for(i=0;i<h/8;i++)
		for(j=0;j<w/8;j++)
		{
			for(t=0;t<8;t++)//获得块的数据
				for(s=0;s<8;s++)
				{
					block[t*8+s] = *(ybuffer[i*8+t]+j*8+s);
				}

			low10 = 0;
			for(t=0;t<64;t++)
			{
				block_tmp[t] = block[t];
				if(block_tmp[t] <= 20)
					low10 = low10 + 1;
			}
			fdct_int32(block_tmp);//将块的数据进行DCT变换
			dc = block_tmp[0];

			//if(dc > 1000 && low10 <1)
			//{
				if(extractwmark->flag)
				{
					if(extractwmark->flag_mark > 0)
					{
						extractwmark->flag_mark--;
						//fdct_int32(block_tmp);//将块的数据进行DCT变换
						extract_mark(block_tmp);

						if(extractwmark->flag_mark == 0)
						{
							for(tmp1=0;tmp1<64;tmp1++)
							{	if(extractwmark->mark_data[tmp1] == 1)
									tmp2++;
							}
							printf("进入提取水印的过程:%d\n",tmp2);
							if( tmp2 < 45 )
							{
								extractwmark->flag = 0;
							}
						}
					}
					else if(extractwmark->flag_length >= 0)
					{
						if(temp20 == WATERMARK_HEAD_INFO_FREQ)
							extractwmark->flag_length--;
						temp20--;
						//fdct_int32(block_tmp);//将块的数据进行DCT变换
						extract_length(block_tmp);
						if(temp20 == 0)
						{
							count1= 0;
							for(c=0;c<WATERMARK_HEAD_INFO_FREQ;c++)
							{
								if(temp[c] == 1)
									count1++;
							}
							if(2*count1 >= WATERMARK_HEAD_INFO_FREQ)
								extractwmark->length_data[extractwmark->flag_length] = 1;
							else
								extractwmark->length_data[extractwmark->flag_length] = 0;
							temp20 = WATERMARK_HEAD_INFO_FREQ;
						}
						if(temp20 == WATERMARK_HEAD_INFO_FREQ && extractwmark->flag_length == 0)
							extractwmark->flag_length--;

						if(extractwmark->flag_length == -1)
						{
							for(tmp1=0;tmp1<LENGTH_FREQ;tmp1++)
							{
								if(extractwmark->length_data[LENGTH_FREQ-1-tmp1] == 0)
									extractwmark->length = (extractwmark->length) & 0xfffffffe;
								else if(extractwmark->length_data[LENGTH_FREQ-1-tmp1] == 1)
									extractwmark->length = (extractwmark->length) | 0x00000001;

								if(tmp1 != (LENGTH_FREQ-1))
									extractwmark->length = (extractwmark->length)<<1;
							}
							printf("提取的水印长度为:%d\n",extractwmark->length);
							extractwmark->mark = (char *)malloc(extractwmark->length*sizeof(char)+1);//*sizeof(char));
							if(extractwmark->mark == NULL)
							{
								printf("malloc is error\n");
								return 0;
							}
							extractwmark->tmp = extractwmark->mark;
							extractwmark->length1 = extractwmark->length;
						}
					}
					else if(extractwmark->flag_freq >= 0)
					{
						if(temp20 == WATERMARK_HEAD_INFO_FREQ)
							extractwmark->flag_freq--;
						temp20--;
						//fdct_int32(block_tmp);//将块的数据进行DCT变换
						extract_freq(block_tmp);
						if(temp20 == 0)
						{
							count1= 0;
							for(c=0;c<WATERMARK_HEAD_INFO_FREQ;c++)
							{
								if(temp[c] == 1)
									count1++;
							}
							if(2*count1 >= WATERMARK_HEAD_INFO_FREQ)
								extractwmark->freq_data[extractwmark->flag_freq] = 1;
							else
								extractwmark->freq_data[extractwmark->flag_freq] = 0;
							temp20 = WATERMARK_HEAD_INFO_FREQ;
						}
						if(temp20 == WATERMARK_HEAD_INFO_FREQ && extractwmark->flag_freq == 0)
							extractwmark->flag_freq--;

						if(extractwmark->flag_freq == -1)
						{
							for(tmp1=0;tmp1<FREQ_FREQ;tmp1++)
							{
								if(extractwmark->freq_data[FREQ_FREQ-1-tmp1] == 0)
									extractwmark->freq = (extractwmark->freq) & 0xFFfe;
								else if(extractwmark->freq_data[FREQ_FREQ-1-tmp1] == 1)
									extractwmark->freq = (extractwmark->freq) | 0x0001;

								if(tmp1 != (FREQ_FREQ-1))
									extractwmark->freq = (extractwmark->freq)<<1;
							}
							printf("提取的扩频大小为:%d\n",extractwmark->freq);
						}
					}
					else if(extractwmark->flag_end > 0)
					{
						extractwmark->flag_end--;
					//	fdct_int32(block_tmp);//将块的数据进行DCT变换
						extract_end(block_tmp);

					}
					else if(extractwmark->length1 > 0)
					{
					//	fdct_int32(block_tmp);
						//if(dc > 1000 && low10 < 1)
						    extract_water_mark(block_tmp);
						extractwmark->length1--;
						if(extractwmark->length1 == 0)
						{
							extractwmark->flag = 0;
							extractwmark->extract_finish = 1;
						}
					}
				}
			//}  //if(dc > 1000 && low10 <1)

		}
    for(i=0;i<h;i++)
    {
    	ybuffer[i] = NULL;//(unsigned char *)malloc(w*sizeof(unsigned char *));
    }
	return 1;
}

