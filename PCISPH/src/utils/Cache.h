#pragma once

#include <map>

namespace utils {
	template<typename Key, typename Value>
	class Cache {
	public:
		Value operator()(const Key& key) {
			auto it = data.find(key);
			if(it != data.end()) {
				return it->second;
			}
			else {
				auto new_entry = create_entry(key);
				data.insert(std::make_pair(key, new_entry));
				return new_entry;
			}
		}
		void clear() {
			data.clear();
		}
	protected:
		virtual Value create_entry(const Key& key) = 0;
	private:
		std::map<Key, Value> data;
	};
}