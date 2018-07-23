#include<intrin.h>
#include<cinttypes>
#include<algorithm>
#include<random>
#include<atomic>
#include<malloc.h>
#include<thread>
#include<vector>
#include<iostream>
#include"Tree.h"
#include"Succinct/L0Block.h"

int main(){
  size_t depth=8;
  Tree<TREE_DENSE> t(depth);

  uint64_t dim=0x0000000000000001ULL<<(depth);

  for(uint64_t x=0;x<dim;x+=1){
    for(uint64_t y=0;y<dim;y+=1){
      for(uint64_t z=0;z<dim;z+=1){
          t.Set({x,y,z},depth);
      }
    }
  }

  /*
  auto f=t.Get({0,0,0},depth);
  f=t.Get({2,2,2},depth);


  for(uint64_t x=0;x<512;x++){
    for(uint64_t y=0;y<512;y++){
      for(uint64_t z=0;z<512;z++){
        auto ret=t.Get({x,y,z},depth);

        if(x%2&&y%2&&z%2&&!ret)
          std::cout<<"Bad output\n";
          
      }
    }
  }
  */
  return 0;
}