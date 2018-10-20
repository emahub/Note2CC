/*
Copyright (C) 2016 Apple Inc. All Rights Reserved.
See LICENSE.txt for this sample’s licensing information

Abstract:
MIDI Processor AU
*/

#ifndef __Note2CCVersion_h__
#define __Note2CCVersion_h__

#ifdef DEBUG
	#define kNote2CCVersion    0xFFFFFFFF
#else
	#define kNote2CCVersion    0x00010000
#endif

#define Note2CC_COMP_TYPE      'aumi'
#define Note2CC_COMP_SUBTYPE   'aump'
#define Note2CC_COMP_MANF      'appl'

#endif

