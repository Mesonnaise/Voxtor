#include<stdexcept>
#include<string>
#include<atomic>
#include<algorithm>
#include<Windows.h>
#include<malloc.h>
#include<intrin.h>
#include"L0Block.h"
#include"Util.h"



namespace Succinct{
  void L0Block::Allocate(){
    if(mAllocatedSize)
      return;

    uint64_t *cmp=mCountersAddr.load(std::memory_order_acquire);
    uint64_t *flag=reinterpret_cast<uint64_t*>(0x8000000000000000ULL);

    if(cmp==nullptr){
      //Race to see which thread gets to allocate the memory.
      //Loser spin locks until the 0x8000000000000000ULL flag is cleared.

      if(mCountersAddr.compare_exchange_strong(cmp,flag,std::memory_order_seq_cst)){
        uint64_t *tmpPointer=nullptr;

        size_t L1GroupCount=(mBitCountReserve+mBitsPerL1-1)/mBitsPerL1;
        size_t allocationSize=(L1GroupCount*mBitsPerL1)/8;

        //Round up to a multible of 4
        L1GroupCount=(L1GroupCount+3)&0xFFFFFFFFFFFFFFFCULL;

        allocationSize+=L1GroupCount*8;
        
        if(allocationSize<(16*1024)){
          tmpPointer=static_cast<uint64_t*>(_mm_malloc(allocationSize,32));
          if(tmpPointer)
            mAllocatedSize=_aligned_msize(tmpPointer,32,0);

          mDirectPageAlloc=false;
        }else{
          uint32_t flags=MEM_RESERVE|MEM_COMMIT;

          //Round up to a multiple of the page size
          mAllocatedSize=allocationSize+4095;
          mAllocatedSize&=0xFFFFFFFFFFFFF000ULL;

          if(allocationSize>=(512*1024)){
            flags|=MEM_64K_PAGES;

            mAllocatedSize=allocationSize+65535;
            mAllocatedSize&=0xFFFFFFFFFFFF0000ULL;
          }

          tmpPointer=static_cast<uint64_t*>(VirtualAlloc(nullptr,allocationSize,flags,PAGE_READWRITE));

          mDirectPageAlloc=true;
        }

        if(tmpPointer==nullptr){
          mAllocatedSize=0;
          throw std::runtime_error("Succinct::L0Block::Allocate: Failed to allocate memory");
        }
        memset(tmpPointer,0,mAllocatedSize);


        mBitArrayAddr.store(tmpPointer+L1GroupCount);
        mCountersAddr.store(tmpPointer);
        
      }else{
        //Dirty spin-lock
        while(flag==mCountersAddr.load(std::memory_order_consume)){}

      }
    }
  }

  void L0Block::Invalidate(){
    if(mCountersAddr){
      if(mDirectPageAlloc)
        VirtualFree(mCountersAddr,0,MEM_RELEASE);
      else
        _mm_free(mCountersAddr);
    }

    mIsValid=false;
  }

  bool L0Block::LockPointer(L0Block **p,uint64_t spin){
    uint64_t next=(uint64_t)*p;
    atomic_thread_fence(std::memory_order_acquire);

    uint64_t exg=next|0x8000000000000000ULL;
    uint64_t cmp=next&&0x7FFFFFFFFFFFFFFFULL;


    if(_InterlockedCompareExchange64((long long*)p,(long long)exg,(long long)cmp)!=cmp){
      spin<<=1;

      for(uint64_t i=0;i<spin;i++)
        atomic_thread_fence(std::memory_order_relaxed);

      return false;
    }
    return true;
  }

  L0Block::L0Block(size_t reserveBitCount,bool fixed):mBitCountReserve(reserveBitCount){
    if(reserveBitCount%mBitsPerL1)
      throw std::runtime_error("Succinct::L0Block: Reserve bit count must be a multible of "+std::to_string(mBitsPerL1));

    if(fixed)
      mBitCountDynamic=reserveBitCount;

  }

  L0Block::~L0Block(){
    if(!mPrevBlock){
      //Root Block in the list
      L0Block *trailingBlock=Next();
      while(trailingBlock&&trailingBlock->Next())
        trailingBlock=trailingBlock->Next();
      if(trailingBlock){
        L0Block *interBlock=trailingBlock->Prev();
        while(interBlock){
          delete trailingBlock;
          trailingBlock=interBlock;
          interBlock=interBlock->Prev();
        }
      }
    }else{
      Prev()->mNextBlock=Next();
      if(Next())
        Next()->mPrevBlock=Prev();
    }

    Invalidate();
  }

  void L0Block::AppendBlock(L0Block *newBlock){
    if(!newBlock)
      throw std::runtime_error("Succinct::L0Block::AppendBlock: No L0Block was provided.");

    uint64_t spin=16;

    while(!LockPointer(&mNextBlock,spin))
      spin<<=1;

    newBlock->mL0Counter=Rank(mBitCountDynamic-1);

    newBlock->mNextBlock=Next();
    newBlock->mPrevBlock=this;

    //This may be a problem
    if(Next())
      Next()->mPrevBlock=newBlock;

    atomic_thread_fence(std::memory_order_release);
    mNextBlock=newBlock;

    
  }

  void L0Block::RemoveBlock(){
    uint64_t spin=16;

    while(!LockPointer(&(Prev()->mNextBlock),spin))
      spin<<=1;

    spin=16;
    while(!LockPointer(&mNextBlock,spin))
      spin<<=1;

    if(Next())
      Next()->mPrevBlock=Prev();

    atomic_thread_fence(std::memory_order_release);

    if(Prev())
      Prev()->mNextBlock=Next();
  }

  void L0Block::Size(size_t size){
    if(size>mBitCountReserve)
      throw std::runtime_error("Succinct::L0Block::Size: New size is bigger than the available space");

    mBitCountDynamic=size;
  }

  uint64_t L0Block::Rank(uint64_t pos){
    if(mCountersAddr==nullptr)
      return 0;

    Rebuild();

    uint64_t *counter=mCountersAddr.load(std::memory_order_relaxed);
    uint64_t L1L2Counter=*(counter+(pos/mBitsPerL1));

    uint64_t rollCount=L1L2Counter&0x00000000FFFFFFFFULL;

    L1L2Counter>>=32;

    uint64_t L2End=(pos/(mBitsPerL1/4))&0x0000000000000003ULL;
    L2End*=10;
    for(int L2Idx=0;L2Idx<L2End;L2Idx+=10)
      rollCount+=(L1L2Counter>>L2Idx)&0x00000000000003FFULL;

    uint64_t *bitArrayAddr=mBitArrayAddr+((pos/64)&0xFFFFFFFFFFFFFFF8ULL);
    uint64_t *bitArrayAddrEnd=mBitArrayAddr+(pos/64);

    while(bitArrayAddr<bitArrayAddrEnd)
      rollCount+=__popcnt64(*bitArrayAddr++);

    uint64_t mask=0xFFFFFFFFFFFFFFFFULL<<((pos&0x000000000000003FULL)+1);
    rollCount+=__popcnt64(*bitArrayAddr&~mask);
    
    return mL0Counter+rollCount;
  }

  uint64_t L0Block::Select(uint64_t count){
    Rebuild();

    uint64_t *L1Addr=mCountersAddr.load(std::memory_order_relaxed);
    uint64_t *L1AddrEnd=L1Addr+(mBitCountDynamic+(mBitsPerL1-1))/mBitsPerL1;

    uint64_t rollposition=0;

    count-=mL0Counter;

    while(count>(*L1Addr&0x00000000FFFFFFFFULL)&&L1Addr<L1AddrEnd){
      count-=*L1Addr++&0x00000000FFFFFFFFULL;
      rollposition+=mBitsPerL1;
    }

    //Dirty ugly hack :D
    uint64_t L2Counters=(*L1Addr>>32)|0xFFFFFFFFC0000000ULL;
    while(count>(L2Counters&0x00000000000003FFULL)){
      count-=L2Counters&0x00000000000003FFULL;
      L2Counters>>=10;
      rollposition+=mBitsPerL1/4;
    }

    uint64_t *bitAddr=mBitArrayAddr.load(std::memory_order_relaxed);
    uint64_t *bitAddrEnd=bitAddr+(mBitCountDynamic+63)/64;
    bitAddr+=(rollposition/64);

    uint64_t popcnt=__popcnt64(*bitAddr);

    while(count>popcnt&&bitAddr<bitAddrEnd){
      count-=popcnt;
      rollposition+=64;
      popcnt=__popcnt64(*++bitAddr);
    }
    rollposition+=BwordSelect(*--bitAddr,count);

    return rollposition;
  }

  bool L0Block::Get(uint64_t pos)const{
    if(mAllocatedSize==0)
      return false;

    return *(mBitArrayAddr+((pos/64)>>(pos&0x000000000000003FULL)))&0x0000000000000001ULL;
  }

  void L0Block::Set(uint64_t pos){
    Allocate();

    Rebuild();

    uint64_t *addr=mCountersAddr.load(std::memory_order_relaxed);
    uint64_t *addrEnd=addr+(mBitCountDynamic+(mBitsPerL1-1))/mBitsPerL1;
    addr+=(pos/mBitsPerL1);

    uint64_t L2Pos=(pos/(mBitsPerL1/4))&0x0000000000000003ULL;
    int64_t  counter=0x0000000000000001ULL<<(32+L2Pos*10);

    _InterlockedExchangeAdd64((long long*)addr++,counter);

    while(addr<addrEnd)
      _InterlockedIncrement64((long long*)(addr++));

    *(mBitArrayAddr+(pos/64))|=0x0000000000000001ULL<<(pos&0x000000000000003FULL);

    auto next=Next();
    while(next){
      next->mL0Counter++;
      next=next->Next();
    }
  }

  void L0Block::Clear(uint64_t pos){
    Rebuild();

    uint64_t *addr=mCountersAddr+(pos/mBitsPerL1);
    uint64_t *addrEnd=mCountersAddr+(mBitCountDynamic+(mBitsPerL1-1))/mBitsPerL1;
    
    uint64_t L2Pos=(pos/(mBitsPerL1/4))&0x0000000000000003ULL;
    int64_t  counter=(0x0000000000000001ULL<<(32+L2Pos*10))*-1;

    _InterlockedExchangeAdd64((long long*)addr++,counter);

    while(addr<addrEnd)
      _InterlockedDecrement64((long long*)(addr++));

    *(mBitArrayAddr+(pos/64))&=~(0x0000000000000001ULL<<(pos&0x000000000000003FULL));

    auto next=Next();
    while(next){
      next->mL0Counter--;
      next=next->Next();
    }

  }

  uint64_t L0Block::GetGroup(uint64_t pos,size_t size)const{
    if(mAllocatedSize==0)
      return 0;

    return *(mBitArrayAddr+((pos/64)>>(pos&0x000000000000003FULL)))&~(0xFFFFFFFFFFFFFFFFULL<<size);
  }

  uint64_t L0Block::PopCount(){
    return Rank(mBitCountDynamic-1);
  }

  uint64_t L0Block::Insert(uint64_t pos,size_t ammount,uint64_t value){
    if(pos>mBitCountDynamic&&(pos+ammount)>=mBitCountReserve)
      throw std::range_error("Succinct::L0Block::Insert: Position exceeds upper bounds of bit array");

    Allocate();

    mBitCountDynamic=std::min(mBitCountDynamic+ammount,mBitCountReserve);

    uint64_t invSize=64-ammount;
    uint64_t wordOffset=pos/64;
    uint64_t bitOffset=pos&0xFFFFFFFFFFFFFF3FULL;
    uint64_t lowerMask=~(0xFFFFFFFFFFFFFFFFULL<<bitOffset);

    uint64_t *addr=mBitArrayAddr+wordOffset;

    uint64_t endOffset=((mBitCountDynamic+255)/64)&0xFFFFFFFFFFFFFFFCULL;
    uint64_t alignOffset=((wordOffset+4)&0xFFFFFFFFFFFFFFFCULL);

    uint64_t *addrEnd=mBitArrayAddr+endOffset;
    uint64_t *addrAlign=mBitArrayAddr+std::min(endOffset,alignOffset);


    uint64_t carry=*addr>>invSize;
    *(addr++)=(*addr&~lowerMask)<<ammount|*addr&lowerMask|value<<bitOffset;

    carry=ShiftLeft64(addr,addrAlign,ammount,carry);
    carry=ShiftLeft256(addrAlign,addrEnd,ammount,carry);

    mIsCountersDirty=true;
    mDirtyBitOffset=pos;
    return carry;
  }

  uint64_t L0Block::Remove(uint64_t pos,size_t ammount,uint64_t carry){
    if(pos>mBitCountDynamic&&(pos+ammount)>=mBitCountReserve)
      throw std::range_error("Succinct::L0Block::Remove: Position exceeds upper bounds of bit array");

    uint64_t invSize=64-ammount;
    uint64_t wordOffset=pos/64;
    uint64_t bitOffset=pos&0xFFFFFFFFFFFFFF3FULL;
    uint64_t lowerMask=~(0xFFFFFFFFFFFFFFFFULL<<bitOffset);

    uint64_t startOffset=((mBitCountDynamic+255)/64)&0xFFFFFFFFFFFFFFFCULL;
    uint64_t alignOffset=((wordOffset+4)&0xFFFFFFFFFFFFFFFCULL);

    uint64_t *addr=mBitArrayAddr+startOffset;
    uint64_t *addrAlign=mBitArrayAddr+std::min(startOffset,alignOffset);

    uint64_t *addrEnd=mBitArrayAddr+wordOffset;

    carry=ShiftRight256(addr,addrAlign,ammount,carry);
    carry=ShiftLeft64(addrAlign,addrEnd,ammount,carry);

    carry=*addrEnd>>invSize;

    *addrEnd=*addrEnd<<ammount|*addr&lowerMask;

    mBitCountDynamic-=ammount;

    mIsCountersDirty=true;
    mDirtyBitOffset=pos;
    return carry;
  }

  size_t L0Block::Filter(L0Block *mask,uint64_t maskOffset,uint64_t maskScale){
    const uint64_t depMap[]={0xFFFFFFFFFFFFFFFFULL,0x5555555555555555ULL,
                             0x1111111111111111ULL,0x0101010101010101ULL,
                             0x0001000100010001ULL,0x0000000100000001ULL,
                             0x0000000000000001UL};

    const uint64_t spread=depMap[_tzcnt_u64(maskScale)];
    const uint64_t spreadSize=64/maskScale;
    const uint64_t spreadFill=maskScale==64?0xFFFFFFFFFFFFFFFFULL:
                                          ~(0xFFFFFFFFFFFFFFFFULL<<maskScale);
 

    uint64_t *dstAddr=mBitArrayAddr;
    uint64_t *srcAddr=mBitArrayAddr;
    uint64_t *addrEnd=mBitArrayAddr+(mBitCountDynamic+63)/64;

    uint64_t *maskAddr=mask->mBitArrayAddr+maskOffset;
    uint64_t *maskEnd=mask->mBitArrayAddr+(mask->mBitCountDynamic+63)/64;
    
    uint64_t dstBitPos=0;
    uint64_t maskBitPos=0;

    while(srcAddr<addrEnd&&maskAddr<maskEnd){
      uint64_t inter;
      inter=_pdep_u64(*maskAddr>>maskBitPos,spread);
      inter*=spreadFill;

      maskBitPos+=spreadSize;
      maskAddr+=maskBitPos/64;
      maskBitPos&=0x000000000000003FULL;

      uint64_t packedSize=__popcnt64(inter);
      uint64_t packed=_pext_u64(*srcAddr++,inter);

      //Insert the now packed quadword into the destination address and
      //take care of any miss alignment where depositing would take place across quadwords

      uint64_t used;
      while(packedSize){
        *dstAddr&=~(0xFFFFFFFFFFFFFFFFULL<<dstBitPos);
        *dstAddr|=packed<<dstBitPos;

        used=std::min(packedSize,64-dstBitPos);

        packedSize-=used;
        dstBitPos+=used;

        dstAddr+=dstBitPos/64;

        dstBitPos&=0x000000000000003FULL;
        packed>>=used;
      }
    }

    size_t newSize=reinterpret_cast<uint64_t>(dstAddr)-
                   reinterpret_cast<uint64_t>(mBitArrayAddr.load());

    newSize=(newSize<<4)-(64-dstBitPos);
    mBitCountDynamic=newSize;

    *dstAddr++&=~(0xFFFFFFFFFFFFFFFFULL<<dstBitPos);

    while(dstAddr<addrEnd)
      *dstAddr++=0x0000000000000000ULL;

    size_t usedMaskBits=reinterpret_cast<uint64_t>(maskAddr)-
                        reinterpret_cast<uint64_t>(mask->mBitArrayAddr.load())+
                        maskOffset;

    mIsCountersDirty=true;
    mDirtyBitOffset=0;
    return usedMaskBits;
  }

  size_t L0Block::AppendDonation(L0Block *donor){
    if(donor->mBitCountDynamic==0)
      return 0;

    //size will be the ammount of avilable space in the current block
    //or the ammount of data in the donner, depends on which one is smaller
    uint64_t size=mBitCountDynamic-mBitCountReserve;
    size=std::min(size,donor->mBitCountDynamic);

    //mBitCountDynamic is divided by 64 to get quadword offset, 
    //and then rounded down by a multiple of 4
    uint64_t *dst=mBitArrayAddr+((mBitCountDynamic/64)&0xFFFFFFFFFFFFFFFCULL);
    uint64_t *src=donor->mBitArrayAddr;
    //Same as dst but rounded up
    uint64_t *srcEnd=donor->mBitArrayAddr+(((size+254)/64)&0xFFFFFFFFFFFFFFFCULL);
    uint64_t  alignment=mBitCountDynamic&0x00000000000000FFULL;

    CopyUnalignedDestination256(dst,src,srcEnd,alignment);

    if(size<donor->mBitCountDynamic){
      //If the donor is empty don't do the copy
      dst=donor->mBitArrayAddr;
      src=donor->mBitArrayAddr+((size/64)&0xFFFFFFFFFFFFFFFCULL);
      srcEnd=donor->mBitArrayAddr+(((donor->mBitCountDynamic+254)/64)&0xFFFFFFFFFFFFFFFCULL);
      alignment=size&0x00000000000000FFULL;

      CopyUnalignedSource256(dst,src,srcEnd,alignment);
      //May require bit zeroing after copy
    }

    mIsCountersDirty=true;
    mDirtyBitOffset=mBitCountDynamic;
    donor->mIsCountersDirty=true;

    mBitCountDynamic+=size;
    donor->mBitCountDynamic-=size;
    return size;
  }

  size_t L0Block::Slice(L0Block *destinationBlock,uint64_t start,uint64_t end){
    //This needs to be redone 
    //Should be from the perspective destination and not source
    if(destinationBlock->mAllocatedSize==0)
      destinationBlock->Allocate();

    uint64_t *dst=destinationBlock->mBitArrayAddr;
    uint64_t *src=mBitArrayAddr+((start/64)&0xFFFFFFFFFFFFFFFCULL);
    uint64_t *srcEnd=mBitArrayAddr+(((end+254)/64)&0xFFFFFFFFFFFFFFFCULL);
    uint64_t  alignment=start&0x00000000000000FFULL;

    CopyUnalignedSource256(dst,src,srcEnd,alignment);
    //May require bit zeroing after copy

    destinationBlock->mBitCountDynamic=end-start;
    destinationBlock->mIsCountersDirty=true;

    return destinationBlock->mBitCountDynamic;
  }

  size_t L0Block::Clone(L0Block *destinationBlock){
    if(destinationBlock->mAllocatedSize==0)
      destinationBlock->Allocate();

    std::memcpy(destinationBlock->mBitArrayAddr,
                mBitArrayAddr,
               (mBitCountDynamic+7)>>4);

    std::memcpy(destinationBlock->mCountersAddr,
                mCountersAddr,
                ((mBitCountDynamic+2047)>>8)&0xFFFFFFFFFFFFFFF8ULL);

    destinationBlock->mBitCountDynamic=mBitCountDynamic;
    return destinationBlock->mBitCountDynamic;
  }

  void L0Block::Rebuild(){
    if(!mIsCountersDirty)
      return;

    uint64_t *addr=mCountersAddr+(mDirtyBitOffset/mBitsPerL1);
    uint64_t *addrEnd=mCountersAddr+(mBitCountDynamic+(mBitsPerL1-1))/mBitsPerL1;

    uint64_t *bitAddr=mBitArrayAddr+(mDirtyBitOffset/64);

    uint64_t rollCount=*addr&0x00000000FFFFFFFFULL;

    uint64_t L2Count=0;
    uint64_t counters=0;

    while(addr<addrEnd){
      counters=rollCount;

      for(int L2Idx=32;L2Idx<62;L2Idx+=10){
        L2Count=0;

        for(int i=0;i<4;i++)
          L2Count+=__popcnt64(*bitAddr++);

        rollCount+=L2Count;

        counters|=L2Count<<L2Idx;
      }

      for(int i=0;i<4;i++)
        rollCount+=__popcnt64(*bitAddr++);

      *addr++=counters;
    }

    L0Block *curBlock=Next();
    if(curBlock){
      int64_t delta=int64_t(mL0Counter+rollCount)-curBlock->mL0Counter;

      curBlock->mL0Counter=mL0Counter+rollCount;

      curBlock=curBlock->Next();
      while(curBlock){
        curBlock->mL0Counter+=delta;
        curBlock=curBlock->Next();
      }
    }
  }
}