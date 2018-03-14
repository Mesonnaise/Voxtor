#include"L0Segment.h"

namespace Succinct{
  L0Segment::L0Segment(uint64_t *baseAddr,size_t size,uint64_t maxBits,uint64_t initalBitCount):
    L1L2Interleave(baseAddr,maxBits,initalBitCount),mMemorySegmentSize(size){}

  uint64_t L0Segment::Rank(uint64_t pos){
    return mL0Counter+L1L2Interleave::Rank(pos);
  }

  uint64_t L0Segment::Select(uint64_t count){
    count-=mL0Counter;
    return L1L2Interleave::Select(count);
  }

  void L0Segment::Set(uint64_t pos){
    L1L2Interleave::Set(pos);
    
    for(auto ns=mNextSegment;ns;ns=ns->mNextSegment)
      ns->mL0Counter++;
  }

  void L0Segment::Clear(uint64_t pos){
    L1L2Interleave::Clear(pos);

    for(auto ns=mNextSegment;ns;ns=ns->mNextSegment)
      ns->mL0Counter--;
  }
}