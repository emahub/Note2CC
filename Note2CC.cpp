/*
Copyright (C) 2016 Apple Inc. All Rights Reserved.
See LICENSE.txt for this sample’s licensing information

Abstract:
MIDI Processor AU
*/

#include "Note2CC.h"

#define kNoteOn 0x90
#define kNoteOff 0x80
#define kControlChange 0xb0

using namespace std;

static const int kMIDIPacketListSize = 2048;

AUDIOCOMPONENT_ENTRY(AUMIDIEffectFactory, Note2CC)

enum {
    kParameter_Ch = 0,
    kParameter_Note = 1,
    kParameter_CC = 2,
    kNumberOfParameters = 3
};

static const CFStringRef kParamName_Ch = CFSTR("Ch: ");
static const CFStringRef kParamName_Note = CFSTR("Note: ");
static const CFStringRef kParamName_CC = CFSTR("CC: ");

#pragma mark Note2CC

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Note2CC::SetProperty
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Note2CC::Note2CC(AudioUnit component) : AUMIDIEffectBase(component), mOutputPacketFIFO(LockFreeFIFO<MIDIPacket>(32))
{
#ifdef DEBUG_PRINT
    string bPath, bFullFileName;
    bPath = getenv("HOME");
    if (!bPath.empty()) {
        bFullFileName = bPath + "/Desktop/" + "Debug.log";
    } else {
        bFullFileName = "Debug.log";
    }
    
    baseDebugFile.open(bFullFileName.c_str());
    DEBUGLOG_B("Plug-in constructor invoked with parameters:" << endl);
#endif
    
	CreateElements();
    
    Globals()->UseIndexedParameters(kNumberOfParameters);
    Globals()->SetParameter(kParameter_Ch, 1);
    Globals()->SetParameter(kParameter_Note, 21);
    Globals()->SetParameter(kParameter_CC, 1);
    
    mMIDIOutCB.midiOutputCallback = nullptr;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//    Note2CC::GetParameterInfo
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus            Note2CC::GetParameterInfo(        AudioUnitScope                    inScope,
                                               AudioUnitParameterID            inParameterID,
                                               AudioUnitParameterInfo &        outParameterInfo)
{
    
#ifdef DEBUG_PRINT
    DEBUGLOG_B("GetParameterInfo - inScope: " << inScope << endl);
    DEBUGLOG_B("GetParameterInfo - inParameterID: " << inParameterID << endl);
#endif
    
    if (inScope != kAudioUnitScope_Global) return kAudioUnitErr_InvalidScope;
    
    outParameterInfo.flags += kAudioUnitParameterFlag_IsWritable;
    outParameterInfo.flags += kAudioUnitParameterFlag_IsReadable;
    
    switch (inParameterID) {
        case kParameter_Ch:
            AUBase::FillInParameterName(outParameterInfo, kParamName_Ch, false);
            outParameterInfo.unit = kAudioUnitParameterUnit_Indexed;
            outParameterInfo.minValue = 1;
            outParameterInfo.maxValue = 16;
            outParameterInfo.defaultValue = 1;
            break;
        case kParameter_CC:
            AUBase::FillInParameterName(outParameterInfo, kParamName_CC,
                                        false);
            outParameterInfo.unit = kAudioUnitParameterUnit_MIDINoteNumber;
            outParameterInfo.minValue = 1;
            outParameterInfo.maxValue = 127;
            outParameterInfo.defaultValue = 48;
            break;
        case kParameter_Note:
            AUBase::FillInParameterName(outParameterInfo, kParamName_Note,
                                        false);
            outParameterInfo.unit = kAudioUnitParameterUnit_Indexed;
            outParameterInfo.minValue = 1;
            outParameterInfo.maxValue = 127;
            outParameterInfo.defaultValue = 1;
            break;
        default:
            return kAudioUnitErr_InvalidParameter;
    }
    
    return noErr;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Note2CC::SetProperty
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Note2CC::~Note2CC()
{
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Note2CC::SetProperty
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus Note2CC::GetPropertyInfo(AudioUnitPropertyID inID, AudioUnitScope inScope, AudioUnitElement inElement, UInt32 & outDataSize, Boolean & outWritable)
{
    if (inScope == kAudioUnitScope_Global)
    {
        switch( inID )
        {
            case kAudioUnitProperty_MIDIOutputCallbackInfo:
                outWritable = false;
                outDataSize = sizeof(CFArrayRef);
                return noErr;
                
            case kAudioUnitProperty_MIDIOutputCallback:
                outWritable = true;
                outDataSize = sizeof(AUMIDIOutputCallbackStruct);
                return noErr;
        }
	}
	return AUMIDIEffectBase::GetPropertyInfo(inID, inScope, inElement, outDataSize, outWritable);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Note2CC::SetProperty
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus Note2CC::GetProperty( AudioUnitPropertyID inID, AudioUnitScope inScope, AudioUnitElement inElement, void* outData )
{
	if (inScope == kAudioUnitScope_Global) 
	{
        switch (inID)
		{
            case kAudioUnitProperty_MIDIOutputCallbackInfo:
                CFStringRef string = CFSTR("midiOut");
                CFArrayRef array = CFArrayCreate(kCFAllocatorDefault, (const void**)&string, 1, nullptr);
                CFRelease(string);
                *((CFArrayRef*)outData) = array;
                return noErr;
		}
	}
	return AUMIDIEffectBase::GetProperty(inID, inScope, inElement, outData);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Note2CC::SetProperty
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus Note2CC::SetProperty(	AudioUnitPropertyID inID, AudioUnitScope inScope, AudioUnitElement inElement, const void* inData, UInt32 inDataSize)
{
    if (inScope == kAudioUnitScope_Global)
    {
        switch (inID)
        {
            case kAudioUnitProperty_MIDIOutputCallback:
            mMIDIOutCB = *((AUMIDIOutputCallbackStruct*)inData);
            return noErr;
        }
    }
	return AUMIDIEffectBase::SetProperty(inID, inScope, inElement, inData, inDataSize);
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Note2CC::HandleMidiEvent
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus Note2CC::HandleMidiEvent(UInt8 status, UInt8 channel, UInt8 data1, UInt8 data2, UInt32 inOffsetSampleFrame)
{
    // data1 : note number, data2 : velocity
    
#ifdef DEBUG_PRINT
    DEBUGLOG_B("HandleMidiEvent - status:"
               << (int)status << " ch:" << (int)channel << "/"
               << (Globals()->GetParameter(kParameter_Ch) - 1)
               << " data1:" << (int)data1 << " data2:" << (int)data2 << endl);
#endif
    
    if (!IsInitialized()) return kAudioUnitErr_Uninitialized;
  
    MIDIPacket* packet = mOutputPacketFIFO.WriteItem();
    mOutputPacketFIFO.AdvanceWritePtr();
    
    if (packet == NULL)
        return kAudioUnitErr_FailedInitialization;
    
    
    /*
    memset(packet->data, 0, sizeof(Byte)*256);
    packet->length = 3;
    packet->data[0] = status | channel;
    packet->data[1] = data1;
    packet->data[2] = data2;
    packet->timeStamp = inOffsetSampleFrame;
     */
    
    // Handle Logic
    int _ch = Globals()->GetParameter(kParameter_Ch);
    int _note = Globals()->GetParameter(kParameter_Note);
    
    if (channel + 1 == _ch && (status == kNoteOn || status == kNoteOff) && data1 == _note)
    {
        int _cc = Globals()->GetParameter(kParameter_CC);
        
        memset(packet->data, 0, sizeof(Byte)*256);
        packet->length = 3;
        packet->data[0] = kControlChange | channel;
        packet->data[1] = _cc;
        packet->data[2] = ((status == kNoteOff || data2 == 0) ? 0 : 127);
        packet->timeStamp = inOffsetSampleFrame;
        
    } else {
        memset(packet->data, 0, sizeof(Byte)*256);
        packet->length = 3;
        packet->data[0] = status | channel;
        packet->data[1] = data1;
        packet->data[2] = data2;
        packet->timeStamp = inOffsetSampleFrame;
    }
    
	return noErr;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Note2CC::Render
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus Note2CC::Render (AudioUnitRenderActionFlags &ioActionFlags, const AudioTimeStamp& inTimeStamp, UInt32 nFrames)
{
    Byte listBuffer[kMIDIPacketListSize];
    MIDIPacketList* packetList = (MIDIPacketList*)listBuffer;
    MIDIPacket* packetListIterator = MIDIPacketListInit(packetList);
  
    MIDIPacket* packet = mOutputPacketFIFO.ReadItem();
    while (packet != NULL)
    {
        //----------------------------------------------------------------------//
        // This is where the midi packets get processed
        //
        //----------------------------------------------------------------------//
        
        if (packetListIterator == NULL) return noErr;
        packetListIterator = MIDIPacketListAdd(packetList, kMIDIPacketListSize, packetListIterator, packet->timeStamp, packet->length, packet->data);
        mOutputPacketFIFO.AdvanceReadPtr();
        packet = mOutputPacketFIFO.ReadItem();
    }
    
    if (mMIDIOutCB.midiOutputCallback != NULL && packetList->numPackets > 0)
    {
        mMIDIOutCB.midiOutputCallback(mMIDIOutCB.userData, &inTimeStamp, 0, packetList);
    }
      
    return noErr;
}



