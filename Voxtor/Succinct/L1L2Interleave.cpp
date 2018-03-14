#include"L1L2Interleave.h"
#include"Util.h"

namespace Succinct{
  inline void L1L2Interleave::L2Inc(uint64_t *val,int pos){
    int shift=32+pos*10;
    uint64_t exg;
    uint64_t cmp;
    uint64_t ret;
    do{
      cmp=*val;
      uint64_t subVal=_rotr64(cmp,shift);
      subVal+=!((pos>>1)&pos);
      exg=_rotl64(subVal,shift);
      ret=(uint64_t)_InterlockedCompareExchange64((int64_t*)val,
                                                  (int64_t)exg,
                                                  (int64_t)cmp);
    } while(ret!=cmp);
  }

  inline void L1L2Interleave::L2Dec(uint64_t *val,int pos){
    int shift=32+pos*10;
    uint64_t exg;
    uint64_t cmp;
    uint64_t ret;
    do{
      cmp=*val;
      uint64_t subVal=_rotr64(*val,shift);
      subVal-=!((pos>>1)&pos);
      exg=_rotl64(subVal,shift);
      ret=(uint64_t)_InterlockedCompareExchange64((int64_t*)val,
        (int64_t)exg,
                                                  (int64_t)cmp);
    } while(ret!=cmp);
  }

  inline void L1L2Interleave::L2Set(uint64_t *val,int pos,uint64_t count){
    pos*=10;
    uint64_t mask=~(0x000003FF00000000ULL<<pos);
    
    *val=*val&mask|count<<(32+pos);
  }

  inline uint64_t L1L2Interleave::Rebuild(uint64_t pos){
    //TODO: This needs to be redesigned to make it faster. 
    uint64_t *addr=mBaseAddr+pos/mBlockSize;
    uint64_t *addrEnd=mBaseAddr+mL1BlockCount;

    uint64_t *bitAddr=BitArray::mBaseAddr+(pos/64);

    uint64_t rollCount=*addr&0x00000000FFFFFFFFULL;

    uint64_t localCount=0;

    while(addr<addrEnd){
      *addr=rollCount;

      for(int L2Idx=0;L2Idx<3;L2Idx++){
        localCount=0;

        for(int i=0;i<8;i++)
          localCount+=__popcnt64(*bitAddr++);

    //    localCount+=PopCnt2Pass(bitAddr);
    //    bitAddr+=8;

        L2Set(addr,L2Idx,localCount);
        rollCount+=localCount;
      }

      for(int i=0;i<8;i++)
        rollCount+=__popcnt64(*bitAddr++);

  //    rollCount+=PopCnt2Pass(bitAddr);
  //    bitAddr+=8;

      ++addr;
    }

    return rollCount;
  }

  L1L2Interleave::L1L2Interleave(uint64_t *baseAddr,uint64_t MaxBits,uint64_t initalBitCount):
    mBaseAddr(baseAddr),mL1BlockCount((initalBitCount+mBlockSize-1)/mBlockSize),
    BitArray(baseAddr+((MaxBits+mBlockSize-1)/mBlockSize),MaxBits,initalBitCount){}

  L1L2Interleave::~L1L2Interleave(){}


  uint64_t L1L2Interleave::Rank(uint64_t pos)const{
    if((pos>>11)>=mL1BlockCount)
      throw new std::range_error("Succinct::L1L2Interleave::Rank: position exceeds upper bounds of bit array");

    uint64_t *addr=mBaseAddr+(pos/mBlockSize);
    uint64_t skip=pos&(mBlockSize-1);
    //Get the L1 count located at the bottom of block
    uint64_t rollCount=*addr&0x00000000FFFFFFFFULL;

    //The L2 count is not absolute and needs to be summed together
    //The L2 is a set of 3 10bit intergers 
    const uint64_t L2Position=(pos>>9)&0x0000000000000003ULL;
    for(int L2Idx=0;L2Idx<L2Position;L2Idx++)
      rollCount+=*addr>>(32+L2Idx*10)&0x00000000000003FFULL;

    //Each L1 holds 4 512bit bitArray blocks, and each L2 holds a single 512bit block
    rollCount+=BitArray::Rank(pos&0x00000000000001FFULL,skip+L2Position*512);
    return rollCount;
  }

  uint64_t L1L2Interleave::Select(uint64_t count)const{
    uint64_t rollPosition=0;
    uint64_t localCount;

    uint64_t       *addr=mBaseAddr;
    const uint64_t *addrEnd=addr+mL1BlockCount;

    localCount=*(addr+1)&0x00000000FFFFFFFFULL;
    while(localCount<count&&addr<addrEnd){
      rollPosition+=mBlockSize;
      localCount=*(++addr+1)&0x00000000FFFFFFFFULL;
    }
    count-=*addr&0x00000000FFFFFFFFULL;

    //Accumulate the L2 counters while less than the target count
    uint64_t L2Idx=0;
    localCount=*addr>>(32+L2Idx*10)&0x00000000000003FFULL;
    while(localCount<count&&L2Idx<4){
      rollPosition+=512;
      count-=localCount;
      localCount=*addr>>(32+(++L2Idx*10))&0x00000000000003FFULL;
    }

    //Jump the BitArray to current L2 block and finish the select process
    rollPosition+=BitArray::Select(count,rollPosition);
    return rollPosition;
  }

  void L1L2Interleave::Set(uint64_t pos){
    if((pos>>11)>=mL1BlockCount)
      throw new std::range_error("Succinct::L1L2Interleave::Set: position exceeds upper bounds of bit array");

    BitArray::Set(pos);

    uint64_t L1Pos=pos/2048;
    uint64_t L2Pos=(pos/512)%4;

    uint64_t       *addr=mBaseAddr+L1Pos;
    const uint64_t *addrEnd=mBaseAddr+mL1BlockCount;

    //Increment the L2 value before scanning to the end of the L1 array
    L2Inc(addr,L2Pos);

    while(++addr<addrEnd)
      _InterlockedIncrement64((long long*)addr);
  }

  void L1L2Interleave::Clear(uint64_t pos){
    if((pos>>11)>=mL1BlockCount)
      throw new std::range_error("Succinct::L1L2Interleave::Clear: position exceeds upper bounds of bit array");

    BitArray::Set(pos);

    uint64_t L1Pos=pos/2048;
    uint64_t L2Pos=(pos/512)%4;

    uint64_t       *addr=mBaseAddr+L1Pos;
    const uint64_t *addrEnd=mBaseAddr+mL1BlockCount;

    //Increment the L2 value before scanning to the end of the L1 array
    L2Dec(addr,L2Pos);

    while(++addr<addrEnd)
      _InterlockedDecrement64((long long*)addr);
  }

  L1L2Interleave::CTPair L1L2Interleave::Insert(uint64_t pos,size_t ammount,uint64_t value){
    uint64_t carry=BitArray::Insert(pos,ammount,value);

    mL1BlockCount=(mBitCount+mBlockSize-1)/mBlockSize;
    uint64_t total=Rebuild(pos);

    return std::make_tuple(carry,total);
  }

  L1L2Interleave::CTPair L1L2Interleave::Remove(uint64_t pos,size_t ammount){
    uint64_t carry=0;
    carry=BitArray::Remove(pos,ammount,carry);

    mL1BlockCount=(mBitCount+mBlockSize-1)/mBlockSize;
    uint64_t total=Rebuild(pos);

    return std::make_tuple(carry,total);
  }

}