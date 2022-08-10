#include <bits/stdc++.h>

#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std;

void emb_vec_io_unbuf(int fd, vector<double> &v, int ID) {
	lseek(fd, v.size() * ID * sizeof(double), SEEK_SET);
	read(fd, &v[0], v.size() * sizeof(double));
}

int main() {
	auto path = "/home/nctu/dlrm-file/dlrm/rmc1/EmbTable1";
	int fd = open(path, O_RDONLY);
	if (fd == -1) {
		perror("open");
		exit(EXIT_FAILURE);
	}

	struct stat sb;
	if (stat(path, &sb) == -1) {
		perror("stat");
		exit(EXIT_FAILURE);
	}

	auto C = 32;
	auto K = 1;
	auto Lengths = vector<int> (K, 80);
	auto ids = vector<int> {  123, 3674134, 2943413,  726072, 1969324,  405583, 1561514,  363273, 1283359,
		1256597, 2498229, 3791585,  945843,  109794,  553554,  618802,  924158,  945990,
		1321064,  998971, 1251187, 2422052, 1167193,  486463, 2020975,  867923,   92589,
		 941749, 1397696, 2394522, 1079322,  918576, 3407102, 3945102,  971255,  858037,
		3542453, 1726005, 2072531, 1806358,  649729, 1700593,  639070, 2707088, 1921092,
		3163343, 2443723, 3750788, 3217452, 2764748, 3543431, 1243687, 3798905,  576903,
		3390453,  123591, 3741465, 1732519, 3266047,  148446, 3606721,  501742, 3088635,
		 404753,  750959, 2794533, 3783530, 3683121, 2459965, 2476712, 2334429, 1364360,
		3246256, 1246191, 2638337, 3778962, 2045017,  438399, 2398032,  796955};

	auto v = vector<double> (C, 0);
	auto ans = vector<double> (K * C, 0);

	int curID = 0, outID = 0;
	// sls
	for (auto L : Lengths) {
		for (size_t j=curID; j<curID+L; ++j) {
			int ID = ids[j];
			emb_vec_io_unbuf(fd, v, ID);

			for (size_t idx=0; idx<C; ++idx)
				ans[outID * C + idx] += v[idx];
		}
		outID += 1;
		curID += L;
	}

	for (auto e : ans)
		cout << e << ' ';

	close(fd);

	return 0;
}
