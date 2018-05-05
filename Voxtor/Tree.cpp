#include<algorithm>
#include "Tree.h"
#include"Succinct\L0Block.h"

Tree::CursorPair Tree::Cursor()const{
  size_t idx=mCursorStack.size()-1;

  return std::make_tuple(StackPosition{mCursorStack[idx],mCursorIndex[idx]},idx);
}

void Tree::CursorClear()const{
  mCursorStack.resize(0);
  mCursorPostion=0;
  mCursorIndex=Index(Vector(0,0,0),0);
}

Tree::CursorPair Tree::CursorUpdate(Vector vec)const{
  Index newIndex(vec,mStack.size());

  size_t cursorSize=mCursorIndex.IntersectionPoint(newIndex);
  cursorSize=std::min(cursorSize,mCursorStack.size());
  cursorSize=std::max(cursorSize,1ULL);
  mCursorStack.resize(cursorSize);

  mCursorIndex=newIndex;

  mCursorPostion=std::min(mCursorPostion,cursorSize-1ULL);

  return Cursor();
}

Tree::CursorPair Tree::CursorPrev()const{
  --mCursorPostion;
  return Cursor();
}

Tree::CursorPair Tree::CursorNext()const{
  ++mCursorPostion;
  return Cursor();
}

bool Tree::CursorPop()const{
  mCursorStack.pop_back();

  mCursorPostion=std::min(mCursorStack.size()-1,mCursorPostion);

  return mCursorStack.size()!=0;
}

Tree::CursorPair Tree::CursorWalk()const{
  uint64_t offset=mCursorStack.back();
  size_t idx=mCursorStack.size()-1;

  if(mIsDense){
    for(;idx<mCursorIndex.Size()-1;idx++){
      offset+=mCursorIndex[idx];

      if(!mStack[idx].Get(offset))
        break;

      offset*=8ULL;
      mCursorStack.push_back(offset);

      mCursorPostion++;
    }
  }else{
    for(;idx<mCursorIndex.Size()-1;idx++){
      offset+=mCursorIndex[idx];

      if(!mStack[idx].Get(offset))
        break;

      offset=(mStack[idx].Rank(offset)-1ULL)*8ULL;
      mCursorStack.push_back(offset);

      mCursorPostion++;
    }
  }

  return Cursor();
}

Tree::Tree(size_t depth,bool dense):mIsDense(dense){
  mStack.reserve(depth);
  mCursorStack.reserve(depth);

  for(size_t level=0;level<depth;level++)
    mStack.emplace_back(level,dense);
    
  mCursorStack.push_back(0);
}

Tree::~Tree(){}

size_t Tree::AllocatedSize()const{
  size_t size=0;

  for(auto &array:mStack)
    size+=array.AllocatedSize();

  return size;
}

size_t Tree::ReserveSize()const{
  size_t size=0;

  for(auto &array:mStack)
    size+=array.ReserveSize();

  return size;
}

size_t Tree::Size()const{
  size_t size=0;

  for(auto &array:mStack)
    size+=array.Size();

  return size;
}

Vector Tree::Dimensions()const{
  if(mIsDense){
    //2^depth-1
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

bool Tree::Empty()const{
  return mStack[0].Empty();
}

bool Tree::Get(Vector vec)const{
  uint64_t idx;
  StackPosition pos;

  CursorUpdate(vec);
  std::tie(pos,idx)=CursorWalk();

  if(idx!=mStack.size()-1)
    return false;

  return mStack[idx].Get(pos);
}

void Tree::Set(Vector vec){
  uint64_t idx;
  StackPosition pos;

  CursorUpdate(vec);
  std::tie(pos,idx)=CursorWalk();

  uint64_t offset=pos.mOffset;

  if(mIsDense){
    for(;idx<mCursorIndex.Size()-1;idx++){
      offset+=mCursorIndex[idx];
      mStack[idx].Set(offset);

      offset*=8ULL;
      mCursorStack.push_back(offset);
      mCursorPostion++;
    }
  }else{
    //The root node is always there
    if(idx==0)
      mStack[idx].Set(mCursorIndex[idx]);

    for(++idx;idx<mCursorIndex.Size()-1;idx++){
      mStack[idx].Insert(offset,8,0x0000000000000001ULL<<mCursorIndex[idx]);

      offset=(mStack[idx].Rank(offset+mCursorIndex[idx])-1ULL)*8ULL;
      mCursorStack.push_back(offset);
      mCursorPostion++;
    }
  }

  if(idx<mCursorIndex.Size()){
    auto t=mCursorIndex[idx]+offset;
    mStack[idx].Set(t);
  }
}

void Tree::Clear(Vector vec){
  uint64_t idx;
  StackPosition pos;

  CursorUpdate(vec);
  CursorWalk();

  if(mIsDense){
    do{
      std::tie(pos,idx)=Cursor();
      mStack[idx].Clear(pos);
    }while(CursorPop()&&mStack[idx].GetGroup(pos,8)==0);
  }else{
    std::tie(pos,idx)=Cursor();

    mStack[idx].Clear(pos);
    CursorPop();

    while(idx>1&&mStack[idx].GetGroup(pos,8)==0){
      mStack[idx].Remove(pos,8);

      CursorPop();
      
      std::tie(pos,idx)=Cursor();
      mStack[idx].Clear(pos);
    }
  }
}

uint64_t Tree::NodeCount()const{
  uint64_t count=0;
  for(int idx=0;idx<(mStack.size()-1);idx++)
    count+=mStack[idx].PopCount();

  return count;
}

uint64_t Tree::LeafCount()const{
  return mStack.back().PopCount();
}

Tree::MapValues Tree::Map(Vector vec)const{
  CursorUpdate(vec);
  CursorWalk();

  MapValues values;

  for(int idx=0;mCursorStack.size();idx++)
    values.push_back(mCursorStack[idx]+mCursorIndex[idx]);

  return values;
}

void Tree::ToSparse(){
  if(!mIsDense)
    return;

  for(size_t idx=mStack.size()-1;idx;idx--){
    mStack[idx].Fixed(false);
    mStack[idx].Filter(&mStack[idx-1],0,8);
  }

  Compact();

  mIsDense=false;
}

void Tree::Compact(){
  if(mIsDense)
    return;

  for(int idx=0;idx<mStack.size();idx++){
    mStack[idx].Fixed(false);
    mStack[idx].Compact();
  }
  CursorClear();
}
