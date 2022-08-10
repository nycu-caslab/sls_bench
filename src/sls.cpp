#include "sls.hpp"

#include <iostream>
#include <iterator>
#include <vector>
#include <numeric>
#include <algorithm>

#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std;

void pre_hook(sls_config &config, string s) {
	printf("-------- [BEG] %s --------\n", s.c_str());

	system("echo 3 | sudo tee /proc/sys/vm/drop_caches > /dev/null");

	printf("emb-size: %u\n", config.emb_row);
	printf("feature-size: %u\n", config.emb_col);
	printf("batch-size: %u\n", config.lengths);
	printf("num-indices-per-lookup: %u\n", config.lengths_size);
	printf("ram-ratio: %u\n", config.ram_ratio);
}

void post_hook(sls_config &config, string s) {
	printf("total-lookup: %lu\n", config.ids.size());
	printf("output-size: %ux%u\n", config.lengths, config.emb_col);
	printf("-------- [END] %s --------\n\n", s.c_str());
}

void emb_vec_io_buf(FILE *fp, vector<double> &v, u32 ID) {
	rewind(fp);
	fseek(fp, v.size() * ID * sizeof(double), SEEK_SET);
	fread(&v[0], sizeof(double), v.size(), fp);
}

void emb_vec_io_unbuf(int fd, vector<double> &v, u32 ID) {
	lseek(fd, v.size() * ID * sizeof(double), SEEK_SET);
	read(fd, &v[0], v.size() * sizeof(double));
}

void sls_io_buf(vector<sls_config> &config_set) {
	auto C = config_set[0].emb_col;
	auto K = config_set[0].lengths;
	auto Lengths = vector<u32> (K, config_set[0].lengths_size);

	auto ans = vector<double> (K * C, 0);
	for (auto config : config_set) {
		FILE *fp = fopen(config.table.c_str(), "rb");
		if (fp == NULL) {
			fputs("File error", stderr);
			exit(1);
		}

		struct stat sb;
		if (stat(config.table.c_str(), &sb) == -1) {
			perror("stat");
			exit(EXIT_FAILURE);
		}

		auto v = vector<double> (C, 0);
		u32 curID = 0, outID = 0;
		// sls
		for (auto L : Lengths) {
			for (size_t i=curID; i<curID+L; ++i) {
				auto ID = config.ids[i];
				emb_vec_io_buf(fp, v, ID);

				for (size_t idx=0; idx<C; ++idx)
					ans[outID * C + idx] += v[idx];
			}
			outID += 1;
			curID += L;
		}

		fclose(fp);
	}
}

void sls_io_unbuf(vector<sls_config> &config_set) {
	auto C = config_set[0].emb_col;
	auto K = config_set[0].lengths;
	auto Lengths = vector<u32> (K, config_set[0].lengths_size);

	auto ans = vector<double> (K * C, 0);
	for (auto config : config_set) {
		int fd = open(config.table.c_str(), O_RDONLY);
		if (fd == -1) {
			perror("open");
			exit(EXIT_FAILURE);
		}

		struct stat sb;
		if (stat(config.table.c_str(), &sb) == -1) {
			perror("stat");
			exit(EXIT_FAILURE);
		}

		auto v = vector<double> (C, 0);
		u32 curID = 0, outID = 0;
		// sls
		for (auto L : Lengths) {
			for (size_t j=curID; j<curID+L; ++j) {
				u32 ID = config.ids[j];
				emb_vec_io_unbuf(fd, v, ID);

				for (size_t idx=0; idx<C; ++idx)
					ans[outID * C + idx] += v[idx];
			}
			outID += 1;
			curID += L;
		}

		close(fd);
	}
}

void sls_mmap(vector<sls_config> &config_set) {
	auto R = config_set[0].emb_row;
	auto C = config_set[0].emb_col;
	auto K = config_set[0].lengths;
	auto Lengths = vector<u32> (K, config_set[0].lengths_size);

	auto ans = vector<double> (K * C, 0);
	for (auto config : config_set) {
		int fd = open(config.table.c_str(), O_RDONLY);
		if (fd == -1) {
			perror("open");
			exit(EXIT_FAILURE);
		}

		struct stat sb;
		if (stat(config.table.c_str(), &sb) == -1) {
			perror("stat");
			exit(EXIT_FAILURE);
		}

		double *map = (double *) mmap(NULL, R * C * sizeof(double), PROT_READ, MAP_SHARED, fd, 0);

		u32 curID = 0, outID = 0;
		// sls
		for (auto L : Lengths) {
			for (size_t j = curID; j < curID + L; ++j) {
				u32 ID = config.ids[j];
				for (size_t idx = 0; idx < C; ++idx)
					ans[outID * C + idx] += map[ID * C + idx];
			}
			outID += 1;
			curID += L;
		}

		munmap(map, R * C * sizeof(double));
		close(fd);
	}
}

void sls_ram(vector<sls_config> &config_set) {
	auto R = config_set[0].emb_row;
	auto C = config_set[0].emb_col;
	auto K = config_set[0].lengths;
	auto Lengths = vector<u32> (K, config_set[0].lengths_size);

	auto emb_set = vector<vector<double>> ();
	for (auto config : config_set) {
		int fd = open(config.table.c_str(), O_RDONLY);
		if (fd == -1) {
			perror("open");
			exit(EXIT_FAILURE);
		}

		struct stat sb;
		if (stat(config.table.c_str(), &sb) == -1) {
			perror("stat");
			exit(EXIT_FAILURE);
		}

		auto emb = vector<double> (R * C);
		read(fd, &emb[0], R * C * sizeof(double));

		emb_set.push_back(emb);

		close(fd);
	}

	auto ans = vector<double> (K * C, 0);

	auto len = config_set.size();
	for (auto i=0ul; i<len; ++i) {
		// sls
		auto curID = 0, outID = 0;
		for (auto L : Lengths) {
			for (size_t j = curID; j < curID + L; ++j) {
				u32 ID = config_set[i].ids[j];
				for (size_t idx = 0; idx < C; ++idx)
					ans[outID * C + idx] += emb_set[i][ID * C + idx];
			}
			outID += 1;
			curID += L;
		}
	}
}

void sls_ratio(vector<sls_config> &config_set) {
	auto R = config_set[0].emb_row;
	auto C = config_set[0].emb_col;
	auto K = config_set[0].lengths;
	auto Lengths = vector<u32> (K, config_set[0].lengths_size);

	auto ram_ratio = config_set[0].ram_ratio;
	auto R_size = (u32) R/10;
	auto ratio_R = ram_ratio * R_size;

	auto ans = vector<double> (K * C, 0);
	auto emb_set = vector<vector<double>> ();
	for (auto config : config_set) {
		int fd = open(config.table.c_str(), O_RDONLY);
		if (fd == -1) {
			perror("open");
			exit(EXIT_FAILURE);
		}

		struct stat sb;
		if (stat(config.table.c_str(), &sb) == -1) {
			perror("stat");
			exit(EXIT_FAILURE);
		}

		auto emb = vector<double> (ratio_R * C);
		read(fd, &emb[0], ratio_R * C * sizeof(double));
		emb_set.push_back(emb);

		close(fd);
	}

	auto v = vector<double> (C, 0);
	auto len = config_set.size();
	for (auto i=0ul; i<len; ++i) {
		int fd = open(config_set[i].table.c_str(), O_RDONLY); 
		int curID = 0, outID = 0;
		// sls
		for (auto L : Lengths) {
			for (size_t j = curID; j < curID + L; ++j) {
				u32 ID = config_set[i].ids[j];

				if (ID > (ratio_R-1)) {
					emb_vec_io_unbuf(fd, v, ID);
					for (size_t idx = 0; idx < C; ++idx)
						ans[outID * C + idx] += v[idx];
				}
				else {
					for (size_t idx = 0; idx < C; ++idx) 
						ans[outID * C + idx] += emb_set[i][ID * C + idx];
				}
			}
			outID += 1;
			curID += L;
		}
		close(fd);
	}
}

void sls_opt(vector<sls_config> &config_set) {
	auto R = config_set[0].emb_row;
	auto C = config_set[0].emb_col;
	auto K = config_set[0].lengths;
	auto Lengths = vector<u32> (K, config_set[0].lengths_size);
	
	auto R_size = (u32) R/10;
	auto ans = vector<double> (K * C, 0);
	auto emb_set = vector<vector<double>> ();
	auto max_idx_set = vector<u32> ();
	for (auto config : config_set) {
		int fd = open(config.table.c_str(), O_RDONLY); 
		if (fd == -1) {
			perror("open");
			exit(EXIT_FAILURE);
		}

		struct stat sb;
		if (stat(config.table.c_str(), &sb) == -1) {
			perror("stat");
			exit(EXIT_FAILURE);
		}

		auto cnt = vector<u32> (10, 0);
		for (auto ID : config.ids)
			cnt[ID/R_size]++;
		auto max_cnt = max_element(cnt.begin(), cnt.end());
		auto max_idx = distance(cnt.begin(), max_cnt);

		auto emb = vector<double> (R_size * C);
		lseek(fd, C * (max_idx*R_size) * sizeof(double), SEEK_SET);
		read(fd, &emb[0], R_size * C * sizeof(double));
		emb_set.push_back(emb);
		max_idx_set.push_back(max_idx);

		close(fd);
	}

	auto v = vector<double> (C, 0);
	auto len = config_set.size();
	for (auto i=0ul; i<len; ++i) {
		int fd = open(config_set[i].table.c_str(), O_RDONLY); 
		auto max_idx = max_idx_set[i];
		// sls
		auto curID = 0, outID = 0;
		for (auto L : Lengths) {
			for (size_t j = curID; j < curID + L; ++j) {
				u32 ID = config_set[i].ids[j];
				u32 emb_dis = ID - max_idx*R_size;

				if ((max_idx * R_size - 1) < ID and ID < ((max_idx+1) * R_size)) {
					for (size_t idx = 0; idx < C; ++idx) 
						ans[outID * C + idx] += emb_set[i][emb_dis * C + idx];
				}
				else {
					emb_vec_io_unbuf(fd, v, ID);
					for (size_t idx = 0; idx < C; ++idx)
						ans[outID * C + idx] += v[idx];
				}
			}
			outID += 1;
			curID += L;
		}
		close(fd);
	}
}
