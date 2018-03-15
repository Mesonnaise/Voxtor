#pragma once
#include<optional>
#include"L0Segment.h"
namespace Succinct{

  class Sparse{
  public:
    static const size_t SegmentAllocationSize=2048;

    struct Cache{
      uint64_t   mSparseOffset=0;//Offset in the whole Sparse Array
//      uint64_t   mSegmentOffset=0;//Offset in current Segment
      L0Segment *mSegment=nullptr;
    };
  protected:
    uint64_t   mRevision=0;
    L0Segment *mRoot=nullptr;
    size_t     mSize=0;
    size_t     mBitCount;
  protected:
    L0Segment*  AllocateSegment(uint64_t segmentSize,size_t &initalBitCount);//multiples of 64KiB
    L0Segment*  FindSegment(L0Segment *curSegment,uint64_t &subCount,uint64_t &pos)const;
    void        Rebuild(L0Segment *curSegment,int64_t newCount);

    inline void CacheAssign(Sparse::Cache *c,uint64_t global,uint64_t pos,L0Segment *segment)const{
      if(c){
        c->mSparseOffset=global;
//        c->mSegmentOffset=pos;
        c->mSegment=segment;
      }
    }
    inline void CacheVars(Cache *c,uint64_t &global,uint64_t &pos,L0Segment *&segment)const{
      global=0;
      segment=mRoot;

      if(c&&pos<c->mSparseOffset){
        global=c->mSparseOffset;
        segment=c->mSegment?c->mSegment:segment;
        pos-=global;
      }
    }

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

    uint64_t Rank(uint64_t pos,Cache *c=nullptr)const;
    uint64_t Select(uint64_t count)const;
   // uint64_t Range(uint64_t pos,uint64_t limit)const;

    void Set(uint64_t pos,Cache *c=nullptr);
    void Clear(uint64_t pos,Cache *c=nullptr);
    bool Get(uint64_t pos,Cache *c=nullptr)const;

    void Insert(uint64_t pos,size_t ammount,uint64_t value,Cache *c=nullptr);
    uint64_t Remove(uint64_t pos,size_t ammount);


    void Extract(const Sparse &maskArray);
    //Removes the gaps out of the array
    void Compact();


    size_t MemUsage()const;
  };
}