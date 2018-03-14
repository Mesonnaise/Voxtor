#include<algorithm>
#include "Tree.h"



Tree::Tree(size_t depth,bool dense):mIsDense(dense){
  mStack.reserve(depth);

  if(mIsDense){
    uint64_t bitCount=8;
    while(depth--){
      mStack.emplace_back(bitCount);

      bitCount<<=3;
    }

  }else{
    while(depth--)
      mStack.emplace_back(0);
  }
}


Tree::~Tree(){}

void Tree::Insert(Index &idx){
  size_t max=std::min(idx.Size(),mStack.size());
  uint64_t offset=0;

  if(mIsDense){
    for(int i=0;i<max;i++){
      offset+=idx[i];

      mStack[i].Set(offset);

      offset*=8;
    }
  } else{
    uint64_t pos=0;
    for(;pos<max;pos++){
      offset+=idx[pos];

      if(!mStack[pos].Get(offset)){
        mStack[pos].Set(offset);
        offset=mStack[pos].Rank(offset)-1;
        break;
      }
      offset=mStack[pos].Rank(offset)-1;
    }

    for(++pos;pos<max;pos++){
      mStack[pos].Insert(offset,8,0x01<<idx[pos]);
      offset=mStack[pos].Rank(offset+idx[pos])-1;
    }
  }
}

bool Tree::Test(Index &idx)const{
  size_t max=std::min(idx.Size(),mStack.size());

  uint64_t offset=0;

  if(mIsDense){
    for(int i=0;i<max;i++){
      offset+=idx[i];

      if(!mStack[i].Get(offset))
        return false;
      
      offset*=8;
    }
  }else{
    for(int i=0;i<max;i++){
      offset+=idx[i];

      if(!mStack[i].Get(offset))
        return false;

      offset=mStack[i].Rank(offset)-1;
    }
  }
  return true;
}

std::vector<Tree::Level> Tree::FlattenPosition(Index &idx)const{
  size_t max=std::min(idx.Size(),mStack.size());

  uint64_t offset=0;

  std::vector<Tree::Level> levels;
  levels.reserve(max);
  
  if(mIsDense){
    for(size_t i=0;i<max;i++){
      offset+=idx[i];
      
      if(!mStack[i].Get(offset))
        break;

      levels.push_back({offset,mStack[i].Size()});
      offset*=8;
    }
  }else{
    for(size_t i=0;i<max;i++){
      offset+=idx[i];

      if(!mStack[i].Get(offset))
        break;

      levels.push_back({offset,mStack[i].Size()});
      offset=mStack[i].Rank(offset)-1;
    }
  }

  return levels;
}

void Tree::Compact(){
  //This function can be parallelized
  size_t level=mStack.size()-1;
  if(mIsDense){
    while(level){
      mStack[level].Extract(mStack[level-1]);
    }
  }

  for(auto &level:mStack)
    level.Compact();
}

uint64_t Tree::VoxelCount()const{
  return mStack[mStack.size()-1].PopCount();
}

uint64_t Tree::NodeCount()const{
  uint64_t count=1;//Root Node

  for(int idx=0;idx<mStack.size()-1;idx++)
    count+=mStack[idx].PopCount();

  return count;
}

size_t Tree::MemUsage()const{
  size_t total=sizeof(Tree);

  for(auto &level:mStack)
    total+=level.MemUsage();

  return total;
}