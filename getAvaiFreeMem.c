//  gcc -O3 -o getAvaiFreeMem getAvaiFreeMem.c

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#define ZONEINFOFILE "/proc/zoneinfo"
#define NODEROUGH "/sys/devices/system/node/node%d/meminfo"

int verbose = 0;

void usage() {
	fprintf(stderr, "usage: APP [-v] [-n memory node id]\n");
	exit(-1);
}

int coreProcess() {
	FILE *file;
	file = fopen(ZONEINFOFILE, "r");
	if (!file) {
		perror(ZONEINFOFILE);
		return 1;
	}

	char linebuf[256];
	linebuf[0] = '\0';
	while (fgets(linebuf, sizeof(linebuf), file)) {
		//Node 0, zone   Normal
		int zoneid = -1;
		int nr = -1;

		nr = sscanf(linebuf, "Node %d, zone", &zoneid);
		if(nr != 1 || strstr(linebuf, "Normal") == NULL) {
			continue;
		}

		linebuf[0] = '\0';
		long long free_page = 0;
		long long low = 0;
		long long nr_inactive_file = -1;
		int cnt_got = 0;

		while (cnt_got < 2 && fgets(linebuf, sizeof(linebuf), file)) {

			int j = 0;
			while(linebuf[j] == ' ' || linebuf[j] == '\t') {
				j++;
			}
			char *ptr = linebuf + j;
			// printf("*** %s", ptr);

			// kernel version >= 4.9
			if(nr_inactive_file < 0 && strstr(ptr, "nr_zone_inactive_file") != NULL) {
				if(sscanf(ptr, "nr_zone_inactive_file %lld", &nr_inactive_file) == 1) {
					// printf("NODE %d FREE %lld INACTZ %lld %s\n", zoneid, free_page, nr_inactive_file, ptr);
					cnt_got++;
				}
				// need go to last cnt_got checking
			}

			// kernel version <= 4.9, some node may has both nr_zone_inactive_file and nr_inactive_file
			if(nr_inactive_file < 0 && strstr(ptr, "nr_inactive_file") != NULL) {
				if(sscanf(ptr, "nr_inactive_file %lld", &nr_inactive_file) == 1) {
					// printf("NODE %d FREE %lld INACT %lld %s\n", zoneid, free_page, nr_inactive_file, ptr);
					cnt_got++;
				}
				// need go to last cnt_got checking
			}

			if(strstr(ptr, "pages free") != NULL) {
				//  pages free     47410363
				nr = sscanf(ptr, "pages free %lld", &free_page);
				if(nr != 1) {
					// wrong section
					break;
				}

				// printf("NODE %d FREE %lld\n", zoneid, free_page);
				// suc to continue;
				linebuf[0] = '\0';
				int sub = 0;
				// expected low within the first 3 lines followed
				while (sub < 3 && fgets(linebuf, sizeof(linebuf), file)) {
					sub++;
					// printf("XXX %s", linebuf);
					if((ptr = strstr(linebuf, "low")) == NULL) {
						continue;
					}

					// low      60864
					nr = sscanf(ptr, "low %lld", &low);
					if(nr != 1) {
						continue;
					} else {
						//printf("NODE %d FREE %lld LOW %lld\n", zoneid, free_page, low);
						cnt_got++;
						// force to 3
						sub = 3;
						// need go to last cnt_got checking
					}
				}
			}

			if(cnt_got == 2) {
				printf("NODE %d FREE %lld LOW %lld INACT %lld AVLB %lld\n",
						zoneid, free_page, low, nr_inactive_file, free_page + nr_inactive_file - low);
				cnt_got == 0;
				break;
			}

		}


		// for next node
	}
}

int main(int argc, char *argv[]) {

	int i, opt;
	int memNode = 0;

	while ((opt = getopt(argc, argv, "n:memNode")) != -1) {
		switch (opt) {
		case 'v':
			verbose++;
			break;
		case 'n':
			memNode = atoi(optarg);
			if (memNode < 0) {
				memNode = 0;
			}
			break;
		case 'h':
		default:
			usage();
			break;
		}
	}

	coreProcess();
}
