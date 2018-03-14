#pragma once
#include"L0Segment.h"
namespace Succinct{
  class Sparse{
  public:
    static const size_t SegmentAllocationSize=2048;
  protected:
    uint64_t   mRevision=0;
    L0Segment *mRoot=nullptr;
    size_t     mSize=0;
    size_t     mBitCount;
  protected:
    L0Segment* AllocateSegment(uint64_t segmentSize,size_t &initalBitCount);//multiples of 64KiB
    L0Segment* FindSegment(uint64_t &pos)const;
    void       Rebuild(L0Segment *curSegment,int64_t newCount);
  public:
    Sparse(size_t initalSize);
    ~Sparse();

    constexpr size_t Size()const{
      return mSize;
    }
    constexpr size_t BitCount()const{
      return mBitCount;
    }
    constexpr uint64_t Revision()const{
      return mRevision;
    }

    constexpr uint64_t PopCount()const{
      L0Segment *curSegment=mRoot;

      while(curSegment->mNextSegment)
        curSegment=curSegment->mNextSegment;

      return curSegment->Rank(curSegment->mBitCount-1);
    }

    uint64_t Rank(uint64_t pos)const;
    uint64_t Select(uint64_t count)const;
   // uint64_t Range(uint64_t pos,uint64_t limit)const;

    void Set(uint64_t pos);
    void Clear(uint64_t pos);
    bool Get(uint64_t pos)const;

    void Insert(uint64_t pos,size_t ammount,uint64_t value);
    uint64_t Remove(uint64_t pos,size_t ammount);


    void Extract(const Sparse &maskArray);
    //Removes the gaps out of the array
    void Compact();


    size_t MemUsage()const;
  };
}