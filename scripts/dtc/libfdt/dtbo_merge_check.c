#include <stdio.h>
#include "libfdt_env.h"
#include <fdt.h>
#include <libfdt.h>

#define FDT_ADD_SIZE (0x20000)

int dtbo_merge_chk_main(int argc, char *argv[])
{
	int ret = -1;
	char *path = NULL;
	char *path_tmp = NULL;
	unsigned char *dtbo_addr = NULL;
	unsigned char *dtb_addr = NULL;
	FILE *fp_dtb = NULL;
	FILE *fp_dtbo = NULL;
	FILE *fp_fdt = NULL;
	int len_dtb = 0;
	int len_dtbo = 0;
	size_t len = 0;
	unsigned int fdt_size = 0;

	if (argc != 4) {
		fprintf(stderr, "Error: Not find dtb or dtbo file\n");
		return 0;
	}

	fp_dtb = fopen(argv[2], "rb");
	if (fp_dtb) {
		fseek(fp_dtb, 0L, SEEK_END);
		len_dtb = ftell(fp_dtb);
		fseek(fp_dtb, 0L, SEEK_SET);
	} else {
		printf("dtb not exit\n");
		goto error;
	}

	fp_dtbo = fopen(argv[3], "rb");
	if (fp_dtbo) {
		fseek(fp_dtbo, 0L, SEEK_END);
		len_dtbo = ftell(fp_dtbo);
		fseek(fp_dtbo, 0L, SEEK_SET);
	} else {
		printf("dtbo not exit\n");
		goto error;
	}

	if (len_dtb > 0) {
		dtb_addr = malloc(len_dtb + FDT_ADD_SIZE);
		if (!dtb_addr) {
			printf("malloc fail\n");
			goto error;
		}
		len = fread(dtb_addr, 1, len_dtb, fp_dtb);
		if (!len) {
			printf("Maybe read %s error!\n", argv[2]);
			goto error;
		}
	} else {
		printf("dtb file data error (0 Bytes)\n");
		goto error;
	}

	if (len_dtbo > 0) {
		dtbo_addr = malloc(len_dtbo);
		if (NULL == dtbo_addr) {
			printf("malloc fail\n");
			goto error;
		}
		len = fread(dtbo_addr, 1, len_dtbo, fp_dtbo);
		if (!len) {
			printf("Maybe read %s error!\n", argv[3]);
			goto error;
		}
	} else {
		printf("dtbo file data error (0 Bytes)\n");
		goto error;
	}

	fdt_size = fdt_totalsize(dtb_addr);
	ret = fdt_open_into(dtb_addr, dtb_addr, fdt_size + FDT_ADD_SIZE);
	if (0 != ret) {
		printf("libfdt fdt_open_into(): %s\n", fdt_strerror(ret));
		goto error;
	}

	ret = fdt_check_header(dtbo_addr);
	if (ret != 0) {
		printf("image is not a fdt error = %d\n", ret);
		goto error;
	}

	ret = fdt_overlay_apply(dtb_addr, dtbo_addr);
	if (ret < 0)
		fprintf(stderr, "\033[;31mError: dtb apply dtbo error value is %s\033[0m\n", fdt_strerror(ret));
	else {
		path = (char *)malloc(strlen(argv[2]) + 10);
		memset(path, 0, (strlen(argv[2]) + 10));
		path_tmp = strrchr(argv[2], '/');
		if (path_tmp) {
			strncpy(path, argv[2], path_tmp - argv[2] + 1);
			strcat(path, "fdt.dtb");
			fdt_size = fdt_totalsize(dtb_addr);
			fp_fdt = fopen(path, "w+");
			if (fp_fdt)
				fwrite(dtb_addr, fdt_size, 1, fp_fdt);
			else
				printf("create error [%s]\n", path);
		}
	}

error:
	if (fp_dtb)
		fclose(fp_dtb);
	if (fp_dtbo)
		fclose(fp_dtbo);
	if (fp_fdt)
		fclose(fp_fdt);
	if (dtb_addr)
		free(dtb_addr);
	if (dtbo_addr)
		free(dtbo_addr);
	if (path)
		free(path);

	return ret;
}