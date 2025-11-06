#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <shlwapi.h>
#include <winioctl.h>

#pragma comment(lib, "shlwapi.lib")

// Version 0.7 - Added log copying functionality

void printMenu();
void wipeDrives();
void moveSMSTSLogs();
void findUSBDrives(char drives[][4], int *count);
BOOL wipePhysicalDrive(int driveNumber);
BOOL copyFileToUSB(const char *source, const char *destination);

int main()
{
    int choice;

    printf("=== WinPE System Utility ===\n");
    printf("Version 0.7 - Beta Build\n\n");

    while (1)
    {
        printMenu();
        printf("Enter your choice (1-3): ");
        scanf("%d", &choice);

        switch (choice)
        {
        case 1:
            wipeDrives();
            break;
        case 2:
            moveSMSTSLogs();
            break;
        case 3:
            printf("Exiting...\n");
            return 0;
        default:
            printf("Invalid choice. Please try again.\n");
        }

        printf("\nPress Enter to continue...");
        getchar();
        getchar();
        system("cls");
    }

    return 0;
}

void printMenu()
{
    printf("=== Main Menu ===\n");
    printf("1. Wipe Drives\n");
    printf("2. Move SMSTS Logs to USB\n");
    printf("3. Exit\n\n");
}

void wipeDrives()
{
    char response;
    printf("=== DRIVE WIPE UTILITY ===\n");
    printf("WARNING: This will PERMANENTLY erase all data!\n");
    printf("Continue? (y/N): ");
    scanf(" %c", &response);

    if (response != 'y' && response != 'Y')
    {
        printf("Operation cancelled.\n");
        return;
    }

    printf("\nAvailable Physical Drives:\n");
    for (int i = 0; i < 10; i++)
    {
        char drivePath[32];
        sprintf(drivePath, "\\\\.\\PhysicalDrive%d", i);

        HANDLE hDrive = CreateFile(drivePath, GENERIC_READ,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   NULL, OPEN_EXISTING, 0, NULL);

        if (hDrive != INVALID_HANDLE_VALUE)
        {
            DISK_GEOMETRY diskGeometry;
            DWORD bytesReturned;

            if (DeviceIoControl(hDrive, IOCTL_DISK_GET_DRIVE_GEOMETRY,
                                NULL, 0, &diskGeometry, sizeof(diskGeometry),
                                &bytesReturned, NULL))
            {

                ULONGLONG totalSize = diskGeometry.Cylinders.QuadPart *
                                      diskGeometry.TracksPerCylinder *
                                      diskGeometry.SectorsPerTrack *
                                      diskGeometry.BytesPerSector;

                printf("Drive %d: %.2f GB\n", i, (double)totalSize / (1024 * 1024 * 1024));
            }
            CloseHandle(hDrive);
        }
    }

    int driveNum;
    printf("\nEnter drive number to wipe (or -1 to cancel): ");
    scanf("%d", &driveNum);

    if (driveNum == -1)
    {
        printf("Operation cancelled.\n");
        return;
    }

    printf("\nFinal confirmation: Wipe PhysicalDrive%d? (y/N): ", driveNum);
    scanf(" %c", &response);

    if (response == 'y' || response == 'Y')
    {
        if (wipePhysicalDrive(driveNum))
        {
            printf("Drive %d wiped successfully.\n", driveNum);
        }
        else
        {
            printf("Failed to wipe drive %d.\n", driveNum);
        }
    }
    else
    {
        printf("Operation cancelled.\n");
    }
}

BOOL wipePhysicalDrive(int driveNumber)
{
    char drivePath[32];
    sprintf(drivePath, "\\\\.\\PhysicalDrive%d", driveNumber);

    HANDLE hDrive = CreateFile(drivePath, GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL, OPEN_EXISTING, 0, NULL);

    if (hDrive == INVALID_HANDLE_VALUE)
    {
        printf("Error: Cannot open drive %d. Error code: %lu\n", driveNumber, GetLastError());
        return FALSE;
    }

    DISK_GEOMETRY diskGeometry;
    DWORD bytesReturned;

    if (!DeviceIoControl(hDrive, IOCTL_DISK_GET_DRIVE_GEOMETRY,
                         NULL, 0, &diskGeometry, sizeof(diskGeometry),
                         &bytesReturned, NULL))
    {
        printf("Error: Cannot get drive geometry. Error code: %lu\n", GetLastError());
        CloseHandle(hDrive);
        return FALSE;
    }

    ULONGLONG totalSectors = diskGeometry.Cylinders.QuadPart *
                             diskGeometry.TracksPerCylinder *
                             diskGeometry.SectorsPerTrack;

    const DWORD bufferSize = 1024 * 1024;
    BYTE *buffer = (BYTE *)calloc(bufferSize, 1);

    if (!buffer)
    {
        printf("Error: Cannot allocate memory for wiping.\n");
        CloseHandle(hDrive);
        return FALSE;
    }

    printf("Wiping drive... This may take a while.\n");

    ULONGLONG bytesWritten = 0;
    ULONGLONG totalBytes = totalSectors * diskGeometry.BytesPerSector;
    DWORD written;

    while (bytesWritten < totalBytes)
    {
        DWORD toWrite = (DWORD)min(bufferSize, totalBytes - bytesWritten);

        if (!WriteFile(hDrive, buffer, toWrite, &written, NULL))
        {
            printf("Error writing to drive. Error code: %lu\n", GetLastError());
            break;
        }

        bytesWritten += written;

        if ((bytesWritten / (100 * 1024 * 1024)) > ((bytesWritten - written) / (100 * 1024 * 1024)))
        {
            printf("Progress: %.1f%%\n", (double)bytesWritten / totalBytes * 100);
        }
    }

    free(buffer);
    CloseHandle(hDrive);

    return (bytesWritten == totalBytes);
}

void moveSMSTSLogs()
{
    printf("=== SMSTS LOG MOVER ===\n");

    char usbDrives[26][4];
    int usbCount = 0;
    findUSBDrives(usbDrives, &usbCount);

    if (usbCount == 0)
    {
        printf("No USB drives found. Please insert a USB drive.\n");
        return;
    }

    printf("Available USB drives:\n");
    for (int i = 0; i < usbCount; i++)
    {
        printf("%d. %s\n", i + 1, usbDrives[i]);
    }

    int choice;
    printf("Select USB drive (1-%d): ", usbCount);
    scanf("%d", &choice);

    if (choice < 1 || choice > usbCount)
    {
        printf("Invalid selection.\n");
        return;
    }

    char *targetDrive = usbDrives[choice - 1];

    // Common SMSTS log locations
    const char *logPaths[] = {
        "C:\\Windows\\CCM\\Logs\\smsts.log",
        "C:\\_SMSTaskSequence\\Logs\\smsts.log",
        "X:\\Windows\\Temp\\SMSTSLog\\smsts.log",
        "C:\\SMSTSLog\\smsts.log"};

    char destPath[MAX_PATH];
    sprintf(destPath, "%s\\SMSTSLogs", targetDrive);
    CreateDirectory(destPath, NULL);

    printf("Searching for SMSTS logs...\n");
    int foundLogs = 0;

    for (int i = 0; i < sizeof(logPaths) / sizeof(logPaths[0]); i++)
    {
        if (PathFileExists(logPaths[i]))
        {
            char fileName[MAX_PATH];
            char destFile[MAX_PATH];

            strcpy(fileName, PathFindFileName(logPaths[i]));
            sprintf(destFile, "%s\\%s", destPath, fileName);

            if (copyFileToUSB(logPaths[i], destFile))
            {
                printf("Copied: %s\n", logPaths[i]);
                foundLogs++;
            }
            else
            {
                printf("Failed to copy: %s\n", logPaths[i]);
            }
        }
    }

    // Search for additional log files
    WIN32_FIND_DATA findData;
    HANDLE hFind;

    const char *logDirs[] = {
        "C:\\Windows\\CCM\\Logs\\",
        "C:\\_SMSTaskSequence\\Logs\\",
        "X:\\Windows\\Temp\\SMSTSLog\\",
        "C:\\SMSTSLog\\"};

    for (int i = 0; i < sizeof(logDirs) / sizeof(logDirs[0]); i++)
    {
        char searchPattern[MAX_PATH];
        sprintf(searchPattern, "%s*.log", logDirs[i]);

        hFind = FindFirstFile(searchPattern, &findData);
        if (hFind != INVALID_HANDLE_VALUE)
        {
            do
            {
                char sourcePath[MAX_PATH];
                char destFile[MAX_PATH];
                sprintf(sourcePath, "%s%s", logDirs[i], findData.cFileName);
                sprintf(destFile, "%s\\%s", destPath, findData.cFileName);

                if (copyFileToUSB(sourcePath, destFile))
                {
                    foundLogs++;
                }
            } while (FindNextFile(hFind, &findData));
            FindClose(hFind);
        }
    }

    printf("\nTotal logs copied: %d\n", foundLogs);
    printf("Logs saved to: %s\n", destPath);
}

void findUSBDrives(char drives[][4], int *count)
{
    *count = 0;
    DWORD drivesMask = GetLogicalDrives();

    for (int i = 0; i < 26; i++)
    {
        if (drivesMask & (1 << i))
        {
            char driveLetter[4];
            sprintf(driveLetter, "%c:\\", 'A' + i);

            UINT driveType = GetDriveType(driveLetter);
            if (driveType == DRIVE_REMOVABLE)
            {
                strcpy(drives[*count], driveLetter);
                (*count)++;
            }
        }
    }
}

BOOL copyFileToUSB(const char *source, const char *destination)
{
    return CopyFile(source, destination, FALSE);
}
