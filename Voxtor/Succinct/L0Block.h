#pragma once
#include<cinttypes>
#include<atomic>
#include<tuple>

//Implementation is based off paper 
//Space-Efficient, High-Performance Rank & Select Structures on Uncompressed Bit Sequences, June 2013
//by Dong Zhou, David G. Andersen, Michael Kaminsky

namespace Succinct{
  __declspec(align(64)) class L0Block{
  public:
    /* IMPORTANT: Bit count must be multible of this number */
    const static size_t    mBitsPerL1=2048;//Divisor
  private:
    int64_t                mL0Counter=0;

    L0Block               *mNextBlock=nullptr;
    L0Block               *mPrevBlock=nullptr;

    //These need to be changed to normal pointers 
    //The overhead of the atomic load out ways the allocators simplification
    std::atomic<uint64_t*> mCountersAddr=nullptr;
    std::atomic<uint64_t*> mBitArrayAddr=nullptr;

    uint32_t               mAllocatedSize=0;//In bytes
    const uint32_t         mBitCountReserve;
    uint32_t               mBitCountDynamic=0;

    uint32_t               mDirtyBitOffset=0;//Don't have to rebuild all the counters unless necessary
    
    bool                   mDirectPageAlloc=false;//Did the allocation happen with VirtualAlloc
    bool                   mIsCountersDirty=false;//Indicates if the counters need a rebuild
    bool                   mIsValid=true;         //For Hazard pointer implementations.
  
  protected:
    /* Allocates the needed 256bit aligned memory 
       to fit the number of bits set in mBitCountReserve.
       Will try to allocate full pages after 4k (either 4k or 64K pages)
       Sets mBaseAddr, mCountersAddr, and mAllocatedSize.
    */
    void Allocate();

    void Invalidate();

    bool LockPointer(L0Block **,uint64_t spin);
  public:
    //Smallist overhead: 253952,1302528,2351104,3399680,4448256,6545408 bits
    L0Block(size_t reserveBitCount,bool fixed);
    ~L0Block();

    void AppendBlock(L0Block *newBlock);
    void RemoveBlock();

    constexpr L0Block* Next(){         return (L0Block*)((uint64_t)mNextBlock&0x7FFFFFFFFFFFFFFFULL);}

    constexpr L0Block* Prev(){         return (L0Block*)((uint64_t)mPrevBlock&0x7FFFFFFFFFFFFFFFULL);}

    constexpr size_t   AllocatedSize(){return mAllocatedSize;}

    constexpr size_t   ReserveSize(){  return mBitCountReserve;}

    constexpr size_t   Size(){         return mBitCountDynamic;}

    constexpr bool     Empty(){        return mBitCountDynamic==0;}

    constexpr bool     Full(){         return mBitCountDynamic==mBitCountReserve;}
    
    constexpr bool     IsValid(){      return mIsValid;}

    inline    uint64_t BaseCounter(){  return mL0Counter;}

    void               Size(size_t size);
    uint64_t           Rank(uint64_t pos);
    uint64_t           Select(uint64_t pos);

    bool               Get(uint64_t pos)const;
    void               Set(uint64_t pos);
    void               Clear(uint64_t pos);

    uint64_t           GetGroup(uint64_t pos,size_t size)const;

    uint64_t           PopCount();

    uint64_t           Insert(uint64_t pos,size_t ammount,uint64_t value=0);
    uint64_t           Remove(uint64_t pos,size_t ammount,uint64_t carry=0);
    
    /* Filters out bits from the array based on the array supplied as mask.
       The function will contenue until the end of either mask's array or local array.
       Return the number of mask bytes used. 

       maskOffset is where in the mask array to start.
       maskScale is the proportion of single mask bit to bit(s) in the local array, 
       must be 1 or a power of 2.
    */
    size_t             Filter(L0Block *mask,uint64_t maskOffset,uint64_t maskScale);

    /* Fills any extra space at the end of the array, with the leading bits
       from the donor array. Shrinking donor array in the process 
    */
    size_t             AppendDonation(L0Block *donor);

    /* Copy the region between start and end to the destination Block.
       Will overwrite the destination's Block array. 
       Return the number of bits copied into the destination Block.
    */
    size_t             Slice(L0Block *destinationBlock,uint64_t start,uint64_t end);
    size_t             Clone(L0Block *destinationBlock);

    /* Recalculates the counters L1, L2, then propergates the total count
    to the trailing blocks L0 counters
    */
    void               Rebuild();
  };
}
