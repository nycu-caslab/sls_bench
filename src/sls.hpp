#ifndef SLS_HPP
#define SLS_HPP

#include <vector>
#include <string>
#include <random>
#include <cstdint>
#include <algorithm>
#include <numeric>
#include <map>

#ifndef _U_TYPE
#define _U_TYPE
typedef int8_t			s8; 
typedef int16_t			s16;
typedef int32_t			s32;
typedef int64_t			s64;
typedef uint8_t			u8; 
typedef uint16_t		u16;
typedef uint32_t		u32;
typedef uint64_t		u64;
#endif 

struct sls_config {
	std::string table;		// filename
	u32 emb_row;			// R, embedding-size
	u32 emb_col;			// C, feature-size
	u32 lengths;			// K, lengths-count, batch-size
	u32 lengths_size;		// L, num-indices-per-lookup
	std::vector<u32> ids;	// ids-list
	u32 ram_ratio;

	sls_config(std::string filename, u32 R, u32 C, u32 K, u32 L, u32 val) 
	: table(filename), emb_row(R), emb_col(C), lengths(K), lengths_size(L), ram_ratio(val) {
		gen_ids(false);
	};

	void gen_ids(bool uniform) {
		std::random_device rd;
		std::mt19937 gen(rd());
		auto total = lengths * lengths_size;

		ids = std::vector<u32> (total);
		if (uniform)
			for (auto &ID : ids)  {
				std::uniform_int_distribution<u32> d(0, emb_row-1);
				ID = d(gen);
			}

		else {
			std::binomial_distribution<> d1(5, 0.9);
			std::binomial_distribution<> d2(250, 0.1);

			auto m = std::map<int, int> ();
			for (auto i=0ul; i<100; ++i)
				m[d1(gen)]++, m[d2(gen)]++;

			auto v = std::vector<u32> ();
			for (auto [key, val] : m)
				v.push_back(val);

			auto step = (float) total / std::accumulate(v.begin(), v.end(), 0);
			auto cnt = std::vector<u32> ();
			for (auto e : v)
				cnt.push_back(std::floor(e * step));

			auto len = cnt.size();
			cnt[len-1] += total - std::accumulate(cnt.begin(), cnt.end(), 0);

			auto range = std::floor(emb_row/len);
			auto ID = 0ul;
			for (auto i=0ul; i<len; ++i) {
				auto from = i*range;
				auto to = (i == len-1) ? emb_row-1 : (i+1)*range;
				std::uniform_int_distribution<u32> d_tmp(from, to);
				for (auto j=0ul; j<cnt[i]; ++j) {
					ids[ID] = d_tmp(gen);
					ID++;
				}
			}

			std::random_shuffle(ids.begin(), ids.end());
		}
	}
};

void pre_hook(sls_config &config, std::string pre_state);
void post_hook(sls_config &config, std::string post_state);

void sls_io_buf(std::vector<sls_config> &config_set);
void sls_io_unbuf(std::vector<sls_config> &config_set);
void sls_mmap(std::vector<sls_config> &config_set);
void sls_ram(std::vector<sls_config> &config_set);
void sls_ratio(std::vector<sls_config> &config_set);
void sls_opt(std::vector<sls_config> &config_set);

#endif
