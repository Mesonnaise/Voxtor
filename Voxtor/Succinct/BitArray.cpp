#include<algorithm>
#include<cstring>
#include<atomic>
#include<intrin.h>  
#include"BitArray.h"
#include"Util.h"

namespace Succinct{

  inline uint64_t BitArray::MemAlign(uint64_t size,uint64_t align){
    return (align-(size%align))%align;
  }

  //Returns number of 64bit words required for the bit count
  uint64_t BitArray::RequiredSize(uint64_t count){
    //round up to the next 64bit word
    uint64_t size=(count+63)/64;
    return size+MemAlign(size,8);
  }

  BitArray::BitArray(uint64_t *baseAddr,uint64_t maxBits,uint64_t initalBitCount):
    mBaseAddr(baseAddr),mMaxBitCount(maxBits),mBitCount(initalBitCount){

    if((uintptr_t(baseAddr)%64)!=0)
      throw std::runtime_error("Succinct::BitArray: baseAddress requires 64 bytes of alignment.");
  }

  BitArray::~BitArray(){}

  //skip is multiples of 512bits
  uint64_t BitArray::Rank(uint64_t pos,uint64_t skip)const{
    if((skip+pos)>=mBitCount)
      throw std::range_error("Succinct::BitArray::Rank: position exceeds upper bounds of bit array");

    uint64_t rollCount=0;
    uint64_t *addr=mBaseAddr+skip/64;

    for(;pos>=64;pos-=64)
      rollCount+=__popcnt64(*(addr++));

    uint64_t mask=0xFFFFFFFFFFFFFFFFULL<<(pos+1);
    rollCount+=__popcnt64(*addr&~mask);

    return rollCount;
  }

  uint64_t BitArray::Select(uint64_t count,uint64_t skip)const{
    uint64_t rollPosition=0;
    uint64_t localCount=0;

    uint64_t       *addr=mBaseAddr+skip/64;
    const uint64_t *addrEnd=mBaseAddr+(mBitCount+63)/64;

    localCount=__popcnt64(*addr);
    while(localCount<count&&addr<=addrEnd){
      rollPosition+=64;
      count-=localCount;
      localCount=__popcnt64(*++addr);
    }

    rollPosition+=BwordSelect(*addr,count);

    return rollPosition;
  }

  uint64_t BitArray::Range(uint64_t pos,uint64_t limit)const{
    if((pos+limit)>=mBitCount)
      throw std::range_error("Succinct::BitArray::Range: position exceeds upper bounds of bit array");

    uint64_t rollCount=0;
    uint64_t *addr=mBaseAddr+pos/64;

    uint64_t mask;
    
    uint64_t subPos=pos&0x000000000000003FULL;
    
    if(subPos){
      mask=0xFFFFFFFFFFFFFFFFULL<<subPos;
      mask^=mask<<limit;
      rollCount+=__popcnt64(*(addr++)&mask);

      if(limit+subPos<64)
        return rollCount;
      
      limit-=64-subPos;
    }

    for(;limit>=64;limit-=64)
      rollCount+=__popcnt64(*(addr++));

    mask=0xFFFFFFFFFFFFFFFFULL<<(limit+1);
    rollCount+=__popcnt64(*addr&~mask);

    return rollCount;
  }

  void BitArray::Set(uint64_t pos){
    if(pos>=mBitCount)
      throw std::range_error("Succinct::BitArray::Set: position exceeds upper bounds of bit array");
    
    int ammount=1;
    
    //mBaseAddr[pos/64]|=0x0000000000000001ULL<<(pos%64);
    volatile uint64_t *addr=mBaseAddr+(pos>>6);
    uint64_t exg;
    uint64_t cmp;
    uint64_t ret;
   // std::atomic_fetch_or<uint64_t>(addr,0x0000000000000001ULL<<(pos%64))
    do{
      cmp=*addr;
      exg=cmp|0x0000000000000001ULL<<(pos%64);
      ret=(uint64_t)_InterlockedCompareExchange64((int64_t*)addr,
                                                  (int64_t)exg,
                                                  (int64_t)cmp);
    }while(ret!=cmp&&BackOff(ammount*=2));
   }

  void BitArray::Clear(uint64_t pos){
    if(pos>=mBitCount)
      throw std::range_error("Succinct::BitArray::Clear: position exceeds upper bounds of bit array");

    int ammount=1;

    //mBaseAddr[pos/64]&=~(0x0000000000000001ULL<<(pos%64));
    volatile uint64_t *addr=mBaseAddr+(pos>>6);
    uint64_t exg;
    uint64_t cmp;
    uint64_t ret;
    
    do{
      cmp=*addr;
      exg=cmp&~(0x0000000000000001ULL<<(pos%64));
      ret=(uint64_t)_InterlockedCompareExchange64((int64_t*)addr,
                                                  (int64_t)exg,
                                                  (int64_t)cmp);
    }while(ret!=cmp&&BackOff(ammount*=2));
  }

  bool BitArray::Get(uint64_t pos)const{
    if(pos>=mBitCount)
      throw std::range_error("Succinct::BitArray::Get: position exceeds upper bounds of bit array");

    return (*(mBaseAddr+(pos>>6))>>(pos&0x000000000000003FULL))&0x0000000000000001ULL;
  }

  //The shift will have undefined properties if shifting more than 64bits and a cross word boundries
  uint64_t BitArray::Insert(uint64_t pos,size_t ammount,uint64_t value){
    if(pos>mBitCount)
      throw std::range_error("Succinct::BitArray::Insert: position exceeds upper bounds of bit array");

    mBitCount=std::min(mBitCount+ammount,mMaxBitCount);

    uint64_t invSize=64-ammount;
    uint64_t wordOffset=pos>>6;
    uint64_t bitOffset=pos&0xFFFFFFFFFFFFFF3FULL;
    uint64_t lowerMask=~(0xFFFFFFFFFFFFFFFFULL<<bitOffset);

    uint64_t *addr=mBaseAddr+wordOffset;

    uint64_t endOffset=((mBitCount+255)/64)&0xFFFFFFFFFFFFFFFCULL;
    uint64_t alignOffset=((wordOffset+4)&0xFFFFFFFFFFFFFFFCULL);

    uint64_t *addrEnd=mBaseAddr+endOffset;
    uint64_t *addrAlign=mBaseAddr+std::min(endOffset,alignOffset);


    uint64_t carry=*addr>>invSize;
    *(addr++)=(*addr&~lowerMask)<<ammount|*addr&lowerMask|value<<bitOffset;

    carry=ShiftLeft64(addr,addrAlign,ammount,carry);
    carry=ShiftLeft256(addrAlign,addrEnd,ammount,carry);

    return carry;
  }

  uint64_t BitArray::Remove(uint64_t pos,size_t ammount,uint64_t carry){
    if(mBitCount<ammount)
      throw std::range_error("Succinct::BitArray::Remove: bits to remove exceeds total bits in array");

    uint64_t invSize=64-ammount;
    uint64_t wordOffset=pos>>6;
    uint64_t bitOffset=pos&0xFFFFFFFFFFFFFF3FULL;
    uint64_t lowerMask=~(0xFFFFFFFFFFFFFFFFULL<<bitOffset);

    uint64_t startOffset=((mBitCount+255)/64)&0xFFFFFFFFFFFFFFFCULL;
    uint64_t alignOffset=((wordOffset+4)&0xFFFFFFFFFFFFFFFCULL);

    uint64_t *addr=mBaseAddr+startOffset;
    uint64_t *addrAlign=mBaseAddr+std::min(startOffset,alignOffset);

    uint64_t *addrEnd=mBaseAddr+wordOffset;
   
    carry=ShiftRight256(addr,addrAlign,ammount,carry);
    carry=ShiftLeft64(addrAlign,addrEnd,ammount,carry);

    carry=*addrEnd>>invSize;

    *addrEnd=*addrEnd<<ammount|*addr&lowerMask;

    mBitCount-=ammount;

    return carry;
  }

  void BitArray::Compact(){
    uint64_t *addrEnd=mBaseAddr;
    addrEnd+=(mBitCount+255)>>8;

    //Returns the number of null byte at the end of the buffer
    uint64_t zeros=ZeroByteCompact(mBaseAddr,addrEnd)<<6;

    //Count of how many unset bit exist in the last byte
    uint64_t subByteBitCount=8-(mBitCount&0x0000000000000007ULL);
    //How many bits were processed by ZeroByteCompact
    uint64_t totalBitsProcessed=(addrEnd-mBaseAddr)<<3;

    totalBitsProcessed-=zeros<<3;
    totalBitsProcessed-=subByteBitCount;

    mBitCount=totalBitsProcessed;
  }

  void BitArray::Slice(BitArray &destination,uint64_t from,uint64_t to){
    if(from>=to)
      throw std::range_error("Succinct::BitArray::Slice: from position is greater or equal to the to position");
    if(to>=mBitCount)
      throw std::range_error("Succinct::BitArray::Slice: to position exceeds upper bounds of bit array");

    //Calculate the total number of quadwords to copy
    size_t totalBytes=((to+63)>>6)-(from>>6);
    totalBytes<<=3;

    if((totalBytes<<3)>destination.mMaxBitCount)
      throw std::range_error("Succinct::BitArray::Slice: the slice range can not fit into destination array");

    uint64_t *srcAddr=mBaseAddr+(from>>6);
    
    memcpy(destination.mBaseAddr,srcAddr,totalBytes);

    ShiftRight256(destination.mBaseAddr+(totalBytes>>3),
                  destination.mBaseAddr,
                  from&0x000000000000003FULL,0);

    destination.mBitCount=to-from;
  }

/*
  void BitArray::Copy(const Succinct *b){
    const BitArray *from=dynamic_cast<const BitArray*>(b);

    uint64_t size=std::min<uint64_t>(mBitCount,from->mBitCount);
    
    uint64_t       *addr=mBaseAddr+(size>>6);
    const uint64_t *addrEnd=mBaseAddr+(mBitCount+63)/64;

    std::memcpy(mBaseAddr,from->mBaseAddr,(size+7)/8);

    if(size<mMaxBitCount){
      //If the bitArray being copied is smaller then this one
      //zero out the rest of the array so the counter levels and Rebuild properly
      *addr++&=0xFFFFFFFFFFFFFFFFULL<<(size&0x000000000000003FULL);

      if(addr<addrEnd)
        std::memset(addr,0,addrEnd-addr);
    }
  }

  Succinct* BitArray::operator=(const Succinct *b){
    Copy(b);
    return this;
  }

  bool BitArray::operator==(const Succinct *b)const{
    const BitArray *from=dynamic_cast<const BitArray*>(b);

    if(from->Size()!=Size())
      return false;

    size_t count=(Size()+63)/64;

    while(count--){
      if(*(mBaseAddr+count)!=*(from->mBaseAddr+count))
        return false;
    }

    return true;
  }

  std::tuple<uint64_t,uint64_t*,BitArray*> BitArray::Allocate(uint64_t bitCount){
    uint64_t size=RequiredSize(bitCount);
    uint64_t *localAddr=static_cast<uint64_t*>(_aligned_malloc(size*8,64));

    return std::make_tuple(size*8,localAddr,new BitArray(localAddr,bitCount));
  }
  */
}