#include <iostream> 
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <regex>
#include <exception>

#include "src/sls.hpp"
#include "src/bench.hpp"

using namespace std;
namespace fs = std::filesystem;

void parse_arg(map<string, string> &m, string target, string pattern) {
	auto begin = 0;
	auto end = target.find(pattern);

	auto key = string(), val = string();
	while (end != string::npos) {
		key = target.substr(begin, end - begin);
		begin = end + pattern.size();
		end = target.find(pattern, begin);
	}

	val = target.substr(begin, end - begin);

	m.insert(make_pair(key, val));
}

int main(int argc, char *argv[]) {
	auto arg = map<string, string> ();
	for (auto i=1; i<argc; ++i)
		parse_arg(arg, argv[i], "=");

	auto rnd = 1, shift = 9;

	auto dir = string(), type = string();
	auto R=0, C=0, L=0;

	try {
		dir = arg["--dir"];
		dir = regex_replace(dir, regex("~/"), "/home/nctu/");
		R = stoi(arg["--embedding-size"]);
		C = stoi(arg["--feature-size"]);
		// auto K = stoi(arg["--batch-size"]);
		L = stoi(arg["--num-indices-per-lookup"]);
		type = arg["--type"];
	} catch (exception &e) {
		cout << e.what() << endl;
		exit(1);
	}

	void (*fun)(vector<sls_config> &) = nullptr;
	if (type == "io_buf") fun = sls_io_buf;
	if (type == "io_unbuf") fun = sls_io_unbuf;
	if (type == "mmap") fun = sls_mmap;
	if (type == "ram") fun = sls_ram;
	if (type == "ratio") fun = sls_ratio;
	if (type == "opt") fun = sls_opt;

	for (auto i=shift-1; i<shift; ++i) {
		auto config_set = vector<sls_config> ();
		for (auto it : fs::directory_iterator(dir)) {
			auto emb = fs::absolute(it);
			auto config = sls_config(emb, R, C, 1<<i, L, 1);

			config_set.push_back(config);
		}

		auto test_fun = bind(fun, config_set);
		auto pre_fun = bind(pre_hook, config_set[0], type);
		auto post_fun = bind(post_hook, config_set[0], type);

		auto bench_fun = bm::real_time(test_fun, pre_fun, post_fun);

		auto result = bm::bench(rnd, bm::excl_avg<bm::nanos, 1>, bench_fun);

		printf("[Time]\n(%s, %d): %lu µs\n\n", type.c_str(), (1<<i), result.count()/1000);
	}

	return 0;
}
