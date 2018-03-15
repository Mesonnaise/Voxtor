#include<algorithm>
#include"Sparse.h"
#include"Util.h"
#include<Windows.h>
namespace Succinct{
  L0Segment* Sparse::AllocateSegment(uint64_t segmentSize,size_t &initalBitCount){
    //VirtualAlloc will round up to the next full page
    //TODO: use L1L2Interleave::mBlockSize
    
    size_t L1BlockGroups=(segmentSize*1024)/65472;
    size_t size=L1BlockGroups*65472;
    uint64_t maxBits=248*2048;

    uint64_t *memSegment=static_cast<uint64_t*>(VirtualAlloc(
      nullptr,size,MEM_RESERVE|MEM_COMMIT|MEM_64K_PAGES,PAGE_READWRITE));

    if(memSegment==nullptr)
      return nullptr;

    maxBits*=L1BlockGroups;

    size_t setSize=std::min(initalBitCount,maxBits);
    initalBitCount-=setSize;
    
    L0Segment* segment=new L0Segment(memSegment,size,maxBits,setSize);
    return segment;
  }

  L0Segment* Sparse::FindSegment(L0Segment *curSegment,uint64_t &subCount,uint64_t &pos)const{

    while(curSegment!=nullptr&&curSegment->mBitCount<pos){
      subCount+=curSegment->mBitCount;
      pos-=curSegment->mBitCount;
      curSegment=curSegment->mNextSegment;
    }

    return curSegment;
  }

  void Sparse::Rebuild(L0Segment *curSegment,int64_t newCount){
    int64_t L0Delta=0;

    if(curSegment->mNextSegment){
      newCount+=curSegment->mL0Counter;
      curSegment=curSegment->mNextSegment;

      L0Delta=newCount-curSegment->mL0Counter;
      curSegment->mL0Counter=newCount;

      curSegment=curSegment->mNextSegment;
      while(curSegment){
        curSegment->mL0Counter+=L0Delta;
        curSegment=curSegment->mNextSegment;
      }
    }
  }

  Sparse::Sparse(size_t initialSize):mBitCount(initialSize){
    mRoot=AllocateSegment(SegmentAllocationSize,initialSize);//2MiB
    
    if(mRoot==nullptr)
      throw std::runtime_error("Succinct::Sparse: Unable to allocate memory");

    mSize=mRoot->mMaxBitCount;

    L0Segment *curSegment=mRoot;

    while(initialSize){
      auto newSegment=AllocateSegment(SegmentAllocationSize,initialSize);
      mSize+=newSegment->mMaxBitCount;
      curSegment->mNextSegment=newSegment;
      curSegment=newSegment;
    }
  }

  Sparse::~Sparse(){
    L0Segment *segment=mRoot;
    while(segment){
      VirtualFree(segment->mBaseAddr,0,MEM_RELEASE);
      L0Segment *tmp=segment;
      segment=segment->mNextSegment;
      delete tmp;
    }
  }

  uint64_t Sparse::Rank(uint64_t pos,Cache *c)const{
    if(pos>=mBitCount)
      throw std::range_error("Succinct::Sparse::Rank: position exceeds upper bounds of bit array");

    uint64_t global;
    L0Segment *curSegment;

    CacheVars(c,global,pos,curSegment);

    curSegment=FindSegment(curSegment,global,pos);

    CacheAssign(c,global,pos,curSegment);

    return curSegment->Rank(pos);
  }

  uint64_t Sparse::Select(uint64_t count)const{
    L0Segment *tmpSegment=mRoot;
    L0Segment *curSegment=nullptr;

    uint64_t rollPosition=0;

    while(tmpSegment!=nullptr&&tmpSegment->mL0Counter<count){
      rollPosition+=tmpSegment->mBitCount;
      curSegment=tmpSegment;
      tmpSegment=tmpSegment->mNextSegment;
    }

    return rollPosition+curSegment->Select(count);
  }

  void Sparse::Set(uint64_t pos,Cache *c){
    if(pos>=mBitCount)
      throw std::range_error("Succinct::Sparse::Set: position exceeds upper bounds of bit array");

    uint64_t global;
    L0Segment *curSegment;

    CacheVars(c,global,pos,curSegment);

    curSegment=FindSegment(curSegment,global,pos);

    curSegment->Set(pos);

    mRevision++;

    CacheAssign(c,global,pos,curSegment);
  }

  void Sparse::Clear(uint64_t pos,Cache *c){
    if(pos>=mBitCount)
      throw std::range_error("Succinct::Sparse::Clear: position exceeds upper bounds of bit array");

    uint64_t global;
    L0Segment *curSegment;

    CacheVars(c,global,pos,curSegment);

    curSegment=FindSegment(curSegment,global,pos);

    curSegment->Clear(pos);

    mRevision++;

    CacheAssign(c,global,pos,curSegment);
  }

  bool Sparse::Get(uint64_t pos,Cache *c)const{
    if(pos>=mBitCount)
      throw std::range_error("Succinct::Sparse::Get: position exceeds upper bounds of bit array");

    uint64_t global;
    L0Segment *curSegment;

    CacheVars(c,global,pos,curSegment);

    L0Segment *curSegment=FindSegment(mRoot,global,pos);

    CacheAssign(c,global,pos,curSegment);

    return curSegment->Get(pos);
  }

  void Sparse::Insert(uint64_t pos,size_t ammount,uint64_t value,Cache *c){
    if(pos>mBitCount)
      throw std::range_error("Succinct::Sparse::Insert: position exceeds upper bounds of bit array");

    uint64_t global;
    L0Segment *curSegment;

    CacheVars(c,global,pos,curSegment);

    L0Segment *curSegment=FindSegment(mRoot,global,pos);

    bool overflow=curSegment->mBitCount+ammount>curSegment->mMaxBitCount;
    uint64_t carry;
    int64_t accumulator;

    std::tie(carry,accumulator)=curSegment->Insert(pos,ammount,value);

    //The ammount of data currently held in the segment plus the new data doesn't fit
    //Try to fit the extra data in the next segment or create a new one
    if(overflow){
      accumulator+=curSegment->mL0Counter;

      auto nextSegment=curSegment->mNextSegment;
      if(nextSegment==nullptr||nextSegment->mBitCount+ammount>nextSegment->mMaxBitCount){
        size_t tmpSize=0;
        auto newSegment=AllocateSegment(SegmentAllocationSize,tmpSize);

        mSize+=newSegment->mMaxBitCount;

        newSegment->mNextSegment=nextSegment;
        curSegment->mNextSegment=newSegment;
        nextSegment=newSegment;
      }

      //Set the next segment's L0Counter to the previous segments counter,
      //plus the accumulation of previous segments L1L2 rebuild result
      nextSegment->mL0Counter=accumulator;

      std::tie(carry,accumulator)=nextSegment->Insert(0,ammount,carry);

      curSegment=nextSegment;
    }

    Rebuild(curSegment,accumulator);

    mBitCount+=ammount;

    mRevision++;

    CacheAssign(c,global,pos,curSegment);
  }

  uint64_t Sparse::Remove(uint64_t pos,size_t ammount){
    if(pos+ammount>mBitCount)
      throw std::range_error("Succinct::Sparse::Remove: position exceeds upper bounds of bit array");

    L0Segment *curSegment=mRoot;
    L0Segment *lastSegment=mRoot;

    while(curSegment!=nullptr&&curSegment->mBitCount<pos){
      pos-=curSegment->mBitCount;
      lastSegment=curSegment;
      curSegment=curSegment->mNextSegment;
    }

    //carry is the value that has been removed by the operation
    uint64_t carry;
    int64_t accumulator;

    std::tie(carry,accumulator)=curSegment->Remove(pos,ammount);

    if(curSegment->mBitCount==0&&lastSegment!=curSegment){
      lastSegment->mNextSegment=curSegment->mNextSegment;

      VirtualFree(curSegment->mBaseAddr,0,MEM_RELEASE);
      delete curSegment;
      
      curSegment=lastSegment->mNextSegment;
    }

    Rebuild(curSegment,accumulator);

    mRevision++;

    return carry;
  }

  void Sparse::Extract(const Sparse &maskArray){
    L0Segment *curSegment=mRoot;
    L0Segment *maskSegment=maskArray.mRoot;

    size_t SegmentSize=curSegment->mBitCount>>6;

    uint64_t *addr=nullptr;
    uint64_t *maskAddr=nullptr;

    while(curSegment&&maskSegment){
      maskAddr=maskSegment->BitArray::mBaseAddr;

      for(int i=0;i<8;i++){
        addr=curSegment->BitArray::mBaseAddr;

        uint64_t zeros=ExtractByte(addr,addr+SegmentSize,
                                   maskAddr,maskAddr+=(SegmentSize>>3));

        curSegment=curSegment->mNextSegment;
      }
      maskSegment=maskSegment->mNextSegment;
    }
  }

  void Sparse::Compact(){
    L0Segment *curSegment=mRoot;
    L0Segment *nextSegment=mRoot->mNextSegment;

    uint64_t L0Accumulator=0;

    while(curSegment&&nextSegment){
      uint64_t delta=curSegment->mMaxBitCount-curSegment->mBitCount;
      while(delta){
        size_t remAmmount=std::min(delta,nextSegment->mBitCount);

        uint64_t carry=nextSegment->BitArray::Remove(0,remAmmount);
        curSegment->BitArray::Insert(curSegment->mBitCount,carry);

        if(nextSegment->mBitCount==0){
          L0Segment *tmp=nextSegment;

          curSegment->mNextSegment=nextSegment->mNextSegment;

          VirtualFree(tmp->mBaseAddr,0,MEM_RELEASE);
          delete tmp;

          nextSegment=curSegment->mNextSegment;
        }
      }

      curSegment->mL0Counter=L0Accumulator;
      L0Accumulator+=curSegment->L1L2Interleave::Rebuild(0);

      curSegment=nextSegment;
      nextSegment=curSegment->mNextSegment;
    }
    
  }

  size_t Sparse::MemUsage()const{
    size_t total=sizeof(Sparse);

    L0Segment *curSegment=mRoot;

    while(curSegment){
      total+=sizeof(L0Segment);
      total+=curSegment->mMemorySegmentSize;
      curSegment=curSegment->mNextSegment;
    }

    return total;
  }
}