/*
 *
 *  Created by nbyouri
 *  Last version: April 2016:
 *		- remove obnoxious hexley
 *		- clean up and improve code style
 *		- use NetBSD pkg_install db rather than pkgin's
 *		- could use a structure for all the info and fill it at once
 */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>
#include <pwd.h>
#include <time.h>
#include <sys/sysctl.h>
#include <sys/statvfs.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <mach/mach.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>

#define C0  "\x1B[0m"    /* Reset  */
#define C1  "\x1B[0;32m" /* Green  */
#define C2  "\x1B[0;33m" /* Yellow */
#define C3  "\x1B[1;31m" /* RED    */
#define C4  "\x1B[0;31m" /* Red    */
#define C5  "\x1B[0;35m" /* Purple */
#define C6  "\x1B[0;36m" /* Blue   */
#define RED C3
#define NOR C0

enum envs {
	TERM,
	SHELL,
	USER
};

enum sysctls {
	MODEL = 0,
	CPU,
	MEM,
	OSTYPE,
	OSREL,
	HOSTNAME
};


static const struct {
	const char *ctls;
	const char *names;
} values[] = {
	{ "hw.model", "Model" },			/* MODEL    */
	{ "machdep.cpu.brand_string", "Processor" }, 	/* CPU		 */
	{ "hw.memsize", "Memory" },			/* MEM		 */
	{ "kern.ostype", "OS" },			/* OSTYPE   */
	{ "kern.osrelease", "Release" },		/* OSREL    */
	{ "kern.hostname", "Hostname" },		/* HOSTNAME */
};

static void get_sysctl(enum sysctls);
static void get_env(enum envs);
static void disk(void);
static void get_pkg_count(void);
static void uptime(time_t *nowp);
static void gpu(void);
static void mem(void);
static void print_apple(void);
static void curtime(void);

static void print_apple(void) {
	time_t now;
	time(&now);

	printf(C1"                :++++.        "); get_env(USER);
	printf(C1"               /+++/.         "); get_sysctl(MODEL);
	printf(C1"       .:-::- .+/:-``.::-     "); get_sysctl(MEM);
	printf(C2"    .:/++++++/::::/++++++/:`  "); get_sysctl(OSTYPE);
	printf(C2"  .:///////////////////////:` "); get_sysctl(OSREL);
	printf(C2"  ////////////////////////`   "); mem();
	printf(C3" -+++++++++++++++++++++++`    "); get_env(SHELL);
	printf(C3" /++++++++++++++++++++++/     "); get_env(TERM);
	printf(C4" /sssssssssssssssssssssss.    "); get_sysctl(CPU);
	printf(C4" :ssssssssssssssssssssssss-   "); gpu();
	printf(C5"  osssssssssssssssssssssssso/ "); disk();
	printf(C5"  `syyyyyyyyyyyyyyyyyyyyyyyy+ "); get_pkg_count();
	printf(C5"   `ossssssssssssssssssssss/  "); uptime(&now);
	printf(C6"     :ooooooooooooooooooo+.   "); curtime();
	printf(C6"      `:+oo+/:-..-:/+o+/-     \n"C0);
}

static void curtime(void) {
	time_t t;
	t = time(NULL);

	printf(RED"Time:     : "NOR"%s", ctime(&t));
}

static void mem(void) {
    size_t			len;
    mach_port_t 		myHost;
    vm_statistics64_data_t	vm_stat;
    vm_size_t pageSize = 4096; 	/* set to 4k default */
    unsigned int count = HOST_VM_INFO64_COUNT;
    kern_return_t ret;
    myHost = mach_host_self();
    uint64_t value64;
    len = sizeof(value64);

    sysctlbyname(values[2].ctls, &value64, &len, NULL, 0);
    if (host_page_size(mach_host_self(), &pageSize) == KERN_SUCCESS) {
        if ((ret = host_statistics64(myHost, HOST_VM_INFO64, (host_info64_t)&
                        vm_stat, &count) == KERN_SUCCESS)) {
            printf(RED"%s    : "NOR"%llu MB of %.f MB\n",
                    values[2].names,
                    (uint64_t)(vm_stat.active_count +
                        vm_stat.inactive_count +
                        vm_stat.wire_count)*pageSize >> 20,
                    value64 / 1e+06);
        }
    }
}

/* forget about 80 columns here... thanks apple! */
/* from bottomy */
static void gpu(void) {
    io_iterator_t Iterator;
    kern_return_t err = IOServiceGetMatchingServices(kIOMasterPortDefault,
		IOServiceMatching("IOPCIDevice"), &Iterator);

    if (err != KERN_SUCCESS) {
        fprintf(stderr, "IOServiceGetMatchingServices failed: %u\n", err);
    }

    for (io_service_t Device; IOIteratorIsValid(Iterator)
		&& (Device = IOIteratorNext(Iterator)); IOObjectRelease(Device)) {
        CFStringRef Name = IORegistryEntrySearchCFProperty(Device,
                kIOServicePlane,
                CFSTR("IOName"),
                kCFAllocatorDefault,
                kNilOptions);

        if (Name) {
            if (CFStringCompare(Name,CFSTR("display"), 0) == kCFCompareEqualTo) {
                CFDataRef Model = IORegistryEntrySearchCFProperty(Device,
                        kIOServicePlane,
                        CFSTR("model"),
                        kCFAllocatorDefault,
                        kNilOptions);

                if(Model) {
                    bool ValueInBytes = true;
                    CFTypeRef VRAM = IORegistryEntrySearchCFProperty(Device,
                            kIOServicePlane,
                            CFSTR("VRAM,totalsize"),
                            kCFAllocatorDefault,
                            kIORegistryIterateRecursively);

                    if (!VRAM) {
                        ValueInBytes = false;
                        VRAM = IORegistryEntrySearchCFProperty(Device,
                                kIOServicePlane,
                                CFSTR("VRAM,totalMB"),
                                kCFAllocatorDefault,
                                kIORegistryIterateRecursively);
                    }

                    if (VRAM) {
                        mach_vm_size_t Size = 0;
                        CFTypeID Type = CFGetTypeID(VRAM);

                        if (Type==CFDataGetTypeID()) {

                            Size=(CFDataGetLength(VRAM) == sizeof(uint32_t) ?
                                    (mach_vm_size_t)*(const uint32_t*)CFDataGetBytePtr(VRAM):
                                    *(const uint64_t*)CFDataGetBytePtr(VRAM));

						} else if (Type == CFNumberGetTypeID()) {
							CFNumberGetValue(VRAM,
								kCFNumberSInt64Type, &Size);
						}

						if (ValueInBytes) {
							Size >>= 20;
						}

						printf(RED"Graphics  : "NOR"%s @ %llu MB\n", CFDataGetBytePtr(Model),Size);
						CFRelease(Model);

					} else {
						printf(RED"Graphic  : "NOR"%s @ Unknown VRAM Size\n",
						CFDataGetBytePtr(Model));
						CFRelease(Model);
					}
				}
				CFRelease(Name);
			}
		}
	}
}

void help(void) {
	printf("Mac OS X Info program by nbyouri April 2016\n"
		"\t-a shows a colored apple\n"
		"\t-h shows this help message\n");

	exit(EXIT_SUCCESS);
}

static void get_sysctl(enum sysctls ctl) {
	size_t len;
	if (ctl == MEM) {
		mem();
	} else {
		sysctlbyname(values[ctl].ctls, NULL, &len, NULL, 0);
		char *type = malloc(len);
		sysctlbyname(values[ctl].ctls, type, &len, NULL, 0);
		printf(RED"%-10s: "NOR"%s\n", values[ctl].names, type);
		free(type);
	}
}

static void get_env(enum envs env) {
	char *type;
	struct passwd *passwd;

	if (env == TERM) {
		type = getenv("TERM");
		printf(RED"Terminal  : "NOR"%s\n", type);

	} else if (env == SHELL) {
		passwd = getpwuid(getuid());
		printf(RED"Shell     : "NOR"%s\n", passwd->pw_shell);

	} else if (env == USER) {
		passwd = getpwuid(getuid());
		printf(RED"User      : "NOR"%s\n", passwd->pw_name);
	}
}

/*
 * Get package count from pkg_install file database
 */
static void get_pkg_count(void) {
	int	 	pkgs = 0;
	struct dirent	**namelist;

	printf(RED"Packages  : ");
	pkgs = scandir(DBDIR, &namelist, NULL, NULL);

	if (pkgs < 0) {
		printf(NOR"no packages found\n");
		return;
	}

	for (int i = 0; i < pkgs; i++) {
		free(namelist[i]);
	}
	free(namelist);

	/* amount of directories found excluding . .. and the pkg.byfile.db */
	printf(NOR"%d\n", pkgs - 3);
}

static void disk(void) {
	struct statvfs info;

	if (!statvfs("/", &info)) {
		unsigned long left  = (info.f_bavail * info.f_frsize);
		unsigned long total = (info.f_blocks * info.f_frsize);
		unsigned long used  = total - left;
		float perc  = (float)used / (float)total;

		printf(RED"Disk      : "NOR"%.2f%% of %.2f GB\n",
			perc * 100, (float)total / 1e+09);
	}
}

static void uptime(time_t *nowp) {
	struct timeval boottime;
	time_t uptime;
	int days, hrs, mins, secs;
	int mib[2];
	size_t size;
	char buf[256];

	if (strftime(buf, sizeof(buf), NULL, localtime(nowp)))
		mib[0] = CTL_KERN;

	mib[1] = KERN_BOOTTIME;
	size = sizeof(boottime);

	if (sysctl(mib, 2, &boottime, &size, NULL, 0) != -1 &&
		boottime.tv_sec) {
		uptime = *nowp - boottime.tv_sec;

		if (uptime > 60)
			uptime += 30;

		days = (int)uptime / 86400;
		uptime %= 86400;
		hrs = (int)uptime / 3600;
		uptime %= 3600;
		mins = (int)uptime / 60;
		secs = uptime % 60;
		printf(RED"Uptime    : "NOR);

		if (days > 0)
			printf("%d day%s", days, days > 1 ? "s " : " ");

		if (hrs > 0 && mins > 0)
			printf("%02d:%02d", hrs, mins);

		else if (hrs == 0 && mins > 0)
			printf("0:%02d", mins);

		else
			printf("0:00");

		putchar('\n');
	}
}

int main(int argc, char **argv) {
	bool print_with_apple = false;

	if (argc >= 2) {
		int c;

		while ((c = getopt(argc, argv, "ha")) != -1) {
			switch (c) {

			case 'a':
				print_with_apple = true;
				break;

			case 'h':
			default:
				help();
				break;
			}
		}
	}

	if (print_with_apple) {
		print_apple();

	} else {
		time_t now;
		time(&now);

		get_env(USER);
		curtime();
		get_sysctl(MODEL);
		get_sysctl(CPU);
		get_sysctl(OSTYPE);
		get_sysctl(OSREL);
		disk();
		mem();
		get_env(SHELL);
		get_env(TERM);
		get_sysctl(MEM);
		gpu();
		get_pkg_count();
		uptime(&now);
	}

	return EXIT_SUCCESS;
}
