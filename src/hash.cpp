#include "hash.h"
#include <cstring>
#include <iostream>
#include "main.h"

hash main_hash;

void hash::init(const size_t mb_size){
  const auto new_size=static_cast<size_t>(1)
    <<msb(mb_size*1024*1024/sizeof(bucket));
  if(new_size==buckets_) return;
  buckets_=new_size;
  if(hash_mem_) free(hash_mem_);
  hash_mem_=static_cast<bucket*>(calloc(buckets_*sizeof(bucket)+63,1));
  if(!hash_mem_){
    std::cerr<<"Failed to allocate "<<mb_size
      <<"MB for transposition table."<<std::endl;
    exit(EXIT_FAILURE);
  }
  buckets_=new_size;
  bucket_mask_=(buckets_-1)*sizeof(bucket);
}

void hash::clear() const{
  std::memset(hash_mem_,0,buckets_*sizeof(bucket));
}

main_hash_entry* hash::probe(const uint64_t key) const{
  auto* const hash_entry=entry(key);
  const uint16_t key16=key>>48;
  for(auto i=0;i<bucket_size;++i)
    if(hash_entry[i].key_==key16){
      if((hash_entry[i].flags_&age_mask)!=age_)
        hash_entry[i].flags_=
          static_cast<uint8_t>(age_+(hash_entry[i].flags_&flags_mask));
      return &hash_entry[i];
    }
  return nullptr;
}

main_hash_entry* hash::replace(const uint64_t key) const{
  auto* const hash_entry=entry(key);
  const uint16_t key16=key>>48;
  for(auto i=0;i<bucket_size;++i) if(hash_entry[i].key_==0||hash_entry[i].key_==key16) return &hash_entry[i];
  auto* replacement=hash_entry;
  for(auto i=1;i<bucket_size;++i)
    if(replacement->depth_-
      ((age_-(replacement->flags_&age_mask))&age_mask)>
      hash_entry[i].depth_-
      ((age_-(hash_entry[i].flags_&age_mask))&age_mask))
      replacement=&hash_entry[i];
  return replacement;
}

int hash::hash_full() const{
  constexpr auto i_max=999/bucket_size+1;
  auto cnt=0;
  for(auto i=0;i<i_max;i++){
    const main_hash_entry* hash_entry=&hash_mem_[i].entry[0];
    for(auto j=0;j<bucket_size;j++) if(hash_entry[j].key_&&(hash_entry[j].flags_&age_mask)==age_) cnt++;
  }
  return cnt*1000/(i_max*bucket_size);
}
