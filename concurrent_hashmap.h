//
// Created by max on 20.04.20.
//

#ifndef AKSLAB5_CONCURRENT_HASHMAP_H
#define AKSLAB5_CONCURRENT_HASHMAP_H

#include <iostream>
#include <mutex>
#include <vector>
#include <map>
#include <string>
#include <condition_variable>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/locale.hpp>
#include <thread>

template<class Key, class T>
class ConcurrentHashmap {

 protected:
  static const size_t numSegments = 16;
  std::vector<std::mutex *> mutex_ptr_vect;

  std::hash<Key> hasher;

  size_t hash(Key x) {
    size_t h = this->hasher(x);
    return h % numSegments;
  }

  class Segment {
   protected:
    std::map<Key, T> base_map;
    std::mutex *mutex_ptr;

   public:
    explicit Segment(std::mutex &mut_ptr) {
      this->base_map = {};
      mutex_ptr = &mut_ptr;
    }

    ~Segment() {
      delete this->mutex_ptr;
    }

    size_t size() {
      std::lock_guard<std::mutex> lg{*mutex_ptr};
      auto curSize = this->base_map.size();
      return curSize;
    }

    auto getMap() {
      std::lock_guard<std::mutex> lg{*mutex_ptr};
      auto resMap = &this->base_map;
      return resMap;
    }

    void add_pair(std::pair<Key, T> p) {
      std::lock_guard<std::mutex> lg{*mutex_ptr};
      auto map_iter = base_map.find(p.first);
      if (map_iter != base_map.end())
        map_iter->second = p.second;
      else
        this->base_map.insert(p);
    }

    auto get(Key k) {
      std::lock_guard<std::mutex> lg{*mutex_ptr};
      auto value = this->base_map.find(k);
      return value;

    }

    size_t count(Key k) {
      std::lock_guard<std::mutex> lg{*mutex_ptr};
      auto count = this->base_map.count(k);
      return count;
    }

    std::vector<std::pair<Key, T>> toVector() {
      std::lock_guard<std::mutex> lg{*mutex_ptr};
      auto vect = std::vector<std::pair<Key, T>>(base_map.begin(), base_map.end());
      return vect;

    }

  };

  std::vector<Segment *> segments;

 public:
  ConcurrentHashmap() {
    for (size_t i = 0; i < numSegments; i++) {
      mutex_ptr_vect.emplace_back(new std::mutex());
      this->segments.emplace_back(new Segment(std::ref(*mutex_ptr_vect[i])));
    }

  }

  ~ConcurrentHashmap() {

    for (auto seg_ptr : this->segments) {
      delete seg_ptr;
    }
  }

  void set_pair(std::pair<Key, T> p) {
    //adds new pair if key not exists
    //replaces previous if key exists
    size_t h = this->hash(p.first);
    segments[h]->add_pair(p);
  }

  T get(Key k) {
    size_t h = this->hash(k);
    auto value = segments[h]->get(k)->second;
    return value;

  }

  bool keyExists(Key k) {
    size_t h = this->hash(k);
    auto exists = segments[h]->count(k) > 0;
    return exists;
  }

  size_t size() {
    size_t s = 0;
    for (auto &seg: this->segments) {
      s += seg.size();
    }
    return s;
  }

  std::map<Key, T> getMap() {
    std::map<Key, T> mergedMap = {};
    for (auto &seg: this->segments) {
      auto curMap = seg->getMap();
      for (auto itr = curMap->begin(); itr != curMap->end(); ++itr) {
        mergedMap.emplace(std::pair<Key, T>(itr->first, itr->second));
      }
    }
    return mergedMap;
  }

  std::vector<std::pair<Key, T>> toVector() {
    std::map<Key, T> map = this->getMap();
    return std::vector<std::pair<Key, T >>(map.begin(), map.end());
  }
};

template<class Key, class T>
void pro1(ConcurrentHashmap<Key, T> &m, std::mutex &mut) {
  std::string alpha = "abcdefghijklmnopqrstuvwxyz";
  for (char i : alpha) {
    if (!m.keyExists(std::string(1, i)))
      m.set_pair(std::make_pair(std::string(1, i), 1));
    else {
      auto curVal = m.get(std::string(1, i));
      m.set_pair(std::make_pair(std::string(1, i), curVal + 1));
    }
  }

}

#endif //AKSLAB5_CONCURRENT_HASHMAP_H
