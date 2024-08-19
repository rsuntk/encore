/*
 * Original by @Rem01 Gaming
 * Reworked by Rissu/Faris sang Developer Amatir di Yukiprjkt
 *
 * What changed:
 * - Implement ksu prctl symbol,
 * - Optimize some code.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/prctl.h>

#define MAX_OUTPUT_LEN 1024
#define MAX_CMD_LEN 512

#define KERNEL_SU_OPTION 0xDEADBEEF

static char cmd[MAX_CMD_LEN];
static char path[256];
static bool is_root_ksu = false;

static bool ksuctl(int cmd_ksu, void *arg1, void *arg2)
{
	int32_t result = 0;
	prctl(KERNEL_SU_OPTION, cmd_ksu, arg1, arg2, &result);
	return result == KERNEL_SU_OPTION;
}

static int get_ksu_version(void)
{
	int32_t ksu_version = 0;
	ksuctl(2, &ksu_version, NULL);
	return ksu_version;
}

// 0 = non-KSU, >1 = KSU
static void get_root_method(void)
{
	int ksu_version = get_ksu_version();
	if (ksu_version != 0)
		is_root_ksu = true;
}

static void ksu_escape_to_root(void)
{
	if (is_root_ksu) {
		bool status = ksuctl(0, 0, NULL);
		if (!status) {
			fprintf(stderr, "Permission denied\n");
			exit(EXIT_FAILURE);
		}
	}
}

static char *trim_newline(char *str)
{
	char *end;	
	if (str == NULL)
		return NULL;
	
	if ((end = strchr(str, '\n')) != NULL) {
		*end = '\0';
	}
	return str;
}

/*
 * execute_command has changed!
 * From execute_command(const char *cmd);
 * to execute_cmd(const char *cmd, bool root);
 *
 * Usage example:
 * execute_cmd("sh /sdcard/hello.sh", true);
 * Run hello.sh as Root
 *
 */
static char *execute_cmd(const char *cmd, bool root)
{
	FILE *fp;
	char buf[MAX_OUTPUT_LEN];
	char *result = NULL;
	size_t result_len = 0;
	
	if (is_root_ksu && root)
		ksu_escape_to_root();
	else if (!is_root_ksu && root)
		system("su");
	
	fp = popen(cmd, "r");
	if (fp == NULL) {
		fprintf(stderr, "error: can't execute cmd: %s\n", cmd);
		return NULL;
	}
	
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		size_t buf_len = strlen(buf);
		char *new_result = realloc(result, result_len + buf_len + 1);
		
		if (new_result == NULL) {
			fprintf(stderr, "error: memory allocation err.\n");
			free(result);
			pclose(fp);
			return NULL;
		}
		
		result = new_result;
		strcpy(result + result_len, buf);
		result_len += buf_len;
	}
	
	if (result != NULL)
		result[result_len] = '\0';
	
	if (pclose(fp) == -1) {
		fprintf(stderr, "error: closing cmd stream.\n");
	}
	
	if (!is_root_ksu && root)
		system("exit");
		
	return result;
}

// Allow ksu and magisk/apatch `su -c`
static void su_c(char *su_cmd)
{
	int ksu_version_code = get_ksu_version();
	
	if (is_root_ksu) {
		ksu_escape_to_root();
		printf("KSU Version: %d\n", ksu_version_code);
		system(su_cmd);
	} else {
		snprintf(cmd, sizeof(cmd), "su -c %s", su_cmd);
		system(cmd);
	}
}

static void setPriorities(const char *pid)
{
	snprintf(cmd, sizeof(cmd), "encore-setpriority %s", pid);
	su_c(cmd);
}

static void perf_common(void) { su_c("encore-perfcommon"); }
static void perf_mode(void) { su_c("encore-performance"); }
static void normal_mode(void) { su_c("encore-normal"); }
static void powersave_mode(void)
{
	normal_mode();
	su_c("encore-powersave");
}

int main(void)
{
	char *gamestart = NULL;
	char *screenstate = NULL;
	char *lpm = NULL;
	char *pid = NULL;
	int cur_mode = -1;
	
	get_root_method();
	perf_common();
	
	while (1) {
		if (!gamestart) {
			execute_cmd("sh /data/encore/AppMonitoringUtil.sh | head -n 1", false);
		} else {
			snprintf(path, sizeof(path), "/proc/%s", trim_newline(pid));
			if (access(path, F_OK) == -1) {
				free(pid);
				pid = NULL;
				free(gamestart);
				gamestart = NULL;
				gamestart = execute_cmd("sh /data/encore/AppMonitoringUtil.sh | head -n 1", false);
			}
		}
		
		// TODO: Create working screenstate
		screenstate = execute_cmd(
			"dumpsys power | grep -Eo "
			"\"mWakefulness=Awake|mWakefulness=Asleep\" | awk -F'=' '{print $2}'", true);
			      	
		lpm = execute_cmd(
			"dumpsys power | grep -Eo "
			"\"mSettingBatterySaverEnabled=true|mSettingBatterySaverEnabled="
			"false\" | awk -F'=' '{print $2}'", true);
		
		if (screenstate == NULL) {
			fprintf(stderr, "error: screenstate is null!\n");
		} else if (gamestart && strcmp(trim_newline(screenstate), "Awake") == 0) {
			if (cur_mode != 1) {
				cur_mode = 1;
				printf("Applying performance mode\n");
				snprintf(cmd, sizeof(cmd),
					"/system/bin/am start -a android.intent.action.MAIN -e toasttext "
					"\"Boosting game %s\" -n bellavita.toast/.MainActivity",
					trim_newline(gamestart));
				system(cmd);
				perf_mode();
				
				snprintf(cmd, sizeof(cmd), "pidof %s", trim_newline(gamestart));
				pid = execute_cmd(cmd, false);
				
				if (pid != NULL)
					setPriorities(trim_newline(pid));
				else
					fprintf(stderr, "error: Game PID is null, can't set priority\n");
			}
		} else if (lpm && strcmp(trim_newline(lpm), "true") == 0) {
				if (cur_mode != 2) {
					cur_mode = 2;
					printf("Applying powersave mode\n");
					powersave_mode();
				}
		} else {
				if (cur_mode != 0) {
					cur_mode = 0;
					printf("Applying normal mode\n");
					normal_mode();
				}
		}
		
		if (gamestart)
			printf("gamestart: %s\n", trim_newline(gamestart));
		else
			fprintf(stderr, "gamestart: NULL\n");
			
		if (screenstate) {
			printf("screenstate: %s\n", trim_newline(screenstate));
			free(screenstate);
			screenstate = NULL;
		} else {
			fprintf(stderr, "screenstate: NULL\n");
		}
		
		if (lpm) {
			printf("low_power: %s\n", trim_newline(lpm));
			free(lpm);
			lpm = NULL;
		} else {
			fprintf(stderr, "low_power: NULL\n");
		}
		sleep(12);
	}
	return 0;
}
