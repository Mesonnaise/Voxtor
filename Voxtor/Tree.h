#pragma once
#include<vector>
#include<algorithm>
#include<memory>
#include"Succinct/Array.h"
#include"Index.h"

enum TreeType{
  TREE_DENSE=true,
  TREE_SPARSE=false
};

template<TreeType IsDense>
class Tree{

private:
  std::vector<Succinct::Array>  mStack;
  mutable Index                 mIndex;
  mutable std::vector<uint64_t> mOffsets;
protected:
  void                         Walk()const;
  void                         UpdateVec(Vector vec,size_t depth)const;
  //Truncates the cursor to the size of input index if mIndex and input equal
  //This is used in multithreaded envoroments for node removal
  void                         Truncate(Index to)const;
private:
  Tree(){}
protected:
  constexpr uint64_t Cursor()const{ return mOffsets.back()+mIndex[mOffsets.size()-1]; }
public:
  Tree(size_t depth);
  ~Tree(){};

  size_t   AllocatedSize()const;
  size_t   ReserveSize()const;
  size_t   Size()const;

  uint64_t NodeCount()const;
  uint64_t LeafCount()const;

  Vector   Dimensions()const;
  bool     Empty()const{ return mStack->at(0).Empty(); }

  bool     Get(Vector vec,size_t depth)const;
  void     Set(Vector vec,size_t depth);
  bool     Clear(Vector vec,size_t depth);

  std::vector<uint64_t> Map(Vector vec,size_t depth)const;
};

template<TreeType IsDense> Tree<IsDense>::Tree(size_t depth):mIndex(0){
  mOffsets.reserve(depth);
  mOffsets.push_back(0);

  mStack.reserve(depth);
  for(size_t level=0;level<depth;level++){
    mStack.emplace_back(level,IsDense);
  }
}

template<TreeType IsDense> void Tree<IsDense>::Walk()const{
  auto offset=mOffsets.back();
  auto stackPos=mOffsets.size()-1;

  for(;stackPos<mIndex.Size()-1;stackPos++){
    offset+=mIndex[stackPos];
    
    if(!mStack[stackPos].Get(offset))
      break;

    if constexpr(IsDense)
      offset*=8ULL;
    else
      offset=(mStack[stackPos].Rank(offset)-1ULL)*8ULL;

    mOffsets.push_back(offset);
  }
}

template<TreeType IsDense> void Tree<IsDense>::UpdateVec(Vector vec,size_t depth)const{
  Index newIdx(vec,std::min(depth,mStack.size()));

  //check if the indexes are the same
  if(newIdx==mIndex)
    return;

  if(mIndex.Size()!=0){
    auto tuncateSize=mIndex.IntersectionPoint(newIdx);

    tuncateSize=std::clamp(mOffsets.size()-tuncateSize,1ULL,mOffsets.size());

    mOffsets.resize(tuncateSize);
  }

  mIndex=newIdx;

  Walk();
}

template<TreeType IsDense> void Tree<IsDense>::Truncate(Index to)const{
  if(to.Size()>=mOffsets.size()){
    auto tuncateSize=mIndex.IntersectionPoint(to);

    //If the two indexes line up truncate
    if(truncateSize==to.Size()){
      tuncateSize=std::clamp(tuncateSize,1ULL,mOffsets.size());
      mOffsets.resize(tuncateSize);
    }
  }
}

template<TreeType IsDense> size_t Tree<IsDense>::AllocatedSize()const{
  size_t size=0;

  for(auto &array:mStack)
    size+=array.AllocatedSize();

  return size;
}

template<TreeType IsDense> size_t Tree<IsDense>::ReserveSize()const{
  size_t size=0;

  for(auto &array:mStack)
    size+=array.ReserveSize();

  return size;
}

template<TreeType IsDense> size_t Tree<IsDense>::Size()const{
  size_t size=0;

  for(auto &array:mStack)
    size+=array.Size();

  return size;
}

template<TreeType IsDense> uint64_t Tree<IsDense>::NodeCount()const{
  uint64_t count=0;
  
  for(int pos=0;pos<(mStack.size()-1);pos++)
    count+=mStack[pos].PopCount();

  return count;
}

template<TreeType IsDense> uint64_t Tree<IsDense>::LeafCount()const{
  return mStack.back().PopCount();
}

template<TreeType IsDense> Vector Tree<IsDense>::Dimensions()const{

  if constexpr(IsDense){
    size_t dim=(2<<(mStack.size()-1));
    return Vector(dim,dim,dim);
  }else{
    //Find index for max value
    size_t   depth=mStack.size();
    Index    result(depth);
    uint64_t lastValue=1;
    int      idx=0;


    for(;idx<depth;idx++){
      auto popCount=mStack[idx].PopCount();
      lastValue=(lastValue-1)*8;
      //if the popCount of the current level is
      //less than the position value for the node
      //The node is considered the leaf
      if(lastValue>popCount)
        break;

      result[idx]=popCount-lastValue;
      lastValue=popCount;
    }

    //Finish filling the index up with max values
    //If the tree has been pruned at this location
    for(++idx;idx<depth;idx++){
      result[idx]=7;
    }

    return result.GetVector()+Vector(1,1,1);
  }
}

template<TreeType IsDense> bool Tree<IsDense>::Get(Vector vec,size_t depth)const{
  UpdateVec(vec,depth);

  size_t cursorDepth=mOffsets.size();

  if(cursorDepth!=depth)
    return false;

  return mStack[cursorDepth-1].Get(Cursor());
}

template<TreeType IsDense> void Tree<IsDense>::Set(Vector vec,size_t depth){
  UpdateVec(vec,depth);

  size_t stackPos=mOffsets.size()-1;
  uint64_t offset;

  if constexpr(IsDense){
    while(stackPos<mIndex.Size()){
      offset=Cursor();

      mStack[stackPos++].Set(offset);

      offset*=8ULL;

      mOffsets.push_back(offset);
    }
  } else{
    //This needs to block other thread accesses while inserting
    

    while(stackPos<mIndex.Size()&&(stackPos+1)<mStack.size()){
      offset=Cursor();
      mStack[stackPos].Set(offset);
      offset=(mStack[stackPos].Rank(offset)-1ULL)*8ULL;

      mOffsets.push_back(offset);

      mStack[++stackPos].Insert(offset,8,0x0000000000000000ULL);
    }

    if(stackPos<mIndex.Size())
      mStack[stackPos].Set(Cursor());
  }

  
}

template<TreeType IsDense> bool Tree<IsDense>::Clear(Vector vec,size_t depth){
  //This needs to block other thread accesses while deleting
  UpdateVec(vec,depth);

  //Node removal can only happen if the index can touch a leaf
  //and match the depth parameter. 
  if(mOffsets.size()<mIndex.Size())
    return false;

  uint64_t offset;
  size_t stackPos=mOffsets.size();

  if(stackPos--<mStack.size()){
    uint64_t offset=mCursor;

    //Check if the node that is going to be delete has children
    if constexpr(IsDense)
      offset*=8ULL;
    else
      offset=(mStack[stackPos].Rank(offset)-1ULL)*8ULL;

    if(mStack[stackPos+1].GetGroup(offset,8))
      return false;
  }

  if constexpr(IsDense){
    do{
      offset=mCursor;
      mStack[stackPos].Clear(offset);
      mCursor.mOffsets.pop_back();
      offset&=~0x0000000000000007ULL;
    }while(stackPos>1&&!mStack[stackPos--].GetGroup(offset,8));

  }else{
    offset=mCursor;

    if(stackPos==mStack.size()-1){
      mStack[stackPos].Clear(offset);
      mCursor.mOffsets.pop_back();
    }

    offset&=~0x0000000000000007ULL;
    
    while(stackPos>1&&!mStack->at(stackPos).GetGroup(offset,8)){
      mStack[stackPos].Remove(offset,8);
      offset=mCursor;

      mStack[--stackPos].Clear(offset);
      mCursor.mOffsets.pop_back();
    };
  }

  //Put the root offset back if it gets removed by the above loops
  if(!mCursor.Depth())
    mCursor.mOffsets.push_back(0);

  return true;
}

template<TreeType IsDense> std::vector<uint64_t> Tree<IsDense>::Map(Vector vec,size_t depth)const{
  UpdateVec(vec,depth);
  std::vector<uint64_t> ret;
  
  for(size_t i=0;i<mOffsets.size();i++)
    ret.push_back(mOffsets[i]+mIndex[i]);

  return ret;
}