#pragma once
#include<atomic>
#include"L1L2Interleave.h"
namespace Succinct{
  struct L0Segment:public L1L2Interleave{
    friend class Sparse;
    const size_t           mMemorySegmentSize;

    std::atomic<uint64_t>  mL0Counter=0;
    L0Segment             *mNextSegment=nullptr;

  public:
    L0Segment(size_t size,uint64_t maxBits,uint64_t initalBitCount):
      L1L2Interleave(maxBits,initalBitCount,false),mMemorySegmentSize(size){}

    uint64_t Rank(uint64_t pos){
      return mL0Counter+L1L2Interleave::Rank(pos);
    }
    uint64_t Select(uint64_t count){
      count-=mL0Counter;
      return L1L2Interleave::Select(count);
    }

    void Set(uint64_t pos){
      L1L2Interleave::Set(pos);

      for(auto ns=mNextSegment;ns;ns=ns->mNextSegment)
        ns->mL0Counter++;
    }

    void Clear(uint64_t pos){
      L1L2Interleave::Clear(pos);

      for(auto ns=mNextSegment;ns;ns=ns->mNextSegment)
        ns->mL0Counter--;
    }
   
    uint64_t PopCount()const{
      return mL0Counter;//+L1L2Interleave::PopCount();
    }
  //  void Resize(size_t size,bool RebuildCounters=false);

  };
}