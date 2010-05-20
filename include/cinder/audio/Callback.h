/*
 Copyright (c) 2009, The Barbarian Group
 All rights reserved.

 Redistribution and use in source and binary forms, with or without modification, are permitted provided that
 the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and
	the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
	the following disclaimer in the documentation and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include "cinder/Cinder.h"
#include "cinder/audio/Io.h"
#include "cinder/audio/Buffer.h"

#if defined( CINDER_COCOA )
	#include "cinder/audio/CocoaCaConverter.h"
#endif

namespace cinder { namespace audio {

template<typename,typename> class LoaderSourceCallback;

template<typename T, typename U>
class Callback : public Source {
 public: 
	typedef void (T::*CallbackFunction)( uint64_t inSampleOffset, uint32_t inSampleCount, BufferT<U> *ioBuffer );
	
	virtual ~Callback();
	
	LoaderRef getLoader( Target *target ) { return LoaderSourceCallback<T,U>::createRef( this, target ); }
	double getDuration() const { return 100.0; } //TODO: support for endless sources
	
 private:
	Callback( T* callbackObj, CallbackFunction callbackFn, bool ownCallbackObj, uint32_t aSampleRate, uint16_t aChannelCount, DataType aDataType );
  
	void getData( uint64_t inSampleOffset, uint32_t ioSampleCount, Buffer *ioBuffer );
	
	T* mCallbackObj;
	CallbackFunction mCallbackFn;
	bool mOwnsCallbackObj;
	
	friend class LoaderSourceCallback<T,U>;
	template <typename T2, typename U2>
	friend shared_ptr<Callback<T2,U2> > createCallback( T2* callbackObj, void (T2::*callbackFn)( uint64_t inSampleOffset, uint32_t inSampleCount, BufferT<U2> *ioBuffer ), bool ownCallbackObj, uint32_t aSampleRate, uint16_t aChannelCount, Io::DataType aDataType );
};

template<typename T, typename U>
shared_ptr<Callback<T,U> > createCallback( T* callbackObj, void (T::*callbackFn)( uint64_t inSampleOffset, uint32_t inSampleCount, BufferT<U> *ioBuffer ), bool ownCallbackObj = false, uint32_t aSampleRate = 44100, uint16_t aChannelCount = 2, Io::DataType aDataType = Io::FLOAT32 )
{
	return shared_ptr<Callback<T,U> >( new Callback<T,U>( callbackObj, callbackFn, ownCallbackObj, aSampleRate, aChannelCount, aDataType ) );
}

template<typename T, typename U>
class LoaderSourceCallback : public Loader {
 public:
	static LoaderRef	createRef( Callback<T,U> *source, Target *target )
	{
		return shared_ptr<LoaderSourceCallback<T,U> >( new LoaderSourceCallback<T,U>( source, target ) );
	}
	
	~LoaderSourceCallback() {}
	
	uint64_t getSampleOffset() const { return mSampleOffset; }
	void setSampleOffset( uint64_t anOffset ) { mSampleOffset = anOffset; }
	void loadData( uint32_t *ioSampleCount, BufferList *ioData );
  private:
	LoaderSourceCallback( Callback<T,U> *source, Target *target );
	Callback<T,U>						* mSource;
	uint64_t						mSampleOffset;

#if defined(CINDER_COCOA)
	static void dataInputCallback( Loader* aLoader, uint32_t *ioSampleCount, BufferList *ioData, AudioStreamPacketDescription * packetDescriptions );
	shared_ptr<CocoaCaConverter>	mConverter;
#endif	
};

template<typename T, typename U>
Callback<T,U>::Callback( T* callbackObj, CallbackFunction callbackFn, bool ownCallbackObj, uint32_t aSampleRate, uint16_t aChannelCount, DataType aDataType )
	: Source(), mCallbackFn( callbackFn ), mCallbackObj( callbackObj ), mOwnsCallbackObj( ownCallbackObj )
{
	mIsPcm = true;
	mSampleRate = aSampleRate;
	mChannelCount = aChannelCount;
	mIsInterleaved = true;
	mDataType = aDataType;
	mIsBigEndian = false;
	
	if( mDataType == UINT8 || mDataType == INT8 ) {
		mBitsPerSample = 8;
	} else if( mDataType == INT16 ) {
		mBitsPerSample = 16;
	} else if( mDataType == UINT32 || mDataType == INT32 || mDataType == FLOAT32 ) {
		mBitsPerSample = 32;
	} else {
		if( mOwnsCallbackObj ) {
			delete callbackObj;
		}
		throw IoExceptionUnsupportedDataType();
	}
	mBlockAlign = ( mBitsPerSample / 8 ) * mChannelCount;
}

template<typename T, typename U>
Callback<T,U>::~Callback()
{
	if( mOwnsCallbackObj ) {
		delete mCallbackObj;
	}
}

template<typename T, typename U>
void Callback<T,U>::getData( uint64_t inSampleOffset, uint32_t ioSampleCount, Buffer *ioBuffer ) 
{
	BufferT<U> typedBuffer;
	typedBuffer.mNumberChannels = ioBuffer->mNumberChannels;
	typedBuffer.mDataByteSize = ioBuffer->mDataByteSize;
	typedBuffer.mData = reinterpret_cast<U*>( ioBuffer->mData );
	
	( mCallbackObj->*mCallbackFn )( inSampleOffset, ioSampleCount, &typedBuffer );
	
	ioBuffer->mNumberChannels = typedBuffer.mNumberChannels;
	ioBuffer->mDataByteSize = typedBuffer.mDataByteSize;
	ioBuffer->mData = reinterpret_cast<void*>( typedBuffer.mData );
}
	
template<typename T, typename U>
LoaderSourceCallback<T,U>::LoaderSourceCallback( Callback<T,U> *source, Target *target )
	: mSource( source ), mSampleOffset( 0 )
{
#if defined( CINDER_COCOA )
	AudioStreamBasicDescription sourceDescription;
	
	sourceDescription.mFormatID = kAudioFormatLinearPCM; //kAudioFormatLinearPCM;
	sourceDescription.mFormatFlags = CalculateLPCMFlags( mSource->getBitsPerSample(), mSource->getBlockAlign() * 8, mSource->isFloat(), mSource->isBigEndian(), ( ! mSource->isInterleaved() ) /*is non interleaved*/ );
	sourceDescription.mSampleRate = source->getSampleRate();
	sourceDescription.mBytesPerPacket = ( mSource->getBlockAlign() ); //( mSource->getBitsPerSample() * mSource->getChannelCount() ) / 8;
	sourceDescription.mFramesPerPacket = 1;
	sourceDescription.mBytesPerFrame = ( mSource->getBlockAlign() );//( mSource->getBitsPerSample() * mSource->getChannelCount() ) / 8;
	sourceDescription.mChannelsPerFrame = source->getChannelCount();
	sourceDescription.mBitsPerChannel = source->getBitsPerSample();
	
	AudioStreamBasicDescription targetDescription;
	
	if( ! target->isPcm() ) {
		throw IoExceptionUnsupportedDataFormat();
	}
	
	targetDescription.mFormatID = kAudioFormatLinearPCM; //target->mNativeFormatId;
	targetDescription.mFormatFlags = CalculateLPCMFlags( target->getBitsPerSample(), target->getBlockAlign() * 8, target->isFloat(), target->isBigEndian(), ( ! target->isInterleaved() ) ); //target->mNativeFormatFlags
	targetDescription.mSampleRate = target->getSampleRate();
	targetDescription.mBytesPerPacket =  target->getBlockAlign(); //target->mBytesPerPacket;
	targetDescription.mFramesPerPacket = 1; //target->mFramesPerPacket;
	targetDescription.mBytesPerFrame = ( target->getBlockAlign() ); //target->mBytesPerFrame;
	targetDescription.mChannelsPerFrame = target->getChannelCount();
	targetDescription.mBitsPerChannel = target->getBitsPerSample();
	
	mConverter = shared_ptr<CocoaCaConverter>( new CocoaCaConverter( this, &LoaderSourceCallback<T,U>::dataInputCallback, sourceDescription, targetDescription, mSource->getBlockAlign() ) );
#endif
}

template<typename T, typename U>
void LoaderSourceCallback<T,U>::loadData( uint32_t *ioSampleCount, BufferList *ioData ) {	
#if defined( CINDER_COCOA )
	mConverter->loadData( ioSampleCount, ioData );
	mSampleOffset += *ioSampleCount;
#elif defined( CINDER_MSW )
	mSource->getData( mSampleOffset, *ioSampleCount, &ioData->mBuffers[0] );
	mSampleOffset += *ioSampleCount;
#endif
}	

#if defined( CINDER_COCOA )
template<typename T, typename U>
void LoaderSourceCallback<T,U>::dataInputCallback( Loader* aLoader, uint32_t *ioSampleCount, BufferList *ioData, AudioStreamPacketDescription * packetDescriptions ) {
	LoaderSourceCallback * theLoader = dynamic_cast<LoaderSourceCallback *>( aLoader );
	Callback<T,U> * theSource = theLoader->mSource;	
	theSource->getData( theLoader->mSampleOffset, *ioSampleCount, &ioData->mBuffers[0] );
}
#endif
	

}} //namespace