#include <iostream> 
#include <fstream>
#include <string>
#include <filesystem>
#include <vector>

#include "src/sls.hpp"
#include "src/bench.hpp"

using namespace std;
namespace fs = std::filesystem;

int main() {
	auto rnd = 2;
	auto shift = 9;
	ofstream fout("rmc1.csv");
	bool fout_flag = true;

	string path = "/home/nctu/dlrm-file/dlrm/rmc1/";
	auto sum = vector<vector<u32>> (shift, vector<u32> (9, 0));

	for (auto i=0; i<shift; ++i) {
		for (auto r=1ul; r<10; r++) {
			auto config_set = vector<sls_config> ();
			for (auto it : fs::directory_iterator(path)) {
				string emb = fs::absolute(it);
				auto config = sls_config(emb, 4000000, 32, 1<<i, 80, r);
				config_set.push_back(config);
			}

			auto test_ratio = bind(sls_ratio, config_set);
			auto pre_ratio = bind(pre_hook, config_set[0], "ratio");
			auto post_ratio = bind(post_hook, config_set[0], "ratio");

			auto bench_ratio = bm::real_time(test_ratio, pre_ratio, post_ratio);

			auto result = bm::bench(rnd, bm::excl_avg<bm::nanos, 1>, bench_ratio);

			printf("[Time]\n[%d] (%lu:%lu) %lu µs\n", (1<<i), r, 10-r, result.count()/1000);
			sum[i][r-1] = (result.count()/1000);
		}
	}

	cout << "[Break down]\n";
	for (auto r : sum) {
		for (auto e : r)
			cout << e << ' ';
		cout << endl;
	}

	if (fout_flag) {
		for (auto i=0; i<shift; ++i) {
			fout << (1<<i) << ',';
			for (auto j=0; j<9; ++j)
				fout << sum[i][j] << ',';
			fout << endl;
		}
	}

	fout.close();

	return 0;
}
