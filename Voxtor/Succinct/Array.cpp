#include<stdexcept>
#include<algorithm>
#include "Array.h"


namespace Succinct{
  L0Block* Array::CursorReset()const{
    mCursorBlock=mRootBlock;
    mCursorOffset=0;
    return mCursorBlock;
  }

  L0Block* Array::CursorSeek(uint64_t &pos)const{
    if(!mIsValid){
      mCursorBlock=mRootBlock;
      mCursorOffset=0;
      mIsValid=true;
    }

    if(mIsFixed){
      while(mCursorBlock&&pos>(mCursorOffset+mCursorBlock->Size())){
        mCursorOffset+=mCursorBlock->Size();
        mCursorBlock=mCursorBlock->Next();
      }
    }else{
      while(mCursorBlock->Next()&&pos>(mCursorOffset+mCursorBlock->Size())){
        mCursorOffset+=mCursorBlock->Size();
        mCursorBlock=mCursorBlock->Next();
      }
    }
    while(pos<mCursorOffset&&mCursorBlock->Prev()){
      mCursorBlock=mCursorBlock->Prev();
      mCursorOffset-=mCursorBlock->Size();
    }

    pos-=mCursorOffset;

    return mCursorBlock;
  }

  L0Block* Array::CursorPrev()const{
    mCursorOffset-=mCursorBlock->Size();
    mCursorBlock=mCursorBlock->Prev();
    return mCursorBlock;
  }

  L0Block* Array::CursorNext()const{
    mCursorOffset+=mCursorBlock->Size();
    mCursorBlock=mCursorBlock->Next();
    return mCursorBlock;
  }

  Array::Array(uint64_t level,bool fixed):mIsFixed(fixed){
    //The maximum size for a L0Block at a known tree level
    //Hand picked, may change in the future
    const uint64_t translateTable[]={2048,2048,2048,4096,4096,16384,65536,
                                     131072,524288,524288,2097152,4194304,
                                     8388608,8388608,8388608,8388608};

    mBlockSize=translateTable[level];
    mBlockSize=fixed?mBlockSize:mBlockSize/4;

    

    if(fixed){
      uint64_t bitCount=(level+1)*3;
      bitCount=uint64_t(1)<<bitCount;

      auto blockBitCount=std::min(bitCount,mBlockSize);
      bitCount-=blockBitCount;

      mRootBlock=new L0Block((blockBitCount+L0Block::mBitsPerL1-1)&~(L0Block::mBitsPerL1-1),fixed);
      

      auto currentBlock=mRootBlock;
      while(bitCount){
        blockBitCount=std::min(bitCount,mBlockSize);
        bitCount-=blockBitCount;

        auto newBlock=new L0Block((blockBitCount+L0Block::mBitsPerL1-1)&~(L0Block::mBitsPerL1-1),fixed);
        currentBlock->AppendBlock(newBlock);
        currentBlock=newBlock;
      }
    }else
      mRootBlock=new L0Block((mBlockSize+L0Block::mBitsPerL1-1)&~(L0Block::mBitsPerL1-1),fixed);

    mCursorBlock=mRootBlock;
  }

  Array::~Array(){
    if(mRootBlock)
      delete mRootBlock;
  }

  size_t Array::AllocatedSize()const{
    auto block=mRootBlock;
    size_t size=0;

    for(;block;block=block->Next())
      size+=block->AllocatedSize();

    return size;
  }

  size_t Array::ReserveSize()const{
    auto block=mRootBlock;
    size_t size=0;

    for(;block;block=block->Next())
      size+=block->ReserveSize();

    return size;
  }

  size_t Array::Size()const{
    auto block=mRootBlock;
    size_t size=0;

    for(;block;block=block->Next())
      size+=block->Size();

    return size;
  }

  bool Array::Empty()const{
    auto block=mRootBlock;

    for(;block;block=block->Next()){
      if(!block->Empty())
        return false;
    }
    return true;
  }


  uint64_t Array::Rank(uint64_t pos)const{
    auto block=CursorSeek(pos);

    return block->Rank(pos);
  }

  uint64_t Array::Select(uint64_t count)const{
    if(count<Cursor()->BaseCounter()){
      while(Cursor()&&count<Cursor()->BaseCounter())
        CursorPrev();
    }else{
      while(Cursor()&&count>Cursor()->BaseCounter())
        CursorNext();
    }

    return mCursorOffset+Cursor()->Select(count);
  }

  bool Array::Get(uint64_t pos)const{
    auto block=CursorSeek(pos);
    if(!block){
      mIsValid=false;
      throw std::runtime_error("Succinct::Array::Get: pos exceeds the size of the array");
    }
    return block->Get(pos);
  }

  void Array::Set(uint64_t pos){
    auto block=CursorSeek(pos);
    if(!block){
      mIsValid=false;
      throw std::runtime_error("Succinct::Array::Get: pos exceeds the size of the array");
    }
    block->Set(pos);
    block=block->Next();
  }

  void Array::Clear(uint64_t pos){
    auto block=CursorSeek(pos);
    block->Clear(pos);
  }

  uint64_t Array::GetGroup(uint64_t pos,size_t size)const{
    auto block=CursorSeek(pos);
    if(!block){
      mIsValid=false;
      throw std::runtime_error("Succinct::Array::Get: pos exceeds the size of the array");
    }
    return block->GetGroup(pos,size);
  }

  uint64_t Array::PopCount()const{
    L0Block *nextBlock=Cursor();
    L0Block *block=Cursor();

    while(nextBlock){
      block=nextBlock;
      nextBlock=nextBlock->Next();
    }

    return block->PopCount();
  }

  uint64_t Array::Insert(uint64_t pos,size_t ammount,uint64_t value){
    if(mIsFixed)
      throw std::runtime_error("Array::Insert: Insert can't be used when array has a fixed size");

    auto block=CursorSeek(pos);
    if(!block){
      mIsValid=false;
      throw std::runtime_error("Succinct::Array::Get: pos exceeds the size of the array");
    }

    int64_t overflow=block->Size()+ammount-block->ReserveSize();

    uint64_t carry=block->Insert(pos,ammount,value);
  
    if(overflow>0){
      auto nextBlock=block->Next();

      if(nextBlock==nullptr||(nextBlock->Size()+overflow)<nextBlock->ReserveSize()){
        nextBlock=new L0Block(mBlockAllocateSize,mIsFixed);
        block->AppendBlock(nextBlock);
      }
      carry=nextBlock->Insert(0,overflow,carry);
    }

    return carry;
  }

  uint64_t Array::Remove(uint64_t pos,size_t ammount,uint64_t carry){
    if(mIsFixed)
      throw std::runtime_error("Array::Remove: Remove can't be used when array has a fixed size");

    auto block=CursorSeek(pos);
    if(!block){
      mIsValid=false;
      throw std::runtime_error("Succinct::Array::Get: pos exceeds the size of the array");
    }

    uint64_t value=block->Remove(pos,ammount,carry);
    return value;
  }

  size_t Array::Filter(Array *mask,uint64_t maskOffset,uint64_t maskScale){
    if(mIsFixed)
      throw std::runtime_error("Array::Filter: Filter can't be used when array has a fixed size");

    //This can be multithreaded
    auto block=CursorReset();
    auto maskBlock=mask->CursorReset();

    uint64_t offset=0;
    uint64_t total=0;
    while(block&&maskBlock){
      auto tmp=block->Filter(maskBlock,offset,maskScale);

      offset+=tmp;
      total+=tmp;

      if(offset>=maskBlock->Size()){
        maskBlock=maskBlock->Next();
        offset=0;
      }
    }

    return total;
  }

  void Array::Compact(){
    if(mIsFixed)
      throw std::runtime_error("Array::Compact: Compact can't be used when array has a fixed size");

    auto destination=CursorReset();
    auto source=destination->Next();

    while(source){
      destination->AppendDonation(source);


      if(source->Empty()){
        source->RemoveBlock();
        auto tmp=source->Next();
        delete source;
        source=tmp;
      }

      if(destination->Full())
        destination=destination->Next();
    }
  }

  size_t Array::Slice(Array *destination,uint64_t start,uint64_t end){
    return 0;
  }
}
