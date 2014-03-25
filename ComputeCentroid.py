from __future__ import division 
import os
rootDir = "./gaoqing_mpeg1"
quanindex = 0
def WalkFile(rootDir):
	for root, dirs, files in os.walk(rootDir):
		for filepaths in files:
			yield os.path.join(root,filepaths)

def GetFrameNum(fileName):
	import string
	index = fileName[::-1].find("/")
	return string.atoi(fileName[-index:-5])
	
def GetFrameDCVector(fileName):
	DCVector = []
	with open(fileName,'r') as f:
		for lines in f:
			dctemp = lines.split(",")
			DCVector.append(dctemp)
		return DCVector

def ComputeSingleFrameCentroid(dcvector,qi):
	rownum = len(dcvector) - 1
	colnum = len(dcvector[0]) - 1
	m0 = 0
	mx = 0
	my = 0
	import math
	qi = math.sqrt(qi)
	for r in range(rownum):
		for c in range(colnum):
			import string
			idc = string.atoi(dcvector[r][c]) / pow(10,qi)
			m0 = m0 + idc
			mx = mx + idc * r
			my = my + idc * c
	return m0,mx/m0,my/m0

def GetKeyFrameMap(rootDir,qi):
	dcmap = {}
	for filename in WalkFile(rootDir):
		framenum = GetFrameNum(filename)
		dcv = GetFrameDCVector(filename)
                dcmap[framenum] = ComputeSingleFrameCentroid(dcv,qi)
	return dcmap
        
def CmpAdjacentFrame(f1,f2,qi):
	def getdiff(e1,e2):
		return pow((1 - (e2) / (e1)),2)
	qi = pow(10,qi)
	dm0 = getdiff(f1[0],f2[0]) * qi
	dmxy = getdiff(f1[1],f2[1]) + getdiff(f1[2],f2[2])
	dmxy = dmxy * qi
	return int(dm0),int(dmxy)

def GetQuanindex(width,height):
	macronum = width * height
	for index in range(2,10,2):
		if int(macronum / pow(10,index)) == 0:
			return index - 2

def GetThreshold(elist):
	total = 0;
	for e in elist:
		total = total + e
	return total / len(elist)

def ObtainKeyFrame(rootDir,width,height):
	qi = GetQuanindex(width,height)
	dcmap = GetKeyFrameMap(rootDir,qi)
	sortlist = sorted(dcmap)
	difmap = {}
	for li in range(1,len(sortlist)):
		lfi = sortlist[li - 1]
                cfi = sortlist[li]
		difmap[cfi] = CmpAdjacentFrame(dcmap[lfi],dcmap[cfi],qi)
	m0total = 0
	mxytotal = 0
	m0num = 0
	mxynum = 0
	for key in difmap:
		m0total = m0total + difmap[key][0]
		mxytotal = mxytotal + difmap[key][1]
		if difmap[key][0] != 0:
			m0num = m0num + 1
		if difmap[key][1] != 0:
			mxynum = mxynum + 1
		print key,difmap[key][0],difmap[key][1]
	m0average = m0total / m0num
	mxyaverage = mxytotal / mxynum
	print m0average,mxyaverage
	diflist = [1,]
        for key in difmap:
		if difmap[key][0] >= m0average or difmap[key][1] >= mxyaverage:
			diflist.append(key)
	return diflist



def test():
	print ObtainKeyFrame(rootDir,1920 / 8 ,1080 / 8)

test()
