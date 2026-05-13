#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char* argv[]) {
    int fd;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s 1234\n", argv[0]);
        fprintf(stderr, "       %s clear\n", argv[0]);
        return 1;
    }

    int is_clear = (strcmp(argv[1], "clear") == 0);

    if (!is_clear && strlen(argv[1]) != 4) {
        fprintf(stderr, "4자리 숫자 또는 'clear'를 입력하세요\n");
        return 1;
    }

    // 1. 디바이스 드라이버 파일 열기
    fd = open("/dev/fnd_driver", O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        return 1;
    }

    // 2. 값 쓰기
    if (is_clear) {
        write(fd, "clear", 5);
    } else {
        while (1) {
            int idx = 4;
            for (int i = 0; i < 4; i++) {
                int val;
                char str[20];
                val = argv[1][i] - '0';
                sprintf(str, "%d %d", idx, val);
                idx--;
                write(fd, str, strlen(str));
                usleep(1000);
            }
        }
    }

    close(fd);
    return 0;
}