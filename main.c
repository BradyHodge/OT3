#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winioctl.h>

// Version 0.5 - Added drive wiping capability

void printMenu();
void wipeDrives();
void moveSMSTSLogs();
void findUSBDrives(char drives[][4], int *count);
BOOL wipePhysicalDrive(int driveNumber);

int main()
{
    int choice;

    printf("=== WinPE System Utility ===\n");
    printf("Version 0.5 - Development Build\n\n");

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
    printf("WARNING: This will erase all data!\n");
    printf("Continue? (y/N): ");
    scanf(" %c", &response);

    if (response != 'y' && response != 'Y')
    {
        printf("Cancelled.\n");
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
    printf("\nEnter drive number (-1 to cancel): ");
    scanf("%d", &driveNum);

    if (driveNum == -1)
    {
        printf("Cancelled.\n");
        return;
    }

    printf("\nFinal confirmation for Drive %d? (y/N): ", driveNum);
    scanf(" %c", &response);

    if (response == 'y' || response == 'Y')
    {
        if (wipePhysicalDrive(driveNum))
        {
            printf("Drive wiped successfully.\n");
        }
        else
        {
            printf("Wipe failed.\n");
        }
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
        printf("Cannot open drive. Error: %lu\n", GetLastError());
        return FALSE;
    }

    DISK_GEOMETRY diskGeometry;
    DWORD bytesReturned;

    if (!DeviceIoControl(hDrive, IOCTL_DISK_GET_DRIVE_GEOMETRY,
                         NULL, 0, &diskGeometry, sizeof(diskGeometry),
                         &bytesReturned, NULL))
    {
        CloseHandle(hDrive);
        return FALSE;
    }

    ULONGLONG totalSectors = diskGeometry.Cylinders.QuadPart *
                             diskGeometry.TracksPerCylinder *
                             diskGeometry.SectorsPerTrack;

    const DWORD bufferSize = 1024 * 1024; // 1MB buffer
    BYTE *buffer = (BYTE *)calloc(bufferSize, 1);

    if (!buffer)
    {
        CloseHandle(hDrive);
        return FALSE;
    }

    printf("Wiping...\n");

    ULONGLONG bytesWritten = 0;
    ULONGLONG totalBytes = totalSectors * diskGeometry.BytesPerSector;
    DWORD written;

    while (bytesWritten < totalBytes)
    {
        DWORD toWrite = (DWORD)min(bufferSize, totalBytes - bytesWritten);

        if (!WriteFile(hDrive, buffer, toWrite, &written, NULL))
        {
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
        printf("No USB drives found.\n");
        return;
    }

    printf("Available USB drives:\n");
    for (int i = 0; i < usbCount; i++)
    {
        printf("%d. %s\n", i + 1, usbDrives[i]);
    }

    printf("\nLog copying feature coming soon.\n");
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
