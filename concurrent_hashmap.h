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
 private:
  int numSegments;
  std::hash<Key> hasher;

  size_t hash(Key x) {
	size_t h = hasher(x);
	return h % numSegments;
  }

  class Segment {
   private:
	std::map<Key, T> base_map{};
   public:
	std::mutex mutex_;
	Segment() = default;

	Segment(Segment &&segment) noexcept {
	  base_map = segment.base_map;
	}

	Segment(Segment &segment) noexcept {
	  base_map = std::move(segment.base_map);
	}

	size_t size() {
	  auto curSize = base_map.size();
	  return curSize;
	}

	auto getMap() {
	  std::map<Key, T> resMap;
	  resMap = std::map<Key, T>(base_map);
	  return resMap;
	}

	void add_pair(std::pair<Key, T> p) {
	  auto map_iter = base_map.find(p.first);
	  if (map_iter != base_map.end())
		map_iter->second = p.second;
	  else
		base_map.insert(p);
	}

	auto get(Key k) {
	  auto value = base_map.find(k);
	  return value;

	}

	size_t count(Key k) {
	  auto count = base_map.count(k);
	  return count;
	}

	std::vector<std::pair<Key, T>> toVector() {
	  auto vect = std::vector<std::pair<Key, T>>(base_map.begin(), base_map.end());
	  return vect;

	}

  };

  std::vector<Segment> segments;

 public:
  explicit ConcurrentHashmap(int mapNumSegments) {
	numSegments = mapNumSegments;
	segments.reserve(numSegments);
	for (int i = 0; i < numSegments; i++) {
	  segments.emplace_back();
	}

  }

  void set_pair(std::pair<Key, T> p) {
	//adds new pair if key not exists
	//replaces previous if key exists
	size_t h = hash(p.first);
	std::lock_guard<std::mutex> lg{segments[h].mutex_};
	segments[h].add_pair(p);
  }

  T get(Key k) {
	size_t h = hash(k);
	std::lock_guard<std::mutex> lg{segments[h].mutex_};
	auto value = segments[h].get(k)->second;
	return value;

  }

  bool keyExists(Key k) {
	size_t h = hash(k);
	std::lock_guard<std::mutex> lg{segments[h].mutex_};
	auto exists = segments[h].count(k) > 0;
	return exists;
  }

  size_t size() {
	size_t s = 0;
	for (size_t i = 0; i < numSegments; i++) {
	  std::lock_guard<std::mutex> lg{segments[i].mutex_};
	  auto seg = segments[i];
	  s += seg.size();
	}

	return s;
  }

  std::map<Key, T> getMap() {
	std::map<Key, T> mergedMap = {};
	for (size_t i = 0; i < numSegments; i++) {
	  segments[i].mutex_.lock();
	}

	for (size_t i = 0; i < numSegments; i++) {
	  auto seg = segments[i];
	  auto curMap = seg.getMap();
	  for (auto itr = curMap.begin(); itr != curMap.end(); ++itr) {
		mergedMap.emplace(std::pair<Key, T>(itr->first, itr->second));
	  }
	  segments[i].mutex_.unlock();

	}
	return mergedMap;
  }

  std::vector<std::pair<Key, T>> toVector() {
	std::map<Key, T> map = getMap();
	return std::vector<std::pair<Key, T >>(map.begin(), map.end());
  }
};

#endif //AKSLAB5_CONCURRENT_HASHMAP_H
