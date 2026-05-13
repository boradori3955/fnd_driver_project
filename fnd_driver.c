#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/device.h>

#define DRIVER_NAME "fnd_driver"
#define CLASS_NAME  "fnd_class"
#define CHIP_START 571

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("RPi 5 FND Driver");

// GPIO 핀 정의 (사용자 환경에 맞춰 수정 가능)
static int dataPin = CHIP_START + 17;
static int clockPin = CHIP_START + 27;
static int latchPin = CHIP_START + 22;
static int comPins[4] = { CHIP_START + 5, CHIP_START + 6, CHIP_START + 13, CHIP_START + 26 };

module_param(dataPin, int, 0644);
MODULE_PARM_DESC(dataPin, "data Pin Number (Check /sys/kernel/debug/gpio)");
module_param(clockPin, int, 0644);
MODULE_PARM_DESC(clockPin, "clock Pin Number (Check /sys/kernel/debug/gpio)");
module_param(latchPin, int, 0644);
MODULE_PARM_DESC(latchPin, "latch Pin Number (Check /sys/kernel/debug/gpio)");

static int num_comPins = 4;
module_param_array(comPins, int, &num_comPins, 0644);
MODULE_PARM_DESC(comPins, "GPIO Pin Numbers for COM Pins (Array of 4)");

static int fnd_data[] = {
    0xFC, 0x60, 0xDA, 0xF2, 0x66, 0xB6, 0xBE, 0xE0, 0xFE, 0xF6, 0x00
}; // 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, clear

static dev_t dev_num;
static struct class* fnd_class = NULL;
static struct device* fnd_device = NULL;
static struct cdev fnd_cdev;


void shiftOut(int dataPin, int clockPin, int bitOrder, int val);


// 74HC595 시프트 아웃
typedef enum { LSBFIRST, RSBFIRST } Bfirst;

void shiftOut(int dataPin, int clockPin, int bitOrder, int val) {
    for (int i = 0; i < 8; i++) {
        // 1. 비트 순서에 따라 보낼 데이터 추출
        if (bitOrder == LSBFIRST)
            gpio_set_value(dataPin, !!(val & (1 << i)));
        else
            gpio_set_value(dataPin, !!(val & (1 << (7 - i))));

        // 2. 클럭 신호를 주어 비트를 한 칸 밀어 넣음
        gpio_set_value(clockPin, 1);
        gpio_set_value(clockPin, 0);
    }
}

static void display_fnd(int digit, int num) {
    int i;
    // 모든 COM 핀
    for (i = 0; i < 4; i++) {
        gpio_set_value(comPins[i], 1); // 1: HIGH (OFF)
    }

    // 74HC595 데이터 래치
    gpio_set_value(latchPin, 0);
    shiftOut(dataPin, clockPin, LSBFIRST, fnd_data[num]);
    gpio_set_value(latchPin, 1);

    // 선택된 자릿수
    if (digit >= 1 && digit <= 4) {
        gpio_set_value(comPins[4 - digit], 0); // 0: LOW (ON)
    }
}

static void clear_fnd(void) {
    int i;
    // 모든 COM 핀
    for (i = 0; i < 4; i++) {
        gpio_set_value(comPins[i], 1); // 1: HIGH (OFF)
    }
    // 74HC595 데이터 래치
    gpio_set_value(latchPin, 0);
    shiftOut(dataPin, clockPin, LSBFIRST, fnd_data[10]);
    gpio_set_value(latchPin, 1);
    printk(KERN_INFO "FND Driver: Cleared\n");
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = fnd_open,
    .release = fnd_release,
    .write = fnd_write,
};

static int fnd_open(struct inode* inode, struct file* file) {
    printk(KERN_INFO "FND Driver: Opened\n");
    return 0;
}

static int fnd_release(struct inode* inode, struct file* file) {
    printk(KERN_INFO "FND Driver: Released\n");
    return 0;
}

static ssize_t fnd_write(struct file* file, const char __user* buf, size_t count, loff_t* offset) {
    char kbuf[16] = { 0 }; // 버퍼 크기를 넉넉히 잡고 초기화
    int digit, num;

    if (count > sizeof(kbuf) - 1) count = sizeof(kbuf) - 1;
    if (copy_from_user(kbuf, buf, count)) return -EFAULT;

    // 개행 문자 제거
    if (count > 0 && (kbuf[count - 1] == '\n' || kbuf[count - 1] == '\r'))
        kbuf[count - 1] = '\0';

    printk(KERN_INFO "FND Driver: %s\n", kbuf);

    // clear 명령 처리
    if (strncmp(kbuf, "clear", 5) == 0) {
        clear_fnd();
        return count;
    }

    if (sscanf(kbuf, "%d %d", &digit, &num) == 2) {
        // 배열 인덱스 범위 체크 (fnd_data는 11개, comPins는 4개)
        if (digit >= 1 && digit <= 4 && num >= 0 && num <= 10) {
            display_fnd(digit, num);
        }
    }
    return count;
}

static int __init fnd_init(void) {
    int ret;

    // 1. 디바이스 번호 할당
    if (alloc_chrdev_region(&dev_num, 0, 1, DRIVER_NAME) < 0) {
        printk(KERN_ERR "FND Driver: Failed to allocate device number\n");
        return -1;
    }

    // 2. cdev 구조체 초기화 및 추가
    cdev_init(&fnd_cdev, &fops);
    if (cdev_add(&fnd_cdev, dev_num, 1) < 0) {
        printk(KERN_ERR "FND Driver: Failed to add cdev\n");
        goto r_cdev_add;
    }

    // 3. 클래스 및 디바이스 생성 (/dev/fnd_driver 자동 생성)
    fnd_class = class_create(CLASS_NAME); // 커널 버전에 따라 파라미터 개수 다를 수 있음 (최신: 1개)
    if (IS_ERR(fnd_class)) {
        printk(KERN_ERR "FND Driver: Failed to create class\n");
        goto r_class_create;
    }

    fnd_device = device_create(fnd_class, NULL, dev_num, NULL, "fnd_driver");
    if (IS_ERR(fnd_device)) {
        printk(KERN_ERR "FND Driver: Failed to create device\n");
        goto r_device_create;
    }

    // 4. GPIO 요청 및 설정
    printk(KERN_INFO "FND Driver: Requesting\n");
    if (!gpio_is_valid(dataPin)) {
        printk(KERN_ERR "FND Driver: Invalid \n");
        goto r_gpio;
    }

    ret = gpio_request(dataPin, "sysfs_fnd");
    if (ret) {
        printk(KERN_ERR "FND Driver: Failed\n");
        goto r_gpio;
    }

    printk(KERN_INFO "FND Driver: Requesting\n");
    if (!gpio_is_valid(clockPin)) {
        printk(KERN_ERR "FND Driver: Invalid \n");
        goto r_gpio;
    }

    ret = gpio_request(clockPin, "sysfs_fnd");
    if (ret) {
        printk(KERN_ERR "FND Driver: Failed\n");
        goto r_gpio;
    }

    printk(KERN_INFO "FND Driver: Requesting\n");
    if (!gpio_is_valid(latchPin)) {
        printk(KERN_ERR "FND Driver: Invalid \n");
        goto r_gpio;
    }

    ret = gpio_request(latchPin, "sysfs_fnd");
    if (ret) {
        printk(KERN_ERR "FND Driver: Failed\n");
        goto r_gpio;
    }

    for (int i = 0; i < 4; i++) {
        printk(KERN_INFO "FND Driver: Requesting\n");
        if (!gpio_is_valid(comPins[i])) {
            printk(KERN_ERR "FND Driver: Invalid \n");
            goto r_gpio;
        }

        ret = gpio_request(comPins[i], "sysfs_fnd");
        if (ret) {
            printk(KERN_ERR "FND Driver: Failed\n");
            goto r_gpio;
        }
    }

    // 초기값 OFF
    gpio_direction_output(dataPin, 0); 
    gpio_direction_output(clockPin, 0);
    gpio_direction_output(latchPin, 0);

    gpio_direction_output(comPins[0], 1);
    gpio_direction_output(comPins[1], 1);
    gpio_direction_output(comPins[2], 1);
    gpio_direction_output(comPins[3], 1);

    printk(KERN_INFO "FND Driver: Initialized\n");
    return 0;

r_gpio:
    device_destroy(fnd_class, dev_num);
r_device_create:
    class_destroy(fnd_class);
r_class_create:
    cdev_del(&fnd_cdev);
r_cdev_add:
    unregister_chrdev_region(dev_num, 1);
    return -1;
}

static void __exit fnd_exit(void) {
    int i;
    for (i = 0; i < 4; i++) {
        gpio_free(comPins[i]);
    }
    gpio_free(latchPin);
    gpio_free(clockPin);
    gpio_free(dataPin);
    device_destroy(fnd_class, dev_num);
    class_destroy(fnd_class);
    cdev_del(&fnd_cdev);
    unregister_chrdev_region(dev_num, 1);
    printk(KERN_INFO "FND Driver: Exited\n");
}

module_init(fnd_init);
module_exit(fnd_exit);