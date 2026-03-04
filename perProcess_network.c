//It Reuqires AppNetworkCounter(windows Utility) to run
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define CSV_PATH "appnet.csv"
#define MAX_PROCNAME_LEN 24

// Helper to trim quotes
void trim_quotes(char* str) {
    size_t len = strlen(str);
    if (len > 1 && str[0] == '"' && str[len-1] == '"') {
        memmove(str, str+1, len-2);
        str[len-2] = '\0';
    }
}

// Simple CSV parser
int parse_csv_line(char *line, char *fields[], int max_fields) {
    int count = 0;
    char *ptr = line;
    while (*ptr && count < max_fields) {
        while (*ptr == ' ' || *ptr == '\t') ptr++;
        if (*ptr == '"') {
            ptr++;
            fields[count++] = ptr;
            while (*ptr && !(*ptr == '"' && (*(ptr+1) == ',' || *(ptr+1) == '\n' || *(ptr+1) == '\0'))) ptr++;
            if (*ptr == '"') {
                *ptr = '\0';
                ptr++;
            }
            if (*ptr == ',') ptr++;
        } else {
            fields[count++] = ptr;
            while (*ptr && *ptr != ',' && *ptr != '\n') ptr++;
            if (*ptr == ',' || *ptr == '\n') {
                *ptr = '\0';
                ptr++;
            }
        }
    }
    return count;
}

int main() {
    // 1. Run AppNetworkCounter for 5 seconds
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
        "AppNetworkCounter.exe /CaptureTime 15000 /scomma %s", CSV_PATH);

    printf("Collecting network usage data (5 seconds)...\n");
    int ret = system(cmd);
    if (ret != 0) {
        printf("Failed to run AppNetworkCounter.exe.\n");
        return 1;
    }

    // 2. Open CSV
    FILE* fp = fopen(CSV_PATH, "r");
    if (!fp) {
        printf("Failed to open %s\n", CSV_PATH);
        return 1;
    }

    char line[1024];
    if (!fgets(line, sizeof(line), fp)) {
        printf("CSV file is empty or invalid.\n");
        fclose(fp);
        return 1;
    }

    // Print header
    printf("%-24s %15s %15s %15s %15s\n", "Process Name", "Recv Bytes", "Sent Bytes", "Recv Packets", "Sent Packets");
    printf("---------------------------------------------------------------------------------------------------\n");

    while (fgets(line, sizeof(line), fp)) {
        char *fields[10] = {0};
        int field_count = parse_csv_line(line, fields, 10);
        if (field_count < 8) continue;

        char procname[MAX_PROCNAME_LEN];
        trim_quotes(fields[0]); // Process Name
        strncpy(procname, fields[0], MAX_PROCNAME_LEN-1);
        procname[MAX_PROCNAME_LEN-1] = '\0';
        trim_quotes(fields[2]); // Bytes Received
        trim_quotes(fields[3]); // Bytes Sent
        trim_quotes(fields[6]); // Packets Received
        trim_quotes(fields[7]); // Packets Sent

        printf("%-24s %15s %15s %15s %15s\n",
            procname,
            fields[2][0] ? fields[2] : "0",
            fields[3][0] ? fields[3] : "0",
            fields[6][0] ? fields[6] : "0",
            fields[7][0] ? fields[7] : "0");
    }

    fclose(fp);
    remove(CSV_PATH);

    printf("\nDone.\n");
    return 0;
}
